// 核心逻辑的单元测试（无第三方测试框架）
#include <cstdio>
#include <string>
#include <vector>

#include "core/date.h"
#include "core/holiday.h"
#include "core/lunar.h"
#include "model/day.h"
#include "model/fixed.h"
#include "model/preference.h"
#include "model/task.h"

using namespace cal;

namespace {

int g_failed = 0;
int g_total = 0;

#define CHECK(expr)                                                     \
  do {                                                                  \
    ++g_total;                                                          \
    if (!(expr)) {                                                      \
      ++g_failed;                                                       \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);       \
    }                                                                   \
  } while (0)

Date D(const char* s) { return *ParseDate(s); }

std::string Lunar(const char* s) {
  const auto ld = ToLunar(D(s));
  if (!ld) return "?";
  return LunarMonthName(*ld) + LunarDayName(ld->day);
}

void TestDate() {
  CHECK(FormatDate(D("2026-11-02")) == "2026-11-02");
  CHECK(!ParseDate("2026-02-30"));
  CHECK(IsoWeekday(D("2026-11-02")) == 1);
  CHECK(AddMonths(D("2026-01-31"), 1) == D("2026-02-28"));
  CHECK(ParseDuration("1:30") == 90);
  CHECK(ParseDuration("1.5") == 90);
  CHECK(ParseDuration("2") == 120);
  CHECK(ParseDuration("45m") == 45);
  CHECK(!ParseDuration("abc"));
  CHECK(FormatMinutes(150) == "2:30");
  CHECK(CeilTo(31, 10) == 40);
}

void TestLunar() {
  // 春节
  for (const char* s : {"1990-01-27", "2000-02-05", "2023-01-22", "2024-02-10", "2025-01-29",
                        "2026-02-17", "2027-02-06"})
    CHECK(Lunar(s) == "正月初一");
  CHECK(Lunar("2026-02-16") == "腊月廿九");
  CHECK(LunarShortLabel(D("2026-02-16")) == "除夕");
  CHECK(LunarShortLabel(D("2024-09-17")) == "中秋");
  CHECK(LunarShortLabel(D("2025-10-06")) == "中秋");
  CHECK(LunarShortLabel(D("2026-09-25")) == "中秋");
  CHECK(Lunar("2025-07-25") == "闰六月初一");
  CHECK(GanzhiYear(2026) == "丙午");
  CHECK(Zodiac(2026) == "马");
  // 节气
  CHECK(SolarTerm(D("2026-02-04")) == "立春");
  CHECK(SolarTerm(D("2026-02-18")) == "雨水");
  CHECK(SolarTerm(D("2026-04-05")) == "清明");
  CHECK(SolarTerm(D("2024-04-04")) == "清明");
  CHECK(SolarTerm(D("2025-12-21")) == "冬至");
  CHECK(SolarTerm(D("2026-12-22")) == "冬至");
  CHECK(SolarTerm(D("2026-11-03")).empty());
}

void TestHoliday() {
  const char* text = R"(```json
{ "country": "CN", "year": 2026,
  "days": [ { "date": "2026-10-01", "name": "国庆节", "off": true },
            { "date": "2026-10-10", "name": "国庆节调休补班", "off": false } ] }
```)";
  auto hy = ParseHolidayYear(text, "CN", 2026);
  CHECK(hy.has_value());
  CHECK(!ParseHolidayYear(text, "JP", 2026).has_value());
  CHECK(!ParseHolidayYear(text, "CN", 2027).has_value());
  CHECK(!ParseHolidayYear("{\"country\":\"CN\",\"year\":2026,\"days\":[{\"date\":\"2027-01-01\",\"name\":\"x\",\"off\":true}]}").has_value());

  HolidayCalendar cal;
  cal.Set(*hy);
  CHECK(cal.IsOff("CN", D("2026-10-01")));
  CHECK(!cal.IsOff("CN", D("2026-10-10")));
  CHECK(cal.Has("CN", 2026));
  CHECK(!cal.Has("CN", 2027));
  // 代码块前后带说明文字时也能取出 JSON
  CHECK(ParseHolidayYear(std::string("好的，如下：\n") + text + "\n以上为官方安排。", "CN", 2026).has_value());
  CHECK(HolidayPrompt("JP", 2027).find("日本2027年") != std::string::npos);
  CHECK(HolidayPrompt("JP", 2027).find("JP_2027.json") != std::string::npos);
}

TaskSpec Spec(int minutes, const char* due) {
  TaskSpec s;
  s.name = "测试";
  s.total_minutes = minutes;
  s.due = D(due);
  s.weekdays = {true, true, true, true, true, true, true};
  s.skip_holiday = false;
  return s;
}

void TestTask() {
  const HolidayCalendar none;
  // 11-02 创建，100 分钟，截止 11-04：40 / 30 / 30
  Task t = NewTask(Spec(100, "2026-11-04"), "a3f9", "2026-11-02T10:00:00");
  CHECK(t.file_stem == "20261102_a3f9");
  auto s = Replay(t);
  CHECK(s.has_value());
  auto p = ComputePlan(*s, D("2026-11-02"), none);
  CHECK(p.daily.size() == 3);
  CHECK(p.daily[D("2026-11-02")] == 40);
  CHECK(p.daily[D("2026-11-03")] == 30);
  CHECK(p.daily[D("2026-11-04")] == 30);

  // 今天录入的工作量不改变今天的计划
  AppendProgress(t, D("2026-11-02"), {60, ""}, "2026-11-02T20:00:00");
  s = Replay(t);
  p = ComputePlan(*s, D("2026-11-02"), none);
  CHECK(p.daily[D("2026-11-02")] == 40);
  // 第二天：剩 40 分钟分给 2 天
  p = ComputePlan(*s, D("2026-11-03"), none);
  CHECK(p.remaining == 40);
  CHECK(p.daily[D("2026-11-03")] == 20);
  CHECK(p.daily[D("2026-11-04")] == 20);
  // 覆盖录入
  AppendProgress(t, D("2026-11-02"), {10, "写了引言"}, "2026-11-02T21:00:00");
  CHECK(Replay(t)->ProgressOn(D("2026-11-02")).minutes == 10);
  CHECK(Replay(t)->ProgressOn(D("2026-11-02")).note == "写了引言");

  // 只在周一工作 + 跳过节假日
  TaskSpec spec = Spec(60, "2026-11-16");
  spec.weekdays = {true, false, false, false, false, false, false};
  spec.skip_holiday = true;
  HolidayCalendar cal;
  cal.Set({"CN", 2026, {{D("2026-11-09"), "假", true}}});
  Task t2 = NewTask(spec, "b001", "2026-11-02T10:00:00");
  p = ComputePlan(*Replay(t2), D("2026-11-02"), cal);
  CHECK(p.daily.size() == 2);  // 11-02、11-16（11-09 放假）
  CHECK(p.daily[D("2026-11-02")] == 30);
  CHECK(!p.daily.contains(D("2026-11-09")));

  // 超期
  p = ComputePlan(*Replay(t2), D("2026-11-17"), cal);
  CHECK(p.overdue);
  CHECK(p.daily.empty());

  // 修改截止日期、暂停/恢复
  TaskSpec spec2 = spec;
  spec2.due = D("2026-11-30");
  CHECK(AppendModify(t2, spec, spec2, "2026-11-17T09:00:00"));
  CHECK(!ComputePlan(*Replay(t2), D("2026-11-17"), cal).overdue);
  AppendOp(t2, "pause", "2026-11-17T09:10:00");
  s = Replay(t2);
  CHECK(s->status == TaskStatus::kPaused);
  CHECK(ComputePlan(*s, D("2026-11-17"), cal).daily.empty());
  AppendOp(t2, "resume", "2026-11-18T09:10:00");
  CHECK(Replay(t2)->status == TaskStatus::kActive);

  // 校验：缺少节假日年份时拒绝
  CHECK(ValidateSpec(Spec(60, "2027-01-10"), D("2026-11-02"), cal) == std::nullopt);
  TaskSpec cross = Spec(60, "2027-01-10");
  cross.skip_holiday = true;
  CHECK(ValidateSpec(cross, D("2026-11-02"), cal).has_value());

  // 日志描述
  CHECK(DescribeLogEntry(t2.log.front()).starts_with("创建："));
  CHECK(DescribeLogEntry(t2.log[1]) == "修改：截止 → 2026-11-30");
  CHECK(DescribeLogEntry(t.log[1]) == "进行 2026-11-02：1:00");
  CHECK(DescribeLogEntry(t.log[2]) == "进行 2026-11-02：0:10，内容：写了引言");
  CHECK(DescribeLogEntry(t2.log[2]) == "暂停");

  // 序列化往返
  auto back = TaskFromJson(ToJson(t2), t2.file_stem);
  CHECK(back.has_value());
  CHECK(Replay(*back)->spec == Replay(t2)->spec);
}

void TestReminder() {
  CHECK(ParseLeadTime("1d") == 1440);
  CHECK(ParseLeadTime("30m") == 30);
  CHECK(FormatLead(90) == "1小时30分钟");
  CHECK(FormatLead(2880) == "2天");

  const Date d = D("2026-11-10");
  Event timed{"组会", "14:00", "15:30", "", 30};
  auto w = RemindWindowOf(d, timed);
  CHECK(w.has_value());
  CHECK(w->from == MinuteStamp(d, 13 * 60 + 30));
  CHECK(w->until == MinuteStamp(d, 15 * 60 + 30));

  Event no_end{"打电话", "09:00", "", "", 0};  // 准时提醒：只在开始那一分钟
  w = RemindWindowOf(d, no_end);
  CHECK(w->from == MinuteStamp(d, 540));
  CHECK(w->until == MinuteStamp(d, 541));

  Event all_day{"交作业", "", "", "", 1440};  // 全天，提前一天
  w = RemindWindowOf(d, all_day);
  CHECK(w->from == MinuteStamp(D("2026-11-09"), 0));
  CHECK(w->until == MinuteStamp(D("2026-11-11"), 0));

  CHECK(!RemindWindowOf(d, Event{"无提醒", "", "", "", std::nullopt}));
  CHECK(ValidateEvent(Event{"x", "", "", "", 31 * 1440}).has_value());
}

}  // namespace

void TestFixed() {
  FixedSchedule f;
  f.id = "b71c";
  f.title = "组会";
  f.start = "09:00";
  f.end = "10:00";
  f.from = D("2026-11-03");  // 周二
  f.weekdays[1] = true;      // 周二
  f.weekdays[3] = true;      // 周四
  CHECK(!ValidateFixed(f));

  // 每周
  CHECK(OccursOn(f, D("2026-11-03")));
  CHECK(OccursOn(f, D("2026-11-05")));
  CHECK(!OccursOn(f, D("2026-11-04")));
  CHECK(OccursOn(f, D("2026-11-10")));
  CHECK(!OccursOn(f, D("2026-10-27")));  // 开始日期之前
  // 开始日期所在周、开始日期之前的星期不算
  f.weekdays[0] = true;
  CHECK(!OccursOn(f, D("2026-11-02")));
  f.weekdays[0] = false;

  // 结束日期（含）
  f.until = D("2026-11-10");
  CHECK(OccursOn(f, D("2026-11-10")));
  CHECK(!OccursOn(f, D("2026-11-12")));
  f.until.reset();

  // 每两周：以开始日期所在周为第一周
  f.repeat = Repeat::kBiweekly;
  CHECK(OccursOn(f, D("2026-11-05")));
  CHECK(!OccursOn(f, D("2026-11-10")));
  CHECK(!OccursOn(f, D("2026-11-12")));
  CHECK(OccursOn(f, D("2026-11-17")));
  CHECK(OccursOn(f, D("2027-01-12")));  // 跨年仍按周数计算（相隔 10 周）

  // 每天
  f.repeat = Repeat::kDaily;
  CHECK(OccursOn(f, D("2026-11-04")));
  CHECK(OccursOn(f, D("2026-11-08")));

  // 每月：没有这一天的月份跳过
  f.repeat = Repeat::kMonthly;
  f.from = D("2026-10-31");
  CHECK(OccursOn(f, D("2026-12-31")));
  CHECK(!OccursOn(f, D("2026-11-30")));
  CHECK(!OccursOn(f, D("2026-12-30")));
  CHECK(RepeatText(f) == "每月 31 日 · 2026-10-31 起");

  // 序列化往返
  f.repeat = Repeat::kWeekly;
  f.until = D("2027-06-30");
  f.note = "带电脑";
  auto back = FixedFromJson(ToJson(f), "20261103_b71c");
  CHECK(back.has_value());
  CHECK(back && back->repeat == Repeat::kWeekly && back->weekdays == f.weekdays);
  CHECK(back && back->until == f.until && back->note == "带电脑" && back->file_stem == "20261103_b71c");
  CHECK(RepeatText(f) == "每周二四 · 2026-10-31 至 2027-06-30");
  // 星期按 日 一 … 六 的顺序显示
  f.weekdays[6] = true;
  CHECK(WeekdaysText(f.weekdays) == "日二四");
  f.weekdays[6] = false;

  // 校验
  FixedSchedule bad = f;
  bad.end = "08:00";
  CHECK(ValidateFixed(bad).has_value());  // 结束早于开始
  bad = f;
  bad.end.clear();
  CHECK(ValidateFixed(bad).has_value());  // 时间段必填
  bad = f;
  bad.weekdays = {};
  CHECK(ValidateFixed(bad).has_value());  // 每周至少一天
  bad.repeat = Repeat::kDaily;
  CHECK(!ValidateFixed(bad).has_value());  // 每天不需要星期
  bad = f;
  bad.until = D("2026-01-01");
  CHECK(ValidateFixed(bad).has_value());  // 结束日期早于开始日期
}

void TestPreference() {
  std::vector<std::string> warnings;
  Preference p = PreferenceFromJson(Json{{"country", "JP"}, {"language", "zh-CN"}}, warnings);
  CHECK(p.country == "JP" && p.language == "zh-CN" && warnings.empty());

  // 不支持的值回退为默认值并给出提示
  p = PreferenceFromJson(Json{{"country", "XX"}, {"language", "fr"}}, warnings);
  CHECK(p.country == "CN" && p.language == "zh-CN" && warnings.size() == 2);

  // 缺少字段时使用默认值，不提示
  warnings.clear();
  p = PreferenceFromJson(Json::object(), warnings);
  CHECK(p.country == "CN" && warnings.empty());

  // 往返
  p.country = "JP";
  CHECK(PreferenceFromJson(ToJson(p), warnings).country == "JP");
  CHECK(LanguageName("zh-CN") == "中文");
}

int main() {
  TestDate();
  TestLunar();
  TestHoliday();
  TestTask();
  TestReminder();
  TestFixed();
  TestPreference();
  std::printf("%d/%d passed\n", g_total - g_failed, g_total);
  return g_failed == 0 ? 0 : 1;
}
