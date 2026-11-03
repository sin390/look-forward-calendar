#include "ui/day_panel.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "core/lunar.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;  // 键盘事件；数据模型的事件写作 cal::Event

namespace {

Color StatusColor(const TaskEntry& e) {
  if (e.plan.overdue) return Color::Red;
  switch (e.state.status) {
    case TaskStatus::kActive: return Color::Green;
    case TaskStatus::kPaused: return Color::Yellow;
    default: return Color::GrayDark;
  }
}

class DayPanelBase : public ComponentBase {
 public:
  DayPanelBase(AppState& s, DayPanelActions actions) : s_(s), actions_(std::move(actions)) {}

  bool Focusable() const override { return true; }

  Element OnRender() override {
    EnsureBuilt();
    return Panel(ShortDate(s_.selected()) + " " + WeekdayName(IsoWeekday(s_.selected())),
                 vbox({Header(), separatorLight(), body_->Render() | vscroll_indicator | yframe | flex}),
                 Focused());
  }

  bool OnEvent(Event e) override {
    EnsureBuilt();
    return ComponentBase::OnEvent(e);
  }

 private:
  // 数据版本变化后才重建，重建发生在下一次渲染/事件开始时，
  // 不会在按钮回调执行中途销毁按钮自身
  void EnsureBuilt() {
    if (built_version_ == s_.version()) return;
    built_version_ = s_.version();
    Rebuild();
  }

  Element Header() {
    const Date d = s_.selected();
    Elements lines;
    if (!s_.show_lunar()) {
      lines.push_back(WrapText(s_.slogan()) | color(Color::Yellow));  // 关闭农历时，农历的位置显示标语
    } else if (const auto ld = ToLunar(d)) {
      lines.push_back(text(Tr("Lunar: {ganzhi}{zodiac} year, {month}{day}",
                              {{"ganzhi", GanzhiYear(ld->year)}, {"zodiac", Zodiac(ld->year)},
                               {"month", LunarMonthName(*ld)}, {"day", LunarDayName(ld->day)}})));
      std::string extra = LunarFestival(*ld);
      if (auto t = SolarTerm(d); !t.empty()) extra += (extra.empty() ? "" : " ") + t;
      if (!extra.empty()) lines.push_back(WrapText(extra) | color(Color::Yellow));
    }
    // 各节假日配置当天的节日，按“国家：假日”显示；国家取自该配置的节假日文件
    for (int config = 1; config <= kHolidayConfigCount; ++config) {
      const HolidayDay* h = s_.holidays().Find(config, d);
      if (!h) continue;
      std::string country = s_.holidays().Country(config, YearNum(d));
      if (country.empty()) country = HolidayConfigName(config);
      lines.push_back(WrapText(h->off ? Tr("{country}: {holiday} (off)", {{"country", country}, {"holiday", h->name}})
                                      : Tr("{country}: {holiday} (workday)", {{"country", country}, {"holiday", h->name}})) |
                      color(h->off ? Color::Red : Color::Blue));
    }
    return vbox(std::move(lines));
  }

  void Rebuild() {
    DetachAllChildren();
    const Date d = s_.selected();
    const Date today = s_.today();
    Components rows;

    // ---- 瞬间型事件 ----
    rows.push_back(Renderer([] { return text(Tr("[Events]")) | bold; }));
    if (const Day* day = s_.FindDay(d)) {
      for (size_t i = 0; i < day->events.size(); ++i) {
        const cal::Event ev = day->events[i];
        auto edit = Button(Tr("Edit"), [this, d, i] { actions_.edit_event(d, i); }, SmallButton());
        auto del = Button(Tr("Del"), [this, d, i] { Report(s_, s_.DeleteEvent(d, i), Tr("Event deleted")); },
                          SmallButton());
        auto row = Container::Horizontal({edit, del});
        rows.push_back(Renderer(row, [ev, edit, del] {
          Elements left = {text(EventTimeText(ev).empty() ? Tr("All day") : EventTimeText(ev)) |
                               color(Color::Magenta) | size(WIDTH, EQUAL, 12),
                           WrapText(ev.title) | flex};
          auto top = hbox({hbox(std::move(left)) | flex, edit->Render(), del->Render()});
          Elements lines = {top};
          if (ev.remind) lines.push_back(text(Tr("  ⏰ {lead} before", {{"lead", FormatLead(*ev.remind)}})) | color(Color::Yellow));
          if (!ev.note.empty()) lines.push_back(hbox({text("  "), WrapText(ev.note) | flex}) | dim);
          return vbox(std::move(lines));
        }));
      }
    }
    rows.push_back(Button(Tr("+ New event (a)"), [this, d] { actions_.new_event(d); }, SmallButton()));

    // ---- 固定日程 ----
    confirm_fixed_id_.clear();
    rows.push_back(Renderer([] { return vbox({text(""), text(Tr("[Fixed schedules]")) | bold}); }));
    for (const FixedSchedule* f : s_.FixedOn(d)) rows.push_back(FixedRow(*f));
    rows.push_back(Button(Tr("+ New fixed schedule (f)"), [this, d] { actions_.new_fixed(d); }, SmallButton()));

    // ---- 长期任务 ----
    rows.push_back(Renderer([] { return vbox({text(""), text(Tr("[Tasks]")) | bold}); }));
    for (const TaskEntry& entry : s_.tasks()) {
      if (!ShowsOn(entry.state, d, today)) continue;
      rows.push_back(TaskRow(entry, d, today));
    }
    rows.push_back(Button(Tr("+ New task (n)"), [this] { actions_.new_task(); }, SmallButton()));

    // 重建后尽量保持原来的焦点行，落在标题行等不可聚焦行时就近调整
    const int n = static_cast<int>(rows.size());
    selector_ = std::clamp(selector_, 0, n - 1);
    for (int step = 0; step < n; ++step) {
      if (selector_ + step < n && rows[static_cast<size_t>(selector_ + step)]->Focusable()) {
        selector_ += step;
        break;
      }
      if (selector_ - step >= 0 && rows[static_cast<size_t>(selector_ - step)]->Focusable()) {
        selector_ -= step;
        break;
      }
    }
    body_ = Container::Vertical(std::move(rows), &selector_);
    Add(body_);
  }

  // 固定日程一行：时间段 + 工作内容 + [删]，下面是重复方式和备注。
  // 删除会删掉所有日期，所以要按两次：第一次变为 [确认删除]
  Component FixedRow(const FixedSchedule& f) {
    const FixedSchedule copy = f;
    auto del = Button(Tr("Del"), [this, id = f.id, title = f.title] {
      if (confirm_fixed_id_ != id) {
        confirm_fixed_id_ = id;
        s_.Info(Tr("Press again to delete the fixed schedule \"{title}\" (all dates)", {{"title", title}}));
        return;
      }
      confirm_fixed_id_.clear();
      Report(s_, s_.DeleteFixed(id), Tr("Deleted fixed schedule: {title}", {{"title", title}}));
    }, SmallButton());
    return Renderer(del, [this, copy, del] {
      Element button = del->Render();
      if (confirm_fixed_id_ == copy.id) {
        button = text("[" + Tr("Confirm delete") + "]") | color(Color::Red) | bold;
        if (del->Focused()) button |= inverted;
      }
      auto top = hbox({text(copy.start + "-" + copy.end) | color(Color::Cyan) | size(WIDTH, EQUAL, 12),
                       WrapText(copy.title) | flex, button});
      Elements lines = {top, hbox({text("  "), WrapText(RepeatText(copy)) | flex}) | dim};
      if (!copy.note.empty()) lines.push_back(hbox({text("  "), WrapText(copy.note) | flex}) | dim);
      return vbox(std::move(lines));
    });
  }

  Component TaskRow(const TaskEntry& entry, Date d, Date today) {
    const std::string id = entry.task.id;
    Components controls;

    // [进行]：填写当天实际进行情况，可反复修改；未来日期锁定（不可聚焦）
    const bool locked = d > today;
    if (!locked)
      controls.push_back(Button(Tr("Log"), [this, id, d] { actions_.edit_progress(id, d); }, SmallButton()));

    // 状态按钮，顺序：进行、暂停/恢复、修改、中止、完成
    const bool ended = entry.state.ended.has_value();
    if (!ended) {
      if (entry.state.status == TaskStatus::kPaused)
        controls.push_back(Button(Tr("Resume"), [this, id] { ChangeStatus(id, "resume", Tr("Task resumed")); }, SmallButton()));
      else if (!entry.plan.overdue)
        controls.push_back(Button(Tr("Pause"), [this, id] { ChangeStatus(id, "pause", Tr("Task paused")); }, SmallButton()));
      controls.push_back(Button(Tr("Modify"), [this, id] { actions_.edit_task(id); }, SmallButton()));
      controls.push_back(Button(Tr("Abort"), [this, id] { ChangeStatus(id, "abort", Tr("Task aborted")); }, SmallButton()));
      controls.push_back(Button(Tr("Done"), [this, id] { ChangeStatus(id, "complete", Tr("Task completed")); }, SmallButton()));
    }

    auto row = Container::Horizontal(controls);
    return Renderer(row, [this, id, d, today, locked, controls] {
      const TaskEntry* e = s_.FindTask(id);
      if (!e) return text("");
      auto id_status = hbox({text(IdTag(id)) | dim, filler(),
                             text(StatusText(e->state, e->plan, today)) | color(StatusColor(*e))});

      std::string plan_text = "-";
      if (e->plan.overdue) {
        plan_text = "-";
      } else if (d >= today) {
        if (const auto it = e->plan.daily.find(d); it != e->plan.daily.end())
          plan_text = FormatMinutes(it->second);
        else if (!IsWorkday(e->state.spec, d, s_.holidays()))
          plan_text = Tr("Off");
      }
      const DayProgress done = e->state.ProgressOn(d);
      auto amounts = hbox({text(Tr(" Plan {time}", {{"time", plan_text}})) | size(WIDTH, EQUAL, 14),
                           text(Tr("Actual {time}", {{"time", locked ? std::string("-") : FormatMinutes(done.minutes)}}))});

      Elements buttons;
      if (locked) buttons.push_back(hbox({text("[" + Tr("Log") + "]") | dim | strikethrough, text(Tr("locked")) | dim}));
      for (const Component& c : controls) buttons.push_back(c->Render());

      Elements lines = {WrapText(e->task.name) | bold, id_status, amounts};
      if (!done.note.empty()) lines.push_back(WrapText(Tr(" Note: {note}", {{"note", done.note}})) | dim);
      lines.push_back(hbox(std::move(buttons)));
      lines.push_back(text(""));  // 任务之间空一行
      return vbox(std::move(lines));
    });
  }

  void ChangeStatus(const std::string& id, std::string_view op, const std::string& ok_msg) {
    Report(s_, s_.ChangeTaskStatus(id, op), ok_msg);
  }

  AppState& s_;
  DayPanelActions actions_;
  uint64_t built_version_ = ~uint64_t{0};
  Component body_;
  int selector_ = 0;
  std::string confirm_fixed_id_;  // 已按过一次 [删]、等待确认的固定日程
};

}  // namespace

Component DayPanel(AppState& state, DayPanelActions actions) {
  return Make<DayPanelBase>(state, std::move(actions));
}

}  // namespace cal::ui
