#include "ui/dialogs.h"

#include <charconv>
#include <sstream>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "platform/platform.h"
#include "version.h"
#include "ui/logo.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;  // 键盘事件；数据模型的事件写作 cal::Event

namespace {

std::vector<std::string> CountryNames() {
  std::vector<std::string> names;
  for (const Country& c : SupportedCountries()) names.push_back(CountryName(c.code));
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

// 星期复选框，按界面顺序（日 一 … 六）排列；weekdays 下标 0 = 周一
Components WeekdayBoxes(std::array<bool, 7>& weekdays) {
  Components boxes;
  for (int w : kDisplayWeekOrder)
    boxes.push_back(Checkbox(WeekdayShortName(w), &weekdays[static_cast<size_t>(w - 1)]));
  return boxes;
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
  return error_.empty() ? text("") : WrapText(error_) | color(Color::Red);
}

// ---------------------------------------------------------------- EventDialog

EventDialog::EventDialog(AppState& s) : s_(s) {
  auto title = Input(&title_, Tr("Required"), Field(&title_));
  auto start = Input(&start_, "HH:MM", Field(&start_));
  auto end = Input(&end_, "HH:MM", Field(&end_));
  auto note = Input(&note_, "", Field(&note_));
  auto remind = Checkbox(Tr("Remind"), &remind_);
  auto lead = Input(&lead_, Tr("e.g. 30m / 1:00 / 1d"), Field(&lead_));
  auto ok = Button(Tr("OK"), [this] { Submit(); }, SmallButton());
  auto cancel = Button(Tr("Cancel"), [this] { Close(); }, SmallButton());
  auto controls = Container::Vertical(
      {title, start, end, note, remind, lead, Container::Horizontal({ok, cancel})});

  Build(Tr("Event"), controls, [=, this] {
    return vbox({
               text(index_ ? Tr("Edit event on {date}", {{"date", FormatDate(date_)}})
                           : Tr("New event on {date}", {{"date", FormatDate(date_)}})) |
                   dim,
               separatorLight(),
               FormRow(Tr("Title"), InputBox(title, 36)),
               FormRow(Tr("Start time"), InputBox(start, 8)),
               FormRow(Tr("End time"), InputBox(end, 8)),
               FormRow(Tr("Note"), InputBox(note, 36)),
               FormRow("", remind->Render()),
               FormRow(Tr("Lead"), hbox({InputBox(lead, 20), text(Tr(" before start"))})) | (remind_ ? nothing : dim),
               text(Tr("   All-day events start at 0:00")) | dim,
               ErrorLine(),
               separatorLight(),
               Buttons(ok, cancel),
           }) |
           size(WIDTH, EQUAL, 60);
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
      error_ = Tr("Lead time must be like 30m / 1:00 / 2 (hours) / 1d");
      return;
    }
  }
  const Result r = index_ ? s_.UpdateEvent(date_, *index_, e) : s_.AddEvent(date_, e);
  if (!r) {
    error_ = r.error();
    return;
  }
  s_.Info(Tr("Event saved"));
  Close();
}

// ---------------------------------------------------------------- TaskDialog

TaskDialog::TaskDialog(AppState& s) : s_(s), country_names_(CountryNames()) {
  auto name = Input(&name_, Tr("Required"), Field(&name_));
  auto total = Input(&total_, Tr("e.g. 40 or 12:30"), Field(&total_));
  auto due = Input(&due_, "YYYY-MM-DD", Field(&due_));
  Components day_boxes = WeekdayBoxes(weekdays_);
  auto days = Container::Horizontal(day_boxes);
  auto skip = Checkbox(Tr("Skip holidays"), &skip_holiday_);
  auto country = Toggle(&country_names_, &country_index_);
  auto ok = Button(Tr("OK"), [this] { Submit(); }, SmallButton());
  auto cancel = Button(Tr("Cancel"), [this] { Close(); }, SmallButton());
  auto controls =
      Container::Vertical({name, total, due, days, skip, country, Container::Horizontal({ok, cancel})});

  Build(Tr("Long-term task"), controls, [=, this] {
    Elements day_elems;
    for (const Component& c : day_boxes) day_elems.push_back(c->Render() | size(WIDTH, EQUAL, 7));
    return vbox({
               text(task_id_.empty() ? Tr("New task (the creation day is included in the plan)")
                                    : Tr("Modify task {id}", {{"id", IdTag(task_id_)}})) | dim,
               separatorLight(),
               FormRow(Tr("Task name"), InputBox(name, 30)),
               FormRow(Tr("Planned hours"), hbox({InputBox(total, 16), text(Tr(" hours")) | dim})),
               FormRow(Tr("Due date"), InputBox(due, 12)),
               FormRow(Tr("Workdays"), hbox(std::move(day_elems))),
               FormRow("", skip->Render()),
               FormRow(Tr("Holiday country"), country->Render()),
               ErrorLine(),
               separatorLight(),
               Buttons(ok, cancel),
           }) |
           size(WIDTH, EQUAL, 64);
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
    error_ = Tr("Invalid planned hours (hours, or H:MM)");
    return;
  }
  spec.total_minutes = *total;
  const auto due = ParseDate(Trim(due_));
  if (!due) {
    error_ = Tr("Due date must be YYYY-MM-DD");
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
  s_.Info(task_id_.empty() ? Tr("Task created: {name}", {{"name", spec.name}})
                           : Tr("Task modified: {name}", {{"name", spec.name}}));
  Close();
}

// ---------------------------------------------------------------- FixedDialog

namespace {
constexpr std::array<Repeat, 4> kRepeats = {Repeat::kDaily, Repeat::kWeekly, Repeat::kBiweekly, Repeat::kMonthly};
}  // namespace

FixedDialog::FixedDialog(AppState& s) : s_(s) {
  for (Repeat r : kRepeats) repeat_names_.push_back(RepeatName(r));
  auto title = Input(&title_, Tr("Required"), Field(&title_));
  auto start = Input(&start_, "HH:MM", Field(&start_));
  auto end = Input(&end_, "HH:MM", Field(&end_));
  auto repeat = Toggle(&repeat_names_, &repeat_index_);
  Components day_boxes = WeekdayBoxes(weekdays_);
  auto days = Container::Horizontal(day_boxes);
  auto from = Input(&from_, "YYYY-MM-DD", Field(&from_));
  auto until = Input(&until_, Tr("Optional: repeat forever"), Field(&until_));
  auto note = Input(&note_, "", Field(&note_));
  auto ok = Button(Tr("OK"), [this] { Submit(); }, SmallButton());
  auto cancel = Button(Tr("Cancel"), [this] { Close(); }, SmallButton());
  auto controls = Container::Vertical(
      {title, start, end, repeat, days, from, until, note, Container::Horizontal({ok, cancel})});

  Build(Tr("Fixed schedule"), controls, [=, this] {
    const Repeat r = kRepeats[static_cast<size_t>(repeat_index_)];
    const bool uses_days = r == Repeat::kWeekly || r == Repeat::kBiweekly;
    Elements day_elems;
    for (const Component& c : day_boxes) day_elems.push_back(c->Render() | size(WIDTH, EQUAL, 7));
    std::string hint = Tr("   Biweekly: every other week, counted from the week of the start date");
    if (r == Repeat::kMonthly) hint = Tr("   Monthly: on the start date's day; months without that day are skipped");
    if (r == Repeat::kDaily || r == Repeat::kWeekly) hint = Tr("   Saved in data/fixed/; deleting removes all dates");
    return vbox({
               text(Tr("New fixed schedule")) | dim,
               separatorLight(),
               FormRow(Tr("Work content"), InputBox(title, 36)),
               FormRow(Tr("Start time"), InputBox(start, 8)),
               FormRow(Tr("End time"), InputBox(end, 8)),
               FormRow(Tr("Repeat"), repeat->Render()),
               FormRow(Tr("Days"), hbox(std::move(day_elems))) | (uses_days ? nothing : dim),
               FormRow(Tr("Start date"), InputBox(from, 12)),
               FormRow(Tr("End date"), InputBox(until, 28)),
               FormRow(Tr("Note"), InputBox(note, 36)),
               text(hint) | dim,
               ErrorLine(),
               separatorLight(),
               Buttons(ok, cancel),
           }) |
           size(WIDTH, EQUAL, 72);
  }, {title, start, end, repeat, days, from, until, note, ok, cancel});
}

void FixedDialog::Open(Date d) {
  title_.clear(), start_.clear(), end_.clear(), until_.clear(), note_.clear();
  from_ = FormatDate(d);
  repeat_index_ = 1;  // 每周
  weekdays_ = {};
  weekdays_[static_cast<size_t>(IsoWeekday(d) - 1)] = true;
  Show();
}

void FixedDialog::Submit() {
  FixedSchedule f;
  f.title = std::string(Trim(title_));
  f.start = std::string(Trim(start_));
  f.end = std::string(Trim(end_));
  f.note = std::string(Trim(note_));
  f.repeat = kRepeats[static_cast<size_t>(repeat_index_)];
  f.weekdays = weekdays_;
  const auto from = ParseDate(Trim(from_));
  if (!from) {
    error_ = Tr("Start date must be YYYY-MM-DD");
    return;
  }
  f.from = *from;
  if (const std::string_view u = Trim(until_); !u.empty()) {
    f.until = ParseDate(u);
    if (!f.until) {
      error_ = Tr("End date must be YYYY-MM-DD, or left empty");
      return;
    }
  }
  if (const Result r = s_.AddFixed(f); !r) {
    error_ = r.error();
    return;
  }
  s_.Info(Tr("Fixed schedule added: {title} ({repeat})", {{"title", f.title}, {"repeat", RepeatText(f)}}));
  Close();
}

// ---------------------------------------------------------------- ProgressDialog

ProgressDialog::ProgressDialog(AppState& s) : s_(s) {
  auto minutes = Input(&minutes_, Tr("e.g. 1:30 / 1.5 / 90m"), Field(&minutes_, [this] { Submit(); }));
  auto note = Input(&note_, Tr("What did you do today?"), Field(&note_, [this] { Submit(); }));
  auto ok = Button(Tr("OK"), [this] { Submit(); }, SmallButton());
  auto cancel = Button(Tr("Cancel"), [this] { Close(); }, SmallButton());
  auto controls = Container::Vertical({minutes, note, Container::Horizontal({ok, cancel})});

  Build(Tr("Log progress"), controls, [=, this] {
    const TaskEntry* e = s_.FindTask(task_id_);
    std::string plan = "-";
    if (e) {
      if (const auto it = e->plan.daily.find(date_); it != e->plan.daily.end())
        plan = FormatMinutes(it->second);
    }
    return vbox({
               text((e ? e->task.name + " " + IdTag(task_id_) : std::string()) + "  " + FormatDate(date_)) | bold,
               text(Tr("Planned for the day: {plan}", {{"plan", plan}})) | dim,
               separatorLight(),
               FormRow(Tr("Actual time"), InputBox(minutes, 20)),
               FormRow(Tr("Progress note"), InputBox(note, 36)),
               text(Tr("   Clear both fields to delete the day's record")) | dim,
               ErrorLine(),
               separatorLight(),
               Buttons(ok, cancel),
           }) |
           size(WIDTH, EQUAL, 60);
  }, {minutes, note, ok, cancel});
}

void ProgressDialog::Open(const std::string& task_id, Date d) {
  const TaskEntry* e = s_.FindTask(task_id);
  if (!e) return;
  if (d > s_.today()) {
    s_.Error(Tr("Future dates are locked"));
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
    error_ = Tr("Time must be like 1:30 / 1.5 / 90m");
    return;
  }
  const Result r = s_.SetProgress(task_id_, date_, {*minutes, std::string(Trim(note_))});
  if (!r) {
    error_ = r.error();
    return;
  }
  s_.Info(Tr("Progress saved for {date}", {{"date", ShortDate(date_)}}));
  Close();
}

// ---------------------------------------------------------------- AboutDialog

AboutDialog::AboutDialog() {
  auto close = Button(Tr("Close"), [this] { Close(); }, SmallButton());
  Build(Tr("About"), close, [close] {
    return vbox({
               LogoBanner() | hcenter,
               separatorLight(),
               WrapText(Tr("This program was written by Claude (Anthropic) based on requirements and design discussions with ZXHAN.")),
               text(""),
               text(Tr("Model: {model}", {{"model", kModel}})),
               text(Tr("Built with FTXUI 7 and nlohmann/json, C++23")) | dim,
               separatorLight(),
               hbox({filler(), close->Render()}),
           }) |
           size(WIDTH, EQUAL, 56);
  }, {close});
}

// ---------------------------------------------------------------- HolidayDialog

HolidayDialog::HolidayDialog(AppState& s) : s_(s), country_names_(CountryNames()) {
  auto country = Toggle(&country_names_, &country_index_);
  auto year = Input(&year_, "YYYY", Field(&year_));
  auto gen = Button(Tr("Generate & copy prompt"), [this] { MakePrompt(); }, SmallButton());
  auto path = Input(&path_, Tr("Path of the file holding the AI's JSON"), Field(&path_, [this] { Import(); }));
  auto import_btn = Button(Tr("Import"), [this] { Import(); }, SmallButton());
  auto close = Button(Tr("Close"), [this] { Close(); }, SmallButton());
  auto controls = Container::Vertical({country, year, gen, path, Container::Horizontal({import_btn, close})});

  Build(Tr("Import holidays"), controls, [=, this] {
    Elements prompt_lines;
    std::istringstream ss(prompt_);
    for (std::string line; std::getline(ss, line);) prompt_lines.push_back(text(line));

    std::string loaded;
    for (const auto& [code, y] : s_.holidays().Loaded())
      loaded += (loaded.empty() ? "" : "  ") + code + " " + std::to_string(y);

    Elements body = {
        text(Tr("Imported: {list}", {{"list", loaded.empty() ? Tr("none") : loaded}})) | dim,
        separatorLight(),
        FormRow(Tr("Country"), country->Render()),
        FormRow(Tr("Year"), InputBox(year, 8)),
        FormRow(Tr("① Prompt"), gen->Render()),
    };
    if (!prompt_lines.empty())
      body.push_back(vbox(std::move(prompt_lines)) | dim | yframe | size(HEIGHT, LESS_THAN, 12) | border);
    body.push_back(FormRow(Tr("② File path"), InputBox(path, 48)));
    body.push_back(text(Tr("   Existing data for the same country and year will be replaced")) | dim);
    if (!info_.empty()) body.push_back(WrapText(info_) | color(Color::Green));
    body.push_back(ErrorLine());
    body.push_back(separatorLight());
    body.push_back(Buttons(import_btn, close));
    return vbox(std::move(body)) | size(WIDTH, EQUAL, 72);
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
    error_ = Tr("Year must be an integer from 1900 to 2100");
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
  info_ = CopyToClipboard(prompt_) ? Tr("Prompt copied to the clipboard; paste it to an AI")
                                   : Tr("Copy to clipboard failed; please copy the prompt below manually");
}

void HolidayDialog::Import() {
  const auto year = ParsedYear();
  if (!year) return;
  // 去掉“复制为路径”带来的引号
  std::string_view p = Trim(path_);
  if (p.size() >= 2 && p.front() == '"' && p.back() == '"') p = p.substr(1, p.size() - 2);
  if (p.empty()) {
    error_ = Tr("Please enter a file path");
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
  info_ = Tr("Imported {country} holidays for {year}", {{"country", CountryName(Country())}, {"year", std::to_string(*year)}});
  s_.Info(info_);
}

}  // namespace cal::ui
