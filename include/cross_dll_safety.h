#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace mpf {

/**
 * @brief Utilities for cross-DLL memory safety on Windows
 * 
 * MinGW DLLs have separate heaps by default. Qt's COW (Copy-On-Write)
 * types like QString, QVariant, etc. can cause heap corruption when
 * memory allocated in one DLL is freed in another.
 * 
 * These functions force deep copies to ensure all memory is allocated
 * in the current DLL's heap.
 */
namespace CrossDllSafety {

/**
 * @brief Deep copy a QString
 */
inline QString deepCopy(const QString& str)
{
    if (str.isEmpty()) return QString();
    return QString(str.constData(), str.size());
}

/**
 * @brief Deep copy a QStringList
 */
inline QStringList deepCopy(const QStringList& list)
{
    QStringList result;
    result.reserve(list.size());
    for (const QString& s : list) {
        result.append(deepCopy(s));
    }
    return result;
}

/**
 * @brief Deep copy a QByteArray
 */
inline QByteArray deepCopy(const QByteArray& ba)
{
    if (ba.isEmpty()) return QByteArray();
    return QByteArray(ba.constData(), ba.size());
}

/**
 * @brief Deep copy a QVariant (recursively handles maps/lists)
 */
inline QVariant deepCopy(const QVariant& var);

/**
 * @brief Deep copy a QVariantMap
 */
inline QVariantMap deepCopy(const QVariantMap& map)
{
    QVariantMap result;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        result.insert(deepCopy(it.key()), deepCopy(it.value()));
    }
    return result;
}

/**
 * @brief Deep copy a QVariantList
 */
inline QVariantList deepCopy(const QVariantList& list)
{
    QVariantList result;
    result.reserve(list.size());
    for (const QVariant& v : list) {
        result.append(deepCopy(v));
    }
    return result;
}

/**
 * @brief Deep copy a QVariant (implementation)
 */
inline QVariant deepCopy(const QVariant& var)
{
    if (!var.isValid()) return QVariant();
    
    switch (var.typeId()) {
    case QMetaType::QString:
        return QVariant(deepCopy(var.toString()));
    case QMetaType::QStringList:
        return QVariant(deepCopy(var.toStringList()));
    case QMetaType::QByteArray:
        return QVariant(deepCopy(var.toByteArray()));
    case QMetaType::QVariantMap:
        return QVariant(deepCopy(var.toMap()));
    case QMetaType::QVariantList:
        return QVariant(deepCopy(var.toList()));
    default:
        // For primitive types (int, double, bool, etc.), QVariant copy is safe
        return var;
    }
}

} // namespace CrossDllSafety
} // namespace mpf
