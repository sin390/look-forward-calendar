#pragma once

#include <nlohmann/json.hpp>

namespace cal {

// 保持字段插入顺序，写出的文件更易读
using Json = nlohmann::ordered_json;

}  // namespace cal
