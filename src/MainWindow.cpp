#include "MainWindow.h"
#include "AudioOutput.h"
#include "AuroraTheme.h"
#include "CueSheet.h"
#include "ExportDialog.h"
#include "PlaylistExporter.h"
#include "LevelMeter.h"
#include "MetadataLoader.h"
#include "Mpris.h"
#include "PlaylistModel.h"
#include "PlaylistWidget.h"
#include "PreferencesDialog.h"
#include "SpectrumAnalyzer.h"
#include "TransportIcons.h"
#include "TrayIcon.h"
#include "CoverArt.h"
#include "VizDelay.h"

#include <QAction>
#include <QMouseEvent>
#include <QWindow>
#include <functional>
#include <QActionGroup>
#include <QFrame>
#include <QGridLayout>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyleHints>
#include <QApplication>
#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QImageReader>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSet>
#include <QSaveFile>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <climits>
#include <cmath>

namespace {
const QStringList kAudioSuffixes = {
    "mp3", "flac", "ogg", "oga", "opus", "wav", "m4a", "aac", "wma", "ape", "wv",
    "mka", "mpc", "aif", "aiff", "ac3", "dts"};
const QStringList kPlaylistSuffixes = {"m3u", "m3u8", "pls"};

// Album tile of the "now playing" card: Aurora gradient with a disc glyph.
class CoverTile : public QWidget
{
public:
    explicit CoverTile(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(128, 128); }
    void setImage(const QImage &img) { m_img = img; update(); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (!m_img.isNull()) {                        // album cover, centre-cropped to a rounded square
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            QPainterPath clip;
            clip.addRoundedRect(QRectF(rect()), 16, 16);
            p.setClipPath(clip);
            const QSize s = m_img.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
            p.drawImage(QRect(QPoint((width() - s.width()) / 2, (height() - s.height()) / 2), s), m_img);
            return;
        }
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        const Aurora::Colors c = Aurora::colors(dark);
        QLinearGradient g(0, 0, width(), height());
        g.setColorAt(0, c.acc);
        g.setColorAt(1, c.acc2);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 16, 16);
        p.fillPath(path, QColor(dark ? "#151827" : "#26215c"));
        if (dark) { p.setPen(QPen(QBrush(g), 3)); p.drawPath(path); }
        const QPointF m = QRectF(rect()).center();
        p.setPen(QPen(QColor("#afa9ec"), 3));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(m, 34, 34);
        p.drawEllipse(m, 20, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#afa9ec"));
        p.drawEllipse(m, 6, 6);
    }
private:
    QImage m_img;
};

// Own title bar (Aurora): drag to move, double-click to maximise. Styled through the style sheet.
class TitleBar : public QFrame
{
public:
    using QFrame::QFrame;
    std::function<void()> onDoubleClick;
protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && window()->windowHandle()) window()->windowHandle()->startSystemMove();
    }
    void mouseDoubleClickEvent(QMouseEvent *) override { if (onDoubleClick) onDoubleClick(); }
};

QPalette darkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(45, 45, 48));
    p.setColor(QPalette::WindowText, QColor(220, 220, 220));
    p.setColor(QPalette::Base, QColor(30, 30, 32));
    p.setColor(QPalette::AlternateBase, QColor(40, 40, 43));
    p.setColor(QPalette::ToolTipBase, QColor(60, 60, 64));
    p.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
    p.setColor(QPalette::Text, QColor(220, 220, 220));
    p.setColor(QPalette::Button, QColor(58, 58, 63));
    p.setColor(QPalette::ButtonText, QColor(228, 228, 228));
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(80, 160, 255));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(135, 135, 135));
    const QColor dis(120, 120, 120);
    p.setColor(QPalette::Disabled, QPalette::WindowText, dis);
    p.setColor(QPalette::Disabled, QPalette::Text, dis);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
    return p;
}
}

// ================================================================== construction
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QSettings s;
    m_cfg.load(s);
    m_headerState = s.value("playlistHeader").toByteArray();
    MetadataLoader::instance().loadCache(dataDir() + "/metadata.cache");

    m_player = new Player(this);
    m_player->setSpectrumBands(m_cfg.spectrum.fftBands);
    m_player->setTruePeak(m_cfg.meter.truePeak);
    buildUi();
    setAcceptDrops(true);

    // player -> shared delay line -> level meters and spectrum analyser (same delay for both)
    m_viz = new VizDelay(this);
    m_viz->setDelayMs(m_cfg.vizDelayMs);                      // fine-tuning only: sync is automatic
    m_viz->setPositionProvider([this] { return m_player->rawPosition(); });
    connect(m_player, &Player::flushed, m_viz, &VizDelay::clear);
    connect(m_player, &Player::levels, m_viz, &VizDelay::pushLevels);
    connect(m_player, &Player::spectrum, m_viz, &VizDelay::pushSpectrum);
    connect(m_viz, &VizDelay::levels, m_meter, &LevelMeter::setLevels);
    connect(m_viz, &VizDelay::spectrum, m_spectrum, &SpectrumAnalyzer::setSpectrum);
    connect(m_player, &Player::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_player, &Player::outputProblem, this, [this](const QString &m) {
        statusBar()->showMessage(m, 12000);
    });
    connect(m_player, &Player::errorMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 8000);
        // skip the unplayable item, but never spin forever on a list of broken files
        if (m_playing && ++m_errorStreak < m_playing->proxy()->rowCount())
            QMetaObject::invokeMethod(this, [this] { step(+1); }, Qt::QueuedConnection);
    });
    connect(m_player, &Player::endOfStream, this, [this] {
        if (!m_eosClock.isValid() || m_eosClock.elapsed() > 1000) m_eosBurst = 0;
        m_eosClock.restart();
        if (++m_eosBurst > 8) {                        // 8 tracks ended within a second: something is wrong
            m_eosBurst = 0;
            m_player->stop();
            statusBar()->showMessage(tr("Playback stopped: tracks end immediately "
                                        "(check the cue sheet and its audio file)"), 10000);
            return;
        }
        QMetaObject::invokeMethod(this, [this] { step(+1); }, Qt::QueuedConnection);
    });

    // ---- desktop integration: dedicated media keys, "now playing" (MPRIS2 over D-Bus)
    m_mpris = new MprisService(m_player, this);
    connect(m_mpris, &MprisService::nextRequested,      this, [this] { step(+1); });
    connect(m_mpris, &MprisService::previousRequested,  this, [this] { step(-1); });
    connect(m_mpris, &MprisService::playPauseRequested, this, &MainWindow::togglePlayPause);
    connect(m_mpris, &MprisService::pauseRequested,     this, &MainWindow::pauseOnly);
    connect(m_mpris, &MprisService::stopRequested,      this, &MainWindow::stopPlayback);
    connect(m_mpris, &MprisService::playRequested,      this, [this] {
        if (m_player->state() == Player::Paused) m_player->resume();
        else if (m_player->state() == Player::Stopped) playClicked();
    });
    connect(m_mpris, &MprisService::seekRequested, this, [this](qint64 off) {
        m_player->seek(std::max<qint64>(0, m_player->position() + off / 1000));
    });
    connect(m_mpris, &MprisService::positionRequested, this, [this](qint64 us) { m_player->seek(us / 1000); });
    connect(m_mpris, &MprisService::volumeRequested, this, [this](double v) {
        m_vol->setValue(int(std::lround(std::clamp(v, 0.0, 1.0) * 100.0)));
    });
    connect(m_mpris, &MprisService::raiseRequested, this, [this] {
        showNormal(); raise(); activateWindow();
    });
    connect(m_mpris, &MprisService::quitRequested, this, &MainWindow::quitApp);

    // ---- panel (tray) icon
    m_tray = new TrayIcon(this);
    connect(m_tray, &TrayIcon::toggleWindowRequested, this, [this] {
        if (isVisible() && !isMinimized() && isActiveWindow()) hide();
        else { showNormal(); raise(); activateWindow(); }
    });
    connect(m_tray, &TrayIcon::playPauseRequested, this, &MainWindow::togglePlayPause);
    connect(m_tray, &TrayIcon::previousRequested, this, [this] { step(-1); });
    connect(m_tray, &TrayIcon::nextRequested, this, [this] { step(+1); });
    connect(m_tray, &TrayIcon::stopRequested, this, &MainWindow::stopPlayback);
    connect(m_tray, &TrayIcon::quitRequested, this, &MainWindow::quitApp);
    m_closeToTrayAct->setEnabled(m_tray->isAvailable());
    // the app ends only through quitApp() / a real close below, never because the window got hidden
    QApplication::setQuitOnLastWindowClosed(false);

    auto *tick = new QTimer(this);
    connect(tick, &QTimer::timeout, this, &MainWindow::onTick);
    tick->start(200);

    restoreGeometry(s.value("geometry").toByteArray());
    if (!restoreState(s.value("windowState").toByteArray()) && m_dock)
        m_dock->hide();                                           // C+ layout: manager off by default
    m_vol->setValue(s.value("volume", 80).toInt());
    m_repeat->setChecked(s.value("repeat", false).toBool());
    m_shuffle->setChecked(s.value("shuffle", false).toBool());
    m_spectrumAct->setChecked(s.value("spectrum", true).toBool());      // visible by default
    m_remainAct->setChecked(s.value("remaining", false).toBool());
    m_closeToTrayAct->setChecked(s.value("closeToTray", true).toBool());
    m_theme = s.value("theme", s.value("darkMode", false).toBool() ? "classic-dark" : "aurora").toString();
    for (QAction *a : m_themeGroup->actions())
        if (a->data().toString() == m_theme) a->setChecked(true);
    applyTheme();
    m_playlistAct->setChecked(s.value("playlistVisible", false).toBool());
    m_split->restoreState(s.value("splitter").toByteArray());

    m_player->setOutput(m_cfg.audio);            // only stored; applied when the first track starts
    updateOutputLabel();
    loadPlaylists();
    QTimer::singleShot(1500, this, [] { MetadataLoader::instance().startScanning(); });
}

// ================================================================== UI
void MainWindow::buildUi()
{
    setWindowTitle("Linux Aurora Player");
    resize(1000, 600);

    auto act = [this](QMenu *m, const QString &text, const QKeySequence &ks, auto slot) {
        QAction *a = m->addAction(text);
        if (!ks.isEmpty()) a->setShortcut(ks);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    auto actKeys = [this](QMenu *m, const QString &text, const QList<QKeySequence> &keys, auto slot) {
        QAction *a = m->addAction(text);
        a->setShortcuts(keys);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    // ---- File
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *aFiles  = act(fileMenu, tr("Add &Files…"), QKeySequence::Open, &MainWindow::addFiles);
    QAction *aFolder = act(fileMenu, tr("Add F&older…"), QKeySequence("Ctrl+Shift+O"), &MainWindow::addFolder);
    QAction *aUrl    = act(fileMenu, tr("Add &URL…"), QKeySequence("Ctrl+U"), &MainWindow::addUrl);
    fileMenu->addSeparator();
    QAction *aImpNew = act(fileMenu, tr("&Import Playlist as New…"), QKeySequence("Ctrl+I"), &MainWindow::importPlaylist);
    QAction *aImpCur = act(fileMenu, tr("Import Playlist into C&urrent…"), {}, &MainWindow::importIntoCurrent);
    act(fileMenu, tr("E&xport Playlist…"), {}, &MainWindow::exportPlaylist);
    fileMenu->addSeparator();
    act(fileMenu, tr("&Preferences…"), QKeySequence("Ctrl+P"), &MainWindow::showPreferences);
    fileMenu->addSeparator();
    act(fileMenu, tr("&Quit"), QKeySequence::Quit, [this] { quitApp(); });

    // ---- Playlist
    auto *plMenu = menuBar()->addMenu(tr("&Playlist"));
    act(plMenu, tr("&New Playlist"), QKeySequence("Ctrl+T"), [this] { createPlaylist(); });
    act(plMenu, tr("&Rename Playlist…"), QKeySequence("F2"), [this] { renamePlaylist(m_tabs->currentIndex()); });
    act(plMenu, tr("&Close Playlist"), QKeySequence("Ctrl+W"), [this] { closePlaylist(m_tabs->currentIndex()); });
    plMenu->addSeparator();
    act(plMenu, tr("&Find / Filter"), QKeySequence::Find, [this] { current()->focusFilter(); });
    act(plMenu, tr("Re&move Selected"), {}, [this] { current()->removeSelected(); });
    act(plMenu, tr("C&lear Playlist"), {}, [this] { current()->model()->clear(); });

    // ---- Playback (Audacious keys + dedicated keyboard media keys)
    auto *pbMenu = menuBar()->addMenu(tr("Play&back"));
    actKeys(pbMenu, tr("&Play"), {QKeySequence("X"), QKeySequence(Qt::Key_MediaPlay)}, &MainWindow::playClicked);
    actKeys(pbMenu, tr("Pa&use"), {QKeySequence("C"), QKeySequence(Qt::Key_MediaPause)}, &MainWindow::pauseClicked);
    actKeys(pbMenu, tr("Play / Pause"), {QKeySequence(Qt::Key_Space), QKeySequence(Qt::Key_MediaTogglePlayPause)},
            &MainWindow::togglePlayPause);
    actKeys(pbMenu, tr("&Stop"), {QKeySequence("V"), QKeySequence(Qt::Key_MediaStop)}, &MainWindow::stopPlayback);
    actKeys(pbMenu, tr("Pre&vious"), {QKeySequence("Z"), QKeySequence(Qt::Key_MediaPrevious)}, [this] { step(-1); });
    actKeys(pbMenu, tr("&Next"), {QKeySequence("B"), QKeySequence(Qt::Key_MediaNext)}, [this] { step(+1); });
    pbMenu->addSeparator();
    act(pbMenu, tr("Seek &backward 5 s"), QKeySequence("Ctrl+Left"), [this] {
        m_player->seek(std::max<qint64>(0, m_player->position() - 5000));
    });
    act(pbMenu, tr("Seek &forward 5 s"), QKeySequence("Ctrl+Right"), [this] {
        m_player->seek(m_player->position() + 5000);
    });
    pbMenu->addSeparator();
    m_repeat = pbMenu->addAction(tr("Re&peat (within filter)"));
    m_repeat->setCheckable(true);
    m_shuffle = pbMenu->addAction(tr("Shu&ffle"));
    m_shuffle->setCheckable(true);

    // ---- View
    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_spectrumAct = m_viewMenu->addAction(tr("&Spectrum analyser"));
    m_spectrumAct->setCheckable(true);
    m_spectrumAct->setChecked(true);
    m_remainAct = m_viewMenu->addAction(tr("Show &remaining time"));
    m_remainAct->setCheckable(true);
    m_closeToTrayAct = m_viewMenu->addAction(tr("&Close to panel icon (keep playing)"));
    m_closeToTrayAct->setCheckable(true);
    auto *themeMenu = m_viewMenu->addMenu(tr("&Theme"));
    m_themeMenu = themeMenu;
    m_themeGroup = new QActionGroup(this);
    const QList<QPair<QString, QString>> themes = {
        {"aurora", tr("Aurora (follows system)")}, {"aurora-light", tr("Aurora light")},
        {"aurora-dark", tr("Aurora dark")}, {"system", tr("System")}, {"sonora", tr("Sonora")}, {"classic-dark", tr("Sonora dark")}};
    for (const auto &t : themes) {
        QAction *a = themeMenu->addAction(t.second);
        a->setCheckable(true);
        a->setData(t.first);
        m_themeGroup->addAction(a);
        if (t.first == "classic-dark") m_darkAct = a;
    }
    connect(m_themeGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        m_theme = a->data().toString();
        applyTheme();
    });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { applyTheme(); });

    act(menuBar()->addMenu(tr("&Help")), tr("&About"), {}, &MainWindow::about);

    // ================= C+ layout =================
    // ---- LED level meter (in the "Level" card)
    m_meter = new LevelMeter;
    m_meter->setConfig(m_cfg.meter);

    // ---- transport: round buttons, one big play/pause in the middle
    auto mkBtn = [this](const QString &tip, bool checkable = false, int size = 46) {
        auto *b = new QToolButton;
        b->setObjectName("round");
        b->setToolTip(tip);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setCheckable(checkable);
        b->setFixedSize(size, size);
        b->setIconSize(QSize(20, 20));
        b->setFocusPolicy(Qt::NoFocus);         // so Space always means play/pause
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    m_btnPrev    = mkBtn(tr("Previous (within filter)"));
    m_btnPlay    = mkBtn(tr("Play / Pause (Space)"), false, 76);
    m_btnPlay->setObjectName("bigPlay");
    m_btnPlay->setIconSize(QSize(34, 34));
    m_btnPause   = m_btnPlay;                   // one button for play and pause
    m_btnStop    = mkBtn(tr("Stop"));
    m_btnNext    = mkBtn(tr("Next (within filter)"));
    m_btnOpen    = mkBtn(tr("Add files… (arrow: more)"));
    m_btnOpen->setFixedWidth(62);
    m_btnRepeat  = mkBtn(tr("Repeat"), true);
    m_btnShuffle = mkBtn(tr("Shuffle"), true);
    connect(m_btnPrev,  &QToolButton::clicked, this, [this] { step(-1); });
    connect(m_btnPlay,  &QToolButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_btnStop,  &QToolButton::clicked, this, &MainWindow::stopPlayback);
    connect(m_btnNext,  &QToolButton::clicked, this, [this] { step(+1); });
    connect(m_btnOpen,  &QToolButton::clicked, this, &MainWindow::addFiles);
    auto *addMenu = new QMenu(this);
    addMenu->addAction(aFiles);
    addMenu->addAction(aFolder);
    addMenu->addAction(aUrl);
    addMenu->addSeparator();
    addMenu->addAction(aImpNew);
    addMenu->addAction(aImpCur);
    m_btnOpen->setMenu(addMenu);
    m_btnOpen->setPopupMode(QToolButton::MenuButtonPopup);
    connect(m_btnRepeat,  &QToolButton::toggled, m_repeat,  &QAction::setChecked);
    connect(m_repeat,     &QAction::toggled, m_btnRepeat,  &QToolButton::setChecked);
    connect(m_btnShuffle, &QToolButton::toggled, m_shuffle, &QAction::setChecked);
    connect(m_shuffle,    &QAction::toggled, m_btnShuffle, &QToolButton::setChecked);

    m_vol = new QSlider(Qt::Horizontal);
    m_vol->setRange(0, 100);
    m_vol->setToolTip(tr("Volume"));
    auto *volPct = new QLabel("80%");
    volPct->setObjectName("muted");
    volPct->setMinimumWidth(36);
    connect(m_vol, &QSlider::valueChanged, this, [this, volPct](int v) {
        m_player->setVolume(std::pow(v / 100.0, 3.0));
        if (m_mpris) m_mpris->setVolume(v / 100.0);
        volPct->setText(QString::number(v) + "%");
    });

    auto *transport = new QHBoxLayout;
    transport->setSpacing(12);
    transport->addStretch(1);
    for (QToolButton *b : {m_btnShuffle, m_btnPrev, m_btnPlay, m_btnNext, m_btnRepeat})
        transport->addWidget(b);
    transport->addStretch(1);
    for (QToolButton *b : {m_btnStop, m_btnOpen}) {
        b->setObjectName("small");
        b->setFixedHeight(34);
        b->setIconSize(QSize(16, 16));
    }
    m_btnStop->setFixedWidth(34);
    m_btnOpen->setFixedWidth(48);

    auto *volRow = new QHBoxLayout;
    auto *volIcon = new QLabel(tr("Vol"));
    volIcon->setObjectName("muted");
    volRow->addWidget(volIcon);
    m_aVol = new QWidget; { auto *l = new QVBoxLayout(m_aVol); l->setContentsMargins(0, 0, 0, 0); l->addWidget(m_vol); }
    volRow->addWidget(m_aVol, 1);
    volRow->addWidget(volPct);
    volRow->addSpacing(8);
    volRow->addWidget(m_btnStop);
    volRow->addWidget(m_btnOpen);

    // ---- seek bar + elapsed / remaining time (click the time to switch)
    m_seek = new QSlider(Qt::Horizontal);
    m_seek->setRange(0, 0);
    m_timeBtn = new QToolButton;
    m_timeBtn->setObjectName("link");
    m_timeBtn->setFocusPolicy(Qt::NoFocus);
    m_timeBtn->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_timeBtn->setToolTip(tr("Click to switch between elapsed and remaining time"));
    m_timeBtn->setText("0:00");
    m_elapsed = new QLabel("0:00");
    m_elapsed->setObjectName("muted");
    connect(m_timeBtn, &QToolButton::clicked, this, [this] { m_remainAct->toggle(); });
    connect(m_remainAct, &QAction::toggled, this, [this](bool on) {
        m_showRemaining = on;
        if (m_player->state() == Player::Stopped) { m_timeBtn->setText("0:00"); m_elapsed->setText("0:00"); }
        else updateTimeLabel(m_player->position(), m_player->duration());
    });
    connect(m_seek, &QSlider::sliderPressed, this, [this] { m_seeking = true; });
    connect(m_seek, &QSlider::sliderMoved, this, [this](int v) {
        updateTimeLabel(v, m_player->duration());
    });
    connect(m_seek, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        m_player->seek(m_seek->value());
    });
    auto *seekRow = new QVBoxLayout;
    seekRow->setSpacing(0);
    m_aSeek = new QWidget; { auto *l = new QVBoxLayout(m_aSeek); l->setContentsMargins(0, 0, 0, 0); l->addWidget(m_seek); }
    seekRow->addWidget(m_aSeek);
    auto *timeRow = new QHBoxLayout;
    timeRow->addWidget(m_elapsed);
    timeRow->addStretch(1);
    m_aTime = new QWidget; { auto *l = new QVBoxLayout(m_aTime); l->setContentsMargins(0, 0, 0, 0); l->addWidget(m_timeBtn); }
    timeRow->addWidget(m_aTime);
    seekRow->addLayout(timeRow);

    // ---- spectrum analyser (shown by default)
    m_spectrum = new SpectrumAnalyzer;
    m_spectrum->setConfig(m_cfg.spectrum);
    connect(m_spectrumAct, &QAction::toggled, m_spectrum, &QWidget::setVisible);

    // ---- "now playing" hero card: cover tile, title, artist / album, format chips
    auto *hero = new QFrame;
    hero->setObjectName("hero");
    hero->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *heroLay = new QHBoxLayout(hero);
    heroLay->setContentsMargins(18, 18, 18, 18);
    heroLay->setSpacing(18);
    m_cover = new CoverTile;
    heroLay->addWidget(m_cover);
    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(4);
    m_heroKicker = new QLabel(tr("Now playing"));
    m_heroKicker->setObjectName("heroKicker");
    m_heroTitle = new QLabel(tr("Nothing playing"));
    m_heroTitle->setObjectName("heroTitle");
    m_heroTitle->setWordWrap(true);
    m_heroSub = new QLabel(tr("Add files or pick a track from the playlist"));
    m_heroSub->setObjectName("heroSub");
    m_heroSub->setWordWrap(true);
    auto *chipRow = new QHBoxLayout;
    chipRow->setSpacing(6);
    for (QLabel *&c : m_chips) {
        c = new QLabel;
        c->setObjectName("chip");
        c->hide();
        chipRow->addWidget(c);
    }
    chipRow->addStretch(1);
    heroText->addStretch(1);
    heroText->addWidget(m_heroKicker);
    heroText->addWidget(m_heroTitle);
    heroText->addWidget(m_heroSub);
    heroText->addSpacing(6);
    heroText->addLayout(chipRow);
    heroText->addStretch(1);
    heroLay->addLayout(heroText, 1);

    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(12);
    leftCol->addWidget(hero);
    m_aSpec = new QWidget; { auto *l = new QVBoxLayout(m_aSpec); l->setContentsMargins(0, 0, 0, 0); l->addWidget(m_spectrum); }
    m_aSpec->setMinimumHeight(130);
    m_aSpec->setMaximumHeight(190);
    leftCol->addWidget(m_aSpec, 1);
    leftCol->addLayout(seekRow);
    leftCol->addLayout(transport);
    leftCol->addLayout(volRow);
    leftCol->addStretch(1);

    // ---- "Level" card: LED ladder with true-peak
    auto *levelCard = new QFrame;
    levelCard->setObjectName("card");
    auto *levelLay = new QVBoxLayout(levelCard);
    levelLay->setContentsMargins(12, 10, 12, 12);
    auto *levelHead = new QHBoxLayout;
    auto *levelTitle = new QLabel(tr("Level"));
    levelTitle->setObjectName("cardTitle");
    m_peakLabel = new QLabel;
    m_peakLabel->setObjectName("peak");
    levelHead->addWidget(levelTitle);
    levelHead->addStretch(1);
    levelHead->addWidget(m_peakLabel);
    levelLay->addLayout(levelHead);
    m_meter->setHorizontal(true);
    m_aMeter = new QWidget; { auto *l = new QVBoxLayout(m_aMeter); l->setContentsMargins(0, 0, 0, 0); l->addWidget(m_meter); }
    levelLay->addWidget(m_aMeter);
    auto *peakTimer = new QTimer(this);
    connect(peakTimer, &QTimer::timeout, this, [this] {
        const double v = m_meter->peakHoldDb();
        const QString unit = m_cfg.meter.truePeak ? QStringLiteral(" dBTP") : QStringLiteral(" dBFS");
        m_peakLabel->setText(v <= m_cfg.meter.minDb + 0.5 ? QStringLiteral("–") + unit
                                                          : QString::number(v, 'f', 1).replace('-', QChar(0x2212)) + unit);
    });
    peakTimer->start(150);

    // ---- "Up next" card: the following tracks of the playing playlist
    auto *nextCard = new QFrame;
    nextCard->setObjectName("card");
    nextCard->setMinimumWidth(260);
    auto *nextLay = new QVBoxLayout(nextCard);
    nextLay->setContentsMargins(12, 10, 12, 12);
    auto *nextHead = new QHBoxLayout;
    auto *nextTitle = new QLabel(tr("Up next"));
    nextTitle->setObjectName("cardTitle");
    auto *showAll = new QToolButton;
    showAll->setObjectName("link");
    showAll->setText(tr("Whole playlist"));
    showAll->setCursor(Qt::PointingHandCursor);
    showAll->setFocusPolicy(Qt::NoFocus);
    nextHead->addWidget(nextTitle);
    nextHead->addStretch(1);
    nextHead->addWidget(showAll);
    m_upNext = new QListWidget;
    m_upNext->setObjectName("upNext");
    m_upNext->setFocusPolicy(Qt::NoFocus);
    m_upNext->setToolTip(tr("Double-click to play"));
    connect(m_upNext, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
        if (!m_playing) return;
        const int src = it->data(Qt::UserRole).toInt();
        const QModelIndex pi = m_playing->proxy()->mapFromSource(m_playing->model()->index(src));
        if (pi.isValid()) playFrom(m_playing, pi.row());
    });
    nextLay->addLayout(nextHead);
    nextLay->addWidget(m_upNext, 1);

    auto *top = new QWidget;
    auto *topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(0, 0, 0, 0);
    topLay->setSpacing(14);
    auto *rightCol = new QVBoxLayout;
    rightCol->setSpacing(12);
    rightCol->addWidget(levelCard);
    rightCol->addWidget(nextCard, 1);
    levelCard->setFixedWidth(320);
    nextCard->setFixedWidth(320);
    topLay->addLayout(leftCol, 1);
    topLay->addLayout(rightCol);

    // ---- playlist tabs (bottom, can be hidden: View > Playlist)
    m_tabs = new QTabWidget;
    m_tabs->setMovable(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setDocumentMode(false);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closePlaylist);
    connect(m_tabs->tabBar(), &QTabBar::tabBarDoubleClicked, this, [this](int i) {
        if (i >= 0) renamePlaylist(i); else createPlaylist();
    });
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested, this, &MainWindow::showTabMenu);
    auto *plus = new QToolButton;
    plus->setText("+");
    plus->setAutoRaise(true);
    plus->setToolTip(tr("New playlist (Ctrl+T)"));
    connect(plus, &QToolButton::clicked, this, [this] { createPlaylist(); });
    m_tabs->setCornerWidget(plus, Qt::TopRightCorner);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { updateUpNext(); });

    m_playlistAct = m_viewMenu->addAction(tr("&Playlist"));
    m_playlistAct->setCheckable(true);
    m_playlistAct->setChecked(true);
    m_playlistAct->setShortcut(QKeySequence("Ctrl+L"));
    connect(m_playlistAct, &QAction::toggled, this, [this](bool on) { if (m_aTabs) m_aTabs->setVisible(on); });
    connect(showAll, &QToolButton::clicked, m_playlistAct, &QAction::toggle);

    m_split = new QSplitter(Qt::Vertical);
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(10);
    m_split->addWidget(top);
    m_aTabs = new QWidget; { auto *l = new QVBoxLayout(m_aTabs); l->setContentsMargins(0, 0, 0, 0); l->addWidget(m_tabs); }
    m_split->addWidget(m_aTabs);
    m_split->setStretchFactor(0, 0);
    m_split->setStretchFactor(1, 1);

    m_outputLabel = new QLabel;
    statusBar()->addPermanentWidget(m_outputLabel);

    buildManager();

    // ---- own title bar: app icon, name, playlist chip, playlist / theme / menu buttons, window buttons.
    // The classic menu bar is hidden; its actions are added to the window so all shortcuts keep working.
    setAttribute(Qt::WA_TranslucentBackground);     // rounded window corners (drawn by the style sheet)
    setWindowFlag(Qt::FramelessWindowHint);
    menuBar()->hide();
    auto *mainMenu = new QMenu(this);
    for (QAction *m : menuBar()->actions()) {
        mainMenu->addAction(m);
        if (m->menu()) addActions(m->menu()->actions());
    }
    mainMenu->addSeparator();
    mainMenu->addAction(m_closeToTrayAct);
    auto *bar = new TitleBar;
    bar->setObjectName("titleBar");
    bar->onDoubleClick = [this] { isMaximized() ? showNormal() : showMaximized(); };
    auto *barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(14, 8, 10, 8);
    barLay->setSpacing(8);
    m_titleIcon = new QLabel;
    m_titleIcon->setFixedSize(24, 24);
    auto *titleText = new QLabel(QStringLiteral("Linux Aurora Player"));
    titleText->setObjectName("titleText");
    m_plChip = new QLabel;
    m_plChip->setObjectName("chip");
    auto mkBarBtn = [](const QString &text, const QString &tip, const char *name = "barBtn") {
        auto *b = new QToolButton;
        b->setObjectName(name);
        b->setText(text);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        b->setAutoRaise(true);
        return b;
    };
    auto *plBtn = mkBarBtn(QStringLiteral("☰"), tr("Show / hide playlist (Ctrl+L)"));
    connect(plBtn, &QToolButton::clicked, m_playlistAct, &QAction::toggle);
    auto *themeBtn = mkBarBtn(QStringLiteral("◐"), tr("Theme"));
    themeBtn->setMenu(m_themeMenu);
    themeBtn->setPopupMode(QToolButton::InstantPopup);
    auto *menuBtn = mkBarBtn(QStringLiteral("⋮"), tr("Menu"));
    menuBtn->setMenu(mainMenu);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    auto *minBtn = mkBarBtn(QStringLiteral("–"), tr("Minimise"), "winBtn");
    auto *maxBtn = mkBarBtn(QStringLiteral("□"), tr("Maximise"), "winBtn");
    auto *closeBtn = mkBarBtn(QStringLiteral("✕"), tr("Close"), "winClose");
    connect(minBtn, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QToolButton::clicked, this, [this] { isMaximized() ? showNormal() : showMaximized(); });
    connect(closeBtn, &QToolButton::clicked, this, &QWidget::close);
    barLay->addWidget(m_titleIcon);
    barLay->addWidget(titleText);
    barLay->addStretch(1);
    barLay->addWidget(m_plChip);
    for (QToolButton *b : {plBtn, themeBtn, menuBtn}) barLay->addWidget(b);
    barLay->addSpacing(10);
    for (QToolButton *b : {minBtn, maxBtn, closeBtn}) barLay->addWidget(b);

    auto *inner = new QWidget;
    auto *lay = new QVBoxLayout(inner);
    lay->setContentsMargins(16, 6, 16, 4);
    lay->addWidget(m_split);
    auto *central = new QWidget;
    central->setObjectName("auroraPage");
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(bar);
    outer->addWidget(inner, 1);
    // ---- Sonora layout (themes "Sonora" / "Sonora dark"): the original Sonora window - menu bar,
    // LED ladder on the left, Audacious-style buttons with separate play and pause, classic analyser.
    // Shared widgets (meter, seek, time, volume, spectrum, playlists) move between the two pages.
    auto mkS = [this](const QString &tip, bool checkable = false) {
        auto *b = new QToolButton;
        b->setToolTip(tip);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setAutoRaise(false);
        b->setCheckable(checkable);
        b->setFixedSize(40, 34);
        b->setIconSize(QSize(22, 22));
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };
    m_sPrev = mkS(tr("Previous (within filter)"));
    m_sPlay = mkS(tr("Play"));
    m_sPause = mkS(tr("Pause / Resume"), true);
    m_sStop = mkS(tr("Stop"));
    m_sNext = mkS(tr("Next (within filter)"));
    m_sOpen = mkS(tr("Add files… (arrow: more)"));
    m_sRepeat = mkS(tr("Repeat"), true);
    m_sShuffle = mkS(tr("Shuffle"), true);
    connect(m_sPrev, &QToolButton::clicked, this, [this] { step(-1); });
    connect(m_sPlay, &QToolButton::clicked, this, &MainWindow::playClicked);
    connect(m_sPause, &QToolButton::clicked, this, &MainWindow::pauseClicked);
    connect(m_sStop, &QToolButton::clicked, this, &MainWindow::stopPlayback);
    connect(m_sNext, &QToolButton::clicked, this, [this] { step(+1); });
    connect(m_sOpen, &QToolButton::clicked, this, &MainWindow::addFiles);
    m_sOpen->setMenu(addMenu);
    m_sOpen->setPopupMode(QToolButton::MenuButtonPopup);
    m_sOpen->setFixedWidth(56);
    connect(m_sRepeat, &QToolButton::toggled, m_repeat, &QAction::setChecked);
    connect(m_repeat, &QAction::toggled, m_sRepeat, &QToolButton::setChecked);
    connect(m_sShuffle, &QToolButton::toggled, m_shuffle, &QAction::setChecked);
    connect(m_shuffle, &QAction::toggled, m_sShuffle, &QToolButton::setChecked);
    auto mkHolder = [] { auto *w = new QWidget; auto *l = new QVBoxLayout(w); l->setContentsMargins(0, 0, 0, 0); return w; };
    m_sMeter = mkHolder(); m_sSeek = mkHolder(); m_sTime = mkHolder(); m_sVol = mkHolder();
    m_sSpec = mkHolder(); m_sTabs = mkHolder();
    m_sVol->setFixedWidth(120);
    auto *sTransport = new QHBoxLayout;
    for (QToolButton *b : {m_sPrev, m_sPlay, m_sPause, m_sStop, m_sNext, m_sOpen}) sTransport->addWidget(b);
    sTransport->addSpacing(12);
    sTransport->addWidget(m_sRepeat);
    sTransport->addWidget(m_sShuffle);
    sTransport->addStretch(1);
    sTransport->addWidget(new QLabel(tr("Vol")));
    sTransport->addWidget(m_sVol);
    auto *sSeekRow = new QHBoxLayout;
    sSeekRow->addWidget(m_sSeek, 1);
    sSeekRow->addWidget(m_sTime);
    auto *sRight = new QVBoxLayout;
    sRight->addLayout(sTransport);
    sRight->addLayout(sSeekRow);
    sRight->addWidget(m_sSpec);
    sRight->addWidget(m_sTabs, 1);
    auto *sonora = new QWidget;
    auto *sLay = new QHBoxLayout(sonora);
    sLay->addWidget(m_sMeter);
    sLay->addLayout(sRight, 1);

    m_stack = new QStackedWidget;
    m_stack->addWidget(central);
    m_stack->addWidget(sonora);
    setCentralWidget(m_stack);
    qApp->installEventFilter(this);          // resize from the window edges (frameless window)

    refreshIcons();
}

void MainWindow::setSonoraLayout(bool on)
{
    if (!m_stack || (m_sonoraLayout == on && m_stack->currentIndex() == (on ? 1 : 0))) return;
    m_sonoraLayout = on;
    auto put = [](QWidget *w, QWidget *holder) { holder->layout()->addWidget(w); w->show(); };
    put(m_meter, on ? m_sMeter : m_aMeter);
    put(m_seek, on ? m_sSeek : m_aSeek);
    put(m_timeBtn, on ? m_sTime : m_aTime);
    put(m_vol, on ? m_sVol : m_aVol);
    put(m_spectrum, on ? m_sSpec : m_aSpec);
    put(m_tabs, on ? m_sTabs : m_aTabs);
    if (!m_spectrumAct->isChecked()) m_spectrum->hide();
    m_meter->setHorizontal(!on);
    m_timeBtn->setObjectName(on ? QString() : QStringLiteral("link"));
    m_timeBtn->setAutoRaise(true);
    m_timeBtn->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_timeBtn->setMinimumWidth(on ? QFontMetrics(m_timeBtn->font()).horizontalAdvance("-00:00:00 / 00:00:00") + 16 : 0);
    m_stack->setCurrentIndex(on ? 1 : 0);
    menuBar()->setVisible(on);
    const bool wasVisible = isVisible();
    setAttribute(Qt::WA_TranslucentBackground, !on);
    setWindowFlag(Qt::FramelessWindowHint, !on);
    setProperty("sonoraLayout", on);
    statusBar()->setProperty("sonoraLayout", on);
    statusBar()->style()->unpolish(statusBar());
    statusBar()->style()->polish(statusBar());
    if (wasVisible) show();
    updateTimeLabel(m_player->position(), m_player->duration());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::Polish) {
        if (auto *menu = qobject_cast<QMenu *>(obj)) {
            menu->setAttribute(Qt::WA_TranslucentBackground);
            menu->setWindowFlag(Qt::NoDropShadowWindowHint);
        }
    }
    if ((e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseMove) && !isMaximized()
        && (windowFlags() & Qt::FramelessWindowHint)) {
        auto *w = qobject_cast<QWidget *>(obj);
        if (w && w->window() == this) {
            auto *me = static_cast<QMouseEvent *>(e);
            const QPoint p = mapFromGlobal(me->globalPosition().toPoint());
            const int m = 6;
            Qt::Edges edges;
            if (p.x() <= m) edges |= Qt::LeftEdge;
            if (p.x() >= width() - m) edges |= Qt::RightEdge;
            if (p.y() <= m) edges |= Qt::TopEdge;
            if (p.y() >= height() - m) edges |= Qt::BottomEdge;
            if (e->type() == QEvent::MouseButtonPress && edges && me->button() == Qt::LeftButton && windowHandle()) {
                windowHandle()->startSystemResize(edges);
                return true;
            }
            if (e->type() == QEvent::MouseMove && me->buttons() == Qt::NoButton) {
                const bool h = edges & (Qt::LeftEdge | Qt::RightEdge), v = edges & (Qt::TopEdge | Qt::BottomEdge);
                const bool diag = (edges == (Qt::LeftEdge | Qt::TopEdge)) || (edges == (Qt::RightEdge | Qt::BottomEdge));
                if (h && v) setCursor(diag ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
                else if (h) setCursor(Qt::SizeHorCursor);
                else if (v) setCursor(Qt::SizeVerCursor);
                else unsetCursor();
            }
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::updateUpNext()
{
    if (!m_upNext) return;
    m_upNext->clear();
    PlaylistWidget *w = m_playing ? m_playing.data() : current();
    if (!w) return;
    PlaylistModel *m = w->model();
    auto *px = w->proxy();
    if (m_plChip) {
        const int i = m_tabs->indexOf(w);
        m_plChip->setText(tr("%1 · %n track(s)", nullptr, m->rowCount()).arg(i >= 0 ? m_tabs->tabText(i).remove('&') : QString()));
    }
    int start = 0;
    if (m->current() >= 0) {
        const QModelIndex pi = px->mapFromSource(m->index(m->current()));
        start = pi.isValid() ? pi.row() + 1 : 0;
    }
    for (int r = start; r < px->rowCount() && m_upNext->count() < 8; ++r) {
        const int src = px->mapToSource(px->index(r, 0)).row();
        const TrackInfo ti = m->infoAt(src);
        QString text = m->titleAt(src);
        if (!ti.artist.isEmpty()) text += QStringLiteral("\n") + ti.artist;
        auto *it = new QListWidgetItem;
        it->setData(Qt::UserRole, src);
        auto *row = new QWidget;
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 2, 4, 2);
        rl->setSpacing(10);
        const bool cue = m->urlAt(src).toString().contains(QStringLiteral(".cue#"));
        auto *tile = new QLabel(cue ? QStringLiteral("CUE") : QStringLiteral("♪"));
        tile->setObjectName(cue ? "tileCue" : "tile");
        tile->setFixedSize(34, 34);
        tile->setAlignment(Qt::AlignCenter);
        const QString pic = CoverArt::findCached(m->urlAt(src));
        if (!pic.isEmpty()) {
            QPixmap pm = m_thumbs.value(pic);
            if (pm.isNull()) {
                QImageReader r(pic);
                r.setAutoTransform(true);
                const qreal dpr = devicePixelRatioF();
                const QImage img = r.read().scaled(QSize(34, 34) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                pm = QPixmap(QSize(34, 34) * dpr);
                pm.fill(Qt::transparent);
                QPainter tp(&pm);
                tp.setRenderHint(QPainter::Antialiasing);
                QPainterPath cl;
                cl.addRoundedRect(QRectF(0, 0, 34 * dpr, 34 * dpr), 8 * dpr, 8 * dpr);
                tp.setClipPath(cl);
                tp.drawImage(QPointF((34 * dpr - img.width()) / 2, (34 * dpr - img.height()) / 2), img);
                tp.end();
                pm.setDevicePixelRatio(dpr);
                if (m_thumbs.size() > 200) m_thumbs.clear();
                m_thumbs.insert(pic, pm);
            }
            tile->setText(QString());
            tile->setPixmap(pm);
            tile->setStyleSheet(QStringLiteral("background: transparent;"));
        }
        auto *txt = new QLabel(QStringLiteral("<span>%1</span><br><span style='font-size:8.5pt'>%2</span>")
                                   .arg(m->titleAt(src).toHtmlEscaped(),
                                        (cue ? tr("from cue sheet") : ti.artist).toHtmlEscaped()));
        txt->setTextFormat(Qt::RichText);
        auto *dur = new QLabel(ti.durationMs > 0 ? formatDuration(ti.durationMs) : QString());
        dur->setObjectName("muted");
        rl->addWidget(tile);
        rl->addWidget(txt, 1);
        rl->addWidget(dur);
        it->setSizeHint(QSize(0, 46));
        m_upNext->addItem(it);
        m_upNext->setItemWidget(it, row);
    }
    if (m_upNext->count() == 0) {
        auto *it = new QListWidgetItem(tr("End of playlist"));
        it->setFlags(Qt::NoItemFlags);
        m_upNext->addItem(it);
    }
}

void MainWindow::updateHero()
{
    if (!m_heroTitle) return;
    const bool idle = !m_playing || m_player->state() == Player::Stopped || m_playing->model()->current() < 0;
    if (idle) {
        m_heroKicker->setText(tr("Stopped"));
        m_heroTitle->setText(tr("Nothing playing"));
        m_heroSub->setText(tr("Add files or pick a track from the playlist"));
        for (QLabel *c : m_chips) c->hide();
        m_coverFor = QUrl();
        static_cast<CoverTile *>(m_cover)->setImage(QImage());
        if (m_tray) { m_tray->setPlaying(false); m_tray->setTrack({}); }
        updateUpNext();
        return;
    }
    PlaylistModel *m = m_playing->model();
    const int row = m->current();
    const TrackInfo ti = m->infoAt(row);
    m_heroKicker->setText(m_player->state() == Player::Paused ? tr("Paused") : tr("Now playing"));
    m_heroTitle->setText(m->titleAt(row));
    const QUrl trackUrl = m->urlAt(row);
    if (trackUrl != m_coverFor) {
        m_coverFor = trackUrl;
        static_cast<CoverTile *>(m_cover)->setImage(QImage());
        if (m_mpris) m_mpris->setArtUrl({});
        CoverArt::findAsync(trackUrl, this, [this, trackUrl](const QString &pic) {
            if (trackUrl != m_coverFor) return;              // the track changed meanwhile
            QImageReader r(pic);
            r.setAutoTransform(true);
            static_cast<CoverTile *>(m_cover)->setImage(pic.isEmpty() ? QImage() : r.read());
            if (m_mpris) m_mpris->setArtUrl(pic.isEmpty() ? QString() : QUrl::fromLocalFile(pic).toString());
            updateUpNext();
        });
    }
    if (m_tray) {
        m_tray->setPlaying(m_player->state() == Player::Playing);
        m_tray->setTrack(ti.artist.isEmpty() ? m->titleAt(row) : ti.artist + QStringLiteral(" – ") + m->titleAt(row));
    }
    QStringList sub;
    if (!ti.artist.isEmpty()) sub << ti.artist;
    if (!ti.album.isEmpty()) sub << (ti.year > 0 ? QStringLiteral("%1 (%2)").arg(ti.album).arg(ti.year) : ti.album);
    m_heroSub->setText(sub.join(QStringLiteral(" · ")));
    QStringList chips;
    if (!ti.codec.isEmpty()) chips << ti.codec;
    if (ti.sampleRate > 0)
        chips << (ti.depth > 0 ? QStringLiteral("%1 kHz / %2 bit").arg(ti.sampleRate / 1000.0, 0, 'g', 3).arg(ti.depth)
                               : QStringLiteral("%1 kHz").arg(ti.sampleRate / 1000.0, 0, 'g', 3));
    if (ti.bitrate > 0) chips << QStringLiteral("%1 kb/s").arg(ti.bitrate / 1000);
    for (int i = 0; i < int(m_chips.size()); ++i) {
        m_chips[i]->setVisible(i < chips.size());
        if (i < chips.size()) m_chips[i]->setText(chips[i]);
    }
    updateUpNext();
}

// ================================================================== theme / icons
bool MainWindow::systemIsDark() const
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void MainWindow::applyTheme()
{
    static bool saved = false;
    static QString style;
    static QPalette pal;
    if (!saved) {                       // remember the system look to be able to return to it
        style = QApplication::style()->name();
        pal = QApplication::palette();
        saved = true;
    }
    auto *app = qApp;
    if (m_theme.startsWith("aurora")) {
        const bool dark = m_theme == "aurora-dark" || (m_theme == "aurora" && systemIsDark());
        QApplication::setStyle(QStringLiteral("Fusion"));
        QApplication::setPalette(Aurora::palette(dark));
        app->setStyleSheet(Aurora::styleSheet(dark));
    } else if (m_theme == "classic-dark") {         // Sonora dark: the original dark mode of Sonora
        QApplication::setStyle(QStringLiteral("Fusion"));
        QApplication::setPalette(darkPalette());
        app->setStyleSheet(QString());
    } else if (m_theme == "sonora") {               // Sonora: the original look with the system style
        if (!style.isEmpty()) QApplication::setStyle(style);
        QApplication::setPalette(pal);
        app->setStyleSheet(QString());
    } else {
        if (!style.isEmpty()) QApplication::setStyle(style);
        QApplication::setPalette(pal);
        app->setStyleSheet(Aurora::styleSheet(systemIsDark()).section("QSlider", 0, 0));
    }
    // dock / window icon follows the system colour scheme (the shell's colours), not the app theme
    const QIcon appIcon(systemIsDark() ? QStringLiteral(":/icons/app-dark.svg") : QStringLiteral(":/icons/app.svg"));
    QApplication::setWindowIcon(appIcon);
    setWindowIcon(appIcon);
    if (m_titleIcon) m_titleIcon->setPixmap(appIcon.pixmap(QSize(24, 24), devicePixelRatioF()));
    const bool sonora = m_theme == "sonora" || m_theme == "classic-dark";
    setSonoraLayout(sonora);
    if (m_spectrum) { m_spectrum->setProperty("aurora", !sonora); m_spectrum->update(); }
    if (m_btnPrev && m_tabs) refreshIcons();
}

void MainWindow::refreshIcons()
{
    using namespace TransportIcons;
    const QPalette pal = palette();
    const qreal dpr = devicePixelRatioF();
    const int sz = 22;
    m_btnPrev->setIcon(make(Prev, pal, sz, dpr));
    {
        QPalette white = pal;               // the big play button is accent-filled: white glyph
        white.setColor(QPalette::Active, QPalette::ButtonText, Qt::white);
        white.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::white);
        m_btnPlay->setIcon(make(m_player->state() == Player::Playing ? Pause : Play, white, 34, dpr));
    }
    m_btnStop->setIcon(make(Stop, pal, sz, dpr));
    m_btnNext->setIcon(make(Next, pal, sz, dpr));
    m_btnOpen->setIcon(make(Eject, pal, sz, dpr));
    m_btnRepeat->setIcon(make(Repeat, pal, sz, dpr));
    m_btnShuffle->setIcon(make(Shuffle, pal, sz, dpr));
    if (m_sPrev) {
        m_sPrev->setIcon(make(Prev, pal, sz, dpr));
        m_sPlay->setIcon(make(Play, pal, sz, dpr));
        m_sPause->setIcon(make(Pause, pal, sz, dpr));
        m_sStop->setIcon(make(Stop, pal, sz, dpr));
        m_sNext->setIcon(make(Next, pal, sz, dpr));
        m_sOpen->setIcon(make(Eject, pal, sz, dpr));
        m_sRepeat->setIcon(make(Repeat, pal, sz, dpr));
        m_sShuffle->setIcon(make(Shuffle, pal, sz, dpr));
    }
    updateTabIcons();
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    if ((e->type() == QEvent::PaletteChange || e->type() == QEvent::ApplicationPaletteChange)
        && m_btnPrev && m_tabs)
        refreshIcons();
}

void MainWindow::updateTabIcons()
{
    for (int i = 0; i < m_tabs->count(); ++i)
        m_tabs->setTabIcon(i, playlistAt(i) == m_playing
                              ? TransportIcons::make(TransportIcons::Play, palette(), 14, devicePixelRatioF())
                              : QIcon());
    scheduleManagerRefresh();
}

// ================================================================== preferences
void MainWindow::showPreferences()
{
    PreferencesDialog dlg(m_cfg, this);
    connect(&dlg, &PreferencesDialog::applied, this, &MainWindow::applyConfig);
    dlg.exec();
}

void MainWindow::applyConfig(const AppConfig &c)
{
    const bool audioChanged = !(c.audio == m_cfg.audio);
    const bool languageChanged = c.language != m_cfg.language;
    m_cfg = c;
    QSettings s;
    m_cfg.save(s);
    m_meter->setConfig(c.meter);
    m_spectrum->setConfig(c.spectrum);
    m_viz->setDelayMs(c.vizDelayMs);
    m_player->setSpectrumBands(c.spectrum.fftBands);
    m_player->setTruePeak(c.meter.truePeak);
    if (audioChanged) m_player->setOutput(c.audio);       // keeps the playing position
    updateOutputLabel();
    if (languageChanged
        && QMessageBox::question(this, tr("Language"), tr("Restart the player now to use the new language?"))
               == QMessageBox::Yes) {
        QProcess::startDetached(QApplication::applicationFilePath(), {});
        quitApp();
    }
}

void MainWindow::updateOutputLabel()
{
    m_outputLabel->setText(tr("Output: %1").arg(AudioOutput::describe(m_cfg.audio)));
}

// ================================================================== playlists
PlaylistWidget *MainWindow::playlistAt(int i) const
{
    return static_cast<PlaylistWidget *>(m_tabs->widget(i));
}

PlaylistWidget *MainWindow::current() const
{
    return static_cast<PlaylistWidget *>(m_tabs->currentWidget());
}

PlaylistWidget *MainWindow::createPlaylist(const QString &name, bool activate)
{
    auto *w = new PlaylistWidget;
    w->restoreHeader(m_tabs->count() > 0 ? current()->saveHeader() : m_headerState);   // same columns everywhere
    connect(w, &PlaylistWidget::playRequested, this, &MainWindow::playFrom);
    connect(w, &PlaylistWidget::contextMenuRequested, this, &MainWindow::showListMenu);
    const int idx = m_tabs->addTab(w, name.isEmpty() ? tr("Playlist %1").arg(++m_nameCounter) : name);
    if (activate) m_tabs->setCurrentIndex(idx);
    for (QAbstractItemModel *m : {static_cast<QAbstractItemModel *>(w->model()),
                                  static_cast<QAbstractItemModel *>(w->proxy())}) {
        connect(m, &QAbstractItemModel::rowsInserted, this, [this] { scheduleManagerRefresh(); });
        connect(m, &QAbstractItemModel::rowsRemoved,  this, [this] { scheduleManagerRefresh(); });
        connect(m, &QAbstractItemModel::modelReset,   this, [this] { scheduleManagerRefresh(); });
    }
    connect(w->model(), &QAbstractItemModel::dataChanged, this, [this, w] {
        scheduleManagerRefresh();                       // lengths change while metadata arrives
        if (w == m_playing) updateNowPlaying();         // tags of the playing track arrived
    });
    scheduleManagerRefresh();
    return w;
}

void MainWindow::renamePlaylist(int index)
{
    if (index < 0 || index >= m_tabs->count()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename playlist"), tr("Name:"),
                                               QLineEdit::Normal, m_tabs->tabText(index), &ok).trimmed();
    if (ok && !name.isEmpty()) { m_tabs->setTabText(index, name); scheduleManagerRefresh(); }
}

void MainWindow::closePlaylist(int index)
{
    if (index < 0 || index >= m_tabs->count()) return;
    PlaylistWidget *w = playlistAt(index);
    const int n = w->model()->rowCount();
    if (n > 0 && QMessageBox::question(this, tr("Close playlist"),
            tr("Close playlist “%1” (%2 items)?").arg(m_tabs->tabText(index)).arg(n)) != QMessageBox::Yes)
        return;

    if (w == m_playing) { m_player->stop(); m_playing = nullptr; }

    if (m_tabs->count() == 1) {            // always keep at least one playlist
        w->model()->clear();
        m_tabs->setTabText(0, tr("Playlist 1"));
    } else {
        m_tabs->removeTab(index);
        w->deleteLater();
    }
    updateTabIcons();
}

void MainWindow::showTabMenu(const QPoint &pos)
{
    const int i = m_tabs->tabBar()->tabAt(pos);
    QMenu menu;
    QAction *newA = menu.addAction(tr("New playlist"));
    QAction *renA = i >= 0 ? menu.addAction(tr("Rename…")) : nullptr;
    QAction *clsA = i >= 0 ? menu.addAction(tr("Close")) : nullptr;
    QAction *r = menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
    if (!r) return;
    if (r == newA) createPlaylist();
    else if (r == renA) renamePlaylist(i);
    else if (r == clsA) closePlaylist(i);
}

void MainWindow::showListMenu(PlaylistWidget *w, const QPoint &globalPos)
{
    QMenu menu;
    QAction *addF = menu.addAction(tr("Add files…"));
    QAction *addD = menu.addAction(tr("Add folder…"));
    menu.addSeparator();
    QAction *play = menu.addAction(tr("Play"));
    QMenu *copy = menu.addMenu(tr("Copy selected to playlist"));
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (playlistAt(i) == w) continue;
        copy->addAction(m_tabs->tabText(i))->setData(i);
    }
    QAction *newPl = copy->addAction(tr("New playlist"));
    QAction *rem = menu.addAction(tr("Remove selected"));

    QAction *r = menu.exec(globalPos);
    if (!r) return;
    if (r == addF) { addFiles(); return; }
    if (r == addD) { addFolder(); return; }
    if (r == play) {
        const QModelIndex i = w->view()->currentIndex();
        if (i.isValid()) playFrom(w, i.row());
    } else if (r == rem) {
        w->removeSelected();
    } else if (r == newPl) {
        createPlaylist(QString(), false)->model()->addUrls(w->selectedUrls());
    } else if (r->data().isValid()) {
        playlistAt(r->data().toInt())->model()->addUrls(w->selectedUrls());
    }
}

QString MainWindow::dataDir() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

void MainWindow::savePlaylists()
{
    QDir d(dataDir());
    const QStringList old = d.entryList({"playlist-*.m3u"}, QDir::Files);
    for (const QString &f : old) d.remove(f);

    QJsonArray arr;
    for (int i = 0; i < m_tabs->count(); ++i) {
        const QString fn = QStringLiteral("playlist-%1.m3u").arg(i);
        playlistAt(i)->model()->saveM3U(d.filePath(fn));
        arr.append(QJsonObject{{"name", m_tabs->tabText(i)}, {"file", fn}});
    }
    QJsonObject root{{"current", m_tabs->currentIndex()}, {"lists", arr}};
    QSaveFile f(d.filePath("playlists.json"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson());
        f.commit();
    }
}

void MainWindow::loadPlaylists()
{
    QDir d(dataDir());
    QFile f(d.filePath("playlists.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue &v : root["lists"].toArray()) {
            const QJsonObject o = v.toObject();
            PlaylistWidget *w = createPlaylist(o["name"].toString(), false);
            w->model()->loadM3U(d.filePath(o["file"].toString()));
        }
        if (m_tabs->count() > 0)
            m_tabs->setCurrentIndex(std::clamp(root["current"].toInt(), 0, m_tabs->count() - 1));
    }
    if (m_tabs->count() == 0) createPlaylist();
}

// ================================================================== adding / importing
void MainWindow::addPaths(const QStringList &paths)
{
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);

    QList<QUrl> urls;
    auto flush = [&] { current()->model()->addUrls(urls); urls.clear(); };
    auto addCue = [&](const QString &cuePath) {        // a cue sheet = one playlist entry per track
        const CueSheet cs = CueSheet::load(cuePath);
        if (!cs.isValid()) {
            statusBar()->showMessage(tr("%1: no playable tracks (audio file not found?)")
                                         .arg(QFileInfo(cuePath).fileName()), 8000);
            return;
        }
        for (const CueTrack &t : cs.tracks) urls << cs.trackUrl(t.number);
    };

    for (const QString &p : paths) {
        const QFileInfo fi(p);
        const QString suffix = fi.suffix().toLower();
        if (fi.isFile() && suffix == "cue") {
            addCue(fi.absoluteFilePath());
        } else if (fi.isFile() && kPlaylistSuffixes.contains(suffix)) {
            flush();                                   // keep the order of mixed input
            current()->model()->importFile(fi.absoluteFilePath());
        } else if (fi.isDir()) {
            QStringList found;
            QDirIterator it(fi.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString f = it.next();
                const QString sfx = QFileInfo(f).suffix().toLower();
                if (sfx == "cue" || kAudioSuffixes.contains(sfx)) found << f;
            }
            std::sort(found.begin(), found.end(), [&](const QString &a, const QString &b) {
                return coll.compare(a, b) < 0;
            });
            QSet<QString> covered;                     // audio files that a cue sheet splits into tracks
            for (const QString &f : found)
                if (QFileInfo(f).suffix().compare("cue", Qt::CaseInsensitive) == 0)
                    covered |= CueSheet::load(f).audioFiles();
            for (const QString &f : found) {
                if (QFileInfo(f).suffix().compare("cue", Qt::CaseInsensitive) == 0)
                    addCue(f);
                else if (!covered.contains(QFileInfo(f).absoluteFilePath()))
                    urls << QUrl::fromLocalFile(f);    // not listed twice: as one big file AND as tracks
            }
        } else if (fi.exists()) {
            urls << QUrl::fromLocalFile(fi.absoluteFilePath());
        } else if (p.contains("://")) {
            urls << QUrl(p);
        }
    }
    flush();
}

void MainWindow::playFirst() { playFrom(current(), 0); }

void MainWindow::addFiles()
{
    QStringList globs;
    for (const QString &e : kAudioSuffixes + kPlaylistSuffixes + QStringList{"cue"}) globs << "*." + e;
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add files to playlist"), {},
        tr("Audio, cue sheet and playlist files (%1);;All files (*)").arg(globs.join(' ')));
    if (!files.isEmpty()) addPaths(files);
}

void MainWindow::addFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add folder"));
    if (!dir.isEmpty()) addPaths({dir});
}

void MainWindow::addUrl()
{
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("Add URL"),
                            tr("Stream or file URL (http://…, https://…, file://…):"),
                            QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || text.isEmpty()) return;
    const QUrl url = QUrl::fromUserInput(text);
    if (!url.isValid() || url.scheme().isEmpty()) {
        statusBar()->showMessage(tr("Invalid URL: %1").arg(text), 6000);
        return;
    }
    current()->model()->addUrls({url});
}

void MainWindow::importPlaylist()          // every chosen file becomes its own new playlist
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Import playlist"), {},
                                tr("Playlists (*.m3u *.m3u8 *.pls);;All files (*)"));
    for (const QString &file : files) {
        PlaylistWidget *w = createPlaylist(QFileInfo(file).completeBaseName());
        if (!w->model()->importFile(file))
            statusBar()->showMessage(tr("Cannot read %1").arg(file), 6000);
    }
}

void MainWindow::importIntoCurrent()       // entries are appended to the visible playlist
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Import playlist into current"), {},
                                tr("Playlists (*.m3u *.m3u8 *.pls);;All files (*)"));
    for (const QString &file : files) {
        const int before = current()->model()->rowCount();
        if (current()->model()->importFile(file))
            statusBar()->showMessage(tr("Imported %1 items from %2")
                .arg(current()->model()->rowCount() - before).arg(QFileInfo(file).fileName()), 6000);
        else
            statusBar()->showMessage(tr("Cannot read %1").arg(file), 6000);
    }
}

namespace {
QString safeFileName(QString s)
{
    for (const QChar c : QStringLiteral("/\\:*?\"<>|")) s.replace(c, QLatin1Char('_'));
    s = s.trimmed();
    return s.isEmpty() ? QStringLiteral("playlist") : s;
}
}

void MainWindow::exportPlaylist()
{
    PlaylistWidget *w = current();
    const QString name = m_tabs->tabText(m_tabs->currentIndex());
    const int total = w->model()->rowCount();
    const int shown = w->proxy()->rowCount();
    const int selected = int(w->view()->selectionModel()->selectedRows().size());

    ExportDialog dlg(name, total, shown, selected, m_tabs->count(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    const PlaylistExporter::Options opt = dlg.options();
    const QString ext = PlaylistExporter::suffix(opt.format);

    // source rows of a playlist for the chosen scope (shown / selected: in the order you see)
    auto rowsOf = [](PlaylistWidget *pw, ExportDialog::Scope scope) {
        QList<int> rows;
        if (scope == ExportDialog::Shown) {
            for (int r = 0; r < pw->proxy()->rowCount(); ++r)
                rows << pw->proxy()->mapToSource(pw->proxy()->index(r, 0)).row();
        } else if (scope == ExportDialog::Selected) {
            QModelIndexList sel = pw->view()->selectionModel()->selectedRows();
            std::sort(sel.begin(), sel.end(), [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
            for (const QModelIndex &i : sel) rows << pw->proxy()->mapToSource(i).row();
        } else {
            for (int r = 0; r < pw->model()->rowCount(); ++r) rows << r;
        }
        return rows;
    };

    QSettings st;
    const QString startDir = st.value("exportDir", QDir::homePath()).toString();

    if (dlg.scope() == ExportDialog::AllPlaylists) {          // one file per playlist into a folder
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Export all playlists into folder"), startDir);
        if (dir.isEmpty()) return;
        QStringList files;
        QSet<QString> used;
        for (int i = 0; i < m_tabs->count(); ++i) {
            const QString base = safeFileName(m_tabs->tabText(i));
            QString file = QDir(dir).filePath(base + '.' + ext);
            for (int n = 2; used.contains(file.toLower()); ++n)
                file = QDir(dir).filePath(QStringLiteral("%1 (%2).%3").arg(base).arg(n).arg(ext));
            used.insert(file.toLower());
            files << file;
        }
        QStringList existing;
        for (const QString &f : files) if (QFileInfo::exists(f)) existing << QFileInfo(f).fileName();
        if (!existing.isEmpty() &&
            QMessageBox::question(this, tr("Overwrite files?"),
                tr("These files already exist and will be overwritten:\n\n%1").arg(existing.join('\n'))) != QMessageBox::Yes)
            return;

        int ok = 0;
        QString err;
        for (int i = 0; i < m_tabs->count(); ++i) {
            PlaylistWidget *pw = playlistAt(i);
            if (PlaylistExporter::write(files[i], pw->model()->exportItems(rowsOf(pw, ExportDialog::Whole)), opt, &err)) ++ok;
            else statusBar()->showMessage(err, 8000);
        }
        st.setValue("exportDir", dir);
        if (ok == m_tabs->count())
            statusBar()->showMessage(tr("Exported %1 playlists to %2").arg(ok).arg(dir), 8000);
        return;
    }

    QString file = QFileDialog::getSaveFileName(this, tr("Export playlist"),
                       QDir(startDir).filePath(safeFileName(name) + '.' + ext),
                       PlaylistExporter::fileFilter(opt.format));
    if (file.isEmpty()) return;
    if (QFileInfo(file).suffix().isEmpty()) file += '.' + ext;

    const QList<PlaylistModel::ExportItem> items = w->model()->exportItems(rowsOf(w, dlg.scope()));
    QString err;
    if (PlaylistExporter::write(file, items, opt, &err)) {
        st.setValue("exportDir", QFileInfo(file).absolutePath());
        statusBar()->showMessage(tr("Exported %1 entries to %2").arg(items.size()).arg(file), 8000);
    } else {
        statusBar()->showMessage(err, 10000);
        QMessageBox::warning(this, tr("Export failed"), err);
    }
}

// ================================================================== playback
void MainWindow::playFrom(PlaylistWidget *w, int proxyRow)
{
    if (!w) return;
    const QModelIndex pidx = w->proxy()->index(proxyRow, 0);
    const QModelIndex src = w->proxy()->mapToSource(pidx);
    if (!src.isValid()) return;

    // For a CUE track this is the audio file plus the range of the track.
    const PlaylistModel::PlayTarget target = w->model()->playTarget(src.row());
    if (!target.url.isValid()) {                       // e.g. the audio file of the cue sheet is missing
        statusBar()->showMessage(tr("Cannot find the audio file for “%1”").arg(w->model()->titleAt(src.row())), 8000);
        if (w->proxy()->rowCount() > 1 && ++m_errorStreak < w->proxy()->rowCount()) {
            m_playing = w;                             // so that Next continues after this entry
            w->model()->setCurrent(src.row());
            QMetaObject::invokeMethod(this, [this] { step(+1); }, Qt::QueuedConnection);
        }
        return;
    }

    if (m_playing && m_playing != w) m_playing->model()->setCurrent(-1);
    m_playing = w;
    w->model()->setCurrent(src.row());
    m_viz->clear();                                   // nothing of the previous track may show up late
    m_player->play(target.url, target.startMs, target.stopMs);
    w->view()->scrollTo(pidx);
    updateTabIcons();
    updateNowPlaying();
}

// Navigation happens inside the *playing* playlist and only through its *filtered*
// (and possibly sorted) view. If the playing item is not part of the filter, Next
// starts at the first match, Previous at the last.
void MainWindow::step(int delta)
{
    PlaylistWidget *w = m_playing ? m_playing.data() : current();
    const int n = w->proxy()->rowCount();
    if (n == 0) { m_player->stop(); return; }

    const QModelIndex cur = w->proxy()->mapFromSource(w->model()->index(w->model()->current()));
    int row;
    if (m_shuffle->isChecked()) {                       // random item of the filtered view
        if (n == 1) {
            if (cur.isValid() && !m_repeat->isChecked()) { m_player->stop(); return; }
            row = 0;
        } else {
            row = QRandomGenerator::global()->bounded(n - 1);
            if (cur.isValid() && row >= cur.row()) ++row;   // never the same item twice in a row
        }
    } else {
        row = cur.isValid() ? cur.row() + delta : (delta > 0 ? 0 : n - 1);
    }

    if (row < 0 || row >= n) {
        if (!m_repeat->isChecked()) { m_player->stop(); return; }
        row = (row + n) % n;
    }
    playFrom(w, row);
}

void MainWindow::playClicked()
{
    switch (m_player->state()) {
    case Player::Paused:  m_player->resume(); break;
    case Player::Playing: m_player->seek(0); break;          // like Audacious: restart the track
    case Player::Stopped: {
        PlaylistWidget *w = current();       // play from the playlist you are looking at
        const QModelIndex cur = w->proxy()->mapFromSource(w->model()->index(w->model()->current()));
        const QModelIndex sel = w->view()->currentIndex();
        playFrom(w, cur.isValid() ? cur.row() : sel.isValid() ? sel.row() : 0);
        break;
    }
    }
}

void MainWindow::pauseClicked()
{
    if (m_player->state() == Player::Playing) m_player->pause();
    else if (m_player->state() == Player::Paused) m_player->resume();
    m_btnPause->setChecked(m_player->state() == Player::Paused);
    if (m_sPause) m_sPause->setChecked(m_player->state() == Player::Paused);
}

void MainWindow::pauseOnly()
{
    if (m_player->state() == Player::Playing) m_player->pause();
    m_btnPause->setChecked(m_player->state() == Player::Paused);
    if (m_sPause) m_sPause->setChecked(m_player->state() == Player::Paused);
}

// Space and the keyboard's Play/Pause key: pause/resume, or start playing when stopped.
void MainWindow::togglePlayPause()
{
    if (m_player->state() == Player::Stopped) playClicked();
    else pauseClicked();
}

void MainWindow::stopPlayback() { m_player->stop(); }

void MainWindow::onStateChanged(Player::State s)
{
    if (m_btnPrev && m_tabs) refreshIcons();
    if (m_sPause) m_sPause->setChecked(s == Player::Paused);
    if (m_mpris) m_mpris->setState(s);
    m_viz->setPaused(s == Player::Paused);          // sound stopped: stop feeding the displays ...
    if (s == Player::Paused) {                      // ... and let LEDs / bars / peak markers drop at once
        m_meter->release();
        m_spectrum->release();
    }
    if (s == Player::Stopped) {
        m_viz->clear();
        m_meter->reset();
        m_spectrum->reset();
        m_seek->setRange(0, 0);
        m_timeBtn->setText("0:00");
        m_elapsed->setText("0:00");
    }
    updateNowPlaying();
}

void MainWindow::updateNowPlaying()
{
    updateHero();
    if (!m_mpris) return;
    if (!m_playing || m_player->state() == Player::Stopped || m_playing->model()->current() < 0) {
        setWindowTitle("Linux Aurora Player");
        m_mpris->clearTrack();
        return;
    }
    PlaylistModel *m = m_playing->model();
    const int row = m->current();
    const TrackInfo ti = m->infoAt(row);
    const QString title = m->titleAt(row);
    setWindowTitle((ti.artist.isEmpty() ? title : ti.artist + QStringLiteral(" – ") + title) + " — Linux Aurora Player");
    m_mpris->setTrack(title, ti.artist.isEmpty() ? QStringList() : QStringList{ti.artist}, ti.album,
                      m->urlAt(row), ti.durationMs > 0 ? ti.durationMs * 1000 : 0, ti.track);
}

void MainWindow::updateTimeLabel(qint64 pos, qint64 dur)
{
    QString a = (m_showRemaining && dur > 0)
        ? QStringLiteral("-") + formatDuration(std::max<qint64>(0, dur - pos))
        : formatDuration(pos);
    if (m_sonoraLayout) {                         // original Sonora: "0:15 / 2:56" in one button
        m_timeBtn->setText(dur > 0 ? a + QStringLiteral(" / ") + formatDuration(dur) : a);
        return;
    }
    m_elapsed->setText(formatDuration(pos));
    m_timeBtn->setText(dur <= 0 ? QStringLiteral("--:--")
                       : m_showRemaining ? QStringLiteral("−") + formatDuration(std::max<qint64>(0, dur - pos))
                                         : formatDuration(dur));
    Q_UNUSED(a);
}

void MainWindow::onTick()
{
    if (m_player->state() == Player::Stopped || m_seeking) return;
    const qint64 dur = m_player->duration();
    const qint64 pos = m_player->position();
    if (m_player->state() == Player::Playing && pos > 500) m_errorStreak = 0;
    m_seek->setRange(0, int(std::min<qint64>(dur, INT_MAX)));
    m_seek->setValue(int(std::min<qint64>(pos, INT_MAX)));
    updateTimeLabel(pos, dur);
}

// ================================================================== playlist manager
// Audacious-like overview of ALL playlists: name, entry count, total length.
void MainWindow::buildManager()
{
    m_mgr = new QTreeWidget;
    m_mgr->setColumnCount(3);
    m_mgr->setHeaderLabels({tr("Playlist"), tr("Entries"), tr("Length")});
    m_mgr->setRootIsDecorated(false);
    m_mgr->setUniformRowHeights(true);
    m_mgr->setSelectionMode(QAbstractItemView::SingleSelection);
    m_mgr->setContextMenuPolicy(Qt::CustomContextMenu);
    m_mgr->header()->setStretchLastSection(false);
    m_mgr->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_mgr->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_mgr->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_mgr->setToolTip(tr("Click: show playlist.  Double-click: play it."));

    auto *bNew    = new QPushButton(tr("New"));
    auto *bImport = new QPushButton(tr("Import…"));
    auto *bClose  = new QPushButton(tr("Close"));
    for (QPushButton *b : {bNew, bImport, bClose}) b->setFocusPolicy(Qt::NoFocus);
    bNew->setToolTip(tr("New playlist (Ctrl+T)"));
    bImport->setToolTip(tr("Import playlist files as new playlists (Ctrl+I)"));
    bClose->setToolTip(tr("Close selected playlist"));
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(bNew);
    buttons->addWidget(bImport);
    buttons->addWidget(bClose);

    m_mgrInfo = new QLabel;
    auto *panel = new QWidget;
    panel->setMinimumWidth(240);
    auto *lay = new QVBoxLayout(panel);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addWidget(m_mgr, 1);
    lay->addWidget(m_mgrInfo);
    lay->addLayout(buttons);

    m_dock = new QDockWidget(tr("Playlists"), this);
    m_dock->setObjectName("playlistManager");
    m_dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, m_dock);            // visible by default
    QAction *toggle = m_dock->toggleViewAction();
    toggle->setText(tr("&Playlist manager"));
    toggle->setShortcut(QKeySequence("Ctrl+Y"));
    m_viewMenu->addAction(toggle);

    m_mgrTimer = new QTimer(this);
    m_mgrTimer->setSingleShot(true);
    m_mgrTimer->setInterval(80);
    connect(m_mgrTimer, &QTimer::timeout, this, &MainWindow::refreshManager);

    connect(m_mgr, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *cur) {
        if (!cur || m_mgrBusy) return;
        const int i = cur->data(0, Qt::UserRole).toInt();
        if (i != m_tabs->currentIndex()) m_tabs->setCurrentIndex(i);
    });
    connect(m_mgr, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it) {
        playPlaylistAt(it->data(0, Qt::UserRole).toInt());
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int i) {
        for (int r = 0; r < m_mgr->topLevelItemCount(); ++r) {
            QTreeWidgetItem *it = m_mgr->topLevelItem(r);
            if (it->data(0, Qt::UserRole).toInt() == i) {
                m_mgrBusy = true; m_mgr->setCurrentItem(it); m_mgrBusy = false;
                break;
            }
        }
    });
    connect(m_tabs->tabBar(), &QTabBar::tabMoved, this, [this] { scheduleManagerRefresh(); });

    connect(bNew, &QPushButton::clicked, this, [this] { createPlaylist(); });
    connect(bImport, &QPushButton::clicked, this, &MainWindow::importPlaylist);
    connect(bClose, &QPushButton::clicked, this, [this] { closePlaylist(m_tabs->currentIndex()); });

    connect(m_mgr, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTreeWidgetItem *it = m_mgr->itemAt(pos);
        const int i = it ? it->data(0, Qt::UserRole).toInt() : -1;
        QMenu menu;
        QAction *play = i >= 0 ? menu.addAction(tr("Play")) : nullptr;
        QAction *ren  = i >= 0 ? menu.addAction(tr("Rename…")) : nullptr;
        QAction *cls  = i >= 0 ? menu.addAction(tr("Close")) : nullptr;
        if (i >= 0) menu.addSeparator();
        QAction *nw   = menu.addAction(tr("New playlist"));
        QAction *imp  = menu.addAction(tr("Import playlist…"));
        QAction *r = menu.exec(m_mgr->viewport()->mapToGlobal(pos));
        if (!r) return;
        if (r == play) playPlaylistAt(i);
        else if (r == ren) renamePlaylist(i);
        else if (r == cls) closePlaylist(i);
        else if (r == nw) createPlaylist();
        else if (r == imp) importPlaylist();
    });

    refreshManager();
}

void MainWindow::scheduleManagerRefresh()
{
    if (m_mgrTimer && !m_mgrTimer->isActive()) m_mgrTimer->start();
}

void MainWindow::refreshManager()
{
    if (!m_mgr) return;
    m_mgrBusy = true;
    m_mgr->clear();

    int totalEntries = 0;
    QTreeWidgetItem *currentItem = nullptr;
    for (int i = 0; i < m_tabs->count(); ++i) {
        PlaylistWidget *w = playlistAt(i);
        const int total = w->model()->rowCount();
        const int shown = w->proxy()->rowCount();
        const qint64 len = w->model()->totalDurationMs();
        totalEntries += total;

        auto *it = new QTreeWidgetItem(m_mgr);
        it->setText(0, m_tabs->tabText(i));
        it->setText(1, shown == total ? QString::number(total)
                                      : QStringLiteral("%1 / %2").arg(shown).arg(total));   // filter active
        it->setText(2, len > 0 ? formatDuration(len) : QString());
        it->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        it->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        it->setData(0, Qt::UserRole, i);
        if (w == m_playing) {
            it->setIcon(0, TransportIcons::make(TransportIcons::Play, palette(), 14, devicePixelRatioF()));
            QFont f = it->font(0); f.setBold(true); it->setFont(0, f);
        }
        if (i == m_tabs->currentIndex()) currentItem = it;
    }
    if (currentItem) m_mgr->setCurrentItem(currentItem);
    m_mgrInfo->setText(tr("%n playlist(s), %1 entries", nullptr, m_tabs->count()).arg(totalEntries));
    m_mgrBusy = false;
}

void MainWindow::playPlaylistAt(int index)
{
    PlaylistWidget *w = playlistAt(index);
    if (!w) return;
    m_tabs->setCurrentIndex(index);
    const QModelIndex cur = w->proxy()->mapFromSource(w->model()->index(w->model()->current()));
    playFrom(w, cur.isValid() ? cur.row() : 0);
}

// ================================================================== misc
void MainWindow::quitApp()
{
    m_quitting = true;
    close();
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    // window close button: keep running in the panel (tray) when that is on; Quit really quits
    if (!m_quitting && m_closeToTrayAct->isChecked() && m_tray && m_tray->isAvailable()) {
        e->ignore();
        hide();
        return;
    }
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);   // real close: end the app (no implicit quit)
    QSettings s;
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());
    s.setValue("volume", m_vol->value());
    s.setValue("repeat", m_repeat->isChecked());
    s.setValue("shuffle", m_shuffle->isChecked());
    s.setValue("spectrum", m_spectrumAct->isChecked());
    s.setValue("remaining", m_remainAct->isChecked());
    s.setValue("closeToTray", m_closeToTrayAct->isChecked());
    s.setValue("theme", m_theme);
    s.setValue("playlistVisible", m_playlistAct->isChecked());
    s.setValue("splitter", m_split->saveState());
    s.setValue("playlistHeader", current()->saveHeader());
    savePlaylists();
    m_player->stop();
    MetadataLoader::instance().shutdown(dataDir() + "/metadata.cache");
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QStringList paths;
    for (const QUrl &u : e->mimeData()->urls())
        if (u.isLocalFile()) paths << u.toLocalFile();
    addPaths(paths);
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Linux Aurora Player"),
        tr("<b>Linux Aurora Player %1</b><br>A fork of Sonora by Przemysław Guza (GPL-3.0+).<br>An Audacious-style player: multiple playlists with metadata columns, "
           "LED level meters, spectrum analyser, selectable sound service, dark mode and media-key "
           "support.<br>Qt 6 + GStreamer.").arg(QApplication::applicationVersion()));
}
