cmake_minimum_required(VERSION 3.20)
project(calendar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v7.0.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ftxui)

add_executable(calendar src/main.cpp)
target_link_libraries(calendar PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

if(MINGW)
  # 静态链接运行时，生成的 exe 可脱离 MinGW 环境直接运行
  target_link_options(calendar PRIVATE -static)
endif()
