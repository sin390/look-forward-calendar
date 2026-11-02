#include "store/repository.h"

#include <algorithm>
#include <cstdio>

namespace cal {

namespace {

std::expected<Json, std::string> ReadJsonFile(const fs::path& p) {
  auto text = ReadTextFile(p);
  if (!text) return std::unexpected(text.error());
  Json j = Json::parse(*text, nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) return std::unexpected(PathToUtf8(p.filename()) + " 不是合法的 JSON");
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
  if (!days) return std::unexpected(PathToUtf8(p.filename()) + "：" + days.error());
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
      warnings.push_back("任务 " + PathToUtf8(p.filename()) + "：" + task.error());
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
  if (ec) return std::unexpected("无法删除文件：" + PathToUtf8(p));
  return {};
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
      warnings.push_back("节假日 " + PathToUtf8(p.filename()) + "：" + hy.error());
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
