#pragma once

// UI 各处共用的小部件与样式。改外观时优先改这里。

#include <functional>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include "app/app_state.h"

namespace cal::ui {

// 始终显示为 [标签]，聚焦时反色
ftxui::ButtonOption SmallButton();
// 单行输入框，回车时调用 on_enter
ftxui::InputOption LineInput(std::function<void()> on_enter = {});

// 输入框外观：固定宽度，带下划线背景
ftxui::Element InputBox(ftxui::Component input, int width);
// 表单一行：左侧标签 + 右侧控件
ftxui::Element FormRow(const std::string& label, ftxui::Element field);
// 面板外框，聚焦时高亮标题
ftxui::Element Panel(const std::string& title, ftxui::Element body, bool focused);

std::string WeekdayName(int iso_weekday);  // 1 -> "周一"
std::string IdTag(const std::string& task_id);  // "(ID:a3f9)"
std::string ShortDate(Date d);             // "11月2日"

// 把操作结果写入状态栏，成功时显示 ok_msg
void Report(AppState& s, const Result& r, const std::string& ok_msg);

}  // namespace cal::ui
