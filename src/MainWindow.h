#pragma once
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <array>
#include <QHash>
#include <QPixmap>
#include <QUrl>
#include "Config.h"
#include "Player.h"

class QTabWidget;
class QLabel;
class QSlider;
class QToolButton;
class QAction;
class QDockWidget;
class QTreeWidget;
class QMenu;
class QTimer;
class QListWidget;
class QFrame;
class QHBoxLayout;
class QBoxLayout;
class QVBoxLayout;
class QSplitter;
class QActionGroup;
class QFrame;
class QDBusVariant;
class LevelMeter;
class SpectrumAnalyzer;
class PlaylistWidget;
class MprisService;
class VizDelay;
class TrayIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.jgu.LinuxAuroraPlayer")
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void addPaths(const QStringList &paths);   // files, folders, playlists -> current playlist
    void playFirst();

public slots:
    // Files opened from the file manager / command line (also from a second start, over D-Bus):
    // they replace the "Opened files" playlist and start playing; other playlists stay untouched.
    Q_SCRIPTABLE void openFiles(const QStringList &paths);

protected:
    void closeEvent(QCloseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void changeEvent(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
    void addFiles();
    void addFolder();
    void addUrl();
    void importPlaylist();
    void importIntoCurrent();
    void exportPlaylist();
    void showPreferences();
    void applyConfig(const AppConfig &cfg);
    void playClicked();
    void pauseClicked();
    void pauseOnly();
    void togglePlayPause();
    void stopPlayback();
    void playFrom(PlaylistWidget *w, int proxyRow);
    void step(int delta);
    void onStateChanged(Player::State s);
    void onTick();
    void renamePlaylist(int index);
    void closePlaylist(int index);
    void showListMenu(PlaylistWidget *w, const QPoint &globalPos);
    void showTabMenu(const QPoint &pos);
    void refreshManager();
    void playPlaylistAt(int index);
    void about();
    void onPortalSetting(const QString &ns, const QString &key, const QDBusVariant &value);
    void quitApp();

private:
    void buildUi();
    void buildManager();
    void scheduleManagerRefresh();
    PlaylistWidget *createPlaylist(const QString &name = {}, bool activate = true);
    PlaylistWidget *playlistAt(int index) const;
    PlaylistWidget *current() const;
    void updateTabIcons();
    void refreshIcons();
    void updateNowPlaying();
    void updateTimeLabel(qint64 posMs, qint64 durMs);
    void updateOutputLabel();
    void applyTheme();
    bool systemIsDark() const;
    void readSystemScheme();
    void updateHero();
    void updateUpNext();
    void setSonoraLayout(bool on);
    void setAuroraCompact(bool compact);
    QString dataDir() const;
    void savePlaylists();
    void loadPlaylists();

    Player *m_player;
    MprisService *m_mpris = nullptr;
    VizDelay *m_viz = nullptr;              // shared delay of meters + spectrum
    LevelMeter *m_meter;
    SpectrumAnalyzer *m_spectrum;
    QTabWidget *m_tabs = nullptr;
    QPointer<PlaylistWidget> m_playing;     // playlist that owns the playing track
    QToolButton *m_timeBtn;
    QSlider *m_seek, *m_vol;
    QToolButton *m_btnPrev = nullptr, *m_btnPlay = nullptr, *m_btnPause = nullptr,
                *m_btnStop = nullptr, *m_btnNext = nullptr, *m_btnOpen = nullptr,
                *m_btnRepeat = nullptr, *m_btnShuffle = nullptr;
    QAction *m_repeat, *m_shuffle, *m_spectrumAct, *m_darkAct, *m_remainAct;
    QLabel *m_outputLabel = nullptr;
    QMenu *m_viewMenu = nullptr;
    QActionGroup *m_themeGroup = nullptr;
    QFrame *m_hero = nullptr;
    QHBoxLayout *m_heroLay = nullptr, *m_specRow = nullptr, *m_transport = nullptr, *m_volRow = nullptr;
    QBoxLayout *m_heroText = nullptr;
    QVBoxLayout *m_leftCol = nullptr;
    bool m_auroraCompact = true;
    QString m_theme = QStringLiteral("aurora");
    QAction *m_playlistAct = nullptr;
    QSplitter *m_split = nullptr;
    QWidget *m_cover = nullptr;
    QLabel *m_heroKicker = nullptr, *m_heroTitle = nullptr, *m_heroSub = nullptr, *m_peakLabel = nullptr;
    std::array<QLabel *, 3> m_chips{};
    QListWidget *m_upNext = nullptr;
    TrayIcon *m_tray = nullptr;
    QUrl m_coverFor;                        // track whose cover is shown / being looked up
    QHash<QString, QPixmap> m_thumbs;       // Up next cover thumbnails
    QLabel *m_titleIcon = nullptr, *m_plChip = nullptr;
    QMenu *m_themeMenu = nullptr;
    QLabel *m_elapsed = nullptr;
    QAction *m_closeToTrayAct = nullptr;
    bool m_quitting = false;
    bool m_sonoraLayout = false;
    int m_portalScheme = -1;                // XDG portal color-scheme: 0 default, 1 dark, 2 light, -1 unknown
    bool m_gsettingsDark = false;
    class QStackedWidget *m_stack = nullptr;
    QWidget *m_aMeter = nullptr, *m_aSeek = nullptr, *m_aTime = nullptr, *m_aVol = nullptr, *m_aSpec = nullptr, *m_aTabs = nullptr;
    QWidget *m_sMeter = nullptr, *m_sSeek = nullptr, *m_sTime = nullptr, *m_sVol = nullptr, *m_sSpec = nullptr, *m_sTabs = nullptr;
    QToolButton *m_sPrev = nullptr, *m_sPlay = nullptr, *m_sPause = nullptr, *m_sStop = nullptr, *m_sNext = nullptr,
                *m_sOpen = nullptr, *m_sRepeat = nullptr, *m_sShuffle = nullptr;

    QDockWidget *m_dock = nullptr;          // playlist manager
    QTreeWidget *m_mgr = nullptr;
    QLabel *m_mgrInfo = nullptr;
    QTimer *m_mgrTimer = nullptr;
    bool m_mgrBusy = false;

    AppConfig m_cfg;
    QByteArray m_headerState;               // playlist column layout
    bool m_seeking = false;
    bool m_showRemaining = false;
    int m_nameCounter = 0;
    int m_errorStreak = 0;
    QElapsedTimer m_eosClock;               // guard against an endless chain of instantly ending tracks
    int m_eosBurst = 0;
};
