#pragma once

#include <ftxui/component/component_base.hpp>

#include "app/app_state.h"

namespace cal::ui {

// 月历网格：方向键选日期，PgUp/PgDn 翻月，Home 回到今天
ftxui::Component MonthView(AppState& state);

}  // namespace cal::ui
