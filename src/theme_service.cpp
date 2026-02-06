#include "theme_service.h"
#include "cross_dll_safety.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace mpf {

using CrossDllSafety::deepCopy;

// ThemeData

ThemeData ThemeData::fromJson(const QJsonObject& json)
{
    ThemeData data;
    
    data.name = json.value("name").toString();
    data.isDark = json.value("isDark").toBool(false);
    
    auto colorFromJson = [&json](const QString& key, const QColor& def) -> QColor {
        QString colorStr = json.value(key).toString();
        return colorStr.isEmpty() ? def : QColor(colorStr);
    };
    
    data.primaryColor = colorFromJson("primaryColor", "#2196F3");
    data.accentColor = colorFromJson("accentColor", "#FF4081");
    data.backgroundColor = colorFromJson("backgroundColor", data.isDark ? "#121212" : "#FFFFFF");
    data.surfaceColor = colorFromJson("surfaceColor", data.isDark ? "#1E1E1E" : "#F5F5F5");
    data.textColor = colorFromJson("textColor", data.isDark ? "#FFFFFF" : "#212121");
    data.textSecondaryColor = colorFromJson("textSecondaryColor", data.isDark ? "#B0B0B0" : "#757575");
    data.errorColor = colorFromJson("errorColor", "#F44336");
    data.warningColor = colorFromJson("warningColor", "#FF9800");
    data.successColor = colorFromJson("successColor", "#4CAF50");
    
    data.spacingTiny = json.value("spacingTiny").toInt(4);
    data.spacingSmall = json.value("spacingSmall").toInt(8);
    data.spacingMedium = json.value("spacingMedium").toInt(16);
    data.spacingLarge = json.value("spacingLarge").toInt(24);
    
    data.radiusSmall = json.value("radiusSmall").toInt(4);
    data.radiusMedium = json.value("radiusMedium").toInt(8);
    data.radiusLarge = json.value("radiusLarge").toInt(16);
    
    return data;
}

ThemeData ThemeData::lightTheme()
{
    ThemeData data;
    data.name = "Light";
    data.isDark = false;
    data.primaryColor = QColor("#2196F3");
    data.accentColor = QColor("#FF4081");
    data.backgroundColor = QColor("#FFFFFF");
    data.surfaceColor = QColor("#F5F5F5");
    data.textColor = QColor("#212121");
    data.textSecondaryColor = QColor("#757575");
    data.errorColor = QColor("#F44336");
    data.warningColor = QColor("#FF9800");
    data.successColor = QColor("#4CAF50");
    return data;
}

ThemeData ThemeData::darkTheme()
{
    ThemeData data;
    data.name = "Dark";
    data.isDark = true;
    data.primaryColor = QColor("#90CAF9");
    data.accentColor = QColor("#FF80AB");
    data.backgroundColor = QColor("#121212");
    data.surfaceColor = QColor("#1E1E1E");
    data.textColor = QColor("#FFFFFF");
    data.textSecondaryColor = QColor("#B0B0B0");
    data.errorColor = QColor("#EF5350");
    data.warningColor = QColor("#FFA726");
    data.successColor = QColor("#66BB6A");
    return data;
}

// ThemeService

ThemeService::ThemeService(QObject* parent)
    : QObject(parent)
{
    // Register built-in themes
    registerTheme(ThemeData::lightTheme());
    registerTheme(ThemeData::darkTheme());
    
    // Default to light theme
    m_current = ThemeData::lightTheme();
}

ThemeService::~ThemeService() = default;

void ThemeService::setTheme(const QString& themeName)
{
    if (!m_themes.contains(themeName)) {
        qWarning() << "ThemeService: Unknown theme:" << themeName;
        return;
    }
    
    m_current = m_themes.value(themeName);
    emit themeChanged();
}

QStringList ThemeService::availableThemes() const
{
    return deepCopy(m_themes.keys());
}

void ThemeService::registerTheme(const ThemeData& theme)
{
    m_themes[theme.name] = theme;
}

bool ThemeService::loadThemes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ThemeService: Cannot open themes file:" << path;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning() << "ThemeService: Invalid themes file format";
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonArray themes = root.value("themes").toArray();
    
    for (const auto& val : themes) {
        ThemeData theme = ThemeData::fromJson(val.toObject());
        if (!theme.name.isEmpty()) {
            registerTheme(theme);
        }
    }
    
    return true;
}

} // namespace mpf
