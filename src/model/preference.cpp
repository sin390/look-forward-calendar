#include "model/preference.h"

#include <algorithm>

#include "core/holiday.h"

namespace cal {

const std::vector<Language>& SupportedLanguages() {
  static const std::vector<Language> kLanguages = {{"zh-CN", "中文"}};
  return kLanguages;
}

std::string LanguageName(std::string_view code) {
  for (const Language& l : SupportedLanguages())
    if (l.code == code) return l.name;
  return std::string(code);
}

Json ToJson(const Preference& p) { return {{"country", p.country}, {"language", p.language}}; }

Preference PreferenceFromJson(const Json& j, std::vector<std::string>& warnings) {
  Preference p;
  if (!j.is_object()) {
    warnings.push_back("preference.json：顶层必须是对象，已使用默认配置");
    return p;
  }
  const std::string country = j.value("country", p.country);
  const auto& countries = SupportedCountries();
  if (std::any_of(countries.begin(), countries.end(), [&](const Country& c) { return c.code == country; }))
    p.country = country;
  else
    warnings.push_back("preference.json：不支持的国家 " + country + "，已改用 " + p.country);

  const std::string language = j.value("language", p.language);
  const auto& languages = SupportedLanguages();
  if (std::any_of(languages.begin(), languages.end(), [&](const Language& l) { return l.code == language; }))
    p.language = language;
  else
    warnings.push_back("preference.json：不支持的语言 " + language + "，已改用" + LanguageName(p.language));
  return p;
}

}  // namespace cal
