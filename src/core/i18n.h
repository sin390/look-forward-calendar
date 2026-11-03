#pragma once

#include <expected>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace cal {

// 界面文本的多语言支持。
//
// 程序中的固定文本以英文原文作为 key：Tr("Events")。
// language/<名称>.json 是 { "key": "译文" } 的平面对象；EN.json 的 key 与 value 相同，作为翻译模板。
// 当前语言中找不到的 key 直接显示 key 本身，即英文。
//
// 文本中的 {name} 是占位符，由 Tr 的第二个参数替换，译文可以调整占位符的位置：
//   Tr("Deleted fixed schedule: {title}", {{"title", f.title}})
//   CN.json: "Deleted fixed schedule: {title}": "已删除固定日程：{title}"
//
// 特殊的 key " "：词与词之间的分隔符（如星期简称 "Mo We"、时长 "1 h 30 min"），中文译为空字符串。
//
// tools/i18n.py 扫描源码中的 Tr("...") 和 N_("...") 生成 EN.json，并检查其他语言文件缺少的 key。

using Translations = std::map<std::string, std::string, std::less<>>;

std::expected<Translations, std::string> ParseTranslations(std::string_view json_text);
void SetTranslations(Translations t);  // 切换当前语言；空表示英文

std::string Tr(std::string_view key);
std::string Tr(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> args);

// 标记稍后才翻译的文本（常量表中的名称等），本身不做任何事，只供 tools/i18n.py 提取 key。
// 使用处再调用 Tr(...)
constexpr const char* N_(const char* key) { return key; }

}  // namespace cal
