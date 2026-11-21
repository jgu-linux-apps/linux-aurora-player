#include "TrayIcon.h"
#include <QAction>
#include <QIcon>
#include <QMenu>

TrayIcon::TrayIcon(QObject *parent) : QObject(parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_menu = new QMenu;
    QAction *show = m_menu->addAction(tr("Show / hide window"));
    m_menu->addSeparator();
    m_playPause = m_menu->addAction(tr("Play"));
    QAction *prev = m_menu->addAction(tr("Previous"));
    QAction *next = m_menu->addAction(tr("Next"));
    QAction *stop = m_menu->addAction(tr("Stop"));
    m_menu->addSeparator();
    QAction *quit = m_menu->addAction(tr("Quit"));
    connect(show, &QAction::triggered, this, &TrayIcon::toggleWindowRequested);
    connect(m_playPause, &QAction::triggered, this, &TrayIcon::playPauseRequested);
    connect(prev, &QAction::triggered, this, &TrayIcon::previousRequested);
    connect(next, &QAction::triggered, this, &TrayIcon::nextRequested);
    connect(stop, &QAction::triggered, this, &TrayIcon::stopRequested);
    connect(quit, &QAction::triggered, this, &TrayIcon::quitRequested);

    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/tray.svg")), this);
    m_tray->setToolTip(QStringLiteral("Linux Aurora Player"));
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) emit toggleWindowRequested();
        else if (r == QSystemTrayIcon::MiddleClick) emit playPauseRequested();
    });
    m_tray->show();
}

bool TrayIcon::isAvailable() const { return m_tray != nullptr; }

void TrayIcon::setPlaying(bool playing)
{
    if (m_playPause) m_playPause->setText(playing ? tr("Pause") : tr("Play"));
}

void TrayIcon::setTrack(const QString &text)
{
    if (m_tray) m_tray->setToolTip(text.isEmpty() ? QStringLiteral("Linux Aurora Player")
                                                  : text + QStringLiteral(" — Linux Aurora Player"));
}
