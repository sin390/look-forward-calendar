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
#include "model/fixed.h"
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
  // language_dir：语言文件目录（exe 所在目录下的 language/）；为空时只用英文
  explicit AppState(fs::path data_root, fs::path language_dir = {});

  // 从磁盘重新读取全部数据，更新“今天”，重算所有任务安排
  void Reload();
  // 定时调用：跨过零点时自动 Reload
  void Tick();

  Date today() const { return today_; }
  Date selected() const { return selected_; }
  void Select(Date d);

  // 配置（国家、语言）：启动和刷新时从 preference.json 读取，切换后立即保存
  int display_config() const { return pref_.holiday_config; }  // 月历上显示的节假日配置
  const std::string& language() const { return pref_.language; }
  bool show_lunar() const { return pref_.show_lunar; }
  const std::string& slogan() const { return pref_.slogan; }
  void CycleHolidayConfig();
  void ToggleLunar();  // 开关农历显示
  Result SetSlogan(std::string_view slogan);  // 留空恢复默认标语
  // 按 language/ 下的 *.json 文件名顺序切换到下一个语言；语言确实改变时返回 true（界面需要重建）
  bool CycleLanguage();
  std::vector<std::string> AvailableLanguages() const;  // 如 {"CN", "EN"}

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

  // ---- 固定日程 ----
  const std::vector<FixedSchedule>& fixed() const { return fixed_; }
  // 某天的固定日程，按开始时间排序。按月展开并缓存：切换到新的月份时才计算该月
  std::vector<const FixedSchedule*> FixedOn(Date d);
  Result AddFixed(FixedSchedule f);  // 自动分配 id 和文件名
  Result DeleteFixed(const std::string& id);  // 删除固定日程文件（所有日期）

  // ---- 节假日 ----
  Result ImportHolidays(const fs::path& file, int config, int year);

  // ---- 状态栏消息 ----
  void Info(std::string msg);
  void Error(std::string msg);
  // 状态栏消息；为空时状态栏显示标语
  const std::string& message() const { return message_; }
  bool message_is_error() const { return message_is_error_; }
  const std::vector<std::string>& warnings() const { return warnings_; }

 private:
  MonthDays& MonthOf(Date d);
  Result SaveMonthOf(Date d);
  TaskEntry* FindTaskMutable(const std::string& id);
  Result CommitTask(TaskEntry& entry, Task updated);
  void RecomputePlans();
  std::string NewId() const;  // 任务和固定日程共用的短 id，互不重复
  const std::map<Date, std::vector<size_t>>& FixedMonthOf(Date d);
  bool SavePreference();  // 失败时在状态栏显示错误
  // 加载 pref_.language 对应的语言文件，失败时回退为英文并写入 warnings（默认 warnings_）
  void LoadLanguage(std::vector<std::string>* warnings = nullptr);
  void Touch() { ++version_; }

  Repository repo_;
  fs::path language_dir_;
  HolidayCalendar holidays_;
  std::vector<TaskEntry> tasks_;
  std::map<std::pair<int, unsigned>, MonthDays> months_;
  std::set<std::pair<int, unsigned>> broken_months_;  // 读取失败的月份，禁止覆盖写入
  std::vector<FixedSchedule> fixed_;
  // 已展开的月份：日期 → fixed_ 中的下标。固定日程增删后整体清空
  std::map<std::pair<int, unsigned>, std::map<Date, std::vector<size_t>>> fixed_months_;

  Date today_{};
  Date selected_{};
  Preference pref_;
  uint64_t version_ = 0;

  std::string message_;
  bool message_is_error_ = false;
  std::vector<std::string> warnings_;
};

}  // namespace cal
