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

// 节假日配置：固定的几个槽位（1、2），不与国家绑定。每个配置每年一个文件 holidays/config<N>_<年>.json。
// 国家只是配置的一个属性（文件中的 country），用于提示词，以及侧边栏显示当天节日时的“国家：假日”。
constexpr int kHolidayConfigCount = 2;
bool IsHolidayConfig(int config);                // 1..kHolidayConfigCount
std::string HolidayConfigName(int config);       // "Holiday config 1"（中文：节假日配置1）
std::string HolidayConfigShortName(int config);  // "Config 1"（中文：配置1），用于较窄的位置

struct HolidayDay {
  Date date;
  std::string name;
  bool off = true;  // true=放假日，false=补班日（排程中不使用）
};

struct HolidayYear {
  int config = 1;       // 所属节假日配置，由文件名或导入时的选择决定，不写入文件内容
  std::string country;  // 国家名称，只用于显示
  int year = 0;
  std::vector<HolidayDay> days;
};

// 解析并校验 AI 返回的 JSON。容忍外层 ``` 代码块。expect_year 非 0 时要求与之一致。
// 返回值的 config 为默认值，由调用方设置。
std::expected<HolidayYear, std::string> ParseHolidayYear(std::string_view text, int expect_year = 0);
Json ToJson(const HolidayYear& hy);  // { country, year, days }

// 发给 AI 的提示词：country 为用户输入的国家名称，生成的文件名为 config<N>_<年>.json
std::string HolidayPrompt(std::string_view country, int config, int year);

class HolidayCalendar {
 public:
  void Set(HolidayYear hy);  // 同配置同年份整体替换
  const HolidayDay* Find(int config, Date d) const;
  bool IsOff(int config, Date d) const;
  bool Has(int config, int year) const;
  // 该配置在该年的国家；没有数据时为空
  std::string Country(int config, int year) const;
  // 该配置最近一年数据的国家，用作导入时国家输入框的默认值；没有数据时为空
  std::string LatestCountry(int config) const;
  std::vector<std::pair<int, int>> Loaded() const;  // (配置, 年)

 private:
  struct Entry {
    std::string country;
    std::map<Date, HolidayDay> days;
  };
  std::map<std::pair<int, int>, Entry> years_;
};

}  // namespace cal
