#pragma once

#include <optional>
#include <string>

#include "core/date.h"

namespace cal {

struct LunarDate {
  int year = 0;
  int month = 0;  // 1..12
  int day = 0;    // 1..30
  bool leap = false;
};

// 支持 1900-01-31 ~ 2100-12-31，超出范围返回 nullopt
std::optional<LunarDate> ToLunar(Date d);

std::string LunarMonthName(const LunarDate& ld);  // 正月 / 闰六月 / 冬月 / 腊月
std::string LunarDayName(int day);                // 初一 … 三十
std::string LunarFestival(const LunarDate& ld);   // 春节、中秋、除夕等，无则空
std::string SolarTerm(Date d);                    // 二十四节气（2000–2099），无则空
std::string GanzhiYear(int lunar_year);           // 丙午
std::string Zodiac(int lunar_year);               // 马

// 月历格子用的短标签：农历节日 > 节气 > 初一显示月名 > 农历日
std::string LunarShortLabel(Date d);

}  // namespace cal
