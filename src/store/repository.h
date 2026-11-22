#pragma once

#include <expected>
#include <set>
#include <string>
#include <vector>

#include "core/holiday.h"
#include "model/day.h"
#include "model/fixed.h"
#include "model/preference.h"
#include "model/task.h"
#include "store/fs_util.h"

namespace cal {

// data/ 目录的读写
//   general/<年>/<月>.json
//   task/<创建日期>_<短id>.json
//   fixed/<创建日期>_<短id>.json
//   holidays/config<配置>_<年>.json
//   diary/<年>/<YYYY-MM-DD>.txt（纯文本，UTF-8）
//   preference.json
class Repository {
 public:
  explicit Repository(fs::path root);

  const fs::path& root() const { return root_; }

  // 文件不存在时返回空月份
  std::expected<MonthDays, std::string> LoadMonth(int year, unsigned month) const;
  std::expected<void, std::string> SaveMonth(int year, unsigned month, const MonthDays& days) const;

  // 无法解析的文件跳过，并在 warnings 中说明
  std::vector<Task> LoadTasks(std::vector<std::string>& warnings) const;
  std::expected<void, std::string> SaveTask(const Task& task) const;
  std::expected<void, std::string> DeleteTask(const Task& task) const;

  std::vector<FixedSchedule> LoadFixed(std::vector<std::string>& warnings) const;
  std::expected<void, std::string> SaveFixed(const FixedSchedule& f) const;
  std::expected<void, std::string> DeleteFixed(const FixedSchedule& f) const;

  // 文件不存在时返回默认配置
  Preference LoadPreference(std::vector<std::string>& warnings) const;
  std::expected<void, std::string> SavePreference(const Preference& p) const;

  // v1.5.0 之前的 <国家代码>_<年>.json（CN → 配置1，JP → 配置2）在读取时自动改名为新格式
  HolidayCalendar LoadHolidays(std::vector<std::string>& warnings) const;
  std::expected<void, std::string> SaveHolidayYear(const HolidayYear& hy) const;

  // 日记：扫描 diary/ 得到有日记的日期（文件名不是合法日期的跳过）
  std::set<Date> ListDiaryDates() const;
  // 文件不存在时返回空字符串
  std::expected<std::string, std::string> LoadDiary(Date d) const;
  // 内容为空（只有空白）时删除文件
  std::expected<void, std::string> SaveDiary(Date d, const std::string& text) const;

 private:
  fs::path DiaryPath(Date d) const;
  fs::path MonthPath(int year, unsigned month) const;
  fs::path HolidayPath(int config, int year) const;
  void MigrateLegacyHolidays(std::vector<std::string>& warnings) const;

  fs::path root_;
};

}  // namespace cal
