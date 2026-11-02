#pragma once

#include <expected>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/date.h"
#include "core/json.h"

namespace cal {

struct Country {
  std::string code;  // "CN"
  std::string name;  // "中国"
};
const std::vector<Country>& SupportedCountries();
std::string CountryName(std::string_view code);

struct HolidayDay {
  Date date;
  std::string name;
  bool off = true;  // true=放假日，false=补班日（排程中不使用）
};

struct HolidayYear {
  std::string country;
  int year = 0;
  std::vector<HolidayDay> days;
};

// 解析并校验 AI 返回的 JSON。容忍外层 ``` 代码块。
// expect_country / expect_year 非空时要求与之一致。
std::expected<HolidayYear, std::string> ParseHolidayYear(std::string_view text,
                                                         std::string_view expect_country = {},
                                                         int expect_year = 0);
Json ToJson(const HolidayYear& hy);

// 发给 AI 的提示词
std::string HolidayPrompt(std::string_view country, int year);

class HolidayCalendar {
 public:
  void Set(HolidayYear hy);  // 同国家同年份整体替换
  const HolidayDay* Find(std::string_view country, Date d) const;
  bool IsOff(std::string_view country, Date d) const;
  bool Has(std::string_view country, int year) const;
  std::vector<std::pair<std::string, int>> Loaded() const;

 private:
  std::map<std::pair<std::string, int>, std::map<Date, HolidayDay>> years_;
};

}  // namespace cal
