#include "navigation_service.h"
#include "cross_dll_safety.h"
#include <QQmlApplicationEngine>
#include <QDebug>

namespace mpf {

using CrossDllSafety::deepCopy;

NavigationService::NavigationService(QObject* parent)
    : QObject(parent)
    , m_engine(nullptr)
{
}

NavigationService::~NavigationService() = default;

void NavigationService::setEngine(QQmlApplicationEngine* engine)
{
    m_engine = engine;
    qDebug() << "NavigationService: Engine set";
}

void NavigationService::registerRoute(const QString& route, const QString& qmlPageUrl)
{
    // Deep copy strings from plugin to ensure they're in host's heap
    RouteEntry entry{deepCopy(route), deepCopy(qmlPageUrl)};
    m_routes.append(entry);
    qDebug() << "NavigationService: Registered route" << route << "->" << qmlPageUrl;
}

QString NavigationService::getPageUrl(const QString& route) const
{
    for (const RouteEntry& entry : m_routes) {
        if (entry.pattern == route) {
            qDebug() << "NavigationService: getPageUrl" << route << "->" << entry.pageUrl;
            // Deep copy before returning to ensure caller gets memory from host's heap
            return deepCopy(entry.pageUrl);
        }
    }
    
    qWarning() << "NavigationService: No page URL found for route:" << route;
    return QString();
}

QString NavigationService::currentRoute() const
{
    return deepCopy(m_currentRoute);
}

void NavigationService::setCurrentRoute(const QString& route)
{
    QString routeCopy = deepCopy(route);
    if (m_currentRoute != routeCopy) {
        m_currentRoute = routeCopy;
        emit navigationChanged(routeCopy, {});
    }
}

} // namespace mpf
