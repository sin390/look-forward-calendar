#pragma once

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "app/app_state.h"
#include "ui/dialogs.h"

namespace cal::ui {

// 组装各面板、对话框与全局快捷键
class App {
 public:
  App(AppState& state, std::function<void()> on_quit);
  ftxui::Component root() const { return root_; }
  // 切换了语言，需要重建界面
  bool restart_requested() const { return restart_; }

 private:
  AppState& state_;
  EventDialog event_dialog_;
  FixedDialog fixed_dialog_;
  TaskDialog task_dialog_;
  ProgressDialog progress_dialog_;
  HolidayDialog holiday_dialog_;
  AboutDialog about_dialog_;
  int view_ = 0;  // 主界面 / 任务总览 / 提醒列表
  int pane_ = 0;  // 主界面中：月历 / 侧边栏
  bool restart_ = false;
  ftxui::Component root_;
};

// 进入交互式事件循环
int RunApp(AppState& state);

// 开发用：依次送入按键后，把界面渲染为文本
std::string RenderSnapshot(AppState& state, int width, int height,
                           const std::vector<ftxui::Event>& keys);

}  // namespace cal::ui
