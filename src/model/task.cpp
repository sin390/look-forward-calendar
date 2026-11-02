#include "model/task.h"

#include <algorithm>

namespace cal {

namespace {

constexpr std::array<const char*, 7> kWeekdayNames = {"一", "二", "三", "四", "五", "六", "日"};

std::optional<Date> DateOfTimestamp(const std::string& at) {
  return at.size() >= 10 ? ParseDate(std::string_view(at).substr(0, 10)) : std::nullopt;
}

Json WeekdaysToJson(const std::array<bool, 7>& wd) {
  Json arr = Json::array();
  for (int i = 0; i < 7; ++i)
    if (wd[static_cast<size_t>(i)]) arr.push_back(i + 1);
  return arr;
}

// 字段名与 create 条目、modify 的 fields 共用
Json SpecFieldsDiff(const TaskSpec& before, const TaskSpec& after, bool all) {
  Json f = Json::object();
  if (all || before.name != after.name) f["name"] = after.name;
  if (all || before.total_minutes != after.total_minutes) f["total_minutes"] = after.total_minutes;
  if (all || before.due != after.due) f["due"] = FormatDate(after.due);
  if (all || before.weekdays != after.weekdays) f["weekdays"] = WeekdaysToJson(after.weekdays);
  if (all || before.skip_holiday != after.skip_holiday) f["skip_holiday"] = after.skip_holiday;
  if (all || before.country != after.country) f["country"] = after.country;
  return f;
}

std::optional<std::string> ApplySpecFields(TaskSpec& spec, const Json& f) {
  try {
    if (f.contains("name")) spec.name = f["name"].get<std::string>();
    if (f.contains("total_minutes")) spec.total_minutes = f["total_minutes"].get<int>();
    if (f.contains("due")) {
      const auto d = ParseDate(f["due"].get<std::string>());
      if (!d) return "due 日期非法";
      spec.due = *d;
    }
    if (f.contains("weekdays")) {
      spec.weekdays = {};
      for (const Json& w : f["weekdays"]) {
        const int i = w.get<int>();
        if (i < 1 || i > 7) return "weekdays 取值应为 1..7";
        spec.weekdays[static_cast<size_t>(i - 1)] = true;
      }
    }
    if (f.contains("skip_holiday")) spec.skip_holiday = f["skip_holiday"].get<bool>();
    if (f.contains("country")) spec.country = f["country"].get<std::string>();
  } catch (const nlohmann::json::exception&) {
    return "任务属性字段类型错误";
  }
  return std::nullopt;
}

}  // namespace

int TaskState::DoneTotal() const {
  int sum = 0;
  for (const auto& [_, p] : progress) sum += p.minutes;
  return sum;
}

int TaskState::DoneBefore(Date d) const {
  int sum = 0;
  for (auto it = progress.begin(); it != progress.end() && it->first < d; ++it) sum += it->second.minutes;
  return sum;
}

DayProgress TaskState::ProgressOn(Date d) const {
  const auto it = progress.find(d);
  return it == progress.end() ? DayProgress{} : it->second;
}

Task NewTask(const TaskSpec& spec, std::string id, const std::string& at) {
  Task t;
  t.file_stem = at.substr(0, 4) + at.substr(5, 2) + at.substr(8, 2) + "_" + id;
  t.id = std::move(id);
  t.name = spec.name;
  Json entry = {{"at", at}, {"op", "create"}};
  entry.update(SpecFieldsDiff(spec, spec, /*all=*/true));
  t.log.push_back(std::move(entry));
  return t;
}

bool AppendModify(Task& task, const TaskSpec& before, const TaskSpec& after, const std::string& at) {
  Json fields = SpecFieldsDiff(before, after, /*all=*/false);
  if (fields.empty()) return false;
  task.log.push_back({{"at", at}, {"op", "modify"}, {"fields", std::move(fields)}});
  task.name = after.name;
  return true;
}

void AppendOp(Task& task, std::string_view op, const std::string& at) {
  task.log.push_back({{"at", at}, {"op", op}});
}

void AppendProgress(Task& task, Date date, const DayProgress& p, const std::string& at) {
  Json entry = {{"at", at}, {"op", "progress"}, {"date", FormatDate(date)}, {"minutes", p.minutes}};
  if (!p.note.empty()) entry["note"] = p.note;
  task.log.push_back(std::move(entry));
}

Json ToJson(const Task& task) {
  return {{"id", task.id}, {"name", task.name}, {"log", task.log}};
}

std::expected<Task, std::string> TaskFromJson(const Json& j, std::string file_stem) {
  if (!j.is_object()) return std::unexpected("顶层必须是对象");
  if (!j.contains("id") || !j["id"].is_string()) return std::unexpected("缺少 id");
  if (!j.contains("log") || !j["log"].is_array()) return std::unexpected("缺少 log");
  Task t;
  t.id = j["id"].get<std::string>();
  t.name = j.value("name", "");
  t.file_stem = std::move(file_stem);
  for (const Json& e : j["log"]) t.log.push_back(e);
  return t;
}

std::expected<TaskState, std::string> Replay(const Task& task) {
  if (task.log.empty() || !task.log.front().is_object() || task.log.front().value("op", "") != "create")
    return std::unexpected("日志的第一条必须是 create");

  TaskState s;
  for (size_t i = 0; i < task.log.size(); ++i) {
    const Json& e = task.log[i];
    const std::string where = "log[" + std::to_string(i) + "]";
    if (!e.is_object() || !e.contains("op") || !e["op"].is_string())
      return std::unexpected(where + " 缺少 op");
    const auto at = DateOfTimestamp(e.value("at", ""));
    if (!at) return std::unexpected(where + " 的 at 时间戳非法");
    const std::string op = e["op"].get<std::string>();

    if (op == "create") {
      if (i != 0) return std::unexpected(where + " 重复的 create");
      if (auto err = ApplySpecFields(s.spec, e)) return std::unexpected(where + " " + *err);
      s.created = *at;
    } else if (op == "modify") {
      if (!e.contains("fields") || !e["fields"].is_object()) return std::unexpected(where + " 缺少 fields");
      if (auto err = ApplySpecFields(s.spec, e["fields"])) return std::unexpected(where + " " + *err);
    } else if (op == "progress") {
      const auto d = ParseDate(e.value("date", ""));
      if (!d || !e.contains("minutes") || !e["minutes"].is_number_integer())
        return std::unexpected(where + " progress 字段非法");
      DayProgress p{e["minutes"].get<int>(), e.value("note", "")};
      if (p.minutes > 0 || !p.note.empty()) s.progress[*d] = std::move(p);
      else s.progress.erase(*d);
    } else if (op == "pause") {
      if (s.status == TaskStatus::kActive) s.status = TaskStatus::kPaused;
    } else if (op == "resume") {
      if (s.status == TaskStatus::kPaused) s.status = TaskStatus::kActive;
    } else if (op == "complete" || op == "abort") {
      s.status = op == "complete" ? TaskStatus::kCompleted : TaskStatus::kAborted;
      s.ended = *at;
    } else {
      return std::unexpected(where + " 未知的 op：" + op);
    }
  }
  return s;
}

bool IsWorkday(const TaskSpec& spec, Date d, const HolidayCalendar& holidays) {
  if (!spec.weekdays[static_cast<size_t>(IsoWeekday(d) - 1)]) return false;
  return !(spec.skip_holiday && holidays.IsOff(spec.country, d));
}

TaskPlan ComputePlan(const TaskState& state, Date today, const HolidayCalendar& holidays) {
  TaskPlan plan;
  plan.remaining = state.spec.total_minutes - state.DoneBefore(today);
  if (state.status != TaskStatus::kActive || plan.remaining <= 0) return plan;

  std::vector<Date> workdays;
  for (Date d = std::max(today, state.created); d <= state.spec.due; d = AddDays(d, 1))
    if (IsWorkday(state.spec, d, holidays)) workdays.push_back(d);
  if (workdays.empty()) {
    plan.overdue = true;
    return plan;
  }

  // 依次把剩余量平均分给剩下的工作日，每天向上取整到 10 分钟
  int rest = plan.remaining;
  for (size_t i = 0; i < workdays.size() && rest > 0; ++i) {
    const int n = static_cast<int>(workdays.size() - i);
    const int quota = CeilTo((rest + n - 1) / n, kMinuteStep);
    plan.daily[workdays[i]] = quota;
    rest -= quota;
  }
  return plan;
}

bool ShowsOn(const TaskState& state, Date d, Date today) {
  if (d < state.created) return false;
  if (state.ended && d > *state.ended) return false;
  return d <= state.spec.due || d <= today || state.progress.contains(d);
}

std::optional<std::string> ValidateSpec(const TaskSpec& spec, Date today, const HolidayCalendar& holidays) {
  if (Trim(spec.name).empty()) return "任务名不能为空";
  if (spec.total_minutes <= 0) return "预定时长必须大于 0";
  if (!spec.due.ok()) return "完成日期非法";
  if (spec.due < today) return "完成日期不能早于今天";
  if (std::none_of(spec.weekdays.begin(), spec.weekdays.end(), [](bool b) { return b; }))
    return "至少选择一个工作日";
  if (spec.skip_holiday) {
    for (int y = YearNum(today); y <= YearNum(spec.due); ++y)
      if (!holidays.Has(spec.country, y))
        return "缺少 " + CountryName(spec.country) + " " + std::to_string(y) +
               " 年的节假日数据，已取消。请先按 i 导入。";
  }
  return std::nullopt;
}

std::string WeekdaysText(const std::array<bool, 7>& weekdays) {
  std::string s;
  for (size_t i = 0; i < 7; ++i)
    if (weekdays[i]) s += kWeekdayNames[i];
  return s;
}

std::string StatusText(const TaskState& state, const TaskPlan& plan, Date today) {
  switch (state.status) {
    case TaskStatus::kCompleted: return "已完成";
    case TaskStatus::kAborted: return "已中止";
    case TaskStatus::kPaused: return "暂停";
    case TaskStatus::kActive: break;
  }
  if (plan.overdue) return today > state.spec.due ? "超期" : "无工作日";
  if (plan.remaining <= 0) return "时长已满";
  return "进行中";
}

}  // namespace cal
