#include "menu_service.h"
#include "cross_dll_safety.h"
#include <algorithm>
#include <QDebug>

namespace mpf {

using CrossDllSafety::deepCopy;

// Deep copy a MenuItem to ensure all strings are in host's heap
static MenuItem deepCopyItem(const MenuItem& item)
{
    MenuItem result;
    result.id = deepCopy(item.id);
    result.label = deepCopy(item.label);
    result.icon = deepCopy(item.icon);
    result.route = deepCopy(item.route);
    result.group = deepCopy(item.group);
    result.order = item.order;
    result.enabled = item.enabled;
    result.badge = deepCopy(item.badge);
    result.pluginId = deepCopy(item.pluginId);
    return result;
}

MenuService::MenuService(QObject* parent)
    : QObject(parent)
{
}

MenuService::~MenuService() = default;

bool MenuService::registerItem(const MenuItem& item)
{
    if (item.id.isEmpty()) {
        qWarning() << "MenuService: Cannot register item with empty ID";
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_indexMap.contains(item.id)) {
        qWarning() << "MenuService: Item already registered:" << item.id;
        return false;
    }
    
    // Deep copy to ensure all strings are in host's heap
    m_items.append(deepCopyItem(item));
    sortItems();
    
    // Rebuild index map after sorting
    m_indexMap.clear();
    for (int i = 0; i < m_items.size(); ++i) {
        m_indexMap[m_items[i].id] = i;
    }
    
    locker.unlock();
    
    qDebug() << "MenuService: Registered" << item.id << "from" << item.pluginId;
    emit menuChanged();
    return true;
}

void MenuService::unregisterItem(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&id](const MenuItem& item) { return item.id == id; });
    
    if (it != m_items.end()) {
        m_items.erase(it);
        m_indexMap.remove(id);
        
        // Rebuild index map
        m_indexMap.clear();
        for (int i = 0; i < m_items.size(); ++i) {
            m_indexMap[m_items[i].id] = i;
        }
        
        locker.unlock();
        emit menuChanged();
    }
}

void MenuService::unregisterPlugin(const QString& pluginId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = std::remove_if(m_items.begin(), m_items.end(),
        [&pluginId](const MenuItem& item) { return item.pluginId == pluginId; });
    
    if (it != m_items.end()) {
        m_items.erase(it, m_items.end());
        
        // Rebuild index map
        m_indexMap.clear();
        for (int i = 0; i < m_items.size(); ++i) {
            m_indexMap[m_items[i].id] = i;
        }
        
        locker.unlock();
        emit menuChanged();
    }
}

bool MenuService::updateItem(const QString& id, const QVariantMap& updates)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_indexMap.contains(id)) {
        return false;
    }
    
    int idx = m_indexMap[id];
    MenuItem& item = m_items[idx];
    
    if (updates.contains("title")) item.label = updates["label"].toString();
    if (updates.contains("icon")) item.icon = updates["icon"].toString();
    if (updates.contains("route")) item.route = updates["route"].toString();
    if (updates.contains("order")) {
        item.order = updates["order"].toInt();
        sortItems();
        // Rebuild index map after sorting
        m_indexMap.clear();
        for (int i = 0; i < m_items.size(); ++i) {
            m_indexMap[m_items[i].id] = i;
        }
    }
    if (updates.contains("enabled")) item.enabled = updates["enabled"].toBool();
    if (updates.contains("badge")) item.badge = updates["badge"].toString();
    if (updates.contains("group")) item.group = updates["group"].toString();
    
    locker.unlock();
    emit menuChanged();
    return true;
}

void MenuService::setBadge(const QString& id, const QString& badge)
{
    updateItem(id, {{"badge", badge}});
}

void MenuService::setEnabled(const QString& id, bool enabled)
{
    updateItem(id, {{"enabled", enabled}});
}

QList<MenuItem> MenuService::items() const
{
    QMutexLocker locker(&m_mutex);
    // Deep copy each item before returning
    QList<MenuItem> result;
    result.reserve(m_items.size());
    for (const MenuItem& item : m_items) {
        result.append(deepCopyItem(item));
    }
    return result;
}

QVariantList MenuService::itemsAsVariant() const
{
    QMutexLocker locker(&m_mutex);
    QVariantList result;
    for (const MenuItem& item : m_items) {
        // toVariantMap creates new QVariants, then deep copy the map
        result.append(deepCopy(item.toVariantMap()));
    }
    return result;
}

QVariantList MenuService::itemsInGroup(const QString& group) const
{
    QMutexLocker locker(&m_mutex);
    QVariantList result;
    for (const MenuItem& item : m_items) {
        if (item.group == group) {
            result.append(deepCopy(item.toVariantMap()));
        }
    }
    return result;
}

QStringList MenuService::groups() const
{
    QMutexLocker locker(&m_mutex);
    QSet<QString> groupSet;
    for (const MenuItem& item : m_items) {
        if (!item.group.isEmpty()) {
            groupSet.insert(item.group);
        }
    }
    return deepCopy(groupSet.values());
}

int MenuService::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_items.size();
}

void MenuService::sortItems()
{
    std::stable_sort(m_items.begin(), m_items.end(),
        [](const MenuItem& a, const MenuItem& b) {
            // First by group, then by order, then by title
            if (a.group != b.group) return a.group < b.group;
            if (a.order != b.order) return a.order < b.order;
            return a.label < b.label;
        });
}

} // namespace mpf
