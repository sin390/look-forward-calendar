#include "core/i18n.h"

#include "core/json.h"

namespace cal {

namespace {

Translations& Current() {
  static Translations t;
  return t;
}

}  // namespace

std::expected<Translations, std::string> ParseTranslations(std::string_view json_text) {
  const Json j = Json::parse(json_text, nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) return std::unexpected(Tr("not valid JSON"));
  if (!j.is_object()) return std::unexpected(Tr("top level must be an object"));
  Translations t;
  for (const auto& [key, value] : j.items()) {
    if (!value.is_string()) return std::unexpected(Tr("value of \"{key}\" is not a string", {{"key", key}}));
    t.emplace(key, value.get<std::string>());
  }
  return t;
}

void SetTranslations(Translations t) { Current() = std::move(t); }

std::string Tr(std::string_view key) {
  const Translations& t = Current();
  const auto it = t.find(key);
  return it == t.end() ? std::string(key) : it->second;
}

std::string Tr(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> args) {
  std::string s = Tr(key);
  for (const auto& [name, value] : args) {
    const std::string placeholder = "{" + std::string(name) + "}";
    for (size_t pos = 0; (pos = s.find(placeholder, pos)) != std::string::npos; pos += value.size())
      s.replace(pos, placeholder.size(), value);
  }
  return s;
}

}  // namespace cal
