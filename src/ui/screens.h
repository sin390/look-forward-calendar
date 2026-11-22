#pragma once

#include <functional>

#include <ftxui/component/component_base.hpp>

#include "app/app_state.h"

namespace cal::ui {

// 子界面：整屏显示，由 App 切换；返回主界面时调用 on_back。

// 任务总览：1 进行中（截止日期排序） 2 全部（创建日期排序） 3 已完成（最终截止日期排序）
// Enter 查看选中任务的全部日志，d 删除选中任务（需按 y 确认）
ftxui::Component OverviewScreen(AppState& state, std::function<void()> on_back);

// 提醒列表：处于提醒期间的事件，按发生时间排序；Enter 跳到该日期
ftxui::Component ReminderScreen(AppState& state, std::function<void(Date)> jump_to,
                                std::function<void()> on_back);

// 日记：显示 / 编辑选中日期的日记，右侧列出该月有日记的日期。
// ←→ 上一篇 / 下一篇日记（可跨月），[ ] 切换月份，↑↓ 滚动正文，Enter 编辑，编辑中 Esc 保存
ftxui::Component DiaryScreen(AppState& state, std::function<void()> on_back);

}  // namespace cal::ui
