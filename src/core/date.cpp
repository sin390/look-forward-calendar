#include "core/date.h"

#include "core/i18n.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace cal {

namespace chr = std::chrono;

namespace {

std::tm LocalNow() {
  const std::time_t t = std::time(nullptr);
  std::tm lt{};
#ifdef _WIN32
  localtime_s(&lt, &t);
#else
  localtime_r(&t, &lt);
#endif
  return lt;
}

template <typename T>
bool ParseNumber(std::string_view s, T& out) {
  if (s.empty()) return false;
  auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
  return ec == std::errc{} && p == s.data() + s.size();
}

}  // namespace

Date Today() {
  const std::tm lt = LocalNow();
  return chr::year{lt.tm_year + 1900} / chr::month{unsigned(lt.tm_mon + 1)} /
         chr::day{unsigned(lt.tm_mday)};
}

std::string NowTimestamp() {
  const std::tm lt = LocalNow();
  char buf[32];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d", lt.tm_year + 1900,
                lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  return buf;
}

LocalMinute NowLocal() {
  const std::tm lt = LocalNow();
  return {chr::year{lt.tm_year + 1900} / chr::month{unsigned(lt.tm_mon + 1)} /
              chr::day{unsigned(lt.tm_mday)},
          lt.tm_hour * 60 + lt.tm_min};
}

long long MinuteStamp(Date d, int minute_of_day) {
  return static_cast<long long>(chr::sys_days{d}.time_since_epoch().count()) * 1440 + minute_of_day;
}

std::string FormatDate(Date d) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", YearNum(d), MonthNum(d), DayNum(d));
  return buf;
}

std::optional<Date> ParseDate(std::string_view s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
  int y = 0;
  unsigned m = 0, d = 0;
  if (!ParseNumber(s.substr(0, 4), y) || !ParseNumber(s.substr(5, 2), m) ||
      !ParseNumber(s.substr(8, 2), d))
    return std::nullopt;
  const Date date = chr::year{y} / chr::month{m} / chr::day{d};
  if (!date.ok()) return std::nullopt;
  return date;
}

Date AddDays(Date d, int n) { return Date{chr::sys_days{d} + chr::days{n}}; }

Date AddMonths(Date d, int n) {
  const chr::year_month ym = d.year() / d.month() + chr::months{n};
  const unsigned last = DaysInMonth(ym);
  return ym / chr::day{std::min(DayNum(d), last)};
}

int DaysBetween(Date from, Date to) {
  return static_cast<int>((chr::sys_days{to} - chr::sys_days{from}).count());
}

int IsoWeekday(Date d) {
  return static_cast<int>(chr::weekday{chr::sys_days{d}}.iso_encoding());
}

unsigned DaysInMonth(chr::year_month ym) {
  return static_cast<unsigned>(chr::year_month_day_last{ym / chr::last}.day());
}

std::string FormatMinutes(int minutes) {
  const char* sign = minutes < 0 ? "-" : "";
  minutes = std::abs(minutes);
  char buf[24];
  std::snprintf(buf, sizeof buf, "%s%d:%02d", sign, minutes / 60, minutes % 60);
  return buf;
}

std::optional<int> ParseDuration(std::string_view s) {
  s = Trim(s);
  if (s.empty()) return std::nullopt;
  if (s.back() == 'm' || s.back() == 'M') {
    int m = 0;
    if (!ParseNumber(Trim(s.substr(0, s.size() - 1)), m) || m < 0) return std::nullopt;
    return m;
  }
  if (auto colon = s.find(':'); colon != std::string_view::npos) {
    int h = 0, m = 0;
    if (!ParseNumber(s.substr(0, colon), h) || !ParseNumber(s.substr(colon + 1), m) ||
        h < 0 || m < 0 || m >= 60)
      return std::nullopt;
    return h * 60 + m;
  }
  double hours = 0;
  if (!ParseNumber(s, hours) || hours < 0) return std::nullopt;
  return static_cast<int>(std::lround(hours * 60));
}

int CeilTo(int value, int step) { return (value + step - 1) / step * step; }

std::optional<int> ParseLeadTime(std::string_view s) {
  s = Trim(s);
  if (!s.empty() && (s.back() == 'd' || s.back() == 'D')) {
    int days = 0;
    if (!ParseNumber(Trim(s.substr(0, s.size() - 1)), days) || days < 0) return std::nullopt;
    return days * 1440;
  }
  return ParseDuration(s);
}

std::string FormatLead(int minutes) {
  if (minutes == 0) return Tr("on time");
  if (minutes % 1440 == 0) return Tr("{n} d", {{"n", std::to_string(minutes / 1440)}});
  std::string s;
  if (minutes >= 60) s += Tr("{n} h", {{"n", std::to_string(minutes / 60)}});
  if (minutes % 60) s += (s.empty() ? "" : Tr(" ")) + Tr("{n} min", {{"n", std::to_string(minutes % 60)}});
  return s;
}

std::string_view Trim(std::string_view s) {
  const auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
  while (!s.empty() && is_space(s.back())) s.remove_suffix(1);
  return s;
}

}  // namespace cal
