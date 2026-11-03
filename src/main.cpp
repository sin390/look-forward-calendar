#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>

#include "app/app_state.h"
#include "platform/platform.h"
#include "store/fs_util.h"
#include "ui/app.h"

namespace {

// "TAB,DOWN,ENTER,n,ESC" -> 按键序列（开发用快照）
std::vector<ftxui::Event> ParseKeys(const std::string& spec) {
  std::vector<ftxui::Event> keys;
  std::istringstream ss(spec);
  for (std::string k; std::getline(ss, k, ',');) {
    if (k == "TAB") keys.push_back(ftxui::Event::Tab);
    else if (k == "ESC") keys.push_back(ftxui::Event::Escape);
    else if (k == "ENTER") keys.push_back(ftxui::Event::Return);
    else if (k == "BS") keys.push_back(ftxui::Event::Backspace);
    else if (k == "UP") keys.push_back(ftxui::Event::ArrowUp);
    else if (k == "DOWN") keys.push_back(ftxui::Event::ArrowDown);
    else if (k == "LEFT") keys.push_back(ftxui::Event::ArrowLeft);
    else if (k == "RIGHT") keys.push_back(ftxui::Event::ArrowRight);
    else if (k == "PGUP") keys.push_back(ftxui::Event::PageUp);
    else if (k == "PGDN") keys.push_back(ftxui::Event::PageDown);
    else
      for (char c : k) keys.push_back(ftxui::Event::Character(c));
  }
  return keys;
}

}  // namespace

// 用法：LookForward [--data <数据目录>] [--snapshot <宽>x<高> [按键序列]]
// 默认数据目录为 exe 所在目录下的 data/
int main(int argc, char** argv) {
  cal::InitConsole();
  cal::fs::path root = cal::ExeDir() / "data";
  std::string snapshot, keys;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--data" && i + 1 < argc) root = argv[++i];
    else if (arg == "--snapshot" && i + 1 < argc) {
      snapshot = argv[++i];
      if (i + 1 < argc && argv[i + 1][0] != '-') keys = argv[++i];
    }
  }

  cal::AppState state(root);
  if (!snapshot.empty()) {
    int w = 160, h = 45;
    std::sscanf(snapshot.c_str(), "%dx%d", &w, &h);
    std::fputs(cal::ui::RenderSnapshot(state, w, h, ParseKeys(keys)).c_str(), stdout);
    std::fputs("\n", stdout);
    return 0;
  }
  return cal::ui::RunApp(state);
}
