#include "SystemAppearance.h"

#include <QGuiApplication>
#include <QStyleHints>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QVariant>
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
namespace {

// The same freedesktop desktop portal setting Qt >= 6.5 itself reads under
// the hood for QStyleHints::colorScheme() -- works across GNOME, KDE, and
// any other portal-backed desktop, without needing a newer Qt.
constexpr char kPortalService[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr char kPortalSettingsInterface[] = "org.freedesktop.portal.Settings";
constexpr char kAppearanceNamespace[] = "org.freedesktop.appearance";
constexpr char kColorSchemeKey[] = "color-scheme";

// Settings.Read's reply is declared as a "v" (variant) whose payload is
// itself the setting's own value -- for color-scheme that's a variant
// wrapping a uint32, so QtDBus hands back a QVariant holding another
// QDBusVariant instead of the uint32 directly. Peel off however many
// layers are actually there before reading it.
QVariant unwrapPortalVariant(QVariant value)
{
    while (value.canConvert<QDBusVariant>()) {
        value = value.value<QDBusVariant>().variant();
    }
    return value;
}

// Portal convention (xdg-desktop-portal Settings spec):
// 0 = no preference, 1 = prefer dark, 2 = prefer light.
bool colorSchemeValueIsDark(const QVariant &value)
{
    return unwrapPortalVariant(value).toUInt() == 1;
}

} // namespace
#endif

SystemAppearance &SystemAppearance::instance()
{
    static SystemAppearance singleton;
    return singleton;
}

SystemAppearance::SystemAppearance()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QStyleHints *hints = QGuiApplication::styleHints();
    m_dark = hints->colorScheme() == Qt::ColorScheme::Dark;
    connect(hints, &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme scheme) { setDarkMode(scheme == Qt::ColorScheme::Dark); });
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QDBusInterface portal(QLatin1String(kPortalService), QLatin1String(kPortalPath),
                          QLatin1String(kPortalSettingsInterface), QDBusConnection::sessionBus());
    if (portal.isValid()) {
        const QDBusReply<QDBusVariant> reply = portal.call(
            QStringLiteral("Read"), QLatin1String(kAppearanceNamespace), QLatin1String(kColorSchemeKey));
        if (reply.isValid()) {
            m_dark = colorSchemeValueIsDark(reply.value().variant());
        }

        QDBusConnection::sessionBus().connect(
            QLatin1String(kPortalService), QLatin1String(kPortalPath), QLatin1String(kPortalSettingsInterface),
            QStringLiteral("SettingChanged"), this,
            SLOT(handlePortalSettingChanged(QString, QString, QDBusVariant)));
    }
    // No portal available (non-portal-backed desktop, or the service isn't
    // running): stay on the light default set by the in-class initializer.
#endif
}

void SystemAppearance::setDarkMode(bool dark)
{
    if (m_dark == dark) {
        return;
    }
    m_dark = dark;
    emit darkModeChanged(dark);
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
void SystemAppearance::handlePortalSettingChanged(const QString &group, const QString &key,
                                                   const QDBusVariant &value)
{
    if (group != QLatin1String(kAppearanceNamespace) || key != QLatin1String(kColorSchemeKey)) {
        return;
    }
    setDarkMode(colorSchemeValueIsDark(value.variant()));
}
#endif
