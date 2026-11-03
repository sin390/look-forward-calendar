#include "app/app_state.h"

#include <algorithm>
#include <cstdio>
#include <random>

namespace cal {

AppState::AppState(fs::path data_root) : repo_(std::move(data_root)) {
  today_ = Today();
  selected_ = today_;
  Reload();
}

void AppState::Reload() {
  warnings_.clear();
  months_.clear();
  broken_months_.clear();
  today_ = Today();
  pref_ = repo_.LoadPreference(warnings_);
  holidays_ = repo_.LoadHolidays(warnings_);
  fixed_ = repo_.LoadFixed(warnings_);
  fixed_months_.clear();

  tasks_.clear();
  for (Task& t : repo_.LoadTasks(warnings_)) {
    auto state = Replay(t);
    if (!state) {
      warnings_.push_back("任务 " + t.file_stem + "：" + state.error());
      continue;
    }
    tasks_.push_back({std::move(t), std::move(*state), {}});
  }
  RecomputePlans();

  if (warnings_.empty())
    Info("已刷新");
  else
    Error(warnings_.front() + (warnings_.size() > 1 ? "（等 " + std::to_string(warnings_.size()) + " 条问题）" : ""));
  Touch();
}

void AppState::Tick() {
  if (Today() != today_) Reload();
}

void AppState::Select(Date d) {
  if (d == selected_) return;
  selected_ = d;
  Touch();
}

void AppState::CycleDisplayCountry() {
  const auto& list = SupportedCountries();
  auto it = std::find_if(list.begin(), list.end(),
                         [&](const Country& c) { return c.code == pref_.country; });
  it = (it == list.end() || std::next(it) == list.end()) ? list.begin() : std::next(it);
  pref_.country = it->code;
  SavePreference();
  Touch();
}

void AppState::CycleLanguage() {
  const auto& list = SupportedLanguages();
  if (list.size() <= 1) {
    Info("目前只支持" + LanguageName(pref_.language));
    return;
  }
  auto it = std::find_if(list.begin(), list.end(),
                         [&](const Language& l) { return l.code == pref_.language; });
  it = (it == list.end() || std::next(it) == list.end()) ? list.begin() : std::next(it);
  pref_.language = it->code;
  SavePreference();
  Touch();
}

void AppState::SavePreference() {
  if (auto r = repo_.SavePreference(pref_); !r)
    Error("配置保存失败：" + r.error());
  else
    Info("已切换：国家 " + CountryName(pref_.country) + "，语言 " + LanguageName(pref_.language));
}

// ---------------------------------------------------------------- 查询

const Day* AppState::FindDay(Date d) {
  const MonthDays& month = MonthOf(d);
  const auto it = month.find(d);
  return it == month.end() ? nullptr : &it->second;
}

const TaskEntry* AppState::FindTask(const std::string& id) const {
  for (const TaskEntry& e : tasks_)
    if (e.task.id == id) return &e;
  return nullptr;
}

TaskEntry* AppState::FindTaskMutable(const std::string& id) {
  return const_cast<TaskEntry*>(std::as_const(*this).FindTask(id));
}

int AppState::PlannedMinutesOn(Date d) const {
  int sum = 0;
  for (const TaskEntry& e : tasks_)
    if (const auto it = e.plan.daily.find(d); it != e.plan.daily.end()) sum += it->second;
  return sum;
}

int AppState::ActualMinutesOn(Date d) const {
  int sum = 0;
  for (const TaskEntry& e : tasks_) sum += e.state.ProgressOn(d).minutes;
  return sum;
}

bool AppState::HasOverdueTask() const {
  return std::any_of(tasks_.begin(), tasks_.end(), [](const TaskEntry& e) { return e.plan.overdue; });
}

std::vector<Reminder> AppState::ActiveReminders(LocalMinute now) {
  const long long stamp = MinuteStamp(now.date, now.minute);
  // 提醒期间不早于事件当天之前 30 天，也不晚于事件当天结束，
  // 所以只需扫描 [今天, 今天 + 30 天] 内的事件
  const Date last = AddDays(now.date, kMaxRemindMinutes / 1440);
  std::vector<Reminder> out;
  for (Date m = now.date.year() / now.date.month() / std::chrono::day{1}; m <= last; m = AddMonths(m, 1)) {
    for (const auto& [date, day] : MonthOf(m)) {
      if (date < now.date || date > last) continue;
      for (const Event& e : day.events) {
        const auto w = RemindWindowOf(date, e);
        if (w && w->from <= stamp && stamp < w->until) out.push_back({date, e, *w});
      }
    }
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const Reminder& a, const Reminder& b) { return a.window.start < b.window.start; });
  return out;
}

// ---------------------------------------------------------------- 事件

MonthDays& AppState::MonthOf(Date d) {
  const std::pair<int, unsigned> key{YearNum(d), MonthNum(d)};
  if (auto it = months_.find(key); it != months_.end()) return it->second;
  auto loaded = repo_.LoadMonth(key.first, key.second);
  if (!loaded) {
    broken_months_.insert(key);
    warnings_.push_back(loaded.error());
    Error(loaded.error());
    return months_[key];
  }
  return months_[key] = std::move(*loaded);
}

Result AppState::SaveMonthOf(Date d) {
  const std::pair<int, unsigned> key{YearNum(d), MonthNum(d)};
  if (broken_months_.contains(key))
    return std::unexpected("该月数据文件读取失败，为避免覆盖已拒绝保存，请先修复文件");
  return repo_.SaveMonth(key.first, key.second, MonthOf(d));
}

Result AppState::AddEvent(Date d, Event e) {
  if (auto err = ValidateEvent(e)) return std::unexpected(*err);
  MonthDays& month = MonthOf(d);
  const MonthDays backup = month;
  auto& events = month[d].events;
  events.push_back(std::move(e));
  SortEvents(events);
  if (auto r = SaveMonthOf(d); !r) {
    month = backup;
    return r;
  }
  Touch();
  return {};
}

Result AppState::UpdateEvent(Date d, size_t index, Event e) {
  if (auto err = ValidateEvent(e)) return std::unexpected(*err);
  MonthDays& month = MonthOf(d);
  const auto it = month.find(d);
  if (it == month.end() || index >= it->second.events.size()) return std::unexpected("事件不存在");
  const MonthDays backup = month;
  it->second.events[index] = std::move(e);
  SortEvents(it->second.events);
  if (auto r = SaveMonthOf(d); !r) {
    month = backup;
    return r;
  }
  Touch();
  return {};
}

Result AppState::DeleteEvent(Date d, size_t index) {
  MonthDays& month = MonthOf(d);
  const auto it = month.find(d);
  if (it == month.end() || index >= it->second.events.size()) return std::unexpected("事件不存在");
  const MonthDays backup = month;
  it->second.events.erase(it->second.events.begin() + static_cast<long>(index));
  if (it->second.Empty()) month.erase(it);
  if (auto r = SaveMonthOf(d); !r) {
    month = backup;
    return r;
  }
  Touch();
  return {};
}

// ---------------------------------------------------------------- 任务

void AppState::RecomputePlans() {
  for (TaskEntry& e : tasks_) e.plan = ComputePlan(e.state, today_, holidays_);
}

std::string AppState::NewId() const {
  static std::mt19937 rng{std::random_device{}()};
  while (true) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "%04x", static_cast<unsigned>(rng() & 0xffff));
    const bool used = FindTask(buf) || std::any_of(fixed_.begin(), fixed_.end(),
                                                   [&](const FixedSchedule& f) { return f.id == buf; });
    if (!used) return buf;
  }
}

// 先写盘，成功后再替换内存中的任务并重算安排
Result AppState::CommitTask(TaskEntry& entry, Task updated) {
  auto state = Replay(updated);
  if (!state) return std::unexpected(state.error());
  if (auto r = repo_.SaveTask(updated); !r) return r;
  entry.task = std::move(updated);
  entry.state = std::move(*state);
  RecomputePlans();
  Touch();
  return {};
}

Result AppState::CreateTask(const TaskSpec& spec) {
  if (auto err = ValidateSpec(spec, today_, holidays_)) return std::unexpected(*err);
  Task task = NewTask(spec, NewId(), NowTimestamp());
  auto state = Replay(task);
  if (!state) return std::unexpected(state.error());
  if (auto r = repo_.SaveTask(task); !r) return r;
  tasks_.push_back({std::move(task), std::move(*state), {}});
  RecomputePlans();
  Touch();
  return {};
}

Result AppState::ModifyTask(const std::string& id, const TaskSpec& spec) {
  TaskEntry* entry = FindTaskMutable(id);
  if (!entry) return std::unexpected("任务不存在");
  if (entry->state.ended) return std::unexpected("任务已结束，不能修改");
  if (auto err = ValidateSpec(spec, today_, holidays_)) return std::unexpected(*err);
  Task updated = entry->task;
  if (!AppendModify(updated, entry->state.spec, spec, NowTimestamp())) return {};
  return CommitTask(*entry, std::move(updated));
}

Result AppState::ChangeTaskStatus(const std::string& id, std::string_view op) {
  TaskEntry* entry = FindTaskMutable(id);
  if (!entry) return std::unexpected("任务不存在");
  const TaskStatus s = entry->state.status;
  const bool ok = (op == "pause" && s == TaskStatus::kActive) ||
                  (op == "resume" && s == TaskStatus::kPaused) ||
                  ((op == "complete" || op == "abort") && !entry->state.ended);
  if (!ok) return std::unexpected("当前状态下不能执行该操作");
  Task updated = entry->task;
  AppendOp(updated, op, NowTimestamp());
  return CommitTask(*entry, std::move(updated));
}

Result AppState::SetProgress(const std::string& id, Date d, const DayProgress& p) {
  TaskEntry* entry = FindTaskMutable(id);
  if (!entry) return std::unexpected("任务不存在");
  if (d > today_) return std::unexpected("不能录入未来日期的工作量");
  if (d < entry->state.created) return std::unexpected("该日期早于任务创建日期");
  if (entry->state.ended && d > *entry->state.ended) return std::unexpected("任务已在该日期前结束");
  if (p.minutes < 0) return std::unexpected("工作量不能为负");
  if (entry->state.ProgressOn(d) == p) return {};
  Task updated = entry->task;
  AppendProgress(updated, d, p, NowTimestamp());
  return CommitTask(*entry, std::move(updated));
}

Result AppState::DeleteTask(const std::string& id) {
  const auto it = std::find_if(tasks_.begin(), tasks_.end(),
                               [&](const TaskEntry& e) { return e.task.id == id; });
  if (it == tasks_.end()) return std::unexpected("任务不存在");
  if (auto r = repo_.DeleteTask(it->task); !r) return r;
  tasks_.erase(it);
  Touch();
  return {};
}

// ---------------------------------------------------------------- 固定日程

const std::map<Date, std::vector<size_t>>& AppState::FixedMonthOf(Date d) {
  const std::pair<int, unsigned> key{YearNum(d), MonthNum(d)};
  if (auto it = fixed_months_.find(key); it != fixed_months_.end()) return it->second;
  std::map<Date, std::vector<size_t>> month;
  const Date first = d.year() / d.month() / std::chrono::day{1};
  const int days = static_cast<int>(DaysInMonth(d.year() / d.month()));
  for (int i = 0; i < days; ++i) {
    const Date day = AddDays(first, i);
    for (size_t k = 0; k < fixed_.size(); ++k)
      if (OccursOn(fixed_[k], day)) month[day].push_back(k);
  }
  for (auto& [_, list] : month)
    std::stable_sort(list.begin(), list.end(),
                     [this](size_t a, size_t b) { return fixed_[a].start < fixed_[b].start; });
  return fixed_months_[key] = std::move(month);
}

std::vector<const FixedSchedule*> AppState::FixedOn(Date d) {
  std::vector<const FixedSchedule*> out;
  const auto& month = FixedMonthOf(d);
  if (const auto it = month.find(d); it != month.end())
    for (size_t k : it->second) out.push_back(&fixed_[k]);
  return out;
}

Result AppState::AddFixed(FixedSchedule f) {
  if (auto err = ValidateFixed(f)) return std::unexpected(*err);
  f.id = NewId();
  const std::string at = NowTimestamp();
  f.file_stem = at.substr(0, 4) + at.substr(5, 2) + at.substr(8, 2) + "_" + f.id;
  if (auto r = repo_.SaveFixed(f); !r) return r;
  fixed_.push_back(std::move(f));
  fixed_months_.clear();
  Touch();
  return {};
}

Result AppState::DeleteFixed(const std::string& id) {
  const auto it = std::find_if(fixed_.begin(), fixed_.end(),
                               [&](const FixedSchedule& f) { return f.id == id; });
  if (it == fixed_.end()) return std::unexpected("固定日程不存在");
  if (auto r = repo_.DeleteFixed(*it); !r) return r;
  fixed_.erase(it);
  fixed_months_.clear();
  Touch();
  return {};
}

// ---------------------------------------------------------------- 节假日

Result AppState::ImportHolidays(const fs::path& file, const std::string& country, int year) {
  auto text = ReadTextFile(file);
  if (!text) return std::unexpected(text.error());
  auto hy = ParseHolidayYear(*text, country, year);
  if (!hy) return std::unexpected("格式错误：" + hy.error());
  if (auto r = repo_.SaveHolidayYear(*hy); !r) return r;
  holidays_.Set(std::move(*hy));
  RecomputePlans();
  Touch();
  return {};
}

// ---------------------------------------------------------------- 消息

void AppState::Info(std::string msg) {
  message_ = std::move(msg);
  message_is_error_ = false;
}

void AppState::Error(std::string msg) {
  message_ = std::move(msg);
  message_is_error_ = true;
}

}  // namespace cal
