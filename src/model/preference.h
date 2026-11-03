#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/json.h"

namespace cal {

// 用户配置，保存在 data/preference.json，启动时读取，修改后立即写回
//   { "country": "CN", "language": "zh-CN" }

struct Language {
  std::string code;  // "zh-CN"
  std::string name;  // "中文"
};
const std::vector<Language>& SupportedLanguages();
std::string LanguageName(std::string_view code);

struct Preference {
  std::string country = "CN";      // 月历上显示的节假日国家
  std::string language = "zh-CN";  // 界面语言（目前只有中文）
};

Json ToJson(const Preference& p);
// 缺少或不支持的字段使用默认值，并在 warnings 中说明
Preference PreferenceFromJson(const Json& j, std::vector<std::string>& warnings);

}  // namespace cal
