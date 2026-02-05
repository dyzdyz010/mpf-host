#pragma once

#include "service_registry.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <memory>

namespace mpf {

class PluginManager;
class Logger;

/**
 * @brief Main application class
 * 
 * Manages application lifecycle, plugin loading, and QML engine setup.
 */
class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(int& argc, char** argv);
    ~Application() override;

    /**
     * @brief Initialize the application
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Run the application event loop
     * @return Exit code
     */
    int run();

    /**
     * @brief Get the QML engine
     */
    QQmlApplicationEngine* engine() const { return m_engine.get(); }

    /**
     * @brief Get the service registry
     */
    ServiceRegistry* serviceRegistry() const { return m_registry.get(); }

    /**
     * @brief Get the plugin manager
     */
    PluginManager* pluginManager() const { return m_pluginManager.get(); }

    /**
     * @brief Get command line arguments
     */
    QStringList arguments() const;

    /**
     * @brief Get application instance
     */
    static Application* instance() { return s_instance; }

signals:
    void initialized();
    void aboutToQuit();

private:
    void setupPaths();
    void setupLogging();
    void setupQmlContext();
    void loadPlugins();
    bool loadMainQml();

    std::unique_ptr<QGuiApplication> m_app;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    std::unique_ptr<ServiceRegistryImpl> m_registry;
    std::unique_ptr<PluginManager> m_pluginManager;
    std::unique_ptr<Logger> m_logger;

    QString m_pluginPath;
    QString m_qmlPath;
    QString m_configPath;
    QStringList m_extraQmlPaths;     // Extra QML import paths (e.g., SDK qml directory)
    QStringList m_extraPluginPaths;  // Extra plugin search paths (for development overrides)

    static Application* s_instance;
};

} // namespace mpf
