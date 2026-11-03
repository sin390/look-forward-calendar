#include <algorithm>
#include <array>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "ui/screens.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;

namespace {

enum Filter { kActive = 0, kAll = 1, kCompleted = 2 };

Color StatusColor(const TaskEntry& e) {
  if (e.plan.overdue) return Color::Red;
  if (e.state.status == TaskStatus::kPaused) return Color::Yellow;
  if (e.state.ended) return Color::GrayDark;
  return Color::Green;
}

// 表格列宽（名称列自适应）
constexpr int kStatusW = 12, kGaugeW = 22, kDoneW = 15, kDateW = 12, kWeekW = 24, kTodayW = 8;

Element Cell(Element e, int width) { return std::move(e) | size(WIDTH, EQUAL, width); }

Element HeaderRow() {
  return hbox({
             text(Tr(" Task")) | flex,
             Cell(text(Tr("Status")), kStatusW),
             Cell(text(Tr("Progress")), kGaugeW),
             Cell(text(Tr("Done/Total")), kDoneW),
             Cell(text(Tr("Created")), kDateW),
             Cell(text(Tr("Due")), kDateW),
             Cell(text(Tr("Weekly")), kWeekW),
             Cell(text(Tr("Today")), kTodayW),
         }) |
         bold;
}

Element TaskRow(const TaskEntry& e, Date today, const HolidayCalendar& holidays) {
  const TaskSpec& spec = e.state.spec;
  const Color c = StatusColor(e);
  const int done = e.state.DoneTotal();
  const float ratio = spec.total_minutes > 0 ? std::min(1.0f, float(done) / float(spec.total_minutes)) : 0.0f;

  std::string today_text = "-";
  if (!e.state.ended) {
    if (const auto it = e.plan.daily.find(today); it != e.plan.daily.end())
      today_text = FormatMinutes(it->second);
    else if (!IsWorkday(spec, today, holidays))
      today_text = Tr("Off");
  }

  auto name = text(" " + e.task.name);
  if (e.plan.overdue) name |= color(Color::Red);
  return hbox({
      hbox({name, text(" " + IdTag(e.task.id)) | dim}) | flex,
      Cell(text(StatusText(e.state, e.plan, today)) | color(c), kStatusW),
      Cell(hbox({gauge(ratio) | color(c) | flex,
                 text(" " + std::to_string(int(ratio * 100 + 0.5f)) + "%") | size(WIDTH, EQUAL, 5)}),
           kGaugeW),
      Cell(text(FormatMinutes(done) + " / " + FormatMinutes(spec.total_minutes)), kDoneW),
      Cell(text(FormatDate(e.state.created)), kDateW),
      Cell(text(FormatDate(spec.due)), kDateW),
      Cell(text(WeekdaysText(spec.weekdays) + (spec.skip_holiday ? Tr(" skip {country}", {{"country", spec.country}}) : "")), kWeekW),
      Cell(text(today_text) | color(Color::Cyan), kTodayW),
  });
}

class OverviewScreenBase : public ComponentBase {
 public:
  OverviewScreenBase(AppState& s, std::function<void()> on_back) : s_(s), on_back_(std::move(on_back)) {}

  bool Focusable() const override { return true; }

  bool OnEvent(Event e) override {
    if (e.is_mouse() || e == Event::Custom) return false;
    const bool back = e == Event::Escape || e == Event::Character('q');

    if (mode_ == kConfirmDelete) {  // 只有 y 确认，其他任意键取消
      if (e == Event::Character('y')) {
        const TaskEntry* t = s_.FindTask(target_);
        const std::string label = t ? t->task.name + " " + IdTag(t->task.id) : target_;
        Report(s_, s_.DeleteTask(target_), Tr("Deleted task {task}", {{"task", label}}));
      } else {
        s_.Info(Tr("Deletion cancelled"));
      }
      mode_ = kList;
      return true;
    }

    if (mode_ == kLog) {
      if (back || e == Event::Return || e == Event::Character('h') || e == Event::ArrowLeft) mode_ = kList;
      else if (e == Event::ArrowDown || e == Event::Character('j')) ++log_selected_;
      else if (e == Event::ArrowUp || e == Event::Character('k')) --log_selected_;
      else return false;
      return true;
    }

    if (back || e == Event::Character('o')) on_back_();
    else if (e == Event::Character('1')) SetFilter(kActive);
    else if (e == Event::Character('2')) SetFilter(kAll);
    else if (e == Event::Character('3')) SetFilter(kCompleted);
    else if (e == Event::ArrowDown || e == Event::Character('j')) ++selected_;
    else if (e == Event::ArrowUp || e == Event::Character('k')) --selected_;
    else if (e == Event::Return || e == Event::Character('l') || e == Event::ArrowRight) OpenSelected(kLog);
    else if (e == Event::Character('d')) OpenSelected(kConfirmDelete);
    else return false;
    return true;
  }

  Element OnRender() override {
    if (mode_ == kLog) {
      if (const TaskEntry* t = s_.FindTask(target_)) return LogView(*t);
      mode_ = kList;
    }
    Element list_view = ListView();
    if (mode_ == kConfirmDelete) {
      if (const TaskEntry* t = s_.FindTask(target_)) return dbox({list_view, ConfirmBox(*t)});
      mode_ = kList;
    }
    return list_view;
  }

 private:
  enum Mode { kList, kLog, kConfirmDelete };

  void OpenSelected(Mode mode) {
    const auto list = Filtered();
    if (selected_ < 0 || selected_ >= static_cast<int>(list.size())) return;
    target_ = list[static_cast<size_t>(selected_)]->task.id;
    log_selected_ = 0;
    mode_ = mode;
  }

  static Element ConfirmBox(const TaskEntry& t) {
    return window(text(Tr(" Delete task ")) | bold | color(Color::Red),
                  vbox({
                      text(Tr("All information of {task} will be deleted", {{"task", t.task.name + " " + IdTag(t.task.id)}})),
                      text(Tr("(the task file and its whole log; this cannot be undone)")) | dim,
                      separatorLight(),
                      hbox({text(Tr("Press ")), text(" y ") | inverted, text(Tr(" to delete, any other key to cancel"))}),
                  })) |
           clear_under | center;
  }

  // 任务日志，按时间顺序
  Element LogView(const TaskEntry& t) {
    const int n = static_cast<int>(t.task.log.size());
    log_selected_ = std::clamp(log_selected_, 0, std::max(0, n - 1));
    Elements rows;
    for (int i = 0; i < n; ++i) {
      const Json& entry = t.task.log[static_cast<size_t>(i)];
      std::string at = entry.is_object() ? entry.value("at", "") : "";
      std::replace(at.begin(), at.end(), 'T', ' ');
      auto row = hbox({
          text(" " + std::to_string(i + 1)) | dim | size(WIDTH, EQUAL, 5),
          text(at) | color(Color::Cyan) | size(WIDTH, EQUAL, 21),
          paragraph(DescribeLogEntry(entry)) | flex,
      });
      if (i == log_selected_) row = row | inverted | focus;
      rows.push_back(row);
    }
    const TaskSpec& spec = t.state.spec;
    auto summary = text(" " + Tr("{status}    Done {done} / {total}    Due {due}    File task/{file}.json",
                                 {{"status", StatusText(t.state, t.plan, s_.today())},
                                  {"done", FormatMinutes(t.state.DoneTotal())},
                                  {"total", FormatMinutes(spec.total_minutes)},
                                  {"due", FormatDate(spec.due)},
                                  {"file", t.task.file_stem}})) |
                   dim;
    return Panel(Tr("Task log: {task} ({n} entries, in chronological order)",
                    {{"task", t.task.name + " " + IdTag(t.task.id)}, {"n", std::to_string(n)}}),
                 vbox({summary, separatorLight(),
                       hbox({text(" #") | size(WIDTH, EQUAL, 5), text(Tr("Time")) | size(WIDTH, EQUAL, 21),
                             text(Tr("Content"))}) | bold,
                       separatorLight(), vbox(std::move(rows)) | vscroll_indicator | yframe | flex}),
                 true);
  }

  Element ListView() {
    const auto list = Filtered();
    selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(list.size()) - 1));

    static constexpr std::array<const char*, 3> kTabs = {N_("1 Active"), N_("2 All"), N_("3 Completed")};
    Elements tabs;
    for (int i = 0; i < 3; ++i) {
      auto t = text(" " + Tr(kTabs[static_cast<size_t>(i)]) + " ");
      tabs.push_back(i == filter_ ? t | inverted | bold : t | dim);
      tabs.push_back(text(" "));
    }
    static constexpr std::array<const char*, 3> kSortHint = {
        N_("Sorted by due date"), N_("Sorted by creation date"), N_("Sorted by final due date")};
    tabs.push_back(filler());
    tabs.push_back(text(Tr(kSortHint[static_cast<size_t>(filter_)])) | dim);

    Elements rows;
    for (size_t i = 0; i < list.size(); ++i) {
      auto row = TaskRow(*list[i], s_.today(), s_.holidays());
      if (static_cast<int>(i) == selected_) row = row | inverted | focus;
      rows.push_back(row);
    }
    if (rows.empty()) rows.push_back(text(Tr(" No matching tasks")) | dim);

    return Panel(Tr("Task overview ({n})", {{"n", std::to_string(list.size())}}),
                 vbox({hbox(std::move(tabs)), separatorLight(), HeaderRow(), separatorLight(),
                       vbox(std::move(rows)) | vscroll_indicator | yframe | flex}),
                 true);
  }

  void SetFilter(int f) {
    filter_ = f;
    selected_ = 0;
  }

  std::vector<const TaskEntry*> Filtered() const {
    std::vector<const TaskEntry*> list;
    for (const TaskEntry& e : s_.tasks()) {
      if (filter_ == kActive && e.state.ended) continue;
      if (filter_ == kCompleted && e.state.status != TaskStatus::kCompleted) continue;
      list.push_back(&e);
    }
    const auto by_created = [](const TaskEntry* a, const TaskEntry* b) {
      return a->state.created != b->state.created ? a->state.created < b->state.created
                                                  : a->task.file_stem < b->task.file_stem;
    };
    const auto by_due = [&](const TaskEntry* a, const TaskEntry* b) {
      return a->state.spec.due != b->state.spec.due ? a->state.spec.due < b->state.spec.due
                                                    : by_created(a, b);
    };
    if (filter_ == kAll) std::stable_sort(list.begin(), list.end(), by_created);
    else std::stable_sort(list.begin(), list.end(), by_due);
    return list;
  }

  AppState& s_;
  std::function<void()> on_back_;
  Mode mode_ = kList;
  int filter_ = kActive;
  int selected_ = 0;
  std::string target_;  // 日志/删除确认针对的任务 id
  int log_selected_ = 0;
};

}  // namespace

Component OverviewScreen(AppState& state, std::function<void()> on_back) {
  return Make<OverviewScreenBase>(state, std::move(on_back));
}

}  // namespace cal::ui
