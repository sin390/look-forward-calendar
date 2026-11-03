#include "ui/app.h"

#include <atomic>
#include <memory>
#include <chrono>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "ui/day_panel.h"
#include "ui/dialogs.h"
#include "ui/month_view.h"
#include "ui/screens.h"
#include "ui/widgets.h"

using namespace ftxui;

namespace cal::ui {

using ftxui::Event;  // 键盘事件；数据模型的事件写作 cal::Event

namespace {

enum Pane { kMonthPane = 0, kDayPane = 1 };
enum View { kMainView = 0, kOverviewView = 1, kReminderView = 2 };

Element StatusBar(AppState& s, int view, int pane) {
  std::vector<std::string> hints;
  if (view == kOverviewView)
    hints = {Tr("1 Active  2 All  3 Completed  ↑↓/jk select  Enter view log  d delete task  Esc/q back")};
  else if (view == kReminderView)
    hints = {Tr("↑↓/jk select  Enter go to date  Esc/q/m back")};
  else if (pane == kMonthPane)
    hints = {Tr("Move      arrows/hjkl date   [ ] month   t today   Tab sidebar"),
             Tr("Actions   a event  f fixed schedule  n task  o overview  m reminders  r refresh  v version  q quit"),
             Tr("Settings  c switch holidays  g language  i import holidays")};
  else
    hints = {Tr("arrows/hjkl select  Enter press button  Tab/Esc back to calendar")};

  Elements lines;
  for (const std::string& h : hints) lines.push_back(text(h) | dim);
  auto msg = WrapText(s.message());
  if (s.message_is_error()) msg |= color(Color::Red);
  Element reminder = text("");
  if (const size_t n = s.ActiveReminders(NowLocal()).size(); n > 0)
    reminder = text(Tr(" ⏰ Reminders: {n} (m to view) ", {{"n", std::to_string(n)}})) | bgcolor(Color::Yellow) | color(Color::Black);
  lines.push_back(hbox({msg | flex, reminder}));
  return vbox(std::move(lines));
}

}  // namespace

App::App(AppState& state, std::function<void()> on_quit)
    : state_(state), event_dialog_(state), fixed_dialog_(state), task_dialog_(state), progress_dialog_(state), holiday_dialog_(state) {
  auto month = MonthView(state_);
  auto day = DayPanel(state_, {
                                  .new_event = [this](Date d) { event_dialog_.OpenAdd(d); },
                                  .edit_event = [this](Date d, size_t i) { event_dialog_.OpenEdit(d, i); },
                                  .new_fixed = [this](Date d) { fixed_dialog_.Open(d); },
                                  .new_task = [this] { task_dialog_.OpenCreate(); },
                                  .edit_task = [this](const std::string& id) { task_dialog_.OpenModify(id); },
                                  .edit_progress = [this](const std::string& id, Date d) { progress_dialog_.Open(id, d); },
                              });
  auto panes = Container::Horizontal({month, day}, &pane_);
  auto main_view = Renderer(panes, [month, day] {
    return hbox({month->Render() | flex, day->Render() | size(WIDTH, EQUAL, 44)});
  });
  const auto back_to_main = [this] { view_ = kMainView; };
  auto overview = OverviewScreen(state_, back_to_main);
  auto reminders = ReminderScreen(
      state_,
      [this](Date d) {
        state_.Select(d);
        view_ = kMainView;
        pane_ = kMonthPane;
      },
      back_to_main);
  auto views = Container::Tab({main_view, overview, reminders}, &view_);

  root_ = Renderer(views, [this, views] {
    return vbox({views->Render() | flex, StatusBar(state_, view_, pane_)});
  });

  // 全局快捷键只在主界面的月历获得焦点时生效，避免和输入框冲突
  root_ |= CatchEvent([this, on_quit = std::move(on_quit)](Event e) {
    if (e == Event::Custom) {  // 定时刷新（提醒列表随时间变化、跨零点）
      state_.Tick();
      return true;
    }
    if (view_ != kMainView) return false;  // 子界面自行处理按键（含返回）
    if (e == Event::Tab || e == Event::TabReverse) {
      pane_ = pane_ == kMonthPane ? kDayPane : kMonthPane;
      return true;
    }
    if (pane_ == kDayPane) {
      if (e == Event::Escape) {
        pane_ = kMonthPane;
        return true;
      }
      return false;
    }
    if (e == Event::Character('q')) {
      on_quit();
      return true;
    }
    if (e == Event::Character('a')) event_dialog_.OpenAdd(state_.selected());
    else if (e == Event::Character('f')) fixed_dialog_.Open(state_.selected());
    else if (e == Event::Character('n')) task_dialog_.OpenCreate();
    else if (e == Event::Character('i')) holiday_dialog_.Open();
    else if (e == Event::Character('c')) state_.CycleDisplayCountry();
    else if (e == Event::Character('g')) {
      // 按钮、标签等文字在构造时生成，切换语言后退出事件循环，由 RunApp 重建界面
      if (state_.CycleLanguage()) {
        restart_ = true;
        on_quit();
      }
    }
    else if (e == Event::Character('r')) state_.Reload();
    else if (e == Event::Character('o')) view_ = kOverviewView;
    else if (e == Event::Character('m')) view_ = kReminderView;
    else if (e == Event::Character('v')) about_dialog_.Open();
    else return false;  // 其余按键交给月历（方向键、hjkl、[ ]、t）
    return true;
  });

  root_ |= Modal(event_dialog_.component(), event_dialog_.shown());
  root_ |= Modal(fixed_dialog_.component(), fixed_dialog_.shown());
  root_ |= Modal(task_dialog_.component(), task_dialog_.shown());
  root_ |= Modal(progress_dialog_.component(), progress_dialog_.shown());
  root_ |= Modal(holiday_dialog_.component(), holiday_dialog_.shown());
  root_ |= Modal(about_dialog_.component(), about_dialog_.shown());
}

int RunApp(AppState& state) {
  auto screen = ScreenInteractive::Fullscreen();

  // 每 30 秒唤醒一次界面
  std::atomic<bool> running = true;
  std::thread ticker([&] {
    using namespace std::chrono_literals;
    while (running) {
      for (int i = 0; i < 300 && running; ++i) std::this_thread::sleep_for(100ms);
      if (running) screen.PostEvent(Event::Custom);
    }
  });
  // 切换语言后重建整个界面，数据状态（AppState）保留
  for (bool restart = true; restart;) {
    App app(state, screen.ExitLoopClosure());
    screen.Loop(app.root());
    restart = app.restart_requested();
  }
  running = false;
  ticker.join();
  return 0;
}

std::string RenderSnapshot(AppState& state, int width, int height, const std::vector<Event>& keys) {
  auto app = std::make_unique<App>(state, [] {});
  for (const Event& e : keys) {
    app->root()->OnEvent(e);
    if (app->restart_requested()) app = std::make_unique<App>(state, [] {});
  }
  auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
  Render(screen, app->root()->Render());
  return screen.ToString();
}

}  // namespace cal::ui
