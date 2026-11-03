#pragma once

#include <ftxui/dom/elements.hpp>

namespace cal::ui {

// 像素风 logo：日历页上升起的太阳，下方日期格中高亮的一天是“期待的那一天”。
// 用半格字符 ▀ 绘制，每个字符格显示上下两个像素
ftxui::Element Logo();
// logo + 程序名 + 版本，用于关于窗口
ftxui::Element LogoBanner();

}  // namespace cal::ui
