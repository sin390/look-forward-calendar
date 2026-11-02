// 环境验证用的最小示例：显示当月月历，按 q 退出。
#include <chrono>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;
namespace chr = std::chrono;

int main() {
  const auto today = chr::year_month_day{
      chr::floor<chr::days>(chr::system_clock::now())};
  const auto first = today.year() / today.month() / chr::day{1};
  const auto last = chr::year_month_day_last{today.year() / today.month() / chr::last};
  const unsigned offset = chr::weekday{chr::sys_days{first}}.c_encoding();  // 周日=0

  std::vector<Element> rows;
  std::vector<Element> header;
  for (auto name : {"日", "一", "二", "三", "四", "五", "六"})
    header.push_back(text(name) | center | size(WIDTH, EQUAL, 4));
  rows.push_back(hbox(header) | bold);

  std::vector<Element> week(offset, text("") | size(WIDTH, EQUAL, 4));
  for (unsigned d = 1; d <= static_cast<unsigned>(last.day()); ++d) {
    auto cell = text(std::to_string(d)) | center | size(WIDTH, EQUAL, 4);
    if (d == static_cast<unsigned>(today.day())) cell = cell | inverted;
    week.push_back(cell);
    if (week.size() == 7) rows.push_back(hbox(std::move(week))), week.clear();
  }
  if (!week.empty()) rows.push_back(hbox(std::move(week)));

  const std::string title = std::to_string(static_cast<int>(today.year())) + " 年 " +
                            std::to_string(static_cast<unsigned>(today.month())) + " 月";

  auto screen = ScreenInteractive::FitComponent();
  auto ui = Renderer([&] {
    return vbox({text(title) | bold | center, separator(), vbox(rows),
                 separator(), text("按 q 退出") | dim | center}) |
           border;
  });
  ui |= CatchEvent([&](Event e) {
    if (e == Event::Character('q')) return screen.Exit(), true;
    return false;
  });
  screen.Loop(ui);
}
