#include "core/lunar.h"

#include <array>
#include <cmath>

namespace cal {

namespace {

// 1900–2100 农历数据。每年一个整数：
//   bit 0-3   闰月月份（0 表示无闰月）
//   bit 4-15  正月…腊月的大小（bit 15 为正月，1=大月 30 天，0=小月 29 天）
//   bit 16    闰月的大小
constexpr std::array<unsigned, 201> kLunarInfo = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,  // 1900
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,  // 1910
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,  // 1920
    0x06566, 0x0d4a0, 0x0ea50, 0x16a95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,  // 1930
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,  // 1940
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,  // 1950
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,  // 1960
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,  // 1970
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,  // 1980
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x05ac0, 0x0ab60, 0x096d5, 0x092e0,  // 1990
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,  // 2000
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,  // 2010
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,  // 2020
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,  // 2030
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,  // 2040
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0,  // 2050
    0x092e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4,  // 2060
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0,  // 2070
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160,  // 2080
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252,  // 2090
    0x0d520,                                                                                    // 2100
};

constexpr int kFirstYear = 1900;
constexpr int kLastYear = 2100;

unsigned Info(int y) { return kLunarInfo[static_cast<size_t>(y - kFirstYear)]; }
int LeapMonth(int y) { return static_cast<int>(Info(y) & 0xf); }
int LeapDays(int y) { return LeapMonth(y) ? ((Info(y) & 0x10000) ? 30 : 29) : 0; }
int MonthDays(int y, int m) { return (Info(y) & (0x10000u >> m)) ? 30 : 29; }

int YearDays(int y) {
  int sum = 0;
  for (int m = 1; m <= 12; ++m) sum += MonthDays(y, m);
  return sum + LeapDays(y);
}

// 农历 1900 年正月初一
const Date kBase = std::chrono::year{1900} / std::chrono::January / std::chrono::day{31};

constexpr std::array<const char*, 24> kTermNames = {
    "小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明", "谷雨", "立夏", "小满", "芒种", "夏至",
    "小暑", "大暑", "立秋", "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至",
};

// 21 世纪节气通式的 C 值（寿星公式：floor(Y*0.2422 + C) - floor(Y/4)）
constexpr std::array<double, 24> kTermC21 = {
    5.4055, 20.12, 3.87,  18.73, 5.63,  20.646, 4.81,  20.1,  5.52,  21.04,  5.678, 21.37,
    7.108,  22.83, 7.5,   23.13, 7.646, 23.042, 8.318, 23.438, 7.438, 22.36, 7.18,  21.94,
};

// 通式在 21 世纪的已知例外：{年份, 节气序号, 修正天数}
struct TermFix {
  int year, term, delta;
};
constexpr std::array<TermFix, 10> kTermFixes = {{
    {2019, 0, -1}, {2082, 1, +1}, {2026, 3, -1}, {2084, 5, +1}, {2008, 9, +1},
    {2016, 12, +1}, {2002, 14, +1}, {2089, 19, +1}, {2089, 20, +1}, {2021, 23, -1},
}};

int SolarTermDay(int year, int term) {
  const int y = year % 100;
  // 小寒、大寒、立春、雨水在公历年初，闰年修正按上一年计
  const int leap_base = term < 4 ? (y - 1) / 4 : y / 4;
  int day = static_cast<int>(std::floor(y * 0.2422 + kTermC21[static_cast<size_t>(term)])) - leap_base;
  for (const TermFix& f : kTermFixes)
    if (f.year == year && f.term == term) day += f.delta;
  return day;
}

}  // namespace

std::optional<LunarDate> ToLunar(Date d) {
  int offset = DaysBetween(kBase, d);
  if (offset < 0) return std::nullopt;
  int y = kFirstYear;
  while (y <= kLastYear && offset >= YearDays(y)) {
    offset -= YearDays(y);
    ++y;
  }
  if (y > kLastYear) return std::nullopt;

  const int leap = LeapMonth(y);
  bool is_leap = false;
  int m = 1;
  while (true) {
    const int md = is_leap ? LeapDays(y) : MonthDays(y, m);
    if (offset < md) break;
    offset -= md;
    if (!is_leap && m == leap) {
      is_leap = true;
    } else {
      is_leap = false;
      ++m;
    }
  }
  return LunarDate{y, m, offset + 1, is_leap};
}

std::string LunarMonthName(const LunarDate& ld) {
  static constexpr std::array<const char*, 12> kNames = {
      "正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "冬月", "腊月"};
  return std::string(ld.leap ? "闰" : "") + kNames[static_cast<size_t>(ld.month - 1)];
}

std::string LunarDayName(int day) {
  static constexpr std::array<const char*, 10> kDigits = {
      "一", "二", "三", "四", "五", "六", "七", "八", "九", "十"};
  if (day == 10) return "初十";
  if (day == 20) return "二十";
  if (day == 30) return "三十";
  static constexpr std::array<const char*, 3> kPrefix = {"初", "十", "廿"};
  return std::string(kPrefix[static_cast<size_t>((day - 1) / 10)]) +
         kDigits[static_cast<size_t>((day - 1) % 10)];
}

std::string LunarFestival(const LunarDate& ld) {
  if (ld.leap) return "";
  const int md = ld.month * 100 + ld.day;
  switch (md) {
    case 101: return "春节";
    case 115: return "元宵";
    case 505: return "端午";
    case 707: return "七夕";
    case 715: return "中元";
    case 815: return "中秋";
    case 909: return "重阳";
    case 1208: return "腊八";
  }
  if (ld.month == 12 && ld.day == MonthDays(ld.year, 12) && LeapMonth(ld.year) != 12)
    return "除夕";
  return "";
}

std::string SolarTerm(Date d) {
  const int year = YearNum(d);
  if (year < 2000 || year > 2099) return "";
  // 每月两个节气：序号 2*(月-1) 与 2*(月-1)+1
  const int first = static_cast<int>(MonthNum(d) - 1) * 2;
  for (int term = first; term <= first + 1; ++term)
    if (SolarTermDay(year, term) == static_cast<int>(DayNum(d)))
      return kTermNames[static_cast<size_t>(term)];
  return "";
}

std::string GanzhiYear(int lunar_year) {
  static constexpr std::array<const char*, 10> kGan = {
      "甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
  static constexpr std::array<const char*, 12> kZhi = {
      "子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
  const int n = lunar_year - 4;
  return std::string(kGan[static_cast<size_t>((n % 10 + 10) % 10)]) +
         kZhi[static_cast<size_t>((n % 12 + 12) % 12)];
}

std::string Zodiac(int lunar_year) {
  static constexpr std::array<const char*, 12> kAnimals = {
      "鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"};
  const int n = lunar_year - 4;
  return kAnimals[static_cast<size_t>((n % 12 + 12) % 12)];
}

std::string LunarShortLabel(Date d) {
  const auto ld = ToLunar(d);
  if (!ld) return "";
  if (auto f = LunarFestival(*ld); !f.empty()) return f;
  if (auto t = SolarTerm(d); !t.empty()) return t;
  if (ld->day == 1) return LunarMonthName(*ld);
  return LunarDayName(ld->day);
}

}  // namespace cal
