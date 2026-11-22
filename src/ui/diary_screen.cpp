#include <algorithm>
#include <cstdio>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "ui/screens.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;

namespace {

constexpr int kListW = 20;  // 右侧日记列表宽度

class DiaryScreenBase : public ComponentBase {
 public:
  DiaryScreenBase(AppState& s, std::function<void()> on_back) : s_(s), on_back_(std::move(on_back)) {
    InputOption option;
    option.multiline = true;
    option.transform = [](InputState state) {
      if (state.is_placeholder) state.element |= dim;
      return state.element;
    };
    option.cursor_position = &cursor_;
    input_ = Input(&text_, Tr("Write your diary here..."), option);
    Add(input_);
  }

  bool Focusable() const override { return true; }

  bool OnEvent(Event e) override {
    if (e.is_mouse() || e == Event::Custom) return false;
    if (editing_) {
      if (e == Event::Escape) {  // 保存并结束编辑
        Save();
        return true;
      }
      input_->OnEvent(e);
      return true;  // 编辑中所有按键都交给输入框
    }

    Date d = s_.selected();
    if (e == Event::Escape || e == Event::Character('q') || e == Event::Character('d')) {
      on_back_();
      return true;
    }
    if (e == Event::Return || e == Event::Character('e')) {
      StartEdit();
      return true;
    }
    // ↑↓ 滚动正文
    if (e == Event::ArrowDown || e == Event::Character('j')) {
      ScrollFor(d);
      ++scroll_;
      return true;
    }
    if (e == Event::ArrowUp || e == Event::Character('k')) {
      ScrollFor(d);
      scroll_ = std::max(0, scroll_ - 1);
      return true;
    }
    const bool left = e == Event::ArrowLeft || e == Event::Character('h');
    if (left || e == Event::ArrowRight || e == Event::Character('l')) {
      // 上一篇 / 下一篇日记（可跨月）
      const auto target = left ? s_.PrevDiary(d) : s_.NextDiary(d);
      if (!target) {
        s_.Info(left ? Tr("No earlier diary") : Tr("No later diary"));
        return true;
      }
      d = *target;
    } else if (e == Event::Character('[') || e == Event::Character(']')) {
      // 切换月份：选中该月最后一篇日记，没有日记时选中 1 日
      const Date first = AddMonths(FirstOfMonth(d), e == Event::Character('[') ? -1 : 1);
      const auto month = MonthDiaries(first);
      d = month.empty() ? first : month.back();
    } else if (e == Event::Character('t')) {
      d = s_.today();
    } else {
      return false;
    }
    s_.Select(d);
    return true;
  }

  Element OnRender() override {
    const Date d = s_.selected();
    // 换了日期或数据有变化（刷新、保存）时重新读取
    if (!editing_ && (!loaded_ || *loaded_ != d || loaded_version_ != s_.version())) Load(d);

    std::string title = Tr("Diary  {date} {weekday}", {{"date", FormatDate(d)}, {"weekday", WeekdayName(IsoWeekday(d))}});
    if (d == s_.today()) title += Tr(" (today)");
    if (editing_) title += Tr("  [editing]");

    Element body;
    if (editing_) {
      body = input_->Render() | flex;
    } else if (text_.empty()) {
      body = vbox({text(""), text(Tr(" No diary for this day. Press Enter to write one.")) | dim});
    } else {
      // 只读显示时自动换行；↑↓ 按段落（原文的行）滚动，跳过前 scroll_ 段
      std::vector<std::string_view> paragraphs;
      for (size_t start = 0; start <= text_.size();) {
        size_t end = text_.find('\n', start);
        if (end == std::string::npos) end = text_.size();
        paragraphs.push_back(std::string_view(text_).substr(start, end - start));
        start = end + 1;
      }
      ScrollFor(d);
      scroll_ = std::clamp(scroll_, 0, static_cast<int>(paragraphs.size()) - 1);
      Elements lines;
      if (scroll_ > 0) lines.push_back(text(Tr(" ↑ {n} more lines above", {{"n", std::to_string(scroll_)}})) | dim);
      for (size_t i = static_cast<size_t>(scroll_); i < paragraphs.size(); ++i) lines.push_back(WrapText(paragraphs[i]));
      body = vbox(std::move(lines)) | yframe;
    }
    auto diary = Panel(title, body | flex, true) | flex;
    return hbox({diary, DateList(d)});
  }

 private:
  void Load(Date d) {
    auto r = s_.LoadDiary(d);
    if (!r) s_.Error(r.error());
    text_ = r ? std::move(*r) : std::string();
    loaded_ = d;
    loaded_version_ = s_.version();
  }

  // 滚动位置属于某一天，换了日期从头显示
  void ScrollFor(Date d) {
    if (scroll_date_ != d) scroll_ = 0;
    scroll_date_ = d;
  }

  void StartEdit() {
    Load(s_.selected());
    cursor_ = static_cast<int>(text_.size());  // 光标放在末尾，接着写
    editing_ = true;
    input_->TakeFocus();
  }

  // 保存失败时停留在编辑状态，避免丢失内容
  void Save() {
    const Date d = *loaded_;
    // 去掉末尾多余的空行
    while (!text_.empty() && (text_.back() == '\n' || text_.back() == ' ')) text_.pop_back();
    if (const Result r = s_.SaveDiary(d, text_); !r) {
      s_.Error(r.error());
      return;
    }
    s_.Info(text_.empty() ? Tr("Diary for {date} deleted", {{"date", ShortDate(d)}})
                          : Tr("Diary saved for {date}", {{"date", ShortDate(d)}}));
    editing_ = false;
  }

  static Date FirstOfMonth(Date d) { return d.year() / d.month() / std::chrono::day{1}; }

  // 某月有日记的日期，按日期排列
  std::vector<Date> MonthDiaries(Date first) const {
    const auto& dates = s_.diary_dates();
    const Date next = AddMonths(first, 1);
    return {dates.lower_bound(first), dates.lower_bound(next)};
  }

  // 右侧：选中日期所在月份的日记，按日期排列，当前日期高亮。
  // 选中的日期没有日记时也列出（灰色），便于看出当前位置
  Element DateList(Date current) {
    std::vector<Date> month = MonthDiaries(FirstOfMonth(current));
    const size_t n = month.size();
    if (!s_.HasDiary(current)) month.insert(std::lower_bound(month.begin(), month.end(), current), current);
    Elements rows;
    for (Date d : month) {
      char day[4];
      std::snprintf(day, sizeof day, "%2u", DayNum(d));
      auto row = text(" " + std::string(day) + "  " + WeekdayName(IsoWeekday(d)));
      if (!s_.HasDiary(d)) row |= dim;
      if (d == current) row = row | inverted | focus;
      rows.push_back(row);
    }
    if (n == 0) rows.push_back(text(Tr(" No diary this month")) | dim);
    return Panel(Tr("{month} ({n})", {{"month", MonthShortTitle(current)}, {"n", std::to_string(n)}}),
                 vbox(std::move(rows)) | vscroll_indicator | yframe | flex, false) |
           size(WIDTH, EQUAL, kListW);
  }

  AppState& s_;
  std::function<void()> on_back_;
  Component input_;
  std::string text_;
  std::optional<Date> loaded_;  // text_ 对应的日期
  uint64_t loaded_version_ = 0;
  int cursor_ = 0;
  bool editing_ = false;
  int scroll_ = 0;  // 只读显示时跳过的段落数
  std::optional<Date> scroll_date_;  // scroll_ 对应的日期
};

}  // namespace

Component DiaryScreen(AppState& state, std::function<void()> on_back) {
  return Make<DiaryScreenBase>(state, std::move(on_back));
}

}  // namespace cal::ui
