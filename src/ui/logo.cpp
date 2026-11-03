#include "ui/logo.h"

#include <array>
#include <optional>
#include <string_view>

#include "version.h"

using namespace ftxui;

namespace cal::ui {

namespace {

// 16×14 像素，行数必须为偶数
constexpr std::array<std::string_view, 14> kPixels = {
    "...K........K...",  // 装订环
    ".RRKRRRRRRRRKRR.",  // 红色页眉
    ".RRRRRRRRRRRRRR.",
    ".WWWWWWWWWWWWWW.",
    ".WWWWWYYYYWWWWW.",  // 升起的太阳
    ".WWWYYYYYYYYWWW.",
    ".WWYYYYYYYYYYWW.",
    ".WWYYYYYYYYYYWW.",
    ".BBBBBBBBBBBBBB.",  // 地平线
    ".WWWWWWWWWWWWWW.",
    ".WGGWGGWGGWOOWW.",  // 日期格，橙色是期待的那一天
    ".WGGWGGWGGWOOWW.",
    ".WWWWWWWWWWWWWW.",
    "..DDDDDDDDDDDDDD",  // 阴影
};

std::optional<Color> PixelColor(char c) {
  switch (c) {
    case 'K': return Color::RGB(70, 70, 80);
    case 'R': return Color::RGB(220, 60, 60);
    case 'W': return Color::RGB(240, 240, 232);
    case 'Y': return Color::RGB(255, 190, 40);
    case 'B': return Color::RGB(70, 150, 230);
    case 'G': return Color::RGB(170, 170, 170);
    case 'O': return Color::RGB(255, 120, 40);
    case 'D': return Color::RGB(100, 100, 100);
    default: return std::nullopt;  // 透明
  }
}

Element Cell(char top, char bottom) {
  const auto t = PixelColor(top);
  const auto b = PixelColor(bottom);
  if (t && b) return text("▀") | color(*t) | bgcolor(*b);
  if (t) return text("▀") | color(*t);
  if (b) return text("▄") | color(*b);
  return text(" ");
}

}  // namespace

Element Logo() {
  Elements rows;
  for (size_t y = 0; y + 1 < kPixels.size(); y += 2) {
    Elements cells;
    for (size_t x = 0; x < kPixels[y].size(); ++x) cells.push_back(Cell(kPixels[y][x], kPixels[y + 1][x]));
    rows.push_back(hbox(std::move(cells)));
  }
  return vbox(std::move(rows));
}

Element LogoBanner() {
  return hbox({
      Logo(),
      text("  "),
      vbox({
          filler(),
          text(kAppName) | bold | color(Color::RGB(255, 190, 40)),
          text(kAppFullName) | dim,
          text(""),
          text(std::string("v") + kVersion + "  " + kReleaseDate) | dim,
          filler(),
      }),
  });
}

}  // namespace cal::ui
