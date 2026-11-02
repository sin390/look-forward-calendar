#!/usr/bin/env bash
# 编译日历程序并运行单元测试。
#
# 用法：
#   ./build.sh          配置 + 编译 + 运行单元测试
#   ./build.sh run      同上，成功后在新的控制台窗口中启动程序
#   ./build.sh clean    删除 build 目录后重新编译
set -euo pipefail

cd "$(dirname "$0")"

if [[ "${1:-}" == "clean" ]]; then
  echo "==> 删除 build/"
  rm -rf build
fi

echo "==> 配置（首次会下载 FTXUI 和 nlohmann/json）"
cmake --preset debug

echo "==> 编译"
cmake --build --preset debug

echo "==> 单元测试"
./build/calendar_tests.exe

echo
echo "编译完成：build/calendar.exe（数据目录：build/data/）"

if [[ "${1:-}" == "run" ]]; then
  # Git Bash 的终端（mintty）不是 Windows 控制台，FTXUI 无法正常接收按键，
  # 因此在新的控制台窗口中启动
  echo "==> 在新窗口中启动"
  cmd //c start "" "build\\calendar.exe"
fi
