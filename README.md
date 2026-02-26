# MPF Host

Qt Modular Plugin Framework - 宿主应用程序

## 概述

MPF Host 是插件框架的宿主应用，负责：
- 加载和管理插件（通过 `PluginManager`）
- 实现核心服务（Navigation, Menu, Theme, Settings, EventBus）
- 提供 QML Shell（Main.qml + SideMenu）
- 运行时自动读取 `dev.json` 发现源码构建的组件

## 依赖

- Qt 6.8+（Core, Gui, Qml, Quick）
- MPF foundation-sdk
- MPF ui-components（运行时通过 QML import 加载，不链接）

## 构建

```bash
# 1. 确保已安装 mpf-dev 和 SDK
mpf-dev setup

# 2. 初始化项目
mpf-dev init

# 3. 构建
cmake --preset dev
cmake --build build
```

## 运行

```bash
# 方式一：通过 mpf-dev（自动注入开发路径）
mpf-dev run

# 方式二：直接运行（Host 自动读取 dev.json）
./build/bin/mpf-host
```

## 源码开发注册

```bash
# 注册 Host 构建输出，使其他组件的 mpf-dev run 使用源码版 Host
mpf-dev link host ./build
```

## dev.json 自动发现

Host 启动时自动读取 `~/.mpf-sdk/dev.json`，将已注册组件的路径添加到搜索路径中：
- 插件 DLL：添加到插件搜索目录
- QML 模块：添加到 QML import 路径
- Windows 上自动更新 `PATH` 确保 DLL 依赖可发现

这意味着在 Qt Creator 中直接调试时，**无需手动配置任何环境变量**，Host 会自动找到所有源码构建的组件。

## 核心服务

| 服务 | QML 名称 | 说明 |
|------|----------|------|
| NavigationService | `Navigation` | 路由管理 |
| MenuService | `AppMenu` | 侧边栏菜单 |
| ThemeService | `Theme` | 主题切换（Light/Dark） |
| SettingsService | `Settings` | 配置持久化 |
| EventBusService | `EventBus` | 跨插件事件总线 |

## 测试

```bash
cd tests/build
./test_event_bus              # EventBus 单元测试
./test_plugin_dependencies    # 插件依赖测试
```

## License

MIT
