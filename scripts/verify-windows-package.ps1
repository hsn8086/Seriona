#requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackageDirectory,
    [Parameter(Mandatory)]
    [string]$SmokeOutputDirectory,
    [switch]$MockOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)
$SmokeOutputDirectory = [IO.Path]::GetFullPath($SmokeOutputDirectory)
$Executable = Join-Path $PackageDirectory 'appSeriona.exe'
$RuntimeDataDirectory = Join-Path $PackageDirectory 'SerionaData'

function Require-File {
    param([Parameter(Mandatory)][string]$RelativePath)

    $path = Join-Path $PackageDirectory $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw ('包缺少必需文件：' + $RelativePath)
    }
}

function Require-Directory {
    param([Parameter(Mandatory)][string]$RelativePath)

    $path = Join-Path $PackageDirectory $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        throw ('包缺少必需目录：' + $RelativePath)
    }
}

function Require-FilePattern {
    param([Parameter(Mandatory)][string]$Pattern)

    $match = Get-ChildItem -LiteralPath $PackageDirectory -File -Filter $Pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $match) {
        throw ('包根目录缺少匹配文件：' + $Pattern)
    }
}

if (-not (Test-Path -LiteralPath $PackageDirectory -PathType Container)) {
    throw ('包目录不存在：' + $PackageDirectory)
}

foreach ($file in @(
    'appSeriona.exe',
    'BUILD-INFO.txt',
    'README.txt',
    'LICENSE',
    'Qt6Core.dll',
    'Qt6Gui.dll',
    'Qt6Qml.dll',
    'Qt6Quick.dll',
    'Qt6QuickControls2.dll',
    'Qt6QuickLayouts.dll',
    'Qt6Widgets.dll',
    'platforms\qwindows.dll',
    'platforms\qoffscreen.dll',
    'imageformats\qjpeg.dll',
    'imageformats\qsvg.dll',
    'qml\QtQuick\qmldir',
    'qml\QtQuick\qtquick2plugin.dll',
    'qml\QtQuick\Controls\Basic\qmldir',
    'qml\QtQuick\Controls\Basic\qtquickcontrols2basicstyleplugin.dll',
    'qml\QtQuick\Layouts\qmldir',
    'qml\QtQuick\Layouts\qquicklayoutsplugin.dll',
    'qml\Qt5Compat\GraphicalEffects\qmldir',
    'qml\Qt5Compat\GraphicalEffects\qtgraphicaleffectsplugin.dll',
    'qml\QtQml\Models\qmldir',
    'qml\QtQml\Models\modelsplugin.dll',
    'qml\Qt\labs\platform\qmldir',
    'qml\Qt\labs\platform\labsplatformplugin.dll'
)) {
    Require-File $file
}
foreach ($directory in @('platforms', 'imageformats', 'qml')) {
    Require-Directory $directory
}

if (-not $MockOnly) {
    foreach ($pattern in @(
        'avformat-*.dll',
        'avcodec-*.dll',
        'avutil-*.dll',
        'avfilter-*.dll',
        'swresample-*.dll',
        'swscale-*.dll',
        'iconv-*.dll'
    )) {
        Require-FilePattern $pattern
    }
    foreach ($file in @('spdlog.dll', 'sqlite3.dll', 'xxhash.dll')) {
        Require-File $file
    }
}

$forbiddenFilePatterns = @(
    '*.pdb', '*.lib', '*.exp', '*.obj', '*.ilk', '*.idb', '*.ipdb', '*.iobj',
    '*.tlog', '*.vcxproj', '*.vcxproj.filters', '*.sln', 'CMakeCache.txt',
    'cmake_install.cmake', 'install_manifest.txt', 'compile_commands.json'
)
foreach ($pattern in $forbiddenFilePatterns) {
    $unexpected = Get-ChildItem -LiteralPath $PackageDirectory -Recurse -File -Filter $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $unexpected) {
        throw ('包中包含构建产物：' + $unexpected.FullName.Substring($PackageDirectory.Length + 1))
    }
}
foreach ($directoryName in @('CMakeFiles', 'Testing', '_deps', 'vcpkg_installed')) {
    $unexpectedDirectory = Get-ChildItem -LiteralPath $PackageDirectory -Recurse -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq $directoryName } | Select-Object -First 1
    if ($null -ne $unexpectedDirectory) {
        throw ('包中包含构建目录：' + $unexpectedDirectory.FullName.Substring($PackageDirectory.Length + 1))
    }
}
if (Test-Path -LiteralPath $RuntimeDataDirectory) {
    throw '验证前暂存包已包含运行时 SerionaData；拒绝发布带用户/Smoke 数据的包。'
}

New-Item -ItemType Directory -Path $SmokeOutputDirectory -Force | Out-Null
$environmentNames = @(
    'PATH',
    'QT_QPA_PLATFORM',
    'QT_PLUGIN_PATH',
    'QT_QPA_PLATFORM_PLUGIN_PATH',
    'QML_IMPORT_PATH',
    'QML2_IMPORT_PATH'
)
$savedEnvironment = @{}
foreach ($name in $environmentNames) {
    $value = [Environment]::GetEnvironmentVariable($name, 'Process')
    $savedEnvironment[$name] = [PSCustomObject]@{
        Exists = $null -ne $value
        Value = $value
    }
}

try {
    $systemRoot = [Environment]::GetEnvironmentVariable('SystemRoot')
    if ([string]::IsNullOrWhiteSpace($systemRoot)) {
        throw 'SystemRoot 未定义，无法构造受限 PATH。'
    }
    $env:PATH = $PackageDirectory + ';' + (Join-Path $systemRoot 'System32')
    $env:QT_QPA_PLATFORM = 'offscreen'
    foreach ($name in @('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH')) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }

    foreach ($scenario in @('startup', 'main-playback', 'lyrics', 'sidebar-tree', 'settings-menu', 'empty-library')) {
        Write-Host ('==> Smoke: ' + $scenario)
        $consoleLog = Join-Path $SmokeOutputDirectory ('console-' + $scenario + '.log')
        $arguments = @(
            ('--smoke-scenario=' + $scenario),
            '--smoke-exit-ms=1000',
            ('--smoke-output-dir=' + $SmokeOutputDirectory)
        )
        $previousPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & $Executable @arguments 2>&1 | Tee-Object -FilePath $consoleLog
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousPreference
        }
        if ($exitCode -ne 0) {
            throw ('Smoke 场景 {0} 失败（退出码 {1}）。输出：{2}' -f $scenario, $exitCode, $consoleLog)
        }
        $smokeLog = Join-Path $SmokeOutputDirectory ('smoke-' + $scenario + '.log')
        if (-not (Test-Path -LiteralPath $smokeLog -PathType Leaf)) {
            throw ('Smoke 场景未生成证据日志：' + $smokeLog)
        }
        $scenarioRecord = Get-Content -LiteralPath $smokeLog | Where-Object { $_ -eq ('scenario=' + $scenario) } | Select-Object -First 1
        if ($null -eq $scenarioRecord) {
            throw ('Smoke 证据日志场景不匹配：' + $smokeLog)
        }
    }
} finally {
    if (Test-Path -LiteralPath $RuntimeDataDirectory) {
        Remove-Item -LiteralPath $RuntimeDataDirectory -Recurse -Force
    }
    foreach ($name in $environmentNames) {
        $saved = $savedEnvironment[$name]
        if ($saved.Exists) {
            [Environment]::SetEnvironmentVariable($name, $saved.Value, 'Process')
        } else {
            [Environment]::SetEnvironmentVariable($name, $null, 'Process')
        }
    }
}

if (Test-Path -LiteralPath $RuntimeDataDirectory) {
    throw 'Smoke 运行后未能清理 SerionaData。'
}
Write-Host 'Windows 包结构与全部 Smoke 场景验证通过。' -ForegroundColor Green
