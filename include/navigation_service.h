#pragma once

#include "mpf/interfaces/inavigation.h"
#include <QList>

class QQmlApplicationEngine;

namespace mpf {

/**
 * @brief Navigation service implementation for Loader-based page switching
 * 
 * Plugins register their main page URL via registerRoute().
 * QML uses getPageUrl() to load pages via Loader.
 * Internal navigation within plugins uses Popup/Dialog.
 */
class NavigationService : public QObject, public INavigation
{
    Q_OBJECT

public:
    explicit NavigationService(QObject* parent = nullptr);
    ~NavigationService() override;

    /**
     * @brief Set the QML engine reference (called after engine is created)
     * Replaces the previous dangerous placement-new pattern.
     */
    void setEngine(QQmlApplicationEngine* engine);

    // INavigation interface
    Q_INVOKABLE void registerRoute(const QString& route, const QString& qmlPageUrl) override;
    Q_INVOKABLE QString getPageUrl(const QString& route) const override;
    Q_INVOKABLE QString currentRoute() const override;
    Q_INVOKABLE void setCurrentRoute(const QString& route) override;

signals:
    void navigationChanged(const QString& route, const QVariantMap& params);

private:
    QQmlApplicationEngine* m_engine;
    QString m_currentRoute;
    
    struct RouteEntry {
        QString pattern;
        QString pageUrl;
    };
    QList<RouteEntry> m_routes;
};

} // namespace mpf
