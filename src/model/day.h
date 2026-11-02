#pragma once

#include <expected>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/date.h"
#include "core/json.h"

namespace cal {

constexpr int kMaxRemindMinutes = 30 * 1440;  // 提醒提前量上限：30 天

// 瞬间型事件：当天有限时间内的事件
struct Event {
  std::string title;
  std::string start;  // "HH:MM"，可空
  std::string end;    // "HH:MM"，可空
  std::string note;
  std::optional<int> remind;  // 提前多少分钟提醒；空表示不提醒
};

// 每天的属性。之后新增字段（备注、标签等）放在这里。
struct Day {
  std::vector<Event> events;

  bool Empty() const { return events.empty(); }
};

// 一个月的数据，只包含非空的日期
using MonthDays = std::map<Date, Day>;

Json ToJson(const MonthDays& month);
std::expected<MonthDays, std::string> MonthDaysFromJson(const Json& j);

std::optional<std::string> ValidateEvent(const Event& e);  // 返回错误信息
std::string EventTimeText(const Event& e);                 // "14:00-15:30" / "14:00" / ""
void SortEvents(std::vector<Event>& events);               // 按开始时间排序，无时间的在前

// 提醒期间 [from, until)，以 MinuteStamp 表示：
//   from  = 开始时间 - 提前量（全天事件以 0:00 为开始时间）
//   until = 结束时间；无结束时间则到开始的那一分钟结束；全天事件取当天结束
struct RemindWindow {
  long long start = 0;  // 事件开始时刻，用于排序
  long long from = 0;
  long long until = 0;
};
std::optional<RemindWindow> RemindWindowOf(Date d, const Event& e);

}  // namespace cal
