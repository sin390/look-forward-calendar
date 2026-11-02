#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/holiday.h"
#include "model/day.h"
#include "model/task.h"
#include "store/repository.h"

namespace cal {

using Result = std::expected<void, std::string>;

struct TaskEntry {
  Task task;
  TaskState state;
  TaskPlan plan;
};

// 处于提醒期间的瞬间型事件
struct Reminder {
  Date date;
  Event event;
  RemindWindow window;
};

// 应用状态：持有全部数据，所有修改都经由这里（写盘 + 重算 + 递增 version）。
// 不依赖 UI，便于测试。
class AppState {
 public:
  explicit AppState(fs::path data_root);

  // 从磁盘重新读取全部数据，更新“今天”，重算所有任务安排
  void Reload();
  // 定时调用：跨过零点时自动 Reload
  void Tick();

  Date today() const { return today_; }
  Date selected() const { return selected_; }
  void Select(Date d);

  const std::string& display_country() const { return display_country_; }
  void CycleDisplayCountry();

  // 数据或选中日期每次变化都会递增，UI 据此决定是否重建动态控件
  uint64_t version() const { return version_; }

  // ---- 查询 ----
  const Repository& repo() const { return repo_; }
  const HolidayCalendar& holidays() const { return holidays_; }
  const Day* FindDay(Date d);
  const std::vector<TaskEntry>& tasks() const { return tasks_; }
  const TaskEntry* FindTask(const std::string& id) const;
  int PlannedMinutesOn(Date d) const;  // 今天及以后：各任务计划量之和
  int ActualMinutesOn(Date d) const;   // 各任务实际录入量之和
  bool HasOverdueTask() const;
  // now 时刻处于提醒期间的事件，按事件发生时间排序
  std::vector<Reminder> ActiveReminders(LocalMinute now);

  // ---- 瞬间型事件 ----
  Result AddEvent(Date d, Event e);
  Result UpdateEvent(Date d, size_t index, Event e);
  Result DeleteEvent(Date d, size_t index);

  // ---- 长期任务 ----
  Result CreateTask(const TaskSpec& spec);
  Result ModifyTask(const std::string& id, const TaskSpec& spec);
  Result ChangeTaskStatus(const std::string& id, std::string_view op);  // pause/resume/complete/abort
  Result SetProgress(const std::string& id, Date d, const DayProgress& p);
  Result DeleteTask(const std::string& id);  // 删除任务文件（全部信息）

  // ---- 节假日 ----
  Result ImportHolidays(const fs::path& file, const std::string& country, int year);

  // ---- 状态栏消息 ----
  void Info(std::string msg);
  void Error(std::string msg);
  const std::string& message() const { return message_; }
  bool message_is_error() const { return message_is_error_; }
  const std::vector<std::string>& warnings() const { return warnings_; }

 private:
  MonthDays& MonthOf(Date d);
  Result SaveMonthOf(Date d);
  TaskEntry* FindTaskMutable(const std::string& id);
  Result CommitTask(TaskEntry& entry, Task updated);
  void RecomputePlans();
  std::string NewTaskId() const;
  void Touch() { ++version_; }

  Repository repo_;
  HolidayCalendar holidays_;
  std::vector<TaskEntry> tasks_;
  std::map<std::pair<int, unsigned>, MonthDays> months_;
  std::set<std::pair<int, unsigned>> broken_months_;  // 读取失败的月份，禁止覆盖写入

  Date today_{};
  Date selected_{};
  std::string display_country_ = "CN";
  uint64_t version_ = 0;

  std::string message_;
  bool message_is_error_ = false;
  std::vector<std::string> warnings_;
};

}  // namespace cal
