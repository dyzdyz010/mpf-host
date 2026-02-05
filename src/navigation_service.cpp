#include "navigation_service.h"
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QMetaObject>
#include <QRegularExpression>
#include <QDebug>
#include <QFile>
#include <QDir>

namespace mpf {

NavigationService::NavigationService(QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
}

NavigationService::~NavigationService() = default;

bool NavigationService::push(const QString& route, const QVariantMap& params)
{
    qDebug() << "NavigationService::push called with route:" << route;
    
    QObject* sv = stackView();
    if (!sv) {
        qWarning() << "NavigationService: StackView not found";
        return false;
    }

    QVariant component = resolveComponent(route);
    if (!component.isValid()) {
        qWarning() << "NavigationService: No component for route:" << route;
        return false;
    }

    // Debug: check if the qrc resource exists
    QUrl url = component.toUrl();
    qDebug() << "NavigationService: Resolved component URL:" << url.toString();
    
    if (url.scheme() == "qrc") {
        // Check if qrc resource exists
        QString resourcePath = ":/" + url.path().mid(1); // Convert qrc:/path to :/path
        QFile resourceFile(resourcePath);
        qDebug() << "NavigationService: Checking qrc resource:" << resourcePath 
                 << "exists:" << resourceFile.exists();
        
        if (!resourceFile.exists()) {
            qWarning() << "NavigationService: QRC resource does NOT exist:" << resourcePath;
            // List what resources are available in the parent directory
            QString parentPath = resourcePath.left(resourcePath.lastIndexOf('/'));
            QDir resourceDir(parentPath);
            qDebug() << "NavigationService: Contents of" << parentPath << ":"
                     << resourceDir.entryList();
        }
    }

    // Pre-check: try to create a component to catch QML errors
    qDebug() << "NavigationService: Pre-checking QML component...";
    QQmlComponent testComponent(m_engine, url);
    if (testComponent.isError()) {
        qWarning() << "NavigationService: QML component has errors:";
        for (const QQmlError& error : testComponent.errors()) {
            qWarning() << "  -" << error.toString();
        }
        return false;
    }
    if (!testComponent.isReady()) {
        qWarning() << "NavigationService: QML component not ready, status:" << testComponent.status();
    }
    qDebug() << "NavigationService: QML component pre-check passed";
    
    qDebug() << "NavigationService: Calling navPush...";
    
    // Push to stack
    QVariant result;
    QMetaObject::invokeMethod(sv, "navPush", 
        Q_RETURN_ARG(QVariant, result),
        Q_ARG(QVariant, component),
        Q_ARG(QVariant, QVariant::fromValue(params)));

    qDebug() << "NavigationService: navPush returned:" << result.isValid();

    if (result.isValid()) {
        StackEntry entry{route, params};
        m_stack.push(entry);
        
        emit navigationChanged(route, params);
        emit canGoBackChanged(canGoBack());
        return true;
    }

    return false;
}

bool NavigationService::pop()
{
    QObject* sv = stackView();
    if (!sv || m_stack.size() <= 1) {
        return false;
    }

    QVariant result;
    QMetaObject::invokeMethod(sv, "navPop",
        Q_RETURN_ARG(QVariant, result));

    if (result.isValid()) {
        m_stack.pop();
        
        if (!m_stack.isEmpty()) {
            const StackEntry& current = m_stack.top();
            emit navigationChanged(current.route, current.params);
        }
        emit canGoBackChanged(canGoBack());
        return true;
    }

    return false;
}

void NavigationService::popToRoot()
{
    QObject* sv = stackView();
    if (!sv) return;

    QVariant result;
    QMetaObject::invokeMethod(sv, "navPopToRoot",
        Q_RETURN_ARG(QVariant, result));
    
    while (m_stack.size() > 1) {
        m_stack.pop();
    }

    if (!m_stack.isEmpty()) {
        const StackEntry& current = m_stack.top();
        emit navigationChanged(current.route, current.params);
    }
    emit canGoBackChanged(canGoBack());
}

bool NavigationService::replace(const QString& route, const QVariantMap& params)
{
    QObject* sv = stackView();
    if (!sv) return false;

    QVariant component = resolveComponent(route);
    if (!component.isValid()) {
        qWarning() << "NavigationService: No component for route:" << route;
        return false;
    }

    QVariant result;
    QMetaObject::invokeMethod(sv, "navReplace",
        Q_RETURN_ARG(QVariant, result),
        Q_ARG(QVariant, component),
        Q_ARG(QVariant, QVariant::fromValue(params)));

    if (result.isValid()) {
        if (!m_stack.isEmpty()) {
            m_stack.pop();
        }
        StackEntry entry{route, params};
        m_stack.push(entry);
        
        emit navigationChanged(route, params);
        return true;
    }

    return false;
}

QString NavigationService::currentRoute() const
{
    return m_stack.isEmpty() ? QString() : m_stack.top().route;
}

int NavigationService::stackDepth() const
{
    return m_stack.size();
}

bool NavigationService::canGoBack() const
{
    return m_stack.size() > 1;
}

void NavigationService::registerRoute(const QString& route, const QString& qmlComponent)
{
    RouteEntry entry{route, qmlComponent};
    m_routes.append(entry);
    qDebug() << "NavigationService: Registered route" << route << "->" << qmlComponent;
}

QObject* NavigationService::stackView() const
{
    if (!m_engine) return nullptr;
    
    QList<QObject*> roots = m_engine->rootObjects();
    if (roots.isEmpty()) return nullptr;

    return roots.first()->findChild<QObject*>(m_stackViewId);
}

QVariant NavigationService::resolveComponent(const QString& route)
{
    // Find matching route
    for (const RouteEntry& entry : m_routes) {
        // Simple pattern matching (could be enhanced with regex)
        if (entry.pattern == route || entry.pattern == "*") {
            return QVariant::fromValue(QUrl(entry.component));
        }
        
        // Wildcard prefix matching
        if (entry.pattern.endsWith("/*")) {
            QString prefix = entry.pattern.left(entry.pattern.length() - 2);
            if (route.startsWith(prefix)) {
                return QVariant::fromValue(QUrl(entry.component));
            }
        }
    }

    // Try direct QML file
    if (route.endsWith(".qml")) {
        return QVariant::fromValue(QUrl(route));
    }

    return QVariant();
}

} // namespace mpf
