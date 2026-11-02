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
constexpr int kStatusW = 8, kGaugeW = 22, kDoneW = 15, kDateW = 12, kWeekW = 16, kTodayW = 8;

Element Cell(Element e, int width) { return std::move(e) | size(WIDTH, EQUAL, width); }

Element HeaderRow() {
  return hbox({
             text(" 任务") | flex,
             Cell(text("状态"), kStatusW),
             Cell(text("进度"), kGaugeW),
             Cell(text("已做/总计"), kDoneW),
             Cell(text("创建"), kDateW),
             Cell(text("截止"), kDateW),
             Cell(text("每周"), kWeekW),
             Cell(text("今日"), kTodayW),
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
      today_text = "休";
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
      Cell(text(WeekdaysText(spec.weekdays) + (spec.skip_holiday ? " 跳" + spec.country : "")), kWeekW),
      Cell(text(today_text) | color(Color::Cyan), kTodayW),
  });
}

class OverviewScreenBase : public ComponentBase {
 public:
  explicit OverviewScreenBase(AppState& s) : s_(s) {}

  bool Focusable() const override { return true; }

  bool OnEvent(Event e) override {
    if (e.is_mouse()) return false;
    if (e == Event::Character('1')) SetFilter(kActive);
    else if (e == Event::Character('2')) SetFilter(kAll);
    else if (e == Event::Character('3')) SetFilter(kCompleted);
    else if (e == Event::ArrowDown || e == Event::Character('j')) ++selected_;
    else if (e == Event::ArrowUp || e == Event::Character('k')) --selected_;
    else return false;
    return true;
  }

  Element OnRender() override {
    const auto list = Filtered();
    selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(list.size()) - 1));

    static constexpr std::array<const char*, 3> kTabs = {"1 进行中", "2 全部", "3 已完成"};
    Elements tabs;
    for (int i = 0; i < 3; ++i) {
      auto t = text(" " + std::string(kTabs[static_cast<size_t>(i)]) + " ");
      tabs.push_back(i == filter_ ? t | inverted | bold : t | dim);
      tabs.push_back(text(" "));
    }
    static constexpr std::array<const char*, 3> kSortHint = {
        "按截止日期排序", "按创建日期排序", "按最终截止日期排序"};
    tabs.push_back(filler());
    tabs.push_back(text(kSortHint[static_cast<size_t>(filter_)]) | dim);

    Elements rows;
    for (size_t i = 0; i < list.size(); ++i) {
      auto row = TaskRow(*list[i], s_.today(), s_.holidays());
      if (static_cast<int>(i) == selected_) row = row | inverted | focus;
      rows.push_back(row);
    }
    if (rows.empty()) rows.push_back(text(" 没有符合条件的任务") | dim);

    return Panel("任务总览（共 " + std::to_string(list.size()) + " 个）",
                 vbox({hbox(std::move(tabs)), separatorLight(), HeaderRow(), separatorLight(),
                       vbox(std::move(rows)) | vscroll_indicator | yframe | flex}),
                 true);
  }

 private:
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
  int filter_ = kActive;
  int selected_ = 0;
};

}  // namespace

Component OverviewScreen(AppState& state) { return Make<OverviewScreenBase>(state); }

}  // namespace cal::ui
