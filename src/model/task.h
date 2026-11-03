#pragma once

#include <array>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/date.h"
#include "core/holiday.h"
#include "core/json.h"

namespace cal {

// 长期任务。文件中只保存操作日志，状态与每日计划都由日志重放计算得到。
//
// 日志条目（都带 "at": 本地时间戳）：
//   create    任务属性（见 TaskSpec）
//   modify    "fields": 变化的属性
//   progress  "date", "minutes", "note"（可选）：某天实际进行情况，后写的覆盖先写的
//   pause / resume / complete / abort

enum class TaskStatus { kActive, kPaused, kCompleted, kAborted };

constexpr int kMinuteStep = 10;  // 计划量的最小单位

struct TaskSpec {
  std::string name;
  int total_minutes = 0;           // 预定完成时长
  Date due{};                      // 预定完成日期
  std::array<bool, 7> weekdays{};  // [0]=周一 … [6]=周日
  bool skip_holiday = true;
  std::string country = "CN";  // 跳过节假日时依据的国家

  bool operator==(const TaskSpec&) const = default;
};

struct Task {
  std::string id;         // 随机短 id，如 "a3f9"
  std::string name;       // 冗余保存当前名称，便于直接阅读文件
  std::string file_stem;  // 文件名（不含扩展名），如 "20261102_a3f9"
  std::vector<Json> log;
};

// 某天的实际进行情况
struct DayProgress {
  int minutes = 0;
  std::string note;  // 进行内容

  bool operator==(const DayProgress&) const = default;
};

struct TaskState {
  TaskSpec spec;
  Date created{};
  TaskStatus status = TaskStatus::kActive;
  std::optional<Date> ended;             // 完成或中止的日期
  std::map<Date, DayProgress> progress;  // 每天的实际进行情况

  int DoneTotal() const;
  int DoneBefore(Date d) const;  // d 之前（不含 d）的实际工作量
  DayProgress ProgressOn(Date d) const;
};

struct TaskPlan {
  std::map<Date, int> daily;  // 今天起每个工作日的计划量（分钟）
  int remaining = 0;          // 总时长 - 今天之前的实际工作量
  bool overdue = false;       // 超期或已无可用工作日但仍有剩余：视为暂停，标红
};

// ---- 日志写入 ----
Task NewTask(const TaskSpec& spec, std::string id, const std::string& at);
bool AppendModify(Task& task, const TaskSpec& before, const TaskSpec& after, const std::string& at);
void AppendOp(Task& task, std::string_view op, const std::string& at);
void AppendProgress(Task& task, Date date, const DayProgress& p, const std::string& at);

// ---- 序列化 ----
Json ToJson(const Task& task);
std::expected<Task, std::string> TaskFromJson(const Json& j, std::string file_stem);

// ---- 计算 ----
std::expected<TaskState, std::string> Replay(const Task& task);
bool IsWorkday(const TaskSpec& spec, Date d, const HolidayCalendar& holidays);
TaskPlan ComputePlan(const TaskState& state, Date today, const HolidayCalendar& holidays);
// 某天的侧边栏是否显示该任务
bool ShowsOn(const TaskState& state, Date d, Date today);

// 创建/修改时的校验，返回错误信息
std::optional<std::string> ValidateSpec(const TaskSpec& spec, Date today,
                                        const HolidayCalendar& holidays);

// 日志条目 → 可读文字，如 "修改：截止日期 → 2027-01-10"
std::string DescribeLogEntry(const Json& entry);

std::string WeekdaysText(const std::array<bool, 7>& weekdays);  // "Mo Tu We"（中文：一二三）
std::string StatusText(const TaskState& state, const TaskPlan& plan, Date today);

}  // namespace cal
