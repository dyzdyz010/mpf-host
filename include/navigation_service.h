#pragma once

#include "mpf/interfaces/inavigation.h"
#include <QHash>
#include <QStack>

class QQmlApplicationEngine;

namespace mpf {

/**
 * @brief Default navigation service implementation
 */
class NavigationService : public QObject, public INavigation
{
    Q_OBJECT

public:
    explicit NavigationService(QQmlApplicationEngine* engine, QObject* parent = nullptr);
    ~NavigationService() override;

    // INavigation interface
    Q_INVOKABLE bool push(const QString& route, const QVariantMap& params = {}) override;
    Q_INVOKABLE bool pop() override;
    Q_INVOKABLE void popToRoot() override;
    Q_INVOKABLE bool replace(const QString& route, const QVariantMap& params = {}) override;
    Q_INVOKABLE QString currentRoute() const override;
    Q_INVOKABLE int stackDepth() const override;
    Q_INVOKABLE bool canGoBack() const override;
    Q_INVOKABLE void registerRoute(const QString& route, const QString& qmlComponent) override;
    
    /**
     * @brief Get the QML page URL for a route (for Loader-based navigation)
     */
    Q_INVOKABLE QString getPageUrl(const QString& route) const;

    /**
     * @brief Set the StackView object ID in QML (deprecated - using Loader now)
     */
    void setStackViewId(const QString& id) { m_stackViewId = id; }

signals:
    void navigationChanged(const QString& route, const QVariantMap& params);
    void canGoBackChanged(bool canGoBack);

private:
    QObject* stackView() const;
    QVariant resolveComponent(const QString& route);

    QQmlApplicationEngine* m_engine;
    QString m_stackViewId = "mainStackView";
    
    struct RouteEntry {
        QString pattern;
        QString component;
    };
    QList<RouteEntry> m_routes;
    
    struct StackEntry {
        QString route;
        QVariantMap params;
    };
    QStack<StackEntry> m_stack;
};

} // namespace mpf
