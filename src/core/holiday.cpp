#include "core/holiday.h"

#include <algorithm>

#include "core/i18n.h"

namespace cal {

namespace {

// 取出 ```json ... ``` 代码块中的内容；代码块前后的说明文字一并忽略
std::string_view StripCodeFence(std::string_view s) {
  s = Trim(s);
  const auto open = s.find("```");
  if (open == std::string_view::npos) return s;
  const auto first_newline = s.find('\n', open);
  if (first_newline == std::string_view::npos) return s;
  s.remove_prefix(first_newline + 1);
  if (const auto close = s.find("```"); close != std::string_view::npos) s = s.substr(0, close);
  return Trim(s);
}

void ReplaceAll(std::string& s, std::string_view from, std::string_view to) {
  for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size())
    s.replace(pos, from.size(), to);
}

constexpr const char* kPromptTemplate = N_(R"PROMPT(List all statutory public holidays of {COUNTRY} for the year {YEAR}, strictly in the JSON format below.
Put the complete JSON in a single ```json code block, and output nothing outside the code block.
If you can create files, also create a downloadable file named config{CONFIG}_{YEAR}.json with exactly the same JSON as the code block.

{
  "country": "{COUNTRY}",
  "year": {YEAR},
  "days": [
    { "date": "YYYY-MM-DD", "name": "Holiday name", "off": true }
  ]
}

Rules:
1. One entry per date; list every day of a multi-day holiday.
2. Days off (including substitute holidays for holidays that fall on a weekend) have "off": true.
3. Also list weekend days that become working days because of holiday adjustments, with "off": false and name "XX make-up workday".
4. Use the common name of each holiday in that country's language.
5. Only list dates within {YEAR}, sorted by date in ascending order.
6. Follow the schedule officially published by the government; if it has not been published yet, append "(forecast)" to the name.)PROMPT");

}  // namespace

bool IsHolidayConfig(int config) { return config >= 1 && config <= kHolidayConfigCount; }

std::string HolidayConfigName(int config) { return Tr("Holiday config {n}", {{"n", std::to_string(config)}}); }

std::string HolidayConfigShortName(int config) { return Tr("Config {n}", {{"n", std::to_string(config)}}); }

std::expected<HolidayYear, std::string> ParseHolidayYear(std::string_view text, int expect_year) {
  const Json j = Json::parse(StripCodeFence(text), nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) return std::unexpected(Tr("not valid JSON"));
  if (!j.is_object()) return std::unexpected(Tr("top level must be an object { ... }"));

  HolidayYear hy;
  if (!j.contains("country") || !j["country"].is_string())
    return std::unexpected(Tr("missing string field country"));
  if (!j.contains("year") || !j["year"].is_number_integer())
    return std::unexpected(Tr("missing integer field year"));
  if (!j.contains("days") || !j["days"].is_array())
    return std::unexpected(Tr("missing array field days"));
  hy.country = j["country"].get<std::string>();
  hy.year = j["year"].get<int>();

  if (expect_year != 0 && hy.year != expect_year)
    return std::unexpected(Tr("year is {year}, which does not match the selected {expected}",
                              {{"year", std::to_string(hy.year)}, {"expected", std::to_string(expect_year)}}));

  for (size_t i = 0; i < j["days"].size(); ++i) {
    const Json& e = j["days"][i];
    const std::string where = "days[" + std::to_string(i) + "]";
    if (!e.is_object()) return std::unexpected(Tr("{where} is not an object", {{"where", where}}));
    if (!e.contains("date") || !e["date"].is_string())
      return std::unexpected(Tr("{where} is missing date", {{"where", where}}));
    const auto date = ParseDate(e["date"].get<std::string>());
    if (!date) return std::unexpected(Tr("{where}: date is not a valid YYYY-MM-DD", {{"where", where}}));
    if (YearNum(*date) != hy.year) return std::unexpected(Tr("{where}: date is not in {year}", {{"where", where}, {"year", std::to_string(hy.year)}}));
    if (!e.contains("name") || !e["name"].is_string() || e["name"].get<std::string>().empty())
      return std::unexpected(Tr("{where} is missing name", {{"where", where}}));
    if (!e.contains("off") || !e["off"].is_boolean())
      return std::unexpected(Tr("{where} is missing boolean field off", {{"where", where}}));
    hy.days.push_back({*date, e["name"].get<std::string>(), e["off"].get<bool>()});
  }

  std::sort(hy.days.begin(), hy.days.end(),
            [](const HolidayDay& a, const HolidayDay& b) { return a.date < b.date; });
  for (size_t i = 1; i < hy.days.size(); ++i)
    if (hy.days[i].date == hy.days[i - 1].date)
      return std::unexpected(Tr("duplicate date: {date}", {{"date", FormatDate(hy.days[i].date)}}));
  return hy;
}

Json ToJson(const HolidayYear& hy) {
  Json days = Json::array();
  for (const HolidayDay& d : hy.days)
    days.push_back({{"date", FormatDate(d.date)}, {"name", d.name}, {"off", d.off}});
  return {{"country", hy.country}, {"year", hy.year}, {"days", std::move(days)}};
}

std::string HolidayPrompt(std::string_view country, int config, int year) {
  std::string s = Tr(kPromptTemplate);
  ReplaceAll(s, "{COUNTRY}", country);
  ReplaceAll(s, "{CONFIG}", std::to_string(config));
  ReplaceAll(s, "{YEAR}", std::to_string(year));
  return s;
}

void HolidayCalendar::Set(HolidayYear hy) {
  Entry& e = years_[{hy.config, hy.year}];
  e.country = std::move(hy.country);
  e.days.clear();
  for (HolidayDay& d : hy.days) e.days.emplace(d.date, std::move(d));
}

const HolidayDay* HolidayCalendar::Find(int config, Date d) const {
  const auto y = years_.find({config, YearNum(d)});
  if (y == years_.end()) return nullptr;
  const auto it = y->second.days.find(d);
  return it == y->second.days.end() ? nullptr : &it->second;
}

bool HolidayCalendar::IsOff(int config, Date d) const {
  const HolidayDay* h = Find(config, d);
  return h && h->off;
}

bool HolidayCalendar::Has(int config, int year) const { return years_.contains({config, year}); }

std::string HolidayCalendar::Country(int config, int year) const {
  const auto it = years_.find({config, year});
  return it == years_.end() ? std::string() : it->second.country;
}

std::string HolidayCalendar::LatestCountry(int config) const {
  std::string country;
  for (const auto& [key, e] : years_)  // 按 (配置, 年) 升序，最后一个即最近一年
    if (key.first == config) country = e.country;
  return country;
}

std::vector<std::pair<int, int>> HolidayCalendar::Loaded() const {
  std::vector<std::pair<int, int>> out;
  for (const auto& [key, _] : years_) out.push_back(key);
  return out;
}

}  // namespace cal
