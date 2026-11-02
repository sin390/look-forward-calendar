cmake_minimum_required(VERSION 3.20)
project(calendar VERSION 1.0.0 LANGUAGES CXX)  # 与 src/version.h 保持一致

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v7.0.3
  GIT_SHALLOW    TRUE
)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ftxui json)

# 不依赖 UI 的部分：日期/农历/节假日、数据模型、存储、应用状态
add_library(calendar_core STATIC
  src/core/date.cpp
  src/core/lunar.cpp
  src/core/holiday.cpp
  src/model/day.cpp
  src/model/task.cpp
  src/store/fs_util.cpp
  src/store/repository.cpp
  src/app/app_state.cpp
  src/platform/platform.cpp
)
target_include_directories(calendar_core PUBLIC src)
target_link_libraries(calendar_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(calendar
  src/main.cpp
  src/ui/app.cpp
  src/ui/month_view.cpp
  src/ui/day_panel.cpp
  src/ui/overview_screen.cpp
  src/ui/reminder_screen.cpp
  src/ui/dialogs.cpp
  src/ui/widgets.cpp
)
target_link_libraries(calendar PRIVATE
  calendar_core
  ftxui::screen
  ftxui::dom
  ftxui::component
)

if(MINGW)
  # 静态链接运行时，生成的 exe 可脱离 MinGW 环境直接运行
  target_link_options(calendar PRIVATE -static)
endif()

enable_testing()
add_executable(calendar_tests tests/test_main.cpp)
target_link_libraries(calendar_tests PRIVATE calendar_core)
if(MINGW)
  target_link_options(calendar_tests PRIVATE -static)
endif()
add_test(NAME calendar_tests COMMAND calendar_tests)
