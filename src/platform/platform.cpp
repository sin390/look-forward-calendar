#include "platform/platform.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <vector>

namespace cal {

#ifdef _WIN32

void InitConsole() {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
}

std::filesystem::path ExeDir() {
  std::vector<wchar_t> buf(MAX_PATH);
  while (true) {
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0) return std::filesystem::current_path();
    if (n < buf.size()) return std::filesystem::path(std::wstring(buf.data(), n)).parent_path();
    buf.resize(buf.size() * 2);
  }
}

bool CopyToClipboard(const std::string& utf8) {
  // 剪贴板文本使用 CRLF 换行
  std::string text;
  for (char c : utf8) {
    if (c == '\n') text += '\r';
    text += c;
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (static_cast<size_t>(n) + 1) * sizeof(wchar_t));
  if (!mem) return false;
  auto* dst = static_cast<wchar_t*>(GlobalLock(mem));
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), dst, n);
  dst[n] = L'\0';
  GlobalUnlock(mem);

  if (!OpenClipboard(nullptr)) {
    GlobalFree(mem);
    return false;
  }
  EmptyClipboard();
  const bool ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
  if (!ok) GlobalFree(mem);  // 成功时内存归剪贴板所有
  CloseClipboard();
  return ok;
}

#else

void InitConsole() {}
std::filesystem::path ExeDir() { return std::filesystem::current_path(); }
bool CopyToClipboard(const std::string&) { return false; }

#endif

}  // namespace cal
