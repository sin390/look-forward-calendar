#include "model/day.h"

#include <algorithm>

#include "core/i18n.h"

namespace cal {

namespace {

bool IsTime(const std::string& s) {
  if (s.size() != 5 || s[2] != ':') return false;
  for (size_t i : {0u, 1u, 3u, 4u})
    if (s[i] < '0' || s[i] > '9') return false;
  const int h = (s[0] - '0') * 10 + (s[1] - '0');
  const int m = (s[3] - '0') * 10 + (s[4] - '0');
  return h < 24 && m < 60;
}

std::string GetString(const Json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

int MinuteOfDay(const std::string& hhmm) {
  return ((hhmm[0] - '0') * 10 + (hhmm[1] - '0')) * 60 + (hhmm[3] - '0') * 10 + (hhmm[4] - '0');
}

}  // namespace

Json ToJson(const MonthDays& month) {
  Json j = Json::object();
  for (const auto& [date, day] : month) {
    if (day.Empty()) continue;
    Json events = Json::array();
    for (const Event& e : day.events) {
      Json ev = {{"title", e.title}, {"start", e.start}, {"end", e.end}, {"note", e.note}};
      if (e.remind) ev["remind"] = *e.remind;
      events.push_back(std::move(ev));
    }
    j[FormatDate(date)] = {{"events", std::move(events)}};
  }
  return j;
}

std::expected<MonthDays, std::string> MonthDaysFromJson(const Json& j) {
  if (!j.is_object()) return std::unexpected(Tr("top level must be an object"));
  MonthDays month;
  for (const auto& [key, value] : j.items()) {
    const auto date = ParseDate(key);
    if (!date) return std::unexpected(Tr("invalid date key: {key}", {{"key", key}}));
    if (!value.is_object()) return std::unexpected(Tr("{key} is not an object", {{"key", key}}));
    Day day;
    if (const auto it = value.find("events"); it != value.end() && it->is_array()) {
      for (const Json& e : *it) {
        if (!e.is_object()) continue;
        Event ev{GetString(e, "title"), GetString(e, "start"), GetString(e, "end"),
                 GetString(e, "note"), std::nullopt};
        if (const auto r = e.find("remind"); r != e.end() && r->is_number_integer())
          ev.remind = r->get<int>();
        day.events.push_back(std::move(ev));
      }
    }
    month[*date] = std::move(day);
  }
  return month;
}

std::optional<std::string> ValidateEvent(const Event& e) {
  if (Trim(e.title).empty()) return Tr("Title cannot be empty");
  if (!e.start.empty() && !IsTime(e.start)) return Tr("Start time must be HH:MM");
  if (!e.end.empty() && !IsTime(e.end)) return Tr("End time must be HH:MM");
  if (!e.end.empty() && e.start.empty()) return Tr("A start time is required when an end time is given");
  if (!e.end.empty() && e.end < e.start) return Tr("End time is earlier than start time");
  if (e.remind && (*e.remind < 0 || *e.remind > kMaxRemindMinutes)) return Tr("Reminder lead time must be between 0 and 30 days");
  return std::nullopt;
}

std::string EventTimeText(const Event& e) {
  if (e.start.empty()) return "";
  return e.end.empty() ? e.start : e.start + "-" + e.end;
}

void SortEvents(std::vector<Event>& events) {
  std::stable_sort(events.begin(), events.end(),
                   [](const Event& a, const Event& b) { return a.start < b.start; });
}

std::optional<RemindWindow> RemindWindowOf(Date d, const Event& e) {
  if (!e.remind) return std::nullopt;
  RemindWindow w;
  if (e.start.empty()) {
    w.start = MinuteStamp(d, 0);
    w.until = MinuteStamp(d, 1440);
  } else {
    w.start = MinuteStamp(d, MinuteOfDay(e.start));
    w.until = e.end.empty() ? w.start : MinuteStamp(d, MinuteOfDay(e.end));
    w.until = std::max(w.until, w.start + 1);  // 至少包含开始的那一分钟
  }
  w.from = w.start - *e.remind;
  return w;
}

}  // namespace cal
