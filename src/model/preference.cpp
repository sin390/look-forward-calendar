#include "model/preference.h"

#include <algorithm>

#include "core/holiday.h"
#include "core/i18n.h"

namespace cal {

Json ToJson(const Preference& p) { return {{"country", p.country}, {"language", p.language}}; }

Preference PreferenceFromJson(const Json& j, std::vector<std::string>& warnings) {
  Preference p;
  if (!j.is_object()) {
    warnings.push_back(Tr("preference.json: top level must be an object, using defaults"));
    return p;
  }
  const std::string country = j.value("country", p.country);
  const auto& countries = SupportedCountries();
  if (std::any_of(countries.begin(), countries.end(), [&](const Country& c) { return c.code == country; }))
    p.country = country;
  else
    warnings.push_back(Tr("preference.json: unsupported country {country}, using {default}",
                          {{"country", country}, {"default", p.country}}));

  // 语言文件是否存在在加载时检查；v1.3.0 写入的 "zh-CN" 对应 CN.json
  p.language = j.value("language", p.language);
  if (p.language == "zh-CN") p.language = "CN";
  return p;
}

}  // namespace cal
