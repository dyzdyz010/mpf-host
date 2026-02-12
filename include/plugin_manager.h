#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <memory>
#include <vector>

namespace mpf {

class PluginLoader;
class ServiceRegistry;
class PluginMetadata;
class IPlugin;

/**
 * @brief Manages plugin discovery, loading, and lifecycle
 */
class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(ServiceRegistry* registry, QObject* parent = nullptr);
    ~PluginManager() override;

    /**
     * @brief Discover plugins in a directory
     * @param path Directory path to scan
     * @return Number of plugins found
     */
    int discover(const QString& path);

    /**
     * @brief Load all discovered plugins
     * @return true if all required plugins loaded successfully
     */
    bool loadAll();

    /**
     * @brief Initialize all loaded plugins (dependency resolution)
     * @return true if all plugins initialized successfully
     */
    bool initializeAll();

    /**
     * @brief Start all initialized plugins
     * @return true if all plugins started successfully
     */
    bool startAll();

    /**
     * @brief Stop all running plugins
     */
    void stopAll();

    /**
     * @brief Unload all plugins
     */
    void unloadAll();

    /**
     * @brief Get list of all plugin loaders
     */
    QList<PluginLoader*> plugins() const;

    /**
     * @brief Get plugin by ID
     */
    PluginLoader* plugin(const QString& id) const;

    /**
     * @brief Get all QML module URIs provided by plugins
     */
    QStringList qmlModuleUris() const;

    /**
     * @brief Get entry QML from a plugin (if any)
     */
    QString entryQml(const QString& pluginId) const;

    /**
     * @brief Resolve a service dependency to the plugin ID that provides it
     * @param serviceId Service name from the "provides" field
     * @return Plugin ID of the provider, or empty string if not found
     */
    QString resolveServiceProvider(const QString& serviceId) const;

    /**
     * @brief Check if all dependencies are satisfied
     * @param metadata Plugin metadata to check
     * @return List of unsatisfied dependencies (empty if all satisfied)
     */
    QStringList checkDependencies(const PluginMetadata& metadata) const;

    /**
     * @brief Get load order respecting dependencies
     * @return Ordered list of plugin IDs
     */
    QStringList loadOrder() const;

signals:
    void pluginDiscovered(const QString& id);
    void pluginLoaded(const QString& id);
    void pluginInitialized(const QString& id);
    void pluginStarted(const QString& id);
    void pluginStopped(const QString& id);
    void pluginUnloaded(const QString& id);
    void pluginError(const QString& id, const QString& error);

private:
    QStringList computeLoadOrder() const;
    bool topologicalSort(const QString& id, 
                         QHash<QString, int>& state, 
                         QStringList& order) const;

    ServiceRegistry* m_registry;
    std::vector<std::unique_ptr<PluginLoader>> m_loaders;
    QHash<QString, PluginLoader*> m_pluginMap;
    QHash<QString, QString> m_serviceProviderMap;  // service name -> plugin ID
};

} // namespace mpf
