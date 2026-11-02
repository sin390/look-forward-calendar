#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace cal {

namespace fs = std::filesystem;

fs::path PathFromUtf8(std::string_view s);
std::string PathToUtf8(const fs::path& p);

std::expected<std::string, std::string> ReadTextFile(const fs::path& p);
// 先写临时文件再替换，避免写一半时损坏原文件
std::expected<void, std::string> WriteTextFileAtomic(const fs::path& p, const std::string& content);

}  // namespace cal
