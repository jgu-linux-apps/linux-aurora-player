#include "AuroraTheme.h"

namespace Aurora {

Colors colors(bool dark)
{
    if (dark)
        return {QColor("#0c0e1a"), QColor("#ecebff"), QColor("#9a98b8"), QColor("#13151f"), QColor("#181a25"),
                QColor("#1f2130"), QColor("#2a2d40"), QColor("#a855f7"), QColor("#22d3ee"), QColor("#ffffff"),
                QColor("#1d1640"), QColor("#f1ecff"), QColor("#b9aef0"), QColor("#2a2350")};
    return {QColor("#f6f3ff"), QColor("#1f1a3a"), QColor("#6b6690"), QColor("#fcfbff"), QColor("#fdfcff"),
            QColor("#f1edff"), QColor("#e2dcf7"), QColor("#7c5cff"), QColor("#22b8e6"), QColor("#ffffff"),
            QColor("#ece6ff"), QColor("#26215c"), QColor("#534ab7"), QColor("#ffffff")};
}

QPalette palette(bool dark)
{
    const Colors c = colors(dark);
    QPalette p;
    p.setColor(QPalette::Window, c.bg);
    p.setColor(QPalette::WindowText, c.fg);
    p.setColor(QPalette::Base, c.view);
    p.setColor(QPalette::AlternateBase, c.cardAlt);
    p.setColor(QPalette::ToolTipBase, c.card);
    p.setColor(QPalette::ToolTipText, c.fg);
    p.setColor(QPalette::Text, c.fg);
    p.setColor(QPalette::Button, c.card);
    p.setColor(QPalette::ButtonText, c.fg);
    p.setColor(QPalette::BrightText, QColor("#ff5c7a"));
    p.setColor(QPalette::Link, c.acc2);
    p.setColor(QPalette::Highlight, c.acc);
    p.setColor(QPalette::HighlightedText, c.accFg);
    p.setColor(QPalette::PlaceholderText, c.muted);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Dark, c.border.darker(130));
    p.setColor(QPalette::Light, c.cardAlt);
    p.setColor(QPalette::Midlight, c.cardAlt);
    p.setColor(QPalette::Shadow, dark ? QColor(0, 0, 0) : QColor(160, 150, 200));
    const QColor dis = c.muted.darker(dark ? 140 : 80);
    p.setColor(QPalette::Disabled, QPalette::WindowText, dis);
    p.setColor(QPalette::Disabled, QPalette::Text, dis);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
    return p;
}

QString styleSheet(bool dark)
{
    const Colors c = colors(dark);
    auto n = [](const QColor &x) { return x.name(); };
    QString s = QStringLiteral(R"(
QMainWindow, QMenuBar { background: %1; }
QFrame#titleBar { background: %1; border-bottom: 1px solid %3; }
QLabel#titleText { font-weight: 600; font-size: 11pt; }
QToolButton#barBtn { border: none; border-radius: 15px; min-width: 30px; min-height: 30px; font-size: 13pt; }
QToolButton#barBtn::menu-indicator { image: none; width: 0; }
QToolButton#barBtn:hover, QToolButton#winBtn:hover { background: %12; }
QToolButton#winBtn, QToolButton#winClose { border: none; border-radius: 13px; min-width: 26px; min-height: 26px; background: %12; }
QToolButton#winClose:hover { background: #e5484d; color: white; }
QFrame#card { background: %2; border: 1px solid %3; border-radius: 14px; }
QFrame#hero { background: %4; border: none; border-radius: 18px; }
QLabel#heroKicker { color: %5; font-size: 9pt; }
QLabel#heroTitle { color: %6; font-size: 17pt; font-weight: 600; }
QLabel#heroSub { color: %5; font-size: 10pt; }
QLabel#chip { background: %7; color: %5; border-radius: 9px; padding: 2px 9px; font-size: 8.5pt; }
QLabel#cardTitle { font-weight: 600; }
QLabel#muted { color: %8; }
QLabel#peak { color: #e5484d; font-weight: 600; }
QLabel#tile { background: %4; color: %5; border-radius: 8px; font-size: 13pt; }
QLabel#tileCue { background: #dff5ec; color: #0f6e56; border-radius: 8px; font-size: 7.5pt; font-weight: 600; }
QToolButton#round { background: %2; border: 1px solid %3; border-radius: 23px; }
QToolButton#round:hover { border-color: %9; }
QToolButton#round:checked { border: 2px solid %9; }
QToolButton#small { background: %2; border: 1px solid %3; border-radius: 17px; }
QToolButton#small:hover { border-color: %9; }
QToolButton#bigPlay { background: %9; border: none; border-radius: 38px; }
QToolButton#bigPlay:hover { background: %10; }
QToolButton#link { color: %11; border: none; background: transparent; }
QToolButton#link:hover { text-decoration: underline; }
QListWidget#upNext { background: transparent; border: none; }
QListWidget#upNext::item { padding: 6px 2px; border-radius: 8px; }
QListWidget#upNext::item:hover { background: %12; }
QSlider::groove:horizontal { height: 6px; background: %12; border-radius: 3px; }
QSlider::sub-page:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %9, stop:1 %11); border-radius: 3px; }
QSlider::handle:horizontal { background: %9; width: 14px; height: 14px; margin: -4px 0; border-radius: 7px; }
QTabWidget::pane { border: 1px solid %3; border-radius: 12px; background: %13; }
QTabBar::tab { padding: 6px 14px; border: none; color: %8; }
QTabBar::tab:selected { color: %14; border-bottom: 2px solid %9; }
QTreeView { background: %13; border: none; border-radius: 12px; }
QTreeView::item:selected { background: %9; color: %15; }
QHeaderView::section { background: %16; color: %8; border: none; padding: 5px 8px; }
QStatusBar { color: %8; }
)");
    return s.arg(n(c.bg), n(c.card), n(c.border), n(c.hero), n(c.heroMuted), n(c.heroFg), n(c.chip), n(c.muted),
                 n(c.acc))
        .arg(n(c.acc.lighter(115)), n(c.acc2), n(c.cardAlt), n(c.view), n(c.fg), n(c.accFg), n(c.card));
}

} // namespace Aurora
