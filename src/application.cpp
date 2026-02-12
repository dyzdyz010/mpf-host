#include "application.h"
#include "plugin_manager.h"
#include "plugin_loader.h"
#include "plugin_metadata.h"
#include "navigation_service.h"
#include "settings_service.h"
#include "theme_service.h"
#include "menu_service.h"
#include "event_bus_service.h"
#include "qml_context.h"

#include "service_registry.h"
#include "logger.h"
#include <mpf/sdk_paths.h>
#include <mpf/interfaces/inavigation.h>
#include <mpf/interfaces/isettings.h>
#include <mpf/interfaces/itheme.h>
#include <mpf/interfaces/imenu.h>
#include <mpf/interfaces/ieventbus.h>

#include <QQmlContext>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QUrl>
#include <QStandardPaths>
#include <QDebug>

namespace mpf {

Application* Application::s_instance = nullptr;

Application::Application(int& argc, char** argv)
{
    s_instance = this;
    
    m_app = std::make_unique<QGuiApplication>(argc, argv);
    m_app->setOrganizationName("MPF");
    m_app->setApplicationName("QtModularPluginFramework");
    m_app->setApplicationVersion("1.0.0");
}

Application::~Application()
{
    if (m_pluginManager) {
        m_pluginManager->stopAll();
        m_pluginManager->unloadAll();
    }
    
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool Application::initialize()
{
    setupPaths();
    setupLogging();
    
    // Create service registry
    m_registry = std::make_unique<ServiceRegistryImpl>(this);
    
    // Create and register core services
    auto* navigation = new NavigationService(this);
    auto* settings = new SettingsService(m_configPath, this);
    auto* theme = new ThemeService(this);
    auto* menu = new MenuService(this);
    auto* eventBus = new EventBusService(this);

    m_registry->add<INavigation>(navigation, INavigation::apiVersion(), "host");
    m_registry->add<ISettings>(settings, ISettings::apiVersion(), "host");
    m_registry->add<ITheme>(theme, ITheme::apiVersion(), "host");
    m_registry->add<IMenu>(menu, IMenu::apiVersion(), "host");
    m_registry->add<ILogger>(m_logger.get(), ILogger::apiVersion(), "host");
    m_registry->add<IEventBus>(eventBus, IEventBus::apiVersion(), "host");
    
    // Create QML engine
    m_engine = std::make_unique<QQmlApplicationEngine>();
    
    // Set engine reference on navigation (safe setter, no placement new)
    navigation->setEngine(m_engine.get());
    
    setupQmlContext();
    loadPlugins();
    
    if (!loadMainQml()) {
        return false;
    }
    
    emit initialized();
    return true;
}

int Application::run()
{
    connect(m_app.get(), &QCoreApplication::aboutToQuit, this, [this]() {
        emit aboutToQuit();
    });
    
    return m_app->exec();
}

QStringList Application::arguments() const
{
    return m_app->arguments();
}

void Application::setupPaths()
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    // SDK detection priority:
    // 1. MPF_SDK_ROOT environment variable (set by mpf-dev run)
    // 2. Auto-detect ~/.mpf-sdk/current (for Qt Creator debugging)
    // 3. Paths relative to executable (local build / installed mode)
    
    QString sdkRoot = qEnvironmentVariable("MPF_SDK_ROOT");
    
    // Auto-detect SDK if not explicitly set
    if (sdkRoot.isEmpty()) {
#ifdef Q_OS_WIN
        QString userHome = qEnvironmentVariable("USERPROFILE");
#else
        QString userHome = qEnvironmentVariable("HOME");
#endif
        QString sdkBaseDir = QDir(userHome).filePath(".mpf-sdk");
        QString currentPointer = QDir(sdkBaseDir).filePath("current.txt");
        
        // Read version from current.txt pointer file
        if (QFile::exists(currentPointer)) {
            QFile file(currentPointer);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString version = QString::fromUtf8(file.readAll()).trimmed();
                file.close();
                if (!version.isEmpty()) {
                    QString versionDir = QDir(sdkBaseDir).filePath(version);
                    if (QDir(versionDir).exists()) {
                        sdkRoot = versionDir;
                        qDebug() << "Auto-detected MPF SDK version:" << version << "at:" << sdkRoot;
                    }
                }
            }
        }
    }
    
    if (!sdkRoot.isEmpty() && QDir(sdkRoot).exists()) {
        // Running with SDK: use SDK directory structure
        qDebug() << "Using MPF SDK root:" << sdkRoot;
        m_pluginPath = QDir(sdkRoot).filePath("plugins");
        m_qmlPath = QDir(sdkRoot).filePath("qml");
        m_configPath = QDir(sdkRoot).filePath("config");
        
        // Add SDK bin and lib to DLL search path
        QString sdkBinPath = QDir(sdkRoot).filePath("bin");
        QString sdkLibPath = QDir(sdkRoot).filePath("lib");
        
        if (QDir(sdkBinPath).exists()) {
            m_app->addLibraryPath(sdkBinPath);
        }
        if (QDir(sdkLibPath).exists()) {
            m_app->addLibraryPath(sdkLibPath);
        }
        
#ifdef Q_OS_WIN
        // On Windows, prepend SDK bin/lib to PATH for DLL dependencies
        // This allows Qt Creator debugging without manually setting PATH
        QByteArray currentPath = qgetenv("PATH");
        QByteArray newPath = sdkBinPath.toLocal8Bit() + ";" + sdkLibPath.toLocal8Bit() + ";" + currentPath;
        qputenv("PATH", newPath);
        qDebug() << "Added SDK to PATH:" << sdkBinPath << "+" << sdkLibPath;
#endif
        
        // Add SDK qml to import paths
        m_extraQmlPaths.append(QDir(m_qmlPath).absolutePath());
    } else {
        // Local development / installed mode: use paths relative to executable
        qDebug() << "Using local paths relative to:" << appDir;
        m_pluginPath = appDir + "/../plugins";
        m_qmlPath = appDir + "/../qml";
        m_configPath = appDir + "/../config";
    }
    
    // Normalize paths
    m_pluginPath = QDir(m_pluginPath).absolutePath();
    m_qmlPath = QDir(m_qmlPath).absolutePath();
    m_configPath = QDir(m_configPath).absolutePath();
    
    // Create config directory if needed
    QDir().mkpath(m_configPath);
    
    // MPF_PLUGIN_PATH: additional plugin search paths (set by mpf-dev for linked plugins)
    // Supports multiple paths separated by platform-specific separator (; on Windows, : on Unix)
    QString envPluginPaths = qEnvironmentVariable("MPF_PLUGIN_PATH");
    if (!envPluginPaths.isEmpty()) {
        m_extraPluginPaths = envPluginPaths.split(QDir::listSeparator(), Qt::SkipEmptyParts);
        for (QString& path : m_extraPluginPaths) {
            path = QDir(path).absolutePath();
        }
        qDebug() << "Extra plugin paths (MPF_PLUGIN_PATH):" << m_extraPluginPaths;
    }
    
    qDebug() << "Plugin path:" << m_pluginPath;
    qDebug() << "QML path:" << m_qmlPath;
    qDebug() << "Config path:" << m_configPath;
    if (!m_extraQmlPaths.isEmpty()) {
        qDebug() << "Extra QML paths:" << m_extraQmlPaths;
    }
    
    // Add library path for plugins
    m_app->addLibraryPath(m_pluginPath);
    
    // Also add extra plugin paths to library search
    for (const QString& path : m_extraPluginPaths) {
        m_app->addLibraryPath(path);
    }
}

void Application::setupLogging()
{
    m_logger = std::make_unique<Logger>(this);
    m_logger->setFormat("[%time%] [%level%] [%tag%] %message%");
    m_logger->setMinLevel(ILogger::Level::Debug);
}

void Application::setupQmlContext()
{
    // Add QML import paths
    m_engine->addImportPath(m_qmlPath);
    m_engine->addImportPath("qrc:/");
    
    // Add SDK QML path (configured at build time via CMake)
#if MPF_SDK_HAS_QML_PATH
    QString sdkQmlPath = QStringLiteral(MPF_SDK_QML_PATH);
    if (!sdkQmlPath.isEmpty() && QDir(sdkQmlPath).exists()) {
        m_engine->addImportPath(sdkQmlPath);
        qDebug() << "Added SDK QML import path:" << sdkQmlPath;
    }
#endif
    
    // Add extra QML import paths from config (allows runtime override)
    for (const QString& path : m_extraQmlPaths) {
        if (QDir(path).exists()) {
            m_engine->addImportPath(path);
            qDebug() << "Added extra QML import path:" << path;
        } else {
            qWarning() << "Extra QML path does not exist:" << path;
        }
    }
    
    // Add host QML module output directory for component discovery
    QString hostQmlDir = m_qmlPath + "/MPF/Host/qml";
    if (QDir(hostQmlDir).exists()) {
        m_engine->addImportPath(hostQmlDir);
    }
    
    // Create and setup QML context helper
    auto* qmlContext = new QmlContext(m_registry.get(), this);
    qmlContext->setup(m_engine.get());
    
    qDebug() << "QML import paths:" << m_engine->importPathList();
}

void Application::loadPlugins()
{
    m_pluginManager = std::make_unique<PluginManager>(m_registry.get(), this);
    
    // Connect signals for logging
    connect(m_pluginManager.get(), &PluginManager::pluginDiscovered,
            [](const QString& id) { qDebug() << "Discovered plugin:" << id; });
    connect(m_pluginManager.get(), &PluginManager::pluginLoaded,
            [](const QString& id) { qDebug() << "Loaded plugin:" << id; });
    connect(m_pluginManager.get(), &PluginManager::pluginError,
            [](const QString& id, const QString& err) { 
                qWarning() << "Plugin error:" << id << "-" << err; 
            });
    
    // Discover plugins from extra paths first (development overrides, higher priority)
    // This allows linked source plugins to override SDK binary plugins
    int count = 0;
    for (const QString& path : m_extraPluginPaths) {
        int found = m_pluginManager->discover(path);
        qDebug() << "Discovered" << found << "plugins from development path:" << path;
        count += found;
    }
    
    // Then discover from default plugin path (SDK fallback)
    int defaultCount = m_pluginManager->discover(m_pluginPath);
    qDebug() << "Discovered" << defaultCount << "plugins from default path:" << m_pluginPath;
    count += defaultCount;
    
    qDebug() << "Total discovered" << count << "plugins";
    
    // Load, initialize, and start
    if (m_pluginManager->loadAll()) {
        if (m_pluginManager->initializeAll()) {
            m_pluginManager->startAll();
        }
    }
    
    // Register plugin QML modules
    for (const QString& uri : m_pluginManager->qmlModuleUris()) {
        qDebug() << "Plugin QML module:" << uri;
    }
}

bool Application::loadMainQml()
{
    // Try to find entry QML from plugins first
    QString entryQml;
    for (auto* loader : m_pluginManager->plugins()) {
        QString pluginEntry = m_pluginManager->entryQml(loader->metadata().id());
        if (!pluginEntry.isEmpty()) {
            entryQml = pluginEntry;
            break;
        }
    }
    
    // Fall back to host's Main.qml
    if (entryQml.isEmpty()) {
        // Try filesystem path first (development), then qrc (release)
        // Note: QT_RESOURCE_ALIAS flattens paths, so no /qml/ subdirectory
        QString fsPath = m_qmlPath + "/MPF/Host/Main.qml";
        if (QFile::exists(fsPath)) {
            entryQml = QUrl::fromLocalFile(fsPath).toString();
        } else {
            // RESOURCE_PREFIX "/" + QT_RESOURCE_ALIAS means qrc:/MPF/Host/Main.qml
            entryQml = "qrc:/MPF/Host/Main.qml";
        }
    }
    
    qDebug() << "Loading main QML:" << entryQml;
    
    m_engine->load(QUrl(entryQml));
    
    if (m_engine->rootObjects().isEmpty()) {
        qCritical() << "Failed to load main QML";
        return false;
    }
    
    return true;
}

} // namespace mpf
