#pragma once

#include <mpf/service_registry.h>
#include <QObject>
#include <QString>
#include <QHash>
#include <QMutex>
#include <typeinfo>
#include <memory>

namespace mpf {

/**
 * @brief Service registration entry
 */
struct ServiceEntry
{
    QString interfaceName;
    int version;
    QObject* instance;
    QString providerId;  // Plugin that provides this service
};

/**
 * @brief Concrete service registry implementation
 * 
 * Inherits from SDK's abstract ServiceRegistry for plugin compatibility
 * and QObject for Qt signals.
 */
class ServiceRegistryImpl : public QObject, public ServiceRegistry
{
    Q_OBJECT

public:
    explicit ServiceRegistryImpl(QObject* parent = nullptr);
    ~ServiceRegistryImpl() override;

    /**
     * @brief Register a service implementation
     * @tparam T Interface type
     * @param instance Service instance (must outlive registry)
     * @param version API version
     * @param providerId ID of plugin providing this service
     * @return true if registration succeeded
     */
    /**
     * @brief Register a service implementation
     * @tparam T Interface type
     * @param instance Service instance (must inherit QObject via multiple inheritance)
     * @param version API version
     * @param providerId ID of plugin providing this service
     * @return true if registration succeeded
     *
     * Note: The service implementation must inherit both QObject and the interface T.
     * If dynamic_cast to QObject* fails, registration is refused to prevent UB.
     */
    template<typename T>
    bool add(T* instance, int version = 1, const QString& providerId = {})
    {
        QObject* obj = dynamic_cast<QObject*>(instance);
        if (!obj) {
            qWarning("ServiceRegistry: Cannot register service '%s' — "
                     "implementation must inherit QObject", typeid(T).name());
            return false;
        }
        return addService(typeid(T).name(), obj, version, providerId);
    }

    /**
     * @brief Get a service implementation
     * @tparam T Interface type
     * @param minVersion Minimum required version (0 = any)
     * @return Service instance or nullptr if not found/version mismatch
     */
    template<typename T>
    T* get(int minVersion = 0)
    {
        QObject* obj = getService(typeid(T).name(), minVersion);
        return dynamic_cast<T*>(obj);
    }

    /**
     * @brief Check if a service is available
     * @tparam T Interface type
     * @param minVersion Minimum required version
     * @return true if service is available
     */
    template<typename T>
    bool has(int minVersion = 0) const
    {
        return hasService(typeid(T).name(), minVersion);
    }

    /**
     * @brief Get service version
     * @tparam T Interface type
     * @return Version number or -1 if not found
     */
    template<typename T>
    int version() const
    {
        return serviceVersion(typeid(T).name());
    }

    /**
     * @brief Remove a service
     * @tparam T Interface type
     */
    template<typename T>
    void remove()
    {
        removeService(typeid(T).name());
    }

    /**
     * @brief Get all registered service names
     * @return List of service type names
     */
    QStringList registeredServices() const;

    /**
     * @brief Get service entry details
     * @param interfaceName Type name of interface
     * @return Service entry or nullptr if not found
     */
    const ServiceEntry* entry(const QString& interfaceName) const;

    /**
     * @brief Get service as QObject* directly (for QML exposure)
     *
     * This avoids the reinterpret_cast issue in the get<T>() template
     * when dealing with multiple inheritance.
     *
     * @tparam T Interface type
     * @param minVersion Minimum required version
     * @return QObject* or nullptr if not found
     */
    template<typename T>
    QObject* getObject(int minVersion = 0)
    {
        return getService(typeid(T).name(), minVersion);
    }

signals:
    void serviceAdded(const QString& interfaceName);
    void serviceRemoved(const QString& interfaceName);

protected:
    // ServiceRegistry interface implementation
    QObject* getService(const char* typeName, int minVersion) override;
    bool addService(const char* typeName, QObject* instance, 
                    int version, const QString& providerId) override;
    bool hasService(const char* typeName, int minVersion) const override;

private:
    int serviceVersion(const char* typeName) const;
    void removeService(const char* typeName);

    mutable QMutex m_mutex;
    QHash<QString, ServiceEntry> m_services;
};

} // namespace mpf
