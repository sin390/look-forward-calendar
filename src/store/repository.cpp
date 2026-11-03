#include "store/repository.h"

#include <algorithm>
#include <cstdio>

#include "core/i18n.h"

namespace cal {

namespace {

std::expected<Json, std::string> ReadJsonFile(const fs::path& p) {
  auto text = ReadTextFile(p);
  if (!text) return std::unexpected(text.error());
  Json j = Json::parse(*text, nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) return std::unexpected(Tr("{file} is not valid JSON", {{"file", PathToUtf8(p.filename())}}));
  return j;
}

std::vector<fs::path> JsonFilesIn(const fs::path& dir) {
  std::vector<fs::path> files;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(dir, ec))
    if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
  std::sort(files.begin(), files.end());
  return files;
}

}  // namespace

Repository::Repository(fs::path root) : root_(std::move(root)) {}

fs::path Repository::MonthPath(int year, unsigned month) const {
  char name[8];
  std::snprintf(name, sizeof name, "%02u.json", month);
  return root_ / "general" / std::to_string(year) / name;
}

std::expected<MonthDays, std::string> Repository::LoadMonth(int year, unsigned month) const {
  const fs::path p = MonthPath(year, month);
  std::error_code ec;
  if (!fs::exists(p, ec)) return MonthDays{};
  auto j = ReadJsonFile(p);
  if (!j) return std::unexpected(j.error());
  auto days = MonthDaysFromJson(*j);
  if (!days) return std::unexpected(Tr("{file}: {error}", {{"file", PathToUtf8(p.filename())}, {"error", days.error()}}));
  return days;
}

std::expected<void, std::string> Repository::SaveMonth(int year, unsigned month,
                                                       const MonthDays& days) const {
  return WriteTextFileAtomic(MonthPath(year, month), ToJson(days).dump(2));
}

std::vector<Task> Repository::LoadTasks(std::vector<std::string>& warnings) const {
  std::vector<Task> tasks;
  for (const fs::path& p : JsonFilesIn(root_ / "task")) {
    auto j = ReadJsonFile(p);
    if (!j) {
      warnings.push_back(j.error());
      continue;
    }
    auto task = TaskFromJson(*j, PathToUtf8(p.stem()));
    if (!task) {
      warnings.push_back(Tr("Task {file}: {error}", {{"file", PathToUtf8(p.filename())}, {"error", task.error()}}));
      continue;
    }
    tasks.push_back(std::move(*task));
  }
  return tasks;
}

std::expected<void, std::string> Repository::SaveTask(const Task& task) const {
  return WriteTextFileAtomic(root_ / "task" / PathFromUtf8(task.file_stem + ".json"),
                             ToJson(task).dump(2));
}

std::expected<void, std::string> Repository::DeleteTask(const Task& task) const {
  const fs::path p = root_ / "task" / PathFromUtf8(task.file_stem + ".json");
  std::error_code ec;
  fs::remove(p, ec);
  if (ec) return std::unexpected(Tr("Cannot delete file: {path}", {{"path", PathToUtf8(p)}}));
  return {};
}

std::vector<FixedSchedule> Repository::LoadFixed(std::vector<std::string>& warnings) const {
  std::vector<FixedSchedule> list;
  for (const fs::path& p : JsonFilesIn(root_ / "fixed")) {
    auto j = ReadJsonFile(p);
    if (!j) {
      warnings.push_back(j.error());
      continue;
    }
    auto f = FixedFromJson(*j, PathToUtf8(p.stem()));
    if (!f) {
      warnings.push_back(Tr("Fixed schedule {file}: {error}", {{"file", PathToUtf8(p.filename())}, {"error", f.error()}}));
      continue;
    }
    list.push_back(std::move(*f));
  }
  return list;
}

std::expected<void, std::string> Repository::SaveFixed(const FixedSchedule& f) const {
  return WriteTextFileAtomic(root_ / "fixed" / PathFromUtf8(f.file_stem + ".json"), ToJson(f).dump(2));
}

std::expected<void, std::string> Repository::DeleteFixed(const FixedSchedule& f) const {
  const fs::path p = root_ / "fixed" / PathFromUtf8(f.file_stem + ".json");
  std::error_code ec;
  fs::remove(p, ec);
  if (ec) return std::unexpected(Tr("Cannot delete file: {path}", {{"path", PathToUtf8(p)}}));
  return {};
}

Preference Repository::LoadPreference(std::vector<std::string>& warnings) const {
  const fs::path p = root_ / "preference.json";
  std::error_code ec;
  if (!fs::exists(p, ec)) return {};
  auto j = ReadJsonFile(p);
  if (!j) {
    warnings.push_back(Tr("{error}, using defaults", {{"error", j.error()}}));
    return {};
  }
  return PreferenceFromJson(*j, warnings);
}

std::expected<void, std::string> Repository::SavePreference(const Preference& p) const {
  return WriteTextFileAtomic(root_ / "preference.json", ToJson(p).dump(2));
}

HolidayCalendar Repository::LoadHolidays(std::vector<std::string>& warnings) const {
  HolidayCalendar cal;
  for (const fs::path& p : JsonFilesIn(root_ / "holidays")) {
    auto text = ReadTextFile(p);
    if (!text) {
      warnings.push_back(text.error());
      continue;
    }
    auto hy = ParseHolidayYear(*text);
    if (!hy) {
      warnings.push_back(Tr("Holidays {file}: {error}", {{"file", PathToUtf8(p.filename())}, {"error", hy.error()}}));
      continue;
    }
    cal.Set(std::move(*hy));
  }
  return cal;
}

std::expected<void, std::string> Repository::SaveHolidayYear(const HolidayYear& hy) const {
  const std::string name = hy.country + "_" + std::to_string(hy.year) + ".json";
  return WriteTextFileAtomic(root_ / "holidays" / name, ToJson(hy).dump(2));
}

}  // namespace cal
