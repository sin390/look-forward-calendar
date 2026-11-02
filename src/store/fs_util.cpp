#include "store/fs_util.h"

#include <fstream>
#include <sstream>

namespace cal {

fs::path PathFromUtf8(std::string_view s) {
  return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

std::string PathToUtf8(const fs::path& p) {
  const std::u8string u = p.u8string();
  return std::string(reinterpret_cast<const char*>(u.data()), u.size());
}

std::expected<std::string, std::string> ReadTextFile(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return std::unexpected("无法打开文件：" + PathToUtf8(p));
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string s = ss.str();
  if (s.starts_with("\xEF\xBB\xBF")) s.erase(0, 3);  // 去掉 UTF-8 BOM
  return s;
}

std::expected<void, std::string> WriteTextFileAtomic(const fs::path& p, const std::string& content) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  if (ec) return std::unexpected("无法创建目录：" + PathToUtf8(p.parent_path()));

  fs::path tmp = p;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return std::unexpected("无法写入文件：" + PathToUtf8(tmp));
    out << content;
    if (!out) return std::unexpected("写入失败：" + PathToUtf8(tmp));
  }
  fs::rename(tmp, p, ec);
  if (ec) return std::unexpected("无法替换文件：" + PathToUtf8(p));
  return {};
}

}  // namespace cal
