#include "core/holiday.h"

#include <algorithm>

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

constexpr std::string_view kPromptTemplate = R"PROMPT(请列出{COUNTRY_NAME}{YEAR}年全年的法定节假日安排，严格按照下面的 JSON 格式，
把完整的 JSON 放在一个 ```json 代码块中输出，代码块外不要输出其他文字。
如果你能够生成文件，请同时生成一个名为 {COUNTRY}_{YEAR}.json 的文件供下载，内容与代码块中的 JSON 完全相同。

{
  "country": "{COUNTRY}",
  "year": {YEAR},
  "days": [
    { "date": "YYYY-MM-DD", "name": "节日名称", "off": true }
  ]
}

规则：
1. 每个日期单独一条，连续放假的每一天都要列出。
2. 放假日（含周末因节日顺延的补休/振替休日）"off" 为 true。
3. 因调休需要上班的周末（补班日）也要列出，"off" 为 false，name 写"XX调休补班"。
4. name 使用该国语言的通用名称。
5. 只列出 {YEAR} 年内的日期，按日期升序排列。
6. 依据该国政府官方公布的安排；如果官方尚未公布，请在 name 后加"(预测)"。)PROMPT";

}  // namespace

const std::vector<Country>& SupportedCountries() {
  static const std::vector<Country> kCountries = {{"CN", "中国"}, {"JP", "日本"}};
  return kCountries;
}

std::string CountryName(std::string_view code) {
  for (const Country& c : SupportedCountries())
    if (c.code == code) return c.name;
  return std::string(code);
}

std::expected<HolidayYear, std::string> ParseHolidayYear(std::string_view text,
                                                         std::string_view expect_country,
                                                         int expect_year) {
  const Json j = Json::parse(StripCodeFence(text), nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) return std::unexpected("不是合法的 JSON");
  if (!j.is_object()) return std::unexpected("顶层必须是对象 { ... }");

  HolidayYear hy;
  if (!j.contains("country") || !j["country"].is_string())
    return std::unexpected("缺少字符串字段 country");
  if (!j.contains("year") || !j["year"].is_number_integer())
    return std::unexpected("缺少整数字段 year");
  if (!j.contains("days") || !j["days"].is_array())
    return std::unexpected("缺少数组字段 days");
  hy.country = j["country"].get<std::string>();
  hy.year = j["year"].get<int>();

  if (!expect_country.empty() && hy.country != expect_country)
    return std::unexpected("country 为 " + hy.country + "，与所选的 " +
                           std::string(expect_country) + " 不一致");
  if (expect_year != 0 && hy.year != expect_year)
    return std::unexpected("year 为 " + std::to_string(hy.year) + "，与所选的 " +
                           std::to_string(expect_year) + " 不一致");

  for (size_t i = 0; i < j["days"].size(); ++i) {
    const Json& e = j["days"][i];
    const std::string where = "days[" + std::to_string(i) + "]";
    if (!e.is_object()) return std::unexpected(where + " 不是对象");
    if (!e.contains("date") || !e["date"].is_string())
      return std::unexpected(where + " 缺少 date");
    const auto date = ParseDate(e["date"].get<std::string>());
    if (!date) return std::unexpected(where + " 的 date 不是合法的 YYYY-MM-DD");
    if (YearNum(*date) != hy.year) return std::unexpected(where + " 的日期不在 " + std::to_string(hy.year) + " 年内");
    if (!e.contains("name") || !e["name"].is_string() || e["name"].get<std::string>().empty())
      return std::unexpected(where + " 缺少 name");
    if (!e.contains("off") || !e["off"].is_boolean())
      return std::unexpected(where + " 缺少布尔字段 off");
    hy.days.push_back({*date, e["name"].get<std::string>(), e["off"].get<bool>()});
  }

  std::sort(hy.days.begin(), hy.days.end(),
            [](const HolidayDay& a, const HolidayDay& b) { return a.date < b.date; });
  for (size_t i = 1; i < hy.days.size(); ++i)
    if (hy.days[i].date == hy.days[i - 1].date)
      return std::unexpected("日期重复：" + FormatDate(hy.days[i].date));
  return hy;
}

Json ToJson(const HolidayYear& hy) {
  Json days = Json::array();
  for (const HolidayDay& d : hy.days)
    days.push_back({{"date", FormatDate(d.date)}, {"name", d.name}, {"off", d.off}});
  return {{"country", hy.country}, {"year", hy.year}, {"days", std::move(days)}};
}

std::string HolidayPrompt(std::string_view country, int year) {
  std::string s(kPromptTemplate);
  ReplaceAll(s, "{COUNTRY_NAME}", CountryName(country));
  ReplaceAll(s, "{COUNTRY}", country);
  ReplaceAll(s, "{YEAR}", std::to_string(year));
  return s;
}

void HolidayCalendar::Set(HolidayYear hy) {
  auto& days = years_[{hy.country, hy.year}];
  days.clear();
  for (HolidayDay& d : hy.days) days.emplace(d.date, std::move(d));
}

const HolidayDay* HolidayCalendar::Find(std::string_view country, Date d) const {
  const auto y = years_.find({std::string(country), YearNum(d)});
  if (y == years_.end()) return nullptr;
  const auto it = y->second.find(d);
  return it == y->second.end() ? nullptr : &it->second;
}

bool HolidayCalendar::IsOff(std::string_view country, Date d) const {
  const HolidayDay* h = Find(country, d);
  return h && h->off;
}

bool HolidayCalendar::Has(std::string_view country, int year) const {
  return years_.contains({std::string(country), year});
}

std::vector<std::pair<std::string, int>> HolidayCalendar::Loaded() const {
  std::vector<std::pair<std::string, int>> out;
  for (const auto& [key, _] : years_) out.push_back(key);
  return out;
}

}  // namespace cal
