#pragma once

#include <array>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>

#include "app/app_state.h"

namespace cal::ui {

// 对话框基类：配合 ftxui::Modal 使用，Esc 关闭
class Dialog {
 public:
  virtual ~Dialog() = default;
  ftxui::Component component() const { return component_; }
  const bool* shown() const { return &shown_; }

 protected:
  // 由子类在构造函数中调用：controls 为可交互控件，body 负责绘制，
  // tab_order 为 Tab 键依次聚焦的控件
  void Build(std::string title, ftxui::Component controls, std::function<ftxui::Element()> body,
             ftxui::Components tab_order);
  void Show();
  void Close() { shown_ = false; }
  ftxui::Element ErrorLine() const;
  // 单行输入框选项；打开对话框时光标自动移到内容末尾
  ftxui::InputOption Field(std::string* content, std::function<void()> on_enter = {});

  std::string error_;

 private:
  void MoveFocus(int dir);

  bool shown_ = false;
  ftxui::Component component_;
  ftxui::Components tab_order_;
  std::deque<std::pair<std::string*, int>> cursors_;  // deque 保证 int 地址稳定
};

// 新建 / 编辑瞬间型事件
class EventDialog : public Dialog {
 public:
  explicit EventDialog(AppState& s);
  void OpenAdd(Date d);
  void OpenEdit(Date d, size_t index);

 private:
  void Submit();

  AppState& s_;
  Date date_{};
  std::optional<size_t> index_;  // 编辑时为事件下标
  std::string title_, start_, end_, note_;
  bool remind_ = false;
  std::string lead_;
};

// 新建 / 修改长期任务
class TaskDialog : public Dialog {
 public:
  explicit TaskDialog(AppState& s);
  void OpenCreate();
  void OpenModify(const std::string& id);

 private:
  void Submit();

  AppState& s_;
  std::string task_id_;  // 空表示新建
  std::string name_, total_, due_;
  std::array<bool, 7> weekdays_{};
  bool skip_holiday_ = true;
  int config_index_ = 0;  // 节假日配置，0 起
  std::vector<std::string> config_names_;
};

// 新建固定日程：工作内容、时间段、重复方式（默认每周）、起止日期
class FixedDialog : public Dialog {
 public:
  explicit FixedDialog(AppState& s);
  void Open(Date d);  // 以 d 为开始日期，默认每周的这一天

 private:
  void Submit();

  AppState& s_;
  std::string title_, start_, end_, from_, until_, note_;
  int repeat_index_ = 1;
  std::vector<std::string> repeat_names_;
  std::array<bool, 7> weekdays_{};
};

// 填写某任务某天的实际进行情况（时长 + 内容），覆盖写入
class ProgressDialog : public Dialog {
 public:
  explicit ProgressDialog(AppState& s);
  void Open(const std::string& task_id, Date d);

 private:
  void Submit();

  AppState& s_;
  std::string task_id_;
  Date date_{};
  std::string minutes_, note_;
};

// 修改标语（没有其他消息时显示在状态栏）
class SloganDialog : public Dialog {
 public:
  explicit SloganDialog(AppState& s);
  void Open();

 private:
  void Submit();

  AppState& s_;
  std::string slogan_;
};

// 版本信息
class AboutDialog : public Dialog {
 public:
  AboutDialog();
  void Open() { Show(); }
};

// 节假日导入：选择节假日配置、输入国家（只用于提示词和侧边栏显示）和年份，
// 生成提示词并复制，输入文件路径后校验导入
class HolidayDialog : public Dialog {
 public:
  explicit HolidayDialog(AppState& s);
  void Open();

 private:
  int Config() const { return config_index_ + 1; }
  std::optional<int> ParsedYear();
  void MakePrompt();
  void Import();

  AppState& s_;
  int config_index_ = 0;  // 0 起
  std::vector<std::string> config_names_;
  std::string country_, year_, path_, prompt_, info_;
};

}  // namespace cal::ui
