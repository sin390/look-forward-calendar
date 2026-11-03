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

std::string Days(long long n) { return Tr("{n} d", {{"n", std::to_string(n)}}); }
std::string Hours(long long n) { return Tr("{n} h", {{"n", std::to_string(n)}}); }
std::string Minutes(long long n) { return Tr("{n} min", {{"n", std::to_string(n)}}); }

// 距离开始还有多久："3 d 2 h" / "1 h 5 min" / "12 min"（中文：3天2小时 / 1小时5分钟 / 12分钟）
std::string Until(long long minutes) {
  if (minutes >= 1440) {
    const long long d = minutes / 1440, h = minutes % 1440 / 60;
    return Days(d) + (h ? Tr(" ") + Hours(h) : "");
  }
  if (minutes >= 60) {
    const long long h = minutes / 60, m = minutes % 60;
    return Hours(h) + (m ? Tr(" ") + Minutes(m) : "");
  }
  return Minutes(minutes);
}

class ReminderScreenBase : public ComponentBase {
 public:
  ReminderScreenBase(AppState& s, std::function<void(Date)> jump_to, std::function<void()> on_back)
      : s_(s), jump_to_(std::move(jump_to)), on_back_(std::move(on_back)) {}

  bool Focusable() const override { return true; }

  bool OnEvent(Event e) override {
    if (e.is_mouse() || e == Event::Custom) return false;
    if (e == Event::Escape || e == Event::Character('q') || e == Event::Character('m')) on_back_();
    else if (e == Event::ArrowDown || e == Event::Character('j')) ++selected_;
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
                         Cell(text(Tr(" Date")), kDateW),
                         Cell(text(Tr("Time")), kTimeW),
                         text(Tr("Event")) | flex,
                         Cell(text(Tr("Lead")), kLeadW),
                         Cell(text(Tr("Status")), kStateW),
                     }) | bold,
                     separatorLight()};
    for (size_t i = 0; i < list.size(); ++i) {
      const Reminder& r = list[i];
      const std::string time = EventTimeText(r.event).empty() ? Tr("All day") : EventTimeText(r.event);
      const bool started = stamp >= r.window.start;
      auto state = started ? text(Tr("Ongoing")) | color(Color::Green)
                           : text(Tr("In {time}", {{"time", Until(r.window.start - stamp)}})) | color(Color::Yellow);
      Elements title = {text(r.event.title)};
      if (!r.event.note.empty()) title.push_back(text("  " + r.event.note) | dim);
      auto row = hbox({
          Cell(text(" " + ShortDate(r.date) + " " + WeekdayName(IsoWeekday(r.date))), kDateW),
          Cell(text(time) | color(Color::Magenta), kTimeW),
          hbox(std::move(title)) | xframe | flex,
          Cell(text(FormatLead(*r.event.remind)), kLeadW),
          Cell(state, kStateW),
      });
      if (static_cast<int>(i) == selected_) row = row | inverted | focus;
      rows.push_back(row);
    }
    if (list.empty()) rows.push_back(text(Tr(" No events need reminding right now")) | dim);

    return Panel(Tr("Reminders ({n}, sorted by time)", {{"n", std::to_string(list.size())}}),
                 vbox(std::move(rows)) | vscroll_indicator | yframe | flex, true);
  }

 private:
  AppState& s_;
  std::function<void(Date)> jump_to_;
  std::function<void()> on_back_;
  int selected_ = 0;
};

}  // namespace

Component ReminderScreen(AppState& state, std::function<void(Date)> jump_to, std::function<void()> on_back) {
  return Make<ReminderScreenBase>(state, std::move(jump_to), std::move(on_back));
}

}  // namespace cal::ui
