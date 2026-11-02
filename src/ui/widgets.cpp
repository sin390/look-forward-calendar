#include "ui/widgets.h"

#include <array>

using namespace ftxui;

namespace cal::ui {

ButtonOption SmallButton() {
  ButtonOption option;
  option.transform = [](const EntryState& s) {
    auto e = text("[" + s.label + "]");
    return s.focused ? e | inverted : e;
  };
  return option;
}

InputOption LineInput(std::function<void()> on_enter) {
  InputOption option;
  option.multiline = false;
  if (on_enter) option.on_enter = std::move(on_enter);
  return option;
}

Element InputBox(Component input, int width) {
  return input->Render() | size(WIDTH, EQUAL, width) | bgcolor(Color::GrayDark);
}

Element FormRow(const std::string& label, Element field) {
  return hbox({text(label) | size(WIDTH, EQUAL, 12), std::move(field)});
}

Element Panel(const std::string& title, Element body, bool focused) {
  auto t = text(" " + title + " ") | bold;
  if (focused) t |= color(Color::Cyan);
  return window(t, std::move(body));
}

std::string WeekdayName(int iso_weekday) {
  static constexpr std::array<const char*, 7> kNames = {"周一", "周二", "周三", "周四",
                                                        "周五", "周六", "周日"};
  return kNames[static_cast<size_t>(iso_weekday - 1)];
}

std::string IdTag(const std::string& task_id) { return "(ID:" + task_id + ")"; }

std::string ShortDate(Date d) {
  return std::to_string(MonthNum(d)) + "月" + std::to_string(DayNum(d)) + "日";
}

void Report(AppState& s, const Result& r, const std::string& ok_msg) {
  if (r)
    s.Info(ok_msg);
  else
    s.Error(r.error());
}

}  // namespace cal::ui
