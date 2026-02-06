#include "settings_service.h"
#include "cross_dll_safety.h"
#include <QStandardPaths>
#include <QDir>

namespace mpf {

using CrossDllSafety::deepCopy;

SettingsService::SettingsService(QObject* parent)
    : QObject(parent)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath);
    m_settings = std::make_unique<QSettings>(configPath + "/settings.ini", QSettings::IniFormat);
}

SettingsService::SettingsService(const QString& configPath, QObject* parent)
    : QObject(parent)
{
    QDir().mkpath(configPath);
    m_settings = std::make_unique<QSettings>(configPath + "/settings.ini", QSettings::IniFormat);
}

SettingsService::~SettingsService() = default;

QVariant SettingsService::value(const QString& pluginId, 
                                 const QString& key, 
                                 const QVariant& defaultValue) const
{
    // Deep copy the returned value to avoid cross-DLL heap issues
    return deepCopy(m_settings->value(makeKey(pluginId, key), defaultValue));
}

void SettingsService::setValue(const QString& pluginId, 
                                const QString& key, 
                                const QVariant& value)
{
    QString fullKey = makeKey(pluginId, key);
    QVariant oldValue = m_settings->value(fullKey);
    
    if (oldValue != value) {
        m_settings->setValue(fullKey, value);
        emit settingChanged(pluginId, key, value);
    }
}

void SettingsService::remove(const QString& pluginId, const QString& key)
{
    m_settings->remove(makeKey(pluginId, key));
}

bool SettingsService::contains(const QString& pluginId, const QString& key) const
{
    return m_settings->contains(makeKey(pluginId, key));
}

QStringList SettingsService::keys(const QString& pluginId) const
{
    m_settings->beginGroup(pluginId);
    QStringList result = m_settings->childKeys();
    m_settings->endGroup();
    return deepCopy(result);
}

void SettingsService::sync()
{
    m_settings->sync();
}

QString SettingsService::makeKey(const QString& pluginId, const QString& key) const
{
    return pluginId + "/" + key;
}

} // namespace mpf
