Phase 1 已实现（工程骨架 + 可加载插件 + 配置页 + 设置测试）。

落地内容
- CMake 工程：根 CMake + src/ 子目录 + autotests/，可用 Ninja 构建。
- 插件目标：kcoreaddons_add_plugin(kateaiinlinecompletion INSTALL_NAMESPACE "ktexteditor")，产物位于 build/bin/ktexteditor/kateaiinlinecompletion.so。
- 插件入口与视图：
  - src/plugin/KateAiInlineCompletionPlugin.*（加载/保存 CompletionSettings，提供 configPage）
  - src/plugin/KateAiInlineCompletionPluginView.*（MainWindow 级实例，使用 MainWindow::showMessage 输出加载与激活提示）
  - src/plugin/kateaiinlinecompletion.json（KPlugin 元数据）
- 设置与凭据：
  - src/settings/CompletionSettings.*（默认值、validated()、KConfigGroup load/save）
  - src/settings/KWalletSecretStore.*（同步打开 KWallet::Wallet，读写 Password entry）
  - src/settings/KateAiConfigPage.*（实现 apply/reset/defaults，changed() 信号，支持保存/清除 API key）
- 单元测试：autotests/CompletionSettingsTest.cpp（边界夹取 + KConfig 往返）。

验证证据
- cmake -B build -S . -GNinja
- cmake --build build
- ctest --test-dir build --output-on-failure
- 测试结果：2/2 passed。

实现偏离
- 采用 find_package(KF6Config ...) 提供 KF6::ConfigCore 目标，匹配系统 CMake 包命名。