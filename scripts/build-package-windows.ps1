#requires -Version 5.1

[CmdletBinding()]
param(
    [string]$QtRoot,
    [string]$VcpkgRoot,
    [string]$VcpkgInstalledDir,
    [string]$Generator,
    [string]$BackendSourceDir,
    [string]$TagReaderSourceDir,
    [switch]$Force,
    [switch]$KeepBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$PackageName = 'Seriona-windows-x64'
$PackageDirectory = Join-Path $RepoRoot $PackageName
$PackageArchive = Join-Path $RepoRoot ($PackageName + '.zip')
$RunId = '{0}-{1}' -f $PID, (Get-Date -Format 'yyyyMMddHHmmss')
$BuildDirectory = Join-Path $RepoRoot ('.build-windows-release-' + $RunId)
$StagingParent = Join-Path $RepoRoot ('.' + $PackageName + '.staging-' + $RunId)
$StagingDirectory = Join-Path $StagingParent $PackageName
$SmokeDirectory = Join-Path $BuildDirectory 'package-smoke'

$CreatedBuildDirectory = $false
$CreatedStagingDirectory = $false
$TemporaryArchive = $null

function Write-Phase {
    param([string]$Message)
    Write-Host ('==> ' + $Message)
}

function Get-CommandPath {
    param([Parameter(Mandatory)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        return $null
    }
    return $command.Source
}

function Get-FullPath {
    param([Parameter(Mandatory)][string]$Path)

    return [IO.Path]::GetFullPath($Path)
}

function Test-Executable {
    param([Parameter(Mandatory)][string]$Path)

    return (Test-Path -LiteralPath $Path -PathType Leaf)
}

function Import-VisualStudioEnvironment {
    if ($null -ne (Get-CommandPath 'cl.exe')) {
        return
    }

    $vswhere = Get-CommandPath 'vswhere.exe'
    if ([string]::IsNullOrWhiteSpace($vswhere)) {
        $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
        $programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles')
        $vswhereCandidates = @()
        if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
            $vswhereCandidates += Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
        }
        if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
            $vswhereCandidates += Join-Path $programFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
        }
        foreach ($candidate in $vswhereCandidates) {
            if (Test-Executable $candidate) {
                $vswhere = $candidate
                break
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($vswhere)) {
        throw 'Microsoft C++ was not found. Open a Visual Studio Developer PowerShell or install the Visual Studio C++ desktop workload.'
    }

    $installationPath = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installationPath)) {
        throw 'Visual Studio with the x64 C++ build tools was not found.'
    }

    $vsDevCmd = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Executable $vsDevCmd)) {
        throw ('Visual Studio environment script was not found: ' + $vsDevCmd)
    }

    Write-Phase 'Importing the Visual Studio x64 build environment'
    $command = 'call "{0}" -arch=x64 -host_arch=x64 >nul && set' -f $vsDevCmd
    $environmentLines = @(& $env:ComSpec /d /s /c $command)
    if ($LASTEXITCODE -ne 0) {
        throw 'Visual Studio environment initialization failed.'
    }

    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -le 0) {
            continue
        }
        $name = $line.Substring(0, $separator)
        if ($name.StartsWith('=')) {
            continue
        }
        $value = $line.Substring($separator + 1)
        [Environment]::SetEnvironmentVariable($name, $value, 'Process')
    }

    if ($null -eq (Get-CommandPath 'cl.exe')) {
        throw 'Visual Studio environment initialization completed, but cl.exe is still unavailable.'
    }
}

function Get-CMakeGenerator {
    param([Parameter(Mandatory)][string]$CMakePath)

    if (-not [string]::IsNullOrWhiteSpace($Generator)) {
        return $Generator
    }

    $ninja = Get-CommandPath 'ninja.exe'
    if (-not [string]::IsNullOrWhiteSpace($ninja)) {
        return 'Ninja'
    }

    $help = @(& $CMakePath '--help')
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to query CMake generators.'
    }

    $visualStudioGenerators = @()
    foreach ($line in $help) {
        if ($line -match '^\s*\*?\s*(Visual Studio \d+ \d{4})\s*=') {
            $visualStudioGenerators += $Matches[1]
        }
    }
    if ($visualStudioGenerators.Count -eq 0) {
        throw 'Ninja was not found and CMake exposes no Visual Studio generator.'
    }

    return ($visualStudioGenerators | Sort-Object -Descending | Select-Object -First 1)
}

function Resolve-QtInstallation {
    $candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($QtRoot)) {
        $candidates.Add($QtRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:Qt6_ROOT)) {
        $candidates.Add($env:Qt6_ROOT)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:Qt6_DIR)) {
        $candidates.Add($env:Qt6_DIR)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:CMAKE_PREFIX_PATH)) {
        foreach ($entry in $env:CMAKE_PREFIX_PATH.Split(';')) {
            if (-not [string]::IsNullOrWhiteSpace($entry)) {
                $candidates.Add($entry)
            }
        }
    }

    $qtCMake = Get-CommandPath 'qt-cmake.bat'
    if (-not [string]::IsNullOrWhiteSpace($qtCMake)) {
        $candidates.Add((Split-Path (Split-Path $qtCMake -Parent) -Parent))
    }

    $windeployqtOnPath = Get-CommandPath 'windeployqt.exe'
    if (-not [string]::IsNullOrWhiteSpace($windeployqtOnPath)) {
        $candidates.Add((Split-Path (Split-Path $windeployqtOnPath -Parent) -Parent))
    }

    foreach ($candidate in $candidates) {
        $fullCandidate = Get-FullPath $candidate
        $prefix = $fullCandidate
        $leaf = Split-Path $fullCandidate -Leaf
        $parentLeaf = Split-Path (Split-Path $fullCandidate -Parent) -Leaf
        if ($leaf -eq 'Qt6' -and $parentLeaf -eq 'cmake') {
            $prefix = Split-Path (Split-Path (Split-Path $fullCandidate -Parent) -Parent) -Parent
        }
        if ($leaf -eq 'bin') {
            $prefix = Split-Path $fullCandidate -Parent
        }

        $qt6Dir = Join-Path $prefix 'lib\cmake\Qt6'
        $windeployqt = Join-Path $prefix 'bin\windeployqt.exe'
        if ((Test-Path -LiteralPath (Join-Path $qt6Dir 'Qt6Config.cmake') -PathType Leaf) -and
            (Test-Executable $windeployqt)) {
            return [PSCustomObject]@{
                Prefix = $prefix
                Qt6Dir = $qt6Dir
                WindeployQt = $windeployqt
            }
        }
    }

    throw 'Qt 6 MSVC installation was not found. Set -QtRoot to a Qt prefix containing lib\cmake\Qt6 and bin\windeployqt.exe.'
}

function Resolve-VcpkgInstallation {
    $rootCandidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($VcpkgRoot)) {
        $rootCandidates.Add($VcpkgRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        $rootCandidates.Add($env:VCPKG_ROOT)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:CMAKE_TOOLCHAIN_FILE)) {
        $toolchain = Get-FullPath $env:CMAKE_TOOLCHAIN_FILE
        $rootCandidates.Add((Split-Path (Split-Path (Split-Path $toolchain -Parent) -Parent) -Parent))
    }
    $vcpkgCommand = Get-CommandPath 'vcpkg.exe'
    if (-not [string]::IsNullOrWhiteSpace($vcpkgCommand)) {
        $rootCandidates.Add((Split-Path $vcpkgCommand -Parent))
    }
    if (-not [string]::IsNullOrWhiteSpace($VcpkgInstalledDir)) {
        $installedCandidate = Get-FullPath $VcpkgInstalledDir
        if ((Split-Path $installedCandidate -Leaf) -eq 'x64-windows') {
            $rootCandidates.Add((Split-Path (Split-Path $installedCandidate -Parent) -Parent))
        } else {
            $rootCandidates.Add((Split-Path $installedCandidate -Parent))
        }
    }

    foreach ($candidate in $rootCandidates) {
        $root = Get-FullPath $candidate
        $toolchainPath = Join-Path $root 'scripts\buildsystems\vcpkg.cmake'
        $installedRoot = Join-Path $root 'installed'
        if (-not [string]::IsNullOrWhiteSpace($VcpkgInstalledDir)) {
            $explicitInstalled = Get-FullPath $VcpkgInstalledDir
            if ((Split-Path $explicitInstalled -Leaf) -eq 'x64-windows') {
                $installedRoot = Split-Path $explicitInstalled -Parent
            } else {
                $installedRoot = $explicitInstalled
            }
        }
        $tripletBin = Join-Path $installedRoot 'x64-windows\bin'
        $applocal = Join-Path $root 'scripts\buildsystems\msbuild\applocal.ps1'
        if ((Test-Path -LiteralPath $toolchainPath -PathType Leaf) -and
            (Test-Path -LiteralPath $tripletBin -PathType Container) -and
            (Test-Path -LiteralPath $applocal -PathType Leaf)) {
            return [PSCustomObject]@{
                Root = $root
                Toolchain = $toolchainPath
                InstalledRoot = $installedRoot
                TripletBin = $tripletBin
                AppLocal = $applocal
            }
        }
    }

    throw 'vcpkg was not found. Set -VcpkgRoot or VCPKG_ROOT to a vcpkg installation with the x64-windows triplet installed.'
}

function Resolve-SourceDirectory {
    param(
        [Parameter(Mandatory)][string]$Name,
        [string]$ExplicitPath,
        [Parameter(Mandatory)][string]$SiblingName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $resolved = Get-FullPath $ExplicitPath
        if (-not (Test-Path -LiteralPath (Join-Path $resolved 'CMakeLists.txt') -PathType Leaf)) {
            throw ($Name + ' source directory does not contain CMakeLists.txt: ' + $resolved)
        }
        return $resolved
    }

    $sibling = Get-FullPath (Join-Path $RepoRoot ('..\' + $SiblingName))
    if (Test-Path -LiteralPath (Join-Path $sibling 'CMakeLists.txt') -PathType Leaf) {
        return $sibling
    }
    return $null
}

function Invoke-Native {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$Description
    )

    Write-Host ('    ' + $FilePath + ' ' + ($Arguments -join ' '))
    & $FilePath @Arguments
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw ('Failed to ' + $Description + ' (exit code ' + $exitCode + ').')
    }
}

function Find-ReleaseExecutable {
    $directCandidates = @(
        (Join-Path $BuildDirectory 'appSeriona.exe'),
        (Join-Path $BuildDirectory 'Release\appSeriona.exe')
    )
    foreach ($candidate in $directCandidates) {
        if (Test-Executable $candidate) {
            return $candidate
        }
    }
    throw 'Release appSeriona.exe was not produced by the build.'
}

function Require-PackageFile {
    param([Parameter(Mandatory)][string]$RelativePath)

    $path = Join-Path $StagingDirectory $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw ('The package is missing required file: ' + $RelativePath)
    }
}

function Require-PackageDirectory {
    param([Parameter(Mandatory)][string]$RelativePath)

    $path = Join-Path $StagingDirectory $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        throw ('The package is missing required directory: ' + $RelativePath)
    }
}

function Invoke-PackagedSmokeTest {
    param([Parameter(Mandatory)][string]$Executable)

    $oldPath = $env:PATH
    $oldPlatform = $env:QT_QPA_PLATFORM
    try {
        $env:PATH = $StagingDirectory + ';' + (Join-Path $env:SystemRoot 'System32')
        $env:QT_QPA_PLATFORM = 'offscreen'
        & $Executable '--smoke-scenario=startup' '--smoke-exit-ms=1000' ('--smoke-output-dir=' + $SmokeDirectory)
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) {
            throw ('The packaged smoke test failed (exit code ' + $exitCode + ').')
        }
    } finally {
        $env:PATH = $oldPath
        if ($null -eq $oldPlatform) {
            Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
        } else {
            $env:QT_QPA_PLATFORM = $oldPlatform
        }
    }
}

try {
    if ((Test-Path -LiteralPath $PackageDirectory) -or (Test-Path -LiteralPath $PackageArchive)) {
        if (-not $Force) {
            throw ('Output already exists. Use -Force to replace ' + $PackageName + ' and its ZIP archive.')
        }
    }

    Import-VisualStudioEnvironment
    $cmake = Get-CommandPath 'cmake.exe'
    if ([string]::IsNullOrWhiteSpace($cmake)) {
        throw 'CMake was not found on PATH or in the Visual Studio build environment.'
    }

    $qt = Resolve-QtInstallation
    $vcpkg = Resolve-VcpkgInstallation
    $selectedGenerator = Get-CMakeGenerator $cmake
    $backend = Resolve-SourceDirectory 'Seriona backend' $BackendSourceDir 'Seriona_Backend'
    $tagReader = Resolve-SourceDirectory 'TagReader' $TagReaderSourceDir 'TagReader'

    Write-Phase ('Configuring Release build with ' + $selectedGenerator)
    New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
    $CreatedBuildDirectory = $true

    $configureArguments = @(
        '-S', $RepoRoot,
        '-B', $BuildDirectory,
        '-G', $selectedGenerator,
        '-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=ON',
        ('-DCMAKE_TOOLCHAIN_FILE=' + $vcpkg.Toolchain),
        '-DVCPKG_TARGET_TRIPLET=x64-windows',
        ('-DQt6_DIR=' + $qt.Qt6Dir)
    )
    if ($selectedGenerator -match 'Visual Studio') {
        $configureArguments += @('-A', 'x64')
    }
    if ($null -ne $backend) {
        $configureArguments += ('-DSERIONA_BACKEND_SOURCE_DIR=' + $backend)
    }
    if ($null -ne $tagReader) {
        $configureArguments += ('-DSERIONA_TAGREADER_SOURCE_DIR=' + $tagReader)
    }
    Invoke-Native $cmake $configureArguments 'configure the Release build'

    Write-Phase 'Building all configured Release targets'
    Invoke-Native $cmake @('--build', $BuildDirectory, '--config', 'Release', '--parallel') 'build the Release targets'

    Write-Phase 'Running the configured test suite'
    $ctest = Get-CommandPath 'ctest.exe'
    if ([string]::IsNullOrWhiteSpace($ctest)) {
        throw 'CTest was not found on PATH.'
    }
    $originalPath = $env:PATH
    try {
        $env:PATH = (Join-Path $qt.Prefix 'bin') + ';' + $vcpkg.TripletBin + ';' + $BuildDirectory + ';' + $originalPath
        Invoke-Native $ctest @('--test-dir', $BuildDirectory, '-C', 'Release', '--output-on-failure') 'run the test suite'
    } finally {
        $env:PATH = $originalPath
    }

    $builtExecutable = Find-ReleaseExecutable
    Write-Phase 'Preparing a clean package staging directory'
    New-Item -ItemType Directory -Path $StagingDirectory -Force | Out-Null
    $CreatedStagingDirectory = $true
    Copy-Item -LiteralPath $builtExecutable -Destination (Join-Path $StagingDirectory 'appSeriona.exe')

    Write-Phase 'Deploying Qt libraries, plugins, and QML modules'
    $stagedExecutable = Join-Path $StagingDirectory 'appSeriona.exe'
    Invoke-Native $qt.WindeployQt @(
        '--release',
        '--force',
        '--qmldir', (Join-Path $RepoRoot 'qml'),
        '--dir', $StagingDirectory,
        $stagedExecutable
    ) 'deploy Qt runtime files'

    Write-Phase 'Deploying vcpkg runtime DLLs'
    $powershell = Get-CommandPath 'powershell.exe'
    if ([string]::IsNullOrWhiteSpace($powershell)) {
        throw 'Windows PowerShell was not found; it is required to run vcpkg applocal.ps1.'
    }
    Invoke-Native $powershell @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $vcpkg.AppLocal,
        '-targetBinary', $stagedExecutable,
        '-installedDir', $vcpkg.TripletBin,
        '-copiedFilesLog', (Join-Path $BuildDirectory 'applocal-files.txt')
    ) 'deploy vcpkg runtime DLLs'

    Write-Phase 'Validating the package contents'
    Require-PackageFile 'appSeriona.exe'
    Require-PackageFile 'Qt6Core.dll'
    Require-PackageFile 'Qt6Gui.dll'
    Require-PackageFile 'Qt6Qml.dll'
    Require-PackageFile 'Qt6Quick.dll'
    Require-PackageDirectory 'platforms'
    Require-PackageFile 'platforms\qwindows.dll'
    Require-PackageDirectory 'qml'

    $readme = @(
        'Seriona Windows x64',
        '',
        'Run appSeriona.exe to start Seriona.',
        'The application stores its database, logs, artwork, and settings in the SerionaData directory beside the executable.',
        'This package was built in Release mode.'
    )
    Set-Content -LiteralPath (Join-Path $StagingDirectory 'README.txt') -Value $readme -Encoding ASCII

    Write-Phase 'Running the packaged smoke test with only package and system DLL paths'
    Invoke-PackagedSmokeTest $stagedExecutable
    $runtimeData = Join-Path $StagingDirectory 'SerionaData'
    if (Test-Path -LiteralPath $runtimeData) {
        Remove-Item -LiteralPath $runtimeData -Recurse -Force
    }

    Write-Phase 'Creating the ZIP archive'
    $TemporaryArchive = Join-Path $RepoRoot ('.' + $PackageName + '.tmp-' + $RunId + '.zip')
    if (Test-Path -LiteralPath $TemporaryArchive) {
        Remove-Item -LiteralPath $TemporaryArchive -Force
    }
    Compress-Archive -Path $StagingDirectory -DestinationPath $TemporaryArchive -CompressionLevel Optimal

    if ($Force -and (Test-Path -LiteralPath $PackageDirectory)) {
        Remove-Item -LiteralPath $PackageDirectory -Recurse -Force
    }
    if ($Force -and (Test-Path -LiteralPath $PackageArchive)) {
        Remove-Item -LiteralPath $PackageArchive -Force
    }
    Move-Item -LiteralPath $StagingDirectory -Destination $PackageDirectory
    Move-Item -LiteralPath $TemporaryArchive -Destination $PackageArchive
    $TemporaryArchive = $null

    Write-Phase ('Package created at ' + $PackageDirectory)
    Write-Host ('ZIP archive created at ' + $PackageArchive)
} catch {
    Write-Error $_
    throw
} finally {
    if (-not $KeepBuild) {
        if ($null -ne $TemporaryArchive -and (Test-Path -LiteralPath $TemporaryArchive)) {
            Remove-Item -LiteralPath $TemporaryArchive -Force -ErrorAction SilentlyContinue
        }
        if ($CreatedStagingDirectory -and (Test-Path -LiteralPath $StagingParent)) {
            Remove-Item -LiteralPath $StagingParent -Recurse -Force -ErrorAction SilentlyContinue
        }
        if ($CreatedBuildDirectory -and (Test-Path -LiteralPath $BuildDirectory)) {
            Remove-Item -LiteralPath $BuildDirectory -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
