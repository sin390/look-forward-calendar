cmake_minimum_required(VERSION 3.20)
project(LFCalendar VERSION 1.6.1 LANGUAGES CXX)  # 与 src/version.h 保持一致

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
  src/core/i18n.cpp
  src/model/day.cpp
  src/model/task.cpp
  src/model/fixed.cpp
  src/model/preference.cpp
  src/store/fs_util.cpp
  src/store/repository.cpp
  src/app/app_state.cpp
  src/platform/platform.cpp
)
target_include_directories(calendar_core PUBLIC src)
target_link_libraries(calendar_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(LookForward
  src/main.cpp
  src/ui/app.cpp
  src/ui/month_view.cpp
  src/ui/day_panel.cpp
  src/ui/overview_screen.cpp
  src/ui/reminder_screen.cpp
  src/ui/dialogs.cpp
  src/ui/widgets.cpp
  src/ui/logo.cpp
)
target_link_libraries(LookForward PRIVATE
  calendar_core
  ftxui::screen
  ftxui::dom
  ftxui::component
)

if(WIN32)
  # exe 图标（res/LookForward.ico 由 tools/make_icon.py 生成）
  enable_language(RC)
  target_sources(LookForward PRIVATE res/app.rc)
endif()

if(MINGW)
  # 静态链接运行时，生成的 exe 可脱离 MinGW 环境直接运行
  target_link_options(LookForward PRIVATE -static)
  # 发布版去掉符号，减小 exe 体积
  target_link_options(LookForward PRIVATE $<$<CONFIG:Release>:-s>)
endif()

# 语言文件复制到 exe 旁边的 language/
add_custom_command(TARGET LookForward POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/language $<TARGET_FILE_DIR:LookForward>/language
)

enable_testing()
add_executable(calendar_tests tests/test_main.cpp)
target_link_libraries(calendar_tests PRIVATE calendar_core)
# 单元测试检查语言文件是否完整
target_compile_definitions(calendar_tests PRIVATE LANGUAGE_DIR="${CMAKE_SOURCE_DIR}/language")
if(MINGW)
  target_link_options(calendar_tests PRIVATE -static)
endif()
add_test(NAME calendar_tests COMMAND calendar_tests)
