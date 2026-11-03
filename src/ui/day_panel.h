#pragma once

#include <functional>
#include <string>

#include <ftxui/component/component_base.hpp>

#include "app/app_state.h"

namespace cal::ui {

// 侧边栏需要打开对话框时的回调，由 App 提供
struct DayPanelActions {
  std::function<void(Date)> new_event;
  std::function<void(Date, size_t)> edit_event;
  std::function<void(Date)> new_fixed;
  std::function<void()> new_task;
  std::function<void(const std::string& task_id)> edit_task;
  std::function<void(const std::string& task_id, Date)> edit_progress;
};

// 选中日期的详情：事件列表、固定日程、任务的计划量 / 实际量输入 / 状态按钮。
// 选中日期或数据变化时自动重建内部控件。
ftxui::Component DayPanel(AppState& state, DayPanelActions actions);

}  // namespace cal::ui
