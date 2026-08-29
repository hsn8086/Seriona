#!/usr/bin/env bash
# Seriona Linux 卸载脚本（installed 模式）
#
# 独立卸载入口：按 <构建目录>/install_manifest.txt 删除安装产物，行为与
# ./scripts/install-linux.sh uninstall 完全一致（安装脚本的 uninstall 模式
# 保留，用于 --clean 覆盖安装；本脚本适合单独执行卸载）。
#
# 注意：用户数据（$XDG_DATA_HOME/org.kaizen857.Seriona 曲库、
# $XDG_CACHE_HOME/org.kaizen857.Seriona 封面缓存、$XDG_STATE_HOME 日志）
# 属于用户数据，卸载不删除，与安装脚本语义一致。
#
# 用法:
#   ./scripts/uninstall-linux.sh         卸载已安装内容（按 install_manifest.txt）
#   ./scripts/uninstall-linux.sh --clean 卸载并删除构建目录（默认 build-installed）
#
# 环境变量:
#   SERIONA_BUILD_DIR   构建目录（默认 build-installed）
#
# 提示: manifest 缺失（构建目录被删除或从未安装）时提示并跳过，不强行猜测文件清单；
# 重新安装请运行 ./scripts/install-linux.sh。

set -euo pipefail

clean="0"
build_dir="${SERIONA_BUILD_DIR:-build-installed}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
prefix="${HOME}/.local"

for arg in "$@"; do
  case "$arg" in
    clean|--clean) clean="1" ;;
    -h|--help)
      sed -n '1,30p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
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

uninstall_installed_files
refresh_desktop_databases
echo "Seriona 已卸载。"
if [[ "$clean" == "1" ]]; then
  rm -rf "$build_dir"
  echo "构建目录 $build_dir 已删除。"
fi
echo "重新安装: ./scripts/install-linux.sh"
