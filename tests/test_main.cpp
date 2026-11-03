// 核心逻辑的单元测试（无第三方测试框架）
#include <cstdio>
#include <string>
#include <vector>

#include "core/date.h"
#include "core/holiday.h"
#include "core/i18n.h"
#include "core/lunar.h"
#include "model/day.h"
#include "model/fixed.h"
#include "model/preference.h"
#include "model/task.h"
#include "store/fs_util.h"
#include "store/repository.h"

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
  auto hy = ParseHolidayYear(text, 2026);
  CHECK(hy.has_value());
  CHECK(hy && hy->country == "CN");
  CHECK(!ParseHolidayYear(text, 2027).has_value());
  CHECK(!ParseHolidayYear("{\"country\":\"CN\",\"year\":2026,\"days\":[{\"date\":\"2027-01-01\",\"name\":\"x\",\"off\":true}]}").has_value());

  // 节假日配置不与国家绑定：同一份数据可以放在任一配置中
  HolidayCalendar cal;
  hy->config = 2;
  cal.Set(*hy);
  CHECK(cal.IsOff(2, D("2026-10-01")));
  CHECK(!cal.IsOff(2, D("2026-10-10")));
  CHECK(!cal.IsOff(1, D("2026-10-01")));
  CHECK(cal.Has(2, 2026));
  CHECK(!cal.Has(2, 2027));
  CHECK(!cal.Has(1, 2026));
  CHECK(cal.Country(2, 2026) == "CN");
  CHECK(cal.LatestCountry(2) == "CN");
  CHECK(cal.LatestCountry(1).empty());
  // 代码块前后带说明文字时也能取出 JSON
  CHECK(ParseHolidayYear(std::string("好的，如下：\n") + text + "\n以上为官方安排。", 2026).has_value());
  CHECK(HolidayPrompt("Japan", 2, 2027).find("Japan for the year 2027") != std::string::npos);
  CHECK(HolidayPrompt("Japan", 2, 2027).find("config2_2027.json") != std::string::npos);
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
  cal.Set({1, "中国", 2026, {{D("2026-11-09"), "假", true}}});
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

  // 跳过节假日的任务可以跨入还没导入节假日的年份：没有数据时按普通工作日计算，导入后重算即可
  CHECK(ValidateSpec(Spec(60, "2027-01-10"), D("2026-11-02")) == std::nullopt);
  TaskSpec cross = Spec(600, "2027-01-08");
  cross.skip_holiday = true;
  CHECK(ValidateSpec(cross, D("2026-11-02")) == std::nullopt);
  Task tc = NewTask(cross, "c0de", "2027-01-04T09:00:00");
  const auto sc = Replay(tc);
  CHECK(ComputePlan(*sc, D("2027-01-04"), cal).daily.contains(D("2027-01-04")));
  HolidayCalendar with_2027 = cal;
  with_2027.Set({1, "中国", 2027, {{D("2027-01-04"), "假", true}}});
  CHECK(!ComputePlan(*sc, D("2027-01-04"), with_2027).daily.contains(D("2027-01-04")));

  // 日志描述
  CHECK(DescribeLogEntry(t2.log.front()).starts_with("Created: "));
  CHECK(DescribeLogEntry(t2.log[1]) == "Modified: Due → 2026-11-30");
  CHECK(DescribeLogEntry(t.log[1]) == "Progress 2026-11-02: 1:00");
  CHECK(DescribeLogEntry(t.log[2]) == "Progress 2026-11-02: 0:10, note: 写了引言");
  CHECK(DescribeLogEntry(t2.log[2]) == "Pause");

  // 序列化往返
  auto back = TaskFromJson(ToJson(t2), t2.file_stem);
  CHECK(back.has_value());
  CHECK(Replay(*back)->spec == Replay(t2)->spec);
}

void TestReminder() {
  CHECK(ParseLeadTime("1d") == 1440);
  CHECK(ParseLeadTime("30m") == 30);
  CHECK(FormatLead(90) == "1 h 30 min");
  CHECK(FormatLead(2880) == "2 d");

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
  CHECK(RepeatText(f) == "Monthly on day 31 · from 2026-10-31");

  // 序列化往返
  f.repeat = Repeat::kWeekly;
  f.until = D("2027-06-30");
  f.note = "带电脑";
  auto back = FixedFromJson(ToJson(f), "20261103_b71c");
  CHECK(back.has_value());
  CHECK(back && back->repeat == Repeat::kWeekly && back->weekdays == f.weekdays);
  CHECK(back && back->until == f.until && back->note == "带电脑" && back->file_stem == "20261103_b71c");
  CHECK(RepeatText(f) == "Weekly Tu Th · 2026-10-31 to 2027-06-30");
  // 星期按 日 一 … 六 的顺序显示
  f.weekdays[6] = true;
  CHECK(WeekdaysText(f.weekdays) == "Su Tu Th");
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
  Preference p = PreferenceFromJson(Json{{"holiday_config", 2}, {"language", "CN"}}, warnings);
  CHECK(p.holiday_config == 2 && p.language == "CN" && warnings.empty());

  // 不合法的节假日配置回退为默认值并给出提示；语言文件是否存在在加载时检查
  p = PreferenceFromJson(Json{{"holiday_config", 3}, {"language", "FR"}}, warnings);
  CHECK(p.holiday_config == 1 && p.language == "FR" && warnings.size() == 1);

  // 缺少字段时使用默认值（英文），不提示
  warnings.clear();
  p = PreferenceFromJson(Json::object(), warnings);
  CHECK(p.holiday_config == 1 && p.language == "EN" && warnings.empty());

  // v1.5.0 之前保存的国家：JP 对应配置2
  CHECK(PreferenceFromJson(Json{{"country", "JP"}}, warnings).holiday_config == 2);
  CHECK(PreferenceFromJson(Json{{"country", "CN"}}, warnings).holiday_config == 1);

  // v1.3.0 写入的 zh-CN 对应 CN.json
  CHECK(PreferenceFromJson(Json{{"language", "zh-CN"}}, warnings).language == "CN");

  // 往返
  p.holiday_config = 2;
  CHECK(PreferenceFromJson(ToJson(p), warnings).holiday_config == 2);
  // 农历显示：默认开，可以关闭
  CHECK(p.show_lunar);
  p.show_lunar = false;
  CHECK(!PreferenceFromJson(ToJson(p), warnings).show_lunar);
  // 标语：默认 Look Forward!，可以修改；空字符串使用默认
  CHECK(PreferenceFromJson(Json::object(), warnings).slogan == "Look Forward!");
  p.slogan = "加油！";
  CHECK(PreferenceFromJson(ToJson(p), warnings).slogan == "加油！");
  CHECK(PreferenceFromJson(Json{{"slogan", "  "}}, warnings).slogan == "Look Forward!");
}

Translations LoadLanguageFile(const char* name) {
  const auto text = ReadTextFile(PathFromUtf8(std::string(LANGUAGE_DIR) + "/" + name));
  if (!text) return {};
  auto t = ParseTranslations(*text);
  return t ? *t : Translations{};
}

void TestI18n() {
  // 占位符替换；没有译文时显示 key（英文）
  SetTranslations({});
  CHECK(Tr("Task created: {name}", {{"name", "A"}}) == "Task created: A");
  CHECK(Tr("{a}-{a}", {{"a", "x"}}) == "x-x");
  CHECK(!ParseTranslations("[1]").has_value());
  CHECK(!ParseTranslations("{\"a\": 1}").has_value());

  // EN.json 是模板：key 与 value 相同
  const Translations en = LoadLanguageFile("EN.json");
  CHECK(en.size() > 200);
  bool identical = true;
  for (const auto& [k, v] : en) identical = identical && k == v;
  CHECK(identical);

  // CN.json 覆盖 EN.json 的全部 key，占位符一致
  const Translations cn = LoadLanguageFile("CN.json");
  bool complete = true;
  for (const auto& [k, _] : en) {
    const auto it = cn.find(k);
    if (it == cn.end()) {
      std::printf("CN.json 缺少：%s\n", k.c_str());
      complete = false;
      continue;
    }
    for (size_t pos = 0; (pos = k.find('{', pos)) != std::string::npos; ++pos) {
      const std::string ph = k.substr(pos, k.find('}', pos) - pos + 1);
      // 月份标题的中文不使用 {month_name}；JSON 示例中的 { 不是占位符
      if (ph == "{month_name}" || ph.find(' ') != std::string::npos || ph.find('\n') != std::string::npos) continue;
      if (it->second.find(ph) == std::string::npos) {
        std::printf("CN.json 缺少占位符 %s：%s\n", ph.c_str(), k.c_str());
        complete = false;
      }
    }
  }
  CHECK(complete);

  // 切换为中文
  SetTranslations(cn);
  FixedSchedule f;
  f.repeat = Repeat::kWeekly;
  f.from = D("2026-11-03");
  f.weekdays[1] = f.weekdays[3] = f.weekdays[6] = true;
  CHECK(RepeatText(f) == "每周日二四 · 2026-11-03 起");
  f.repeat = Repeat::kMonthly;
  f.until = D("2027-06-30");
  CHECK(RepeatText(f) == "每月 3 日 · 2026-11-03 至 2027-06-30");
  CHECK(FormatLead(90) == "1小时30分钟");
  CHECK(HolidayPrompt("日本", 2, 2027).find("日本2027年") != std::string::npos);
  CHECK(HolidayPrompt("日本", 2, 2027).find("config2_2027.json") != std::string::npos);
  SetTranslations({});
  CHECK(FormatLead(90) == "1 h 30 min");
}

// 旧版本的任务日志和节假日文件保存的是国家，读取时转换为节假日配置
void TestLegacyHolidayConfig() {
  Task t;
  t.id = "0001";
  t.log.push_back(Json{{"at", "2026-11-01T10:00:00"}, {"op", "create"}, {"name", "x"}, {"total_minutes", 60},
                       {"due", "2026-11-30"}, {"weekdays", {1, 2, 3, 4, 5}}, {"skip_holiday", true}, {"country", "JP"}});
  auto s = Replay(t);
  CHECK(s && s->spec.holiday_config == 2);
  CHECK(DescribeLogEntry(t.log.front()).find("Config 2") != std::string::npos);

  const fs::path dir = fs::temp_directory_path() / "lfcalendar_test_holidays";
  std::error_code ec;
  fs::remove_all(dir, ec);
  CHECK(WriteTextFileAtomic(dir / "holidays" / "JP_2026.json",
                            R"({"country":"JP","year":2026,"days":[{"date":"2026-11-03","name":"文化の日","off":true}]})")
            .has_value());
  Repository repo(dir);
  std::vector<std::string> warnings;
  const HolidayCalendar cal = repo.LoadHolidays(warnings);
  CHECK(warnings.empty());
  CHECK(cal.IsOff(2, D("2026-11-03")));
  CHECK(cal.Country(2, 2026) == "日本");
  CHECK(fs::exists(dir / "holidays" / "config2_2026.json") && !fs::exists(dir / "holidays" / "JP_2026.json"));

  // 新格式：config<N>_<年>.json；年份与文件名不一致时报错
  HolidayYear hy{1, "Deutschland", 2027, {{D("2027-10-03"), "Tag der Deutschen Einheit", true}}};
  CHECK(repo.SaveHolidayYear(hy).has_value());
  CHECK(fs::exists(dir / "holidays" / "config1_2027.json"));
  CHECK(repo.LoadHolidays(warnings).Country(1, 2027) == "Deutschland");
  fs::remove_all(dir, ec);
}

int main() {
  TestDate();
  TestLunar();
  TestHoliday();
  TestTask();
  TestReminder();
  TestFixed();
  TestPreference();
  TestI18n();
  TestLegacyHolidayConfig();
  std::printf("%d/%d passed\n", g_total - g_failed, g_total);
  return g_failed == 0 ? 0 : 1;
}
