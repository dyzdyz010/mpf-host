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
#include <QRegularExpression>
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
    auto* navigation = new NavigationService(nullptr, this);
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
    
    // Update navigation with engine reference
    static_cast<NavigationService*>(navigation)->~NavigationService();
    new (navigation) NavigationService(m_engine.get(), this);
    
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

// Helper: expand environment variables in path string
// Supports ${VAR}, $VAR (Unix), and %VAR% (Windows) syntax
static QString expandEnvVars(const QString& path)
{
    QString result = path;
    
    // Expand ${VAR} syntax (cross-platform)
    static QRegularExpression re1(R"(\$\{([^}]+)\})");
    QRegularExpressionMatchIterator it1 = re1.globalMatch(result);
    while (it1.hasNext()) {
        QRegularExpressionMatch match = it1.next();
        QString varName = match.captured(1);
        QString varValue = qEnvironmentVariable(varName.toUtf8().constData());
        result.replace(match.captured(0), varValue);
    }
    
    // Expand %VAR% syntax (Windows)
    static QRegularExpression re2(R"(%([^%]+)%)");
    QRegularExpressionMatchIterator it2 = re2.globalMatch(result);
    while (it2.hasNext()) {
        QRegularExpressionMatch match = it2.next();
        QString varName = match.captured(1);
        QString varValue = qEnvironmentVariable(varName.toUtf8().constData());
        result.replace(match.captured(0), varValue);
    }
    
    return result;
}

void Application::setupPaths()
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Determine paths relative to executable
    m_pluginPath = appDir + "/../plugins";
    m_qmlPath = appDir + "/../qml";
    m_configPath = appDir + "/../config";
    
    // Normalize paths
    m_pluginPath = QDir(m_pluginPath).absolutePath();
    m_qmlPath = QDir(m_qmlPath).absolutePath();
    m_configPath = QDir(m_configPath).absolutePath();
    
    // Create config directory if needed
    QDir().mkpath(m_configPath);
    
    // Check for development plugin path override (MPF_PLUGIN_PATH environment variable)
    // Supports multiple paths separated by platform-specific separator (; on Windows, : on Unix)
    QString envPluginPaths = qEnvironmentVariable("MPF_PLUGIN_PATH");
    if (!envPluginPaths.isEmpty()) {
        m_extraPluginPaths = envPluginPaths.split(QDir::listSeparator(), Qt::SkipEmptyParts);
        for (QString& path : m_extraPluginPaths) {
            path = QDir(path).absolutePath();
        }
        qDebug() << "Development plugin paths (MPF_PLUGIN_PATH):" << m_extraPluginPaths;
    }

    // Optional override via config file
    QString pathsConfig = QDir(m_configPath).filePath("paths.json");
    if (QFile::exists(pathsConfig)) {
        QFile file(pathsConfig);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error{};
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                QString baseDir = QFileInfo(pathsConfig).absolutePath();
                
                // Helper to resolve path with env var expansion and relative path support
                auto resolvePath = [&](const QString& key, QString* target) {
                    QJsonValue value = obj.value(key);
                    if (!value.isString()) {
                        return;
                    }
                    QString raw = value.toString().trimmed();
                    if (raw.isEmpty()) {
                        return;
                    }
                    // First expand environment variables
                    raw = expandEnvVars(raw);
                    // Then resolve relative paths
                    QString resolved = QDir::isAbsolutePath(raw)
                        ? raw
                        : QDir(baseDir).filePath(raw);
                    *target = QDir(resolved).absolutePath();
                };

                resolvePath("pluginPath", &m_pluginPath);
                resolvePath("qmlPath", &m_qmlPath);
                
                // Load extra QML import paths (e.g., SDK qml directory)
                if (obj.contains("extraQmlPaths")) {
                    QJsonValue extraPaths = obj.value("extraQmlPaths");
                    if (extraPaths.isArray()) {
                        for (const QJsonValue& v : extraPaths.toArray()) {
                            if (v.isString()) {
                                QString raw = v.toString().trimmed();
                                if (!raw.isEmpty()) {
                                    // First expand environment variables
                                    raw = expandEnvVars(raw);
                                    // Then resolve relative paths
                                    QString resolved = QDir::isAbsolutePath(raw)
                                        ? raw
                                        : QDir(baseDir).filePath(raw);
                                    m_extraQmlPaths.append(QDir(resolved).absolutePath());
                                }
                            }
                        }
                    }
                }
                
                qDebug() << "Loaded paths config:" << pathsConfig;
            } else {
                qWarning() << "Invalid paths config:" << pathsConfig
                           << "-" << error.errorString();
            }
        } else {
            qWarning() << "Failed to open paths config:" << pathsConfig;
        }
    }
    
    qDebug() << "Plugin path:" << m_pluginPath;
    qDebug() << "QML path:" << m_qmlPath;
    qDebug() << "Config path:" << m_configPath;
    if (!m_extraQmlPaths.isEmpty()) {
        qDebug() << "Extra QML paths:" << m_extraQmlPaths;
    }
    
    // Add library path for plugins
    m_app->addLibraryPath(m_pluginPath);
    
    // Add SDK library path for plugin dependencies (e.g., mpf-http-client.dll)
#if MPF_SDK_HAS_QML_PATH
    // SDK bin directory is parallel to qml directory
    QString sdkBinPath = QDir(QStringLiteral(MPF_SDK_QML_PATH)).filePath("../bin");
    sdkBinPath = QDir(sdkBinPath).absolutePath();
    if (QDir(sdkBinPath).exists()) {
        m_app->addLibraryPath(sdkBinPath);
#ifdef Q_OS_WIN
        // On Windows, add SDK bin to PATH for DLL dependencies
        QByteArray currentPath = qgetenv("PATH");
        QByteArray newPath = sdkBinPath.toLocal8Bit() + ";" + currentPath;
        qputenv("PATH", newPath);
        qDebug() << "Added SDK bin to PATH:" << sdkBinPath;
#endif
    }
#endif
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
        QString fsPath = m_qmlPath + "/MPF/Host/qml/Main.qml";
        if (QFile::exists(fsPath)) {
            entryQml = QUrl::fromLocalFile(fsPath).toString();
        } else {
            // RESOURCE_PREFIX "/" means qrc:/MPF/Host/qml/Main.qml
            entryQml = "qrc:/MPF/Host/qml/Main.qml";
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
