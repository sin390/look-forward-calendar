#include "ui/dialogs.h"

#include <charconv>
#include <sstream>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "platform/platform.h"
#include "version.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;  // 键盘事件；数据模型的事件写作 cal::Event

namespace {

std::vector<std::string> CountryNames() {
  std::vector<std::string> names;
  for (const Country& c : SupportedCountries()) names.push_back(c.name);
  return names;
}

int CountryIndex(std::string_view code) {
  const auto& list = SupportedCountries();
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i].code == code) return static_cast<int>(i);
  return 0;
}

// 提前量回填到输入框时使用可再次解析的写法
std::string FormatLeadInput(int minutes) {
  if (minutes > 0 && minutes % 1440 == 0) return std::to_string(minutes / 1440) + "d";
  if (minutes % 60 == 0) return std::to_string(minutes / 60);
  return std::to_string(minutes) + "m";
}

Element Buttons(Component ok, Component cancel) {
  return hbox({filler(), ok->Render(), text("  "), cancel->Render()});
}

}  // namespace

// ---------------------------------------------------------------- Dialog

void Dialog::Build(std::string title, Component controls, std::function<Element()> body,
                   Components tab_order) {
  tab_order_ = std::move(tab_order);
  component_ = Renderer(controls, [title = std::move(title), body = std::move(body)] {
                 return window(text(" " + title + " ") | bold, body()) | clear_under | center;
               }) |
               CatchEvent([this](Event e) {
                 if (e == Event::Escape) {
                   Close();
                   return true;
                 }
                 if (e == Event::Tab || e == Event::TabReverse) {
                   MoveFocus(e == Event::Tab ? 1 : -1);
                   return true;
                 }
                 return false;
               });
}

InputOption Dialog::Field(std::string* content, std::function<void()> on_enter) {
  InputOption option = LineInput(std::move(on_enter));
  option.cursor_position = &cursors_.emplace_back(content, 0).second;
  return option;
}

void Dialog::Show() {
  error_.clear();
  shown_ = true;
  for (auto& [content, cursor] : cursors_) cursor = static_cast<int>(content->size());
  if (!tab_order_.empty()) tab_order_.front()->TakeFocus();
}

// 嵌套容器和 Toggle 会各自吞掉 Tab，因此由对话框按固定顺序统一切换焦点
void Dialog::MoveFocus(int dir) {
  const int n = static_cast<int>(tab_order_.size());
  if (n == 0) return;
  int current = 0;
  for (int i = 0; i < n; ++i)
    if (tab_order_[static_cast<size_t>(i)]->Focused()) current = i;
  tab_order_[static_cast<size_t>((current + dir + n) % n)]->TakeFocus();
}

Element Dialog::ErrorLine() const {
  return error_.empty() ? text("") : paragraph(error_) | color(Color::Red);
}

// ---------------------------------------------------------------- EventDialog

EventDialog::EventDialog(AppState& s) : s_(s) {
  auto title = Input(&title_, "必填", Field(&title_));
  auto start = Input(&start_, "HH:MM", Field(&start_));
  auto end = Input(&end_, "HH:MM", Field(&end_));
  auto note = Input(&note_, "", Field(&note_));
  auto remind = Checkbox("提醒", &remind_);
  auto lead = Input(&lead_, "如 30m / 1:00 / 1d", Field(&lead_));
  auto ok = Button("确定", [this] { Submit(); }, SmallButton());
  auto cancel = Button("取消", [this] { Close(); }, SmallButton());
  auto controls = Container::Vertical(
      {title, start, end, note, remind, lead, Container::Horizontal({ok, cancel})});

  Build("事件", controls, [=, this] {
    return vbox({
               text((index_ ? "编辑 " : "新建 ") + FormatDate(date_) + " 的事件") | dim,
               separatorLight(),
               FormRow("标题", InputBox(title, 36)),
               FormRow("开始时间", InputBox(start, 8)),
               FormRow("结束时间", InputBox(end, 8)),
               FormRow("备注", InputBox(note, 36)),
               FormRow("", remind->Render()),
               FormRow("提前", hbox({InputBox(lead, 20), text(" 前提醒")})) | (remind_ ? nothing : dim),
               text("   全天事件以当天 0:00 为开始时间") | dim,
               ErrorLine(),
               separatorLight(),
               Buttons(ok, cancel),
           }) |
           size(WIDTH, EQUAL, 52);
  }, {title, start, end, note, remind, lead, ok, cancel});
}

void EventDialog::OpenAdd(Date d) {
  date_ = d;
  index_.reset();
  title_.clear(), start_.clear(), end_.clear(), note_.clear();
  remind_ = false;
  lead_ = "30m";
  Show();
}

void EventDialog::OpenEdit(Date d, size_t index) {
  const Day* day = s_.FindDay(d);
  if (!day || index >= day->events.size()) return;
  const cal::Event& e = day->events[index];
  date_ = d;
  index_ = index;
  title_ = e.title, start_ = e.start, end_ = e.end, note_ = e.note;
  remind_ = e.remind.has_value();
  lead_ = e.remind ? FormatLeadInput(*e.remind) : "30m";
  Show();
}

void EventDialog::Submit() {
  cal::Event e{std::string(Trim(title_)), std::string(Trim(start_)), std::string(Trim(end_)),
               std::string(Trim(note_)), std::nullopt};
  if (remind_) {
    e.remind = ParseLeadTime(lead_);
    if (!e.remind) {
      error_ = "提前量格式应为 30m / 1:00 / 2（小时）/ 1d";
      return;
    }
  }
  const Result r = index_ ? s_.UpdateEvent(date_, *index_, e) : s_.AddEvent(date_, e);
  if (!r) {
    error_ = r.error();
    return;
  }
  s_.Info("已保存事件");
  Close();
}

// ---------------------------------------------------------------- TaskDialog

TaskDialog::TaskDialog(AppState& s) : s_(s), country_names_(CountryNames()) {
  auto name = Input(&name_, "必填", Field(&name_));
  auto total = Input(&total_, "如 40 或 12:30", Field(&total_));
  auto due = Input(&due_, "YYYY-MM-DD", Field(&due_));
  static constexpr std::array<const char*, 7> kDays = {"一", "二", "三", "四", "五", "六", "日"};
  Components day_boxes;
  for (size_t i = 0; i < 7; ++i) day_boxes.push_back(Checkbox(kDays[i], &weekdays_[i]));
  auto days = Container::Horizontal(day_boxes);
  auto skip = Checkbox("跳过节假日", &skip_holiday_);
  auto country = Toggle(&country_names_, &country_index_);
  auto ok = Button("确定", [this] { Submit(); }, SmallButton());
  auto cancel = Button("取消", [this] { Close(); }, SmallButton());
  auto controls =
      Container::Vertical({name, total, due, days, skip, country, Container::Horizontal({ok, cancel})});

  Build("长期任务", controls, [=, this] {
    Elements day_elems;
    for (const Component& c : day_boxes) day_elems.push_back(c->Render() | size(WIDTH, EQUAL, 6));
    return vbox({
               text(task_id_.empty() ? "新建任务（创建当天计入安排）" : "修改任务 " + IdTag(task_id_)) | dim,
               separatorLight(),
               FormRow("任务名", InputBox(name, 30)),
               FormRow("预定时长", hbox({InputBox(total, 16), text(" 小时") | dim})),
               FormRow("完成日期", InputBox(due, 12)),
               FormRow("每周工作", hbox(std::move(day_elems))),
               FormRow("", skip->Render()),
               FormRow("节假日国家", country->Render()),
               ErrorLine(),
               separatorLight(),
               Buttons(ok, cancel),
           }) |
           size(WIDTH, EQUAL, 56);
  }, {name, total, due, days, skip, country, ok, cancel});
}

void TaskDialog::OpenCreate() {
  task_id_.clear();
  name_.clear();
  total_.clear();
  due_ = FormatDate(AddDays(s_.today(), 14));
  weekdays_ = {true, true, true, true, true, false, false};
  skip_holiday_ = true;
  country_index_ = CountryIndex(s_.display_country());
  Show();
}

void TaskDialog::OpenModify(const std::string& id) {
  const TaskEntry* e = s_.FindTask(id);
  if (!e) return;
  const TaskSpec& spec = e->state.spec;
  task_id_ = id;
  name_ = spec.name;
  total_ = FormatMinutes(spec.total_minutes);
  due_ = FormatDate(spec.due);
  weekdays_ = spec.weekdays;
  skip_holiday_ = spec.skip_holiday;
  country_index_ = CountryIndex(spec.country);
  Show();
}

void TaskDialog::Submit() {
  TaskSpec spec;
  spec.name = std::string(Trim(name_));
  const auto total = ParseDuration(total_);
  if (!total) {
    error_ = "预定时长格式错误（小时数，或 H:MM）";
    return;
  }
  spec.total_minutes = *total;
  const auto due = ParseDate(Trim(due_));
  if (!due) {
    error_ = "完成日期格式应为 YYYY-MM-DD";
    return;
  }
  spec.due = *due;
  spec.weekdays = weekdays_;
  spec.skip_holiday = skip_holiday_;
  spec.country = SupportedCountries()[static_cast<size_t>(country_index_)].code;

  const Result r = task_id_.empty() ? s_.CreateTask(spec) : s_.ModifyTask(task_id_, spec);
  if (!r) {
    error_ = r.error();
    return;
  }
  s_.Info(task_id_.empty() ? "已创建任务：" + spec.name : "已修改任务：" + spec.name);
  Close();
}

// ---------------------------------------------------------------- ProgressDialog

ProgressDialog::ProgressDialog(AppState& s) : s_(s) {
  auto minutes = Input(&minutes_, "如 1:30 / 1.5 / 90m", Field(&minutes_, [this] { Submit(); }));
  auto note = Input(&note_, "今天做了什么", Field(&note_, [this] { Submit(); }));
  auto ok = Button("确定", [this] { Submit(); }, SmallButton());
  auto cancel = Button("取消", [this] { Close(); }, SmallButton());
  auto controls = Container::Vertical({minutes, note, Container::Horizontal({ok, cancel})});

  Build("进行", controls, [=, this] {
    const TaskEntry* e = s_.FindTask(task_id_);
    std::string plan = "-";
    if (e) {
      if (const auto it = e->plan.daily.find(date_); it != e->plan.daily.end())
        plan = FormatMinutes(it->second);
    }
    return vbox({
               text((e ? e->task.name + " " + IdTag(task_id_) : std::string()) + "  " + FormatDate(date_)) | bold,
               text("当天计划 " + plan) | dim,
               separatorLight(),
               FormRow("实际时长", InputBox(minutes, 20)),
               FormRow("进行内容", InputBox(note, 36)),
               text("   清空时长和内容即删除当天记录") | dim,
               ErrorLine(),
               separatorLight(),
               Buttons(ok, cancel),
           }) |
           size(WIDTH, EQUAL, 52);
  }, {minutes, note, ok, cancel});
}

void ProgressDialog::Open(const std::string& task_id, Date d) {
  const TaskEntry* e = s_.FindTask(task_id);
  if (!e) return;
  if (d > s_.today()) {
    s_.Error("未来的日期已锁定，不能填写");
    return;
  }
  task_id_ = task_id;
  date_ = d;
  const DayProgress p = e->state.ProgressOn(d);
  minutes_ = p.minutes > 0 ? FormatMinutes(p.minutes) : "";
  note_ = p.note;
  Show();
}

void ProgressDialog::Submit() {
  const std::string_view m = Trim(minutes_);
  const auto minutes = m.empty() ? std::optional<int>(0) : ParseDuration(m);
  if (!minutes) {
    error_ = "时长格式应为 1:30 / 1.5 / 90m";
    return;
  }
  const Result r = s_.SetProgress(task_id_, date_, {*minutes, std::string(Trim(note_))});
  if (!r) {
    error_ = r.error();
    return;
  }
  s_.Info("已保存 " + ShortDate(date_) + " 的进行情况");
  Close();
}

// ---------------------------------------------------------------- AboutDialog

AboutDialog::AboutDialog() {
  auto close = Button("关闭", [this] { Close(); }, SmallButton());
  Build("关于", close, [close] {
    return vbox({
               text(std::string(kAppName) + "  v" + kVersion) | bold | hcenter,
               text(std::string("发布日期 ") + kReleaseDate) | dim | hcenter,
               separatorLight(),
               paragraph(kCredits),
               text(""),
               text("基于 FTXUI 7 与 nlohmann/json，C++23") | dim,
               separatorLight(),
               hbox({filler(), close->Render()}),
           }) |
           size(WIDTH, EQUAL, 50);
  }, {close});
}

// ---------------------------------------------------------------- HolidayDialog

HolidayDialog::HolidayDialog(AppState& s) : s_(s), country_names_(CountryNames()) {
  auto country = Toggle(&country_names_, &country_index_);
  auto year = Input(&year_, "YYYY", Field(&year_));
  auto gen = Button("生成并复制提示词", [this] { MakePrompt(); }, SmallButton());
  auto path = Input(&path_, "AI 返回的 JSON 保存后的文件路径", Field(&path_, [this] { Import(); }));
  auto import_btn = Button("导入", [this] { Import(); }, SmallButton());
  auto close = Button("关闭", [this] { Close(); }, SmallButton());
  auto controls = Container::Vertical({country, year, gen, path, Container::Horizontal({import_btn, close})});

  Build("导入节假日", controls, [=, this] {
    Elements prompt_lines;
    std::istringstream ss(prompt_);
    for (std::string line; std::getline(ss, line);) prompt_lines.push_back(text(line));

    std::string loaded;
    for (const auto& [code, y] : s_.holidays().Loaded())
      loaded += (loaded.empty() ? "" : "  ") + code + " " + std::to_string(y);

    Elements body = {
        text("已导入：" + (loaded.empty() ? std::string("无") : loaded)) | dim,
        separatorLight(),
        FormRow("国家", country->Render()),
        FormRow("年份", InputBox(year, 8)),
        FormRow("① 提示词", gen->Render()),
    };
    if (!prompt_lines.empty())
      body.push_back(vbox(std::move(prompt_lines)) | dim | yframe | size(HEIGHT, LESS_THAN, 12) | border);
    body.push_back(FormRow("② 文件路径", InputBox(path, 48)));
    body.push_back(text("   同一国家同一年份的旧数据会被整体覆盖") | dim);
    if (!info_.empty()) body.push_back(paragraph(info_) | color(Color::Green));
    body.push_back(ErrorLine());
    body.push_back(separatorLight());
    body.push_back(Buttons(import_btn, close));
    return vbox(std::move(body)) | size(WIDTH, EQUAL, 66);
  }, {country, year, gen, path, import_btn, close});
}

void HolidayDialog::Open() {
  country_index_ = CountryIndex(s_.display_country());
  year_ = std::to_string(YearNum(s_.selected()));
  path_.clear();
  prompt_.clear();
  info_.clear();
  Show();
}

std::string HolidayDialog::Country() const {
  return SupportedCountries()[static_cast<size_t>(country_index_)].code;
}

std::optional<int> HolidayDialog::ParsedYear() {
  const std::string_view y = Trim(year_);
  int value = 0;
  auto [p, ec] = std::from_chars(y.data(), y.data() + y.size(), value);
  if (ec != std::errc{} || p != y.data() + y.size() || value < 1900 || value > 2100) {
    error_ = "年份应为 1900–2100 的整数";
    info_.clear();
    return std::nullopt;
  }
  return value;
}

void HolidayDialog::MakePrompt() {
  const auto year = ParsedYear();
  if (!year) return;
  prompt_ = HolidayPrompt(Country(), *year);
  error_.clear();
  info_ = CopyToClipboard(prompt_) ? "提示词已复制到剪贴板，粘贴给 AI 即可"
                                   : "复制到剪贴板失败，请手动复制下方提示词";
}

void HolidayDialog::Import() {
  const auto year = ParsedYear();
  if (!year) return;
  // 去掉“复制为路径”带来的引号
  std::string_view p = Trim(path_);
  if (p.size() >= 2 && p.front() == '"' && p.back() == '"') p = p.substr(1, p.size() - 2);
  if (p.empty()) {
    error_ = "请输入文件路径";
    info_.clear();
    return;
  }
  const Result r = s_.ImportHolidays(PathFromUtf8(p), Country(), *year);
  if (!r) {
    error_ = r.error();
    info_.clear();
    return;
  }
  error_.clear();
  info_ = "已导入 " + CountryName(Country()) + " " + std::to_string(*year) + " 年节假日";
  s_.Info(info_);
}

}  // namespace cal::ui
