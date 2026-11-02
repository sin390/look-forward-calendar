#pragma once

#include <filesystem>
#include <string>

namespace cal {

void InitConsole();              // 控制台输入输出切换为 UTF-8
std::filesystem::path ExeDir();  // 可执行文件所在目录
bool CopyToClipboard(const std::string& utf8);

}  // namespace cal
