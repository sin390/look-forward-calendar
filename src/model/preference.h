#pragma once

#include <string>
#include <vector>

#include "core/json.h"

namespace cal {

// 用户配置，保存在 data/preference.json，启动时读取，修改后立即写回
//   { "holiday_config": 1, "language": "EN", "lunar": true, "slogan": "Look Forward!" }

constexpr const char* kDefaultLanguage = "EN";
constexpr const char* kDefaultSlogan = "Look Forward!";

struct Preference {
  int holiday_config = 1;                   // 月历上显示的节假日配置（1..kHolidayConfigCount）
  std::string language = kDefaultLanguage;  // 界面语言：language/ 下语言文件的文件名（不含 .json）
  bool show_lunar = true;                   // 是否显示农历日期、农历节日和节气
  std::string slogan = kDefaultSlogan;      // 标语：没有其他消息时显示在状态栏，用户自己写的文字，不翻译
};

Json ToJson(const Preference& p);
// 缺少或不支持的字段使用默认值，并在 warnings 中说明
Preference PreferenceFromJson(const Json& j, std::vector<std::string>& warnings);

}  // namespace cal
