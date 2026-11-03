#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace cal {

using Date = std::chrono::year_month_day;

Date Today();                // 本地时区的今天
std::string NowTimestamp();  // 本地时间 "YYYY-MM-DDTHH:MM:SS"

std::string FormatDate(Date d);  // "YYYY-MM-DD"
std::optional<Date> ParseDate(std::string_view s);

Date AddDays(Date d, int n);
Date AddMonths(Date d, int n);  // 日超出目标月天数时取月末
int DaysBetween(Date from, Date to);  // to - from
int IsoWeekday(Date d);               // 周一=1 … 周日=7
// 界面上星期的显示顺序（日 一 二 … 六），元素为 IsoWeekday 的值。数据中的星期仍以周一为 1
inline constexpr std::array<int, 7> kDisplayWeekOrder = {7, 1, 2, 3, 4, 5, 6};
unsigned DaysInMonth(std::chrono::year_month ym);

inline int YearNum(Date d) { return static_cast<int>(d.year()); }
inline unsigned MonthNum(Date d) { return static_cast<unsigned>(d.month()); }
inline unsigned DayNum(Date d) { return static_cast<unsigned>(d.day()); }

// 本地时间，精确到分钟
struct LocalMinute {
  Date date;
  int minute = 0;  // 当天第几分钟 0..1439
};
LocalMinute NowLocal();
// 本地日期 + 当天分钟 → 连续的分钟计数（不含时区，只用于比较先后）
long long MinuteStamp(Date d, int minute_of_day);

// 时长统一以分钟为单位
std::string FormatMinutes(int minutes);  // 150 -> "2:30"
// 接受 "1:30"、"1.5"（小时）、"2"（小时）、"90m"（分钟）
std::optional<int> ParseDuration(std::string_view s);
int CeilTo(int value, int step);

// 提醒提前量：在 ParseDuration 基础上接受 "1d"（天）
std::optional<int> ParseLeadTime(std::string_view s);
std::string FormatLead(int minutes);  // "1天" / "2小时" / "1小时30分钟" / "30分钟"

std::string_view Trim(std::string_view s);

}  // namespace cal
