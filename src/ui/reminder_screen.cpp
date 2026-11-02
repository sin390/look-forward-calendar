#include <algorithm>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "ui/screens.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;

namespace {

constexpr int kDateW = 14, kTimeW = 13, kLeadW = 14, kStateW = 16;

Element Cell(Element e, int width) { return std::move(e) | size(WIDTH, EQUAL, width); }

// 距离开始还有多久："3天2小时" / "1小时5分钟" / "12分钟"
std::string Until(long long minutes) {
  if (minutes >= 1440) {
    const long long d = minutes / 1440, h = minutes % 1440 / 60;
    return std::to_string(d) + "天" + (h ? std::to_string(h) + "小时" : "");
  }
  if (minutes >= 60) {
    const long long h = minutes / 60, m = minutes % 60;
    return std::to_string(h) + "小时" + (m ? std::to_string(m) + "分钟" : "");
  }
  return std::to_string(minutes) + "分钟";
}

class ReminderScreenBase : public ComponentBase {
 public:
  ReminderScreenBase(AppState& s, std::function<void(Date)> jump_to) : s_(s), jump_to_(std::move(jump_to)) {}

  bool Focusable() const override { return true; }

  bool OnEvent(Event e) override {
    if (e.is_mouse()) return false;
    if (e == Event::ArrowDown || e == Event::Character('j')) ++selected_;
    else if (e == Event::ArrowUp || e == Event::Character('k')) --selected_;
    else if (e == Event::Return) {
      const auto list = s_.ActiveReminders(NowLocal());
      if (selected_ >= 0 && selected_ < static_cast<int>(list.size()))
        jump_to_(list[static_cast<size_t>(selected_)].date);
    } else {
      return false;
    }
    return true;
  }

  Element OnRender() override {
    const LocalMinute now = NowLocal();
    const long long stamp = MinuteStamp(now.date, now.minute);
    const auto list = s_.ActiveReminders(now);
    selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(list.size()) - 1));

    Elements rows = {hbox({
                         Cell(text(" 日期"), kDateW),
                         Cell(text("时间"), kTimeW),
                         text("事件") | flex,
                         Cell(text("提前"), kLeadW),
                         Cell(text("状态"), kStateW),
                     }) | bold,
                     separatorLight()};
    for (size_t i = 0; i < list.size(); ++i) {
      const Reminder& r = list[i];
      const std::string time = EventTimeText(r.event).empty() ? "全天" : EventTimeText(r.event);
      const bool started = stamp >= r.window.start;
      auto state = started ? text("进行中") | color(Color::Green)
                           : text("还有 " + Until(r.window.start - stamp)) | color(Color::Yellow);
      Elements title = {text(r.event.title)};
      if (!r.event.note.empty()) title.push_back(text("  " + r.event.note) | dim);
      auto row = hbox({
          Cell(text(" " + ShortDate(r.date) + " " + WeekdayName(IsoWeekday(r.date))), kDateW),
          Cell(text(time) | color(Color::Magenta), kTimeW),
          hbox(std::move(title)) | flex,
          Cell(text(FormatLead(*r.event.remind)), kLeadW),
          Cell(state, kStateW),
      });
      if (static_cast<int>(i) == selected_) row = row | inverted | focus;
      rows.push_back(row);
    }
    if (list.empty()) rows.push_back(text(" 当前没有需要提醒的事件") | dim);

    return Panel("提醒列表（共 " + std::to_string(list.size()) + " 条，按发生时间排序）",
                 vbox(std::move(rows)) | vscroll_indicator | yframe | flex, true);
  }

 private:
  AppState& s_;
  std::function<void(Date)> jump_to_;
  int selected_ = 0;
};

}  // namespace

Component ReminderScreen(AppState& state, std::function<void(Date)> jump_to) {
  return Make<ReminderScreenBase>(state, std::move(jump_to));
}

}  // namespace cal::ui
