#!/usr/bin/env bash
# 编译 LookForward 并运行单元测试。
#
# 用法：
#   ./build.sh          配置 + 编译 + 运行单元测试
#   ./build.sh run      同上，成功后在新的控制台窗口中启动程序
#   ./build.sh clean    删除 build 目录后重新编译
#   ./build.sh package  编译发布版（build-release/），打包为开箱即用的 dist/LookForward-v<版本>-win64.zip
set -euo pipefail

cd "$(dirname "$0")"

if [[ "${1:-}" == "package" ]]; then
  version=$(sed -n 's/.*kVersion = "\(.*\)";.*/\1/p' src/version.h)
  name="LookForward-v${version}-win64"

  echo "==> 配置发布版"
  cmake --preset release
  echo "==> 编译发布版"
  cmake --build --preset release
  echo "==> 单元测试"
  ./build-release/calendar_tests.exe

  # 包内结构：exe 与 language/ 放在一起；data/ 在首次使用时自动生成，不打包
  echo "==> 打包 dist/${name}.zip"
  rm -rf "dist/${name}" "dist/${name}.zip"
  mkdir -p "dist/${name}/docs"
  cp build-release/LookForward.exe "dist/${name}/"
  cp -r language "dist/${name}/"
  cp README.md README_CN.md "dist/${name}/"
  cp docs/logo.png "dist/${name}/docs/"  # README 中引用的图片
  # Windows 自带的 tar（bsdtar）可以生成标准 zip
  (cd dist && /c/Windows/System32/tar.exe -a -cf "${name}.zip" "${name}")

  echo
  echo "打包完成：dist/${name}.zip"
  /c/Windows/System32/tar.exe -tf "dist/${name}.zip"
  exit 0
fi

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
echo "编译完成：build/LookForward.exe（数据目录：build/data/）"

if [[ "${1:-}" == "run" ]]; then
  # Git Bash 的终端（mintty）不是 Windows 控制台，FTXUI 无法正常接收按键，
  # 因此在新的控制台窗口中启动
  echo "==> 在新窗口中启动"
  cmd //c start "" "build\\LookForward.exe"
fi
