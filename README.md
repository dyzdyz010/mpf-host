# MPF Host

Qt Modular Plugin Framework - Host Application

## 概述

MPF Host 是插件框架的宿主应用，负责：
- 加载和管理插件
- 实现核心服务 (EventBusService, NavigationService, MenuService, etc.)
- 提供 QML Shell

## 依赖

- Qt 6.8+ (Core, Gui, Qml, Quick)
- mpf-sdk
- mpf-ui-components (可选)

## 构建

```bash
export QT_DIR=/path/to/qt6
export MPF_SDK=/path/to/mpf-sdk

cmake -B build -G Ninja \
    -DCMAKE_PREFIX_PATH="$QT_DIR;$MPF_SDK" \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 运行

```bash
# 配置插件路径
vim build/config/paths.json

# 运行
./build/bin/mpf-host
```

## 配置

`config/paths.json`:

```json
{
  "pluginPath": "../plugins",
  "extraQmlPaths": [
    "${MPF_SDK}/qml"
  ]
}
```

支持环境变量：`${VAR}` (跨平台) 和 `%VAR%` (Windows)

## 测试

```bash
cd tests/build
./test_event_bus              # EventBus 单元测试（27 个）
./test_plugin_dependencies    # 插件依赖测试（10 个）
```

## 许可证

MIT License
