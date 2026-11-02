#pragma once

#include <functional>

#include <ftxui/component/component_base.hpp>

#include "app/app_state.h"

namespace cal::ui {

// 子界面：整屏显示，由 App 切换。Esc / q 返回主界面由 App 处理。

// 任务总览：1 进行中（截止日期排序） 2 全部（创建日期排序） 3 已完成（最终截止日期排序）
ftxui::Component OverviewScreen(AppState& state);

// 提醒列表：处于提醒期间的事件，按发生时间排序；Enter 跳到该日期
ftxui::Component ReminderScreen(AppState& state, std::function<void(Date)> jump_to);

}  // namespace cal::ui
