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
                 vbox({Header(), separatorLight(), body_->Render() | vscroll_indicator | frame | flex}),
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
    if (const auto ld = ToLunar(d)) {
      lines.push_back(text("农历 " + GanzhiYear(ld->year) + Zodiac(ld->year) + "年 " +
                           LunarMonthName(*ld) + LunarDayName(ld->day)));
      std::string extra = LunarFestival(*ld);
      if (auto t = SolarTerm(d); !t.empty()) extra += (extra.empty() ? "" : " ") + t;
      if (!extra.empty()) lines.push_back(text(extra) | color(Color::Yellow));
    }
    for (const Country& c : SupportedCountries()) {
      if (const HolidayDay* h = s_.holidays().Find(c.code, d))
        lines.push_back(text(c.name + "：" + h->name + (h->off ? "（休）" : "（班）")) |
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
    rows.push_back(Renderer([] { return text("【事件】") | bold; }));
    if (const Day* day = s_.FindDay(d)) {
      for (size_t i = 0; i < day->events.size(); ++i) {
        const cal::Event ev = day->events[i];
        auto edit = Button("改", [this, d, i] { actions_.edit_event(d, i); }, SmallButton());
        auto del = Button("删", [this, d, i] { Report(s_, s_.DeleteEvent(d, i), "已删除事件"); },
                          SmallButton());
        auto row = Container::Horizontal({edit, del});
        rows.push_back(Renderer(row, [ev, edit, del] {
          Elements left = {text(EventTimeText(ev).empty() ? "全天" : EventTimeText(ev)) |
                               color(Color::Magenta) | size(WIDTH, EQUAL, 12),
                           text(ev.title) | flex};
          auto top = hbox({hbox(std::move(left)) | flex, edit->Render(), del->Render()});
          Elements lines = {top};
          if (ev.remind) lines.push_back(text("  ⏰ 提前" + FormatLead(*ev.remind)) | color(Color::Yellow));
          if (!ev.note.empty()) lines.push_back(text("  " + ev.note) | dim);
          return vbox(std::move(lines));
        }));
      }
    }
    rows.push_back(Button("+ 新建事件 (a)", [this, d] { actions_.new_event(d); }, SmallButton()));

    // ---- 长期任务 ----
    rows.push_back(Renderer([] { return vbox({text(""), text("【任务】") | bold}); }));
    for (const TaskEntry& entry : s_.tasks()) {
      if (!ShowsOn(entry.state, d, today)) continue;
      rows.push_back(TaskRow(entry, d, today));
    }
    rows.push_back(Button("+ 新建任务 (n)", [this] { actions_.new_task(); }, SmallButton()));

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

  Component TaskRow(const TaskEntry& entry, Date d, Date today) {
    const std::string id = entry.task.id;
    Components controls;

    // [进行]：填写当天实际进行情况，可反复修改；未来日期锁定（不可聚焦）
    const bool locked = d > today;
    Component progress;
    if (!locked) {
      progress = Button("进行", [this, id, d] { actions_.edit_progress(id, d); }, SmallButton());
      controls.push_back(progress);
    }

    // 状态按钮
    const bool ended = entry.state.ended.has_value();
    if (!ended) {
      if (entry.state.status == TaskStatus::kPaused)
        controls.push_back(Button("恢复", [this, id] { ChangeStatus(id, "resume", "已恢复"); }, SmallButton()));
      else if (!entry.plan.overdue)
        controls.push_back(Button("暂停", [this, id] { ChangeStatus(id, "pause", "已暂停"); }, SmallButton()));
      controls.push_back(Button("完成", [this, id] { ChangeStatus(id, "complete", "已完成"); }, SmallButton()));
      controls.push_back(Button("中止", [this, id] { ChangeStatus(id, "abort", "已中止"); }, SmallButton()));
      controls.push_back(Button("修改", [this, id] { actions_.edit_task(id); }, SmallButton()));
    }

    auto row = Container::Horizontal(controls);
    return Renderer(row, [this, id, d, today, locked, progress, controls] {
      const TaskEntry* e = s_.FindTask(id);
      if (!e) return text("");
      auto title = hbox({text(e->task.name) | bold, text(" " + IdTag(id)) | dim, filler(),
                         text(StatusText(e->state, e->plan, today)) | color(StatusColor(*e))});

      std::string plan_text = "-";
      if (e->plan.overdue) {
        plan_text = "-";
      } else if (d >= today) {
        if (const auto it = e->plan.daily.find(d); it != e->plan.daily.end())
          plan_text = FormatMinutes(it->second);
        else if (!IsWorkday(e->state.spec, d, s_.holidays()))
          plan_text = "休";
      }
      const DayProgress done = e->state.ProgressOn(d);
      Element progress_button =
          locked ? hbox({text("[进行]") | dim | strikethrough, text("锁") | dim}) : progress->Render();
      auto line2 = hbox({text(" 计划 " + plan_text) | size(WIDTH, EQUAL, 12),
                         text("实际 " + (locked ? std::string("-") : FormatMinutes(done.minutes))) |
                             size(WIDTH, EQUAL, 12),
                         filler(), progress_button});

      Elements buttons;
      for (const Component& c : controls)
        if (c != progress) buttons.push_back(c->Render());

      Elements lines = {title, line2};
      if (!done.note.empty()) lines.push_back(paragraph(" 内容：" + done.note) | dim);
      if (!buttons.empty()) lines.push_back(hbox(std::move(buttons)));
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
};

}  // namespace

Component DayPanel(AppState& state, DayPanelActions actions) {
  return Make<DayPanelBase>(state, std::move(actions));
}

}  // namespace cal::ui
