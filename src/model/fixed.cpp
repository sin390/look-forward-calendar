#include "model/fixed.h"

#include <utility>

#include "core/i18n.h"

#include "model/day.h"
#include "model/task.h"

namespace cal {

namespace {

constexpr std::array<std::pair<Repeat, const char*>, 4> kRepeatKeys = {{
    {Repeat::kDaily, "daily"},
    {Repeat::kWeekly, "weekly"},
    {Repeat::kBiweekly, "biweekly"},
    {Repeat::kMonthly, "monthly"},
}};

bool UsesWeekdays(Repeat r) { return r == Repeat::kWeekly || r == Repeat::kBiweekly; }

Date MondayOf(Date d) { return AddDays(d, 1 - IsoWeekday(d)); }

}  // namespace

bool OccursOn(const FixedSchedule& f, Date d) {
  if (d < f.from || (f.until && d > *f.until)) return false;
  switch (f.repeat) {
    case Repeat::kDaily: return true;
    case Repeat::kWeekly: return f.weekdays[static_cast<size_t>(IsoWeekday(d) - 1)];
    case Repeat::kBiweekly:
      return f.weekdays[static_cast<size_t>(IsoWeekday(d) - 1)] &&
             DaysBetween(MondayOf(f.from), MondayOf(d)) / 7 % 2 == 0;
    case Repeat::kMonthly: return DayNum(d) == DayNum(f.from);  // 没有这一天的月份跳过
  }
  return false;
}

Json ToJson(const FixedSchedule& f) {
  Json weekdays = Json::array();
  for (size_t i = 0; i < 7; ++i)
    if (f.weekdays[i]) weekdays.push_back(static_cast<int>(i) + 1);
  std::string repeat;
  for (const auto& [r, key] : kRepeatKeys)
    if (r == f.repeat) repeat = key;
  return {{"id", f.id},
          {"title", f.title},
          {"start", f.start},
          {"end", f.end},
          {"note", f.note},
          {"repeat", repeat},
          {"weekdays", std::move(weekdays)},
          {"from", FormatDate(f.from)},
          {"until", f.until ? FormatDate(*f.until) : ""}};
}

std::expected<FixedSchedule, std::string> FixedFromJson(const Json& j, std::string file_stem) {
  if (!j.is_object()) return std::unexpected(Tr("top level must be an object"));
  if (!j.contains("id") || !j["id"].is_string()) return std::unexpected(Tr("missing id"));
  FixedSchedule f;
  f.id = j["id"].get<std::string>();
  f.file_stem = std::move(file_stem);
  f.title = j.value("title", "");
  f.start = j.value("start", "");
  f.end = j.value("end", "");
  f.note = j.value("note", "");

  const std::string repeat = j.value("repeat", "weekly");
  bool known = false;
  for (const auto& [r, key] : kRepeatKeys) {
    if (repeat == key) {
      f.repeat = r;
      known = true;
    }
  }
  if (!known) return std::unexpected(Tr("unknown repeat: {repeat}", {{"repeat", repeat}}));

  if (const auto it = j.find("weekdays"); it != j.end() && it->is_array()) {
    for (const Json& w : *it)
      if (w.is_number_integer() && w.get<int>() >= 1 && w.get<int>() <= 7)
        f.weekdays[static_cast<size_t>(w.get<int>() - 1)] = true;
  }
  const auto from = ParseDate(j.value("from", ""));
  if (!from) return std::unexpected(Tr("from is not a valid YYYY-MM-DD"));
  f.from = *from;
  if (const std::string until = j.value("until", ""); !until.empty()) {
    f.until = ParseDate(until);
    if (!f.until) return std::unexpected(Tr("until is not a valid YYYY-MM-DD"));
  }
  if (auto err = ValidateFixed(f)) return std::unexpected(*err);
  return f;
}

std::optional<std::string> ValidateFixed(const FixedSchedule& f) {
  if (Trim(f.title).empty()) return Tr("Work content cannot be empty");
  if (f.start.empty() || f.end.empty()) return Tr("Please enter a start time and an end time");
  // 时间格式与先后的检查和瞬间型事件相同
  if (auto err = ValidateEvent({f.title, f.start, f.end, f.note, std::nullopt})) return err;
  if (UsesWeekdays(f.repeat) && f.weekdays == std::array<bool, 7>{}) return Tr("Please select at least one day of the week");
  if (f.until && *f.until < f.from) return Tr("End date is earlier than start date");
  return std::nullopt;
}

std::string RepeatName(Repeat r) {
  switch (r) {
    case Repeat::kDaily: return Tr("Daily");
    case Repeat::kWeekly: return Tr("Weekly");
    case Repeat::kBiweekly: return Tr("Biweekly");
    case Repeat::kMonthly: return Tr("Monthly");
  }
  return "";
}

std::string RepeatText(const FixedSchedule& f) {
  std::string s = RepeatName(f.repeat);
  if (UsesWeekdays(f.repeat)) s += Tr(" ") + WeekdaysText(f.weekdays);
  if (f.repeat == Repeat::kMonthly) s += Tr(" on day {day}", {{"day", std::to_string(DayNum(f.from))}});
  s += " · " + (f.until ? Tr("{from} to {until}", {{"from", FormatDate(f.from)}, {"until", FormatDate(*f.until)}})
                        : Tr("from {from}", {{"from", FormatDate(f.from)}}));
  return s;
}

}  // namespace cal
