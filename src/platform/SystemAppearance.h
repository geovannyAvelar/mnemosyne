#pragma once

#include <QObject>

class QDBusVariant;

// Tracks the OS-level light/dark appearance setting, independent of the
// app's own manual "Dark Mode" reading-view toggle (see MainWindow's
// "darkMode" QSetting). main.cpp uses this to pick the taskbar/title-bar
// icon variant that matches the desktop's own theme, live.
class SystemAppearance : public QObject
{
    Q_OBJECT

public:
    static SystemAppearance &instance();

    bool isDarkMode() const { return m_dark; }

signals:
    void darkModeChanged(bool dark);

private:
    SystemAppearance();

    void setDarkMode(bool dark);

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
private slots:
    void handlePortalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value);
#endif

private:
    bool m_dark = false;
};
