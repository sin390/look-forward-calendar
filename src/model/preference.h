#pragma once

#include <string>
#include <vector>

#include "core/json.h"

namespace cal {

// 用户配置，保存在 data/preference.json，启动时读取，修改后立即写回
//   { "country": "CN", "language": "EN" }

constexpr const char* kDefaultLanguage = "EN";

struct Preference {
  std::string country = "CN";              // 月历上显示的节假日国家
  std::string language = kDefaultLanguage;  // 界面语言：language/ 下语言文件的文件名（不含 .json）
};

Json ToJson(const Preference& p);
// 缺少或不支持的字段使用默认值，并在 warnings 中说明
Preference PreferenceFromJson(const Json& j, std::vector<std::string>& warnings);

}  // namespace cal
