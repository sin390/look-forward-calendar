#include "model/preference.h"

#include "core/holiday.h"
#include "core/i18n.h"

namespace cal {

Json ToJson(const Preference& p) {
  return {{"holiday_config", p.holiday_config}, {"language", p.language}, {"lunar", p.show_lunar}, {"slogan", p.slogan}};
}

Preference PreferenceFromJson(const Json& j, std::vector<std::string>& warnings) {
  Preference p;
  if (!j.is_object()) {
    warnings.push_back(Tr("preference.json: top level must be an object, using defaults"));
    return p;
  }
  if (const auto it = j.find("holiday_config"); it != j.end()) {
    if (it->is_number_integer() && IsHolidayConfig(it->get<int>()))
      p.holiday_config = it->get<int>();
    else
      warnings.push_back(Tr("preference.json: invalid holiday_config {value}, using {default}",
                            {{"value", it->dump()}, {"default", std::to_string(p.holiday_config)}}));
  } else if (j.value("country", "") == "JP") {
    p.holiday_config = 2;  // v1.5.0 之前保存的是国家：CN → 配置1，JP → 配置2
  }

  // 语言文件是否存在在加载时检查；v1.3.0 写入的 "zh-CN" 对应 CN.json
  p.language = j.value("language", p.language);
  if (p.language == "zh-CN") p.language = "CN";
  if (const auto it = j.find("lunar"); it != j.end() && it->is_boolean()) p.show_lunar = it->get<bool>();
  if (const auto it = j.find("slogan"); it != j.end() && it->is_string() && !Trim(it->get<std::string>()).empty())
    p.slogan = it->get<std::string>();
  return p;
}

}  // namespace cal
