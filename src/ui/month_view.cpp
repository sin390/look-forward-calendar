#include "ui/month_view.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "core/lunar.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;  // 键盘事件；数据模型的事件写作 cal::Event

namespace {

// 格子等宽：基准宽度相同，再按窗口宽度均分剩余空间
Element CellSize(Element e) { return std::move(e) | size(WIDTH, EQUAL, 8) | xflex; }

class MonthViewBase : public ComponentBase {
 public:
  explicit MonthViewBase(AppState& s) : s_(s) {}

  bool Focusable() const override { return true; }

  bool OnEvent(Event e) override {
    if (e.is_mouse()) return false;
    Date d = s_.selected();
    // 只用小键盘上也有的键：方向键或 hjkl，[ ] 翻月，t 回到今天
    if (e == Event::ArrowLeft || e == Event::Character('h')) d = AddDays(d, -1);
    else if (e == Event::ArrowRight || e == Event::Character('l')) d = AddDays(d, 1);
    else if (e == Event::ArrowUp || e == Event::Character('k')) d = AddDays(d, -7);
    else if (e == Event::ArrowDown || e == Event::Character('j')) d = AddDays(d, 7);
    else if (e == Event::Character('[')) d = AddMonths(d, -1);
    else if (e == Event::Character(']')) d = AddMonths(d, 1);
    else if (e == Event::Character('t')) d = s_.today();
    else return false;
    s_.Select(d);
    return true;
  }

  Element OnRender() override {
    const Date sel = s_.selected();
    const std::chrono::year_month ym = sel.year() / sel.month();
    const Date first = ym / std::chrono::day{1};
    const int lead = IsoWeekday(first) % 7;  // 每周从周日开始
    const int days = static_cast<int>(DaysInMonth(ym));
    const int weeks = (lead + days + 6) / 7;

    Elements header;
    for (size_t c = 0; c < 7; ++c) {
      const int w = kDisplayWeekOrder[c];
      auto t = CellSize(text(WeekdayName(w).substr(3)) | center);
      if (w >= 6) t |= color(Color::RedLight);
      header.push_back(t);
      if (c < 6) header.push_back(separatorLight());
    }

    Elements rows = {hbox(std::move(header)) | bold};
    for (int w = 0; w < weeks; ++w) {
      rows.push_back(separatorLight());
      Elements cells;
      for (int c = 0; c < 7; ++c) {
        const int day_index = w * 7 + c - lead;
        cells.push_back(day_index >= 0 && day_index < days ? Cell(AddDays(first, day_index))
                                                           : EmptyCell());
        if (c < 6) cells.push_back(separatorLight());
      }
      rows.push_back(hbox(std::move(cells)));
    }

    const std::string title = std::to_string(YearNum(sel)) + " 年 " + std::to_string(MonthNum(sel)) +
                              " 月    节假日：" + CountryName(s_.display_country());
    return Panel(title, vbox(std::move(rows)), Focused());
  }

 private:
  static Element EmptyCell() {
    return CellSize(vbox({text(""), text(""), text("")}));
  }

  Element Cell(Date d) {
    const Date today = s_.today();
    const HolidayDay* holiday = s_.holidays().Find(s_.display_country(), d);
    const bool weekend = IsoWeekday(d) >= 6;

    // 第 1 行：日期 + 休/班标记
    auto num = text(std::to_string(DayNum(d))) | bold;
    if ((holiday && holiday->off) || (weekend && !holiday)) num |= color(Color::RedLight);
    if (d == today) num = text(std::to_string(DayNum(d)) + "今") | bold | color(Color::Cyan);
    Element mark = text("");
    if (holiday) mark = holiday->off ? text("休") | color(Color::Red) : text("班") | color(Color::Blue);
    auto line1 = hbox({num, filler(), mark});

    // 第 2 行：节假日名 > 农历节日 > 节气 > 农历日
    Element line2;
    if (holiday) {
      line2 = text(holiday->name) | color(holiday->off ? Color::Red : Color::Blue);
    } else if (const auto ld = ToLunar(d)) {
      if (auto f = LunarFestival(*ld); !f.empty()) line2 = text(f) | color(Color::Yellow);
      else if (auto t = SolarTerm(d); !t.empty()) line2 = text(t) | color(Color::Green);
      else line2 = text(ld->day == 1 ? LunarMonthName(*ld) : LunarDayName(ld->day)) | dim;
    } else {
      line2 = text("");
    }

    // 第 3 行：事件数 + 固定日程数 + 任务工时（今天及以后显示计划量，过去显示实际量）
    Element events = text("");
    if (const Day* day = s_.FindDay(d); day && !day->events.empty())
      events = text("●" + std::to_string(day->events.size())) | color(Color::Magenta);
    if (const size_t n = s_.FixedOn(d).size(); n > 0)
      events = hbox({events, text("◆" + std::to_string(n)) | color(Color::Cyan)});
    Element work = text("");
    if (d >= today) {
      if (const int m = s_.PlannedMinutesOn(d); m > 0) work = text("▶" + FormatMinutes(m));
      if (d == today && s_.HasOverdueTask()) work = hbox({work, text("!") | color(Color::Red) | bold});
    } else if (const int m = s_.ActualMinutesOn(d); m > 0) {
      work = text("✓" + FormatMinutes(m)) | color(Color::Green);
    }
    auto line3 = hbox({events, filler(), work});

    auto cell = CellSize(vbox({line1, line2, line3}));
    if (d == s_.selected()) cell |= inverted;
    return cell;
  }

  AppState& s_;
};

}  // namespace

Component MonthView(AppState& state) { return Make<MonthViewBase>(state); }

}  // namespace cal::ui
