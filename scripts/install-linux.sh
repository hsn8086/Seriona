#!/usr/bin/env bash
# Seriona Linux 安装脚本（installed 模式）
#
# 以 Linux 安装版配置编译并安装：数据目录遵循 XDG Base Directory
# （$XDG_DATA_HOME/org.kaizen857.Seriona 等），并安装 .desktop 启动器
# 与 hicolor 主题图标。Windows 保持便携版（见 build.bat / build-package-windows.ps1）。
#
# 用法:
#   ./scripts/install-linux.sh [前缀]            全新安装或升级（覆盖安装，幂等）
#   ./scripts/install-linux.sh [前缀] --clean    先卸载旧安装再安装（升级时清除残留文件）
#   ./scripts/install-linux.sh uninstall         卸载已安装内容（按 install_manifest.txt）
#   ./scripts/install-linux.sh uninstall --clean 卸载并删除构建目录
#
#   前缀默认 ${HOME}/.local（无需 root）；系统级示例: sudo ./scripts/install-linux.sh /usr/local
#   --clean 同样可用 --clean-install 形式。
#
# 环境变量:
#   SERIONA_BUILD_DIR   构建目录（默认 build-installed）
#
# 版本更新说明:
#   重复执行本脚本即为升级（cmake --install 覆盖同名文件）；残留文件（如旧版图标名）
#   用 --clean 模式清除。安装版（XDG 数据目录）与开发版（./build 下 SerionaData）
#   数据完全隔离，可同时运行：日常使用安装版、开发调试直接跑 ./build/seriona。

set -euo pipefail

mode="install"
clean="0"
prefix="${HOME}/.local"
build_dir="${SERIONA_BUILD_DIR:-build-installed}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

for arg in "$@"; do
  case "$arg" in
    uninstall) mode="uninstall" ;;
    clean|--clean|--clean-install) clean="1" ;;
    *) prefix="$arg" ;;
  esac
done

refresh_desktop_databases() {
  update-desktop-database "${prefix}/share/applications" 2>/dev/null || true
  gtk-update-icon-cache -f -t "${prefix}/share/icons/hicolor" 2>/dev/null || true
  kbuildsycoca6 2>/dev/null || true
}

uninstall_installed_files() {
  local manifest="$build_dir/install_manifest.txt"
  if [[ ! -f "$manifest" ]]; then
    echo "未找到 $manifest（构建目录从未安装过或已删除），跳过卸载。"
    return 0
  fi

  # 实际安装前缀从 manifest 推导（install(TARGETS) 首个条目位于 <prefix>/bin/），
  # 覆盖用户参数误差，保证空目录清理与缓存刷新作用于正确位置。
  local manifest_prefix
  manifest_prefix="$(head -n 1 "$manifest" | sed 's|/bin/.*||')"
  if [[ -n "$manifest_prefix" && -d "$manifest_prefix" ]]; then
    prefix="$manifest_prefix"
  fi

  echo "按 $manifest 卸载已安装文件..."
  local removed=0
  while IFS= read -r file || [[ -n "$file" ]]; do
    [[ -z "$file" ]] && continue
    if [[ -f "$file" || -L "$file" ]]; then
      rm -f "$file"
      removed=$((removed + 1))
    fi
  done < "$manifest"
  echo "已删除 $removed 个文件。"

  # 清理可能遗留的空目录（仅限安装产物目录树，不触碰共享目录本身）
  find "${prefix}/share/applications" "${prefix}/share/icons/hicolor" \
      -type d -empty -delete 2>/dev/null || true
}

if [[ "$mode" == "uninstall" ]]; then
  uninstall_installed_files
  refresh_desktop_databases
  echo "Seriona 已卸载。"
  if [[ "$clean" == "1" ]]; then
    rm -rf "$build_dir"
    echo "构建目录 $build_dir 已删除。"
  fi
  exit 0
fi

if [[ "$clean" == "1" ]]; then
  uninstall_installed_files
fi

cmake -S "$repo_root" -B "$build_dir" \
    -DSERIONA_INSTALLED_MODE=ON \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix"

cpu_cores="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)"
cmake --build "$build_dir" -j"${cpu_cores}"

cmake --install "$build_dir"

refresh_desktop_databases

version="$(git -C "$repo_root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
echo "Seriona 已安装到 ${prefix}（commit ${version}）"
echo "应用数据目录遵循 XDG 规范（库/封面缓存/日志位于 ~/.local/share、~/.cache、~/.local/state 下）"
echo "升级: 再次运行本脚本即可（--clean 清除旧版残留）；卸载: ./scripts/uninstall-linux.sh"
echo "开发提示: 安装版与 ./build 便携开发版数据隔离，可同时运行互不影响。"
