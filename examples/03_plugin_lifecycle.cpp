/**
 * =============================================================================
 * 样例 03: 插件生命周期 —— 发现、加载、初始化、启动、停止
 * =============================================================================
 *
 * 【这个样例展示了什么】
 * Host 如何发现、加载和管理插件的完整生命周期。
 * 这是理解 MPF 框架运行机制的关键。
 *
 * 【插件生命周期阶段】
 *
 *   ┌─────────────┐
 *   │  discover()  │  扫描目录，找到 .dll/.dylib/.so 文件
 *   └──────┬───────┘
 *          ▼
 *   ┌─────────────┐
 *   │   loadAll()  │  用 QPluginLoader 加载动态库
 *   └──────┬───────┘
 *          ▼
 *   ┌──────────────────┐
 *   │ initializeAll()  │  调用每个插件的 initialize(registry)
 *   └──────┬───────────┘  插件在此创建服务、注册 QML 类型
 *          ▼
 *   ┌─────────────┐
 *   │  startAll()  │  调用每个插件的 start()
 *   └──────┬───────┘  插件在此注册路由、菜单、加载数据
 *          ▼
 *   ┌─────────────┐
 *   │  运行中...   │  应用正常运行，插件提供服务
 *   └──────┬───────┘
 *          ▼
 *   ┌─────────────┐
 *   │  stopAll()   │  调用每个插件的 stop()
 *   └──────┬───────┘  插件在此保存数据、清理资源
 *          ▼
 *   ┌──────────────┐
 *   │ unloadAll()  │  卸载动态库
 *   └──────────────┘
 *
 * 【为什么分 initialize 和 start 两个阶段】
 * - initialize: 创建服务实例，注册 QML 类型（此时其他插件可能还未初始化）
 * - start: 使用其他插件的服务（此时所有插件都已初始化）
 *
 * 这种两阶段设计解决了插件间的循环依赖问题。
 * =============================================================================
 */

/**
 * 样例：插件发现流程
 *
 * PluginManager::discover() 扫描指定目录，
 * 找到所有符合 Qt 插件格式的动态库文件。
 */
void example_plugin_discovery()
{
    // Host 的 Application::loadPlugins() 中：
    //
    // m_pluginManager = std::make_unique<PluginManager>(m_registry.get(), this);
    //
    // 发现插件的搜索路径优先级：
    //
    // 1. 开发覆盖路径（m_extraPluginPaths）
    //    来源：MPF_PLUGIN_PATH 环境变量 或 dev.json 配置
    //    用途：开发时让源码编译的插件覆盖 SDK 中的二进制插件
    //
    //    for (const QString& path : m_extraPluginPaths) {
    //        m_pluginManager->discover(path);  // 先搜索开发路径
    //    }
    //
    // 2. 默认插件路径（m_pluginPath）
    //    通常是 SDK_ROOT/plugins 或 app/../plugins
    //
    //    m_pluginManager->discover(m_pluginPath);  // 再搜索默认路径
    //
    // discover() 做了什么：
    // - 遍历目录下所有文件
    // - 用 QPluginLoader 检查是否是有效的 Qt 插件
    // - 读取插件元数据（orders_plugin.json）
    // - 记录插件路径，但不加载
    //
    // 搜索的文件扩展名：
    // - Windows: *.dll
    // - macOS:   *.dylib
    // - Linux:   *.so
}

/**
 * 样例：插件加载和初始化流程
 *
 * loadAll() → initializeAll() → startAll()
 * 三步走，每一步都有严格的顺序。
 */
void example_plugin_loading()
{
    // =========================================================================
    // loadAll(): 加载所有已发现的插件
    // =========================================================================
    // 内部使用 QPluginLoader::load() 加载动态库
    // 加载成功后，通过 qobject_cast<IPlugin*> 获取插件实例
    //
    // if (m_pluginManager->loadAll()) {
    //     // 所有插件加载成功
    // }
    //
    // 加载顺序由 metadata 中的 priority 字段决定：
    // priority 越小越先加载
    // 例如：
    //   orders (priority: 10) → 先加载
    //   rules  (priority: 20) → 后加载

    // =========================================================================
    // initializeAll(): 初始化所有已加载的插件
    // =========================================================================
    // 按 priority 顺序调用每个插件的 initialize(registry)
    //
    // 插件在 initialize() 中应该做的事情：
    // 1. 保存 registry 引用
    // 2. 创建业务服务实例
    // 3. 注册 QML 类型（qmlRegisterSingletonInstance 等）
    // 4. 获取不依赖其他插件的系统服务
    //
    // if (m_pluginManager->initializeAll()) {
    //     // 所有插件初始化成功
    // }

    // =========================================================================
    // startAll(): 启动所有已初始化的插件
    // =========================================================================
    // 按 priority 顺序调用每个插件的 start()
    //
    // 插件在 start() 中应该做的事情：
    // 1. 注册导航路由（INavigation::registerRoute）
    // 2. 注册菜单项（IMenu::registerItem）
    // 3. 订阅 EventBus 事件
    // 4. 加载初始数据
    // 5. 获取其他插件的服务
    //
    // m_pluginManager->startAll();
}

/**
 * 样例：插件元数据（JSON 格式）
 *
 * 每个插件需要一个 JSON 元数据文件，描述插件的基本信息。
 */
void example_plugin_metadata()
{
    // orders_plugin.json 文件内容：
    // {
    //     "id": "com.yourco.orders",        // 插件唯一标识符
    //     "name": "Orders Plugin",          // 显示名称
    //     "version": "1.0.0",               // 版本号
    //     "description": "Order management",// 描述
    //     "vendor": "YourCo",               // 开发者
    //     "requires": [                     // 依赖声明
    //         {"type": "service", "id": "INavigation", "min": "1.0"}
    //     ],
    //     "provides": ["OrdersService"],    // 提供的服务
    //     "qmlModules": ["YourCo.Orders"],  // QML 模块 URI
    //     "priority": 10                    // 加载优先级
    // }
    //
    // Q_PLUGIN_METADATA 宏引用这个 JSON 文件：
    // Q_PLUGIN_METADATA(IID MPF_IPlugin_iid FILE "../orders_plugin.json")
    //
    // 【重要】插件类还需要实现 metadata() 方法返回同样的内容
    // 这是因为 metadata() 可以在运行时被其他代码调用
}

/**
 * 样例：插件的 QML 模块集成
 *
 * 插件可以提供 QML 模块，Host 负责设置正确的导入路径。
 */
void example_qml_module_integration()
{
    // 每个插件可以有一个 QML 模块 URI（如 "YourCo.Orders"）
    // 对应的 QML 文件放在特定目录结构中：
    //
    //   qml/
    //   └── YourCo/
    //       └── Orders/
    //           ├── OrdersPage.qml
    //           ├── OrderCard.qml
    //           └── CreateOrderDialog.qml
    //
    // Host 通过 addImportPath() 让 QML 引擎能解析 import 语句：
    //
    // m_engine->addImportPath(m_qmlPath);          // 主 QML 路径（IDE 补全）
    // m_engine->addImportPath("qrc:/");            // qrc 资源（运行时加载）
    //
    // 插件的 QML 文件由 qt_add_qml_module 嵌入 DLL 的 qrc 资源中，
    // 在 registerRoutes() 中直接使用 qrc URL 注册路由：
    // nav->registerRoute("orders", "qrc:/YourCo/Orders/OrdersPage.qml");
}
