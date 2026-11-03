#include "ui/widgets.h"

#include <array>
#include <utility>

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
  return hbox({text(label) | size(WIDTH, EQUAL, 16), std::move(field)});
}

Element Panel(const std::string& title, Element body, bool focused) {
  auto t = text(" " + title + " ") | bold;
  if (focused) t |= color(Color::Cyan);
  return window(t, std::move(body));
}

Element WrapText(std::string_view s) {
  Elements lines;
  size_t line_start = 0;
  while (line_start <= s.size()) {
    size_t line_end = s.find('\n', line_start);
    if (line_end == std::string_view::npos) line_end = s.size();
    const std::string_view line = s.substr(line_start, line_end - line_start);

    // 断行单位：连续的 ASCII 非空白字符（单词）或单个非 ASCII 字符；空格附在前一个单位后面
    Elements pieces;
    std::string piece;
    for (size_t i = 0; i < line.size();) {
      const auto c = static_cast<unsigned char>(line[i]);
      const size_t len = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
      const std::string_view glyph = line.substr(i, len);
      i += len;
      if (c == ' ') {
        piece += ' ';
        pieces.push_back(text(std::exchange(piece, {})));
      } else if (c < 0x80) {
        piece += glyph;
      } else {
        if (!piece.empty()) pieces.push_back(text(std::exchange(piece, {})));
        pieces.push_back(text(std::string(glyph)));
      }
    }
    if (!piece.empty()) pieces.push_back(text(piece));
    lines.push_back(pieces.empty() ? text("") : flexbox(std::move(pieces)));
    line_start = line_end + 1;
  }
  return lines.size() == 1 ? std::move(lines.front()) : vbox(std::move(lines));
}

std::string WeekdayName(int iso_weekday) {
  static constexpr std::array<const char*, 7> kNames = {N_("Mon"), N_("Tue"), N_("Wed"), N_("Thu"),
                                                        N_("Fri"), N_("Sat"), N_("Sun")};
  return Tr(kNames[static_cast<size_t>(iso_weekday - 1)]);
}

std::string WeekdayShortName(int iso_weekday) {
  static constexpr std::array<const char*, 7> kNames = {N_("Mo"), N_("Tu"), N_("We"), N_("Th"),
                                                        N_("Fr"), N_("Sa"), N_("Su")};
  return Tr(kNames[static_cast<size_t>(iso_weekday - 1)]);
}

std::string IdTag(const std::string& task_id) { return "(ID:" + task_id + ")"; }

std::string ShortDate(Date d) {
  return Tr("{month}/{day}", {{"month", std::to_string(MonthNum(d))}, {"day", std::to_string(DayNum(d))}});
}

void Report(AppState& s, const Result& r, const std::string& ok_msg) {
  if (r)
    s.Info(ok_msg);
  else
    s.Error(r.error());
}

}  // namespace cal::ui
