#pragma once

#include <array>
#include <expected>
#include <optional>
#include <string>

#include "core/date.h"
#include "core/json.h"

namespace cal {

// 固定日程：按周期重复的工作内容和时间段，如“每周一三 9:00-10:00 组会”。
// 每个固定日程一个文件 fixed/<创建日期>_<短id>.json

enum class Repeat { kDaily, kWeekly, kBiweekly, kMonthly };

struct FixedSchedule {
  std::string id;         // 随机短 id，如 "b71c"
  std::string file_stem;  // 文件名（不含扩展名），如 "20261103_b71c"
  std::string title;      // 工作内容
  std::string start;      // "HH:MM"
  std::string end;        // "HH:MM"
  std::string note;
  Repeat repeat = Repeat::kWeekly;
  std::array<bool, 7> weekdays{};  // 每周 / 每两周时使用，[0]=周一 … [6]=周日
  Date from{};                     // 开始日期；每两周以这一周为第一周，每月以这一天为每月的日期
  std::optional<Date> until;       // 结束日期（含），空表示一直重复
};

// 某天是否有该固定日程
bool OccursOn(const FixedSchedule& f, Date d);

Json ToJson(const FixedSchedule& f);
std::expected<FixedSchedule, std::string> FixedFromJson(const Json& j, std::string file_stem);

std::optional<std::string> ValidateFixed(const FixedSchedule& f);  // 返回错误信息

std::string RepeatName(Repeat r);  // "Weekly"（中文：每周）
// "Weekly Mo We · from 2026-11-03"（中文：每周一三 · 2026-11-03 起）
std::string RepeatText(const FixedSchedule& f);

}  // namespace cal
