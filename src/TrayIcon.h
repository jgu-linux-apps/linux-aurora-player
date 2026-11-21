#pragma once
// Panel (tray) icon: one full-colour rimmed icon that is readable on any top bar colour.
// Click shows / hides the main window; the menu mirrors the transport controls.
#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;

class TrayIcon : public QObject
{
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = nullptr);
    bool isAvailable() const;
    void setPlaying(bool playing);
    void setTrack(const QString &text);      // tooltip

signals:
    void toggleWindowRequested();
    void playPauseRequested();
    void previousRequested();
    void nextRequested();
    void stopRequested();
    void quitRequested();

private:
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_playPause = nullptr;
};
