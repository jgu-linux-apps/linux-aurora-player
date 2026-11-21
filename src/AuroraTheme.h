#pragma once
// Aurora: the JGu Linux Apps series theme (violet + cyan), light and dark.
// Applied as a Fusion palette plus a style sheet for the C+ layout widgets
// (cards, round transport buttons, chips). Colours match the GTK apps of the series.
#include <QColor>
#include <QPalette>
#include <QString>

namespace Aurora {

struct Colors
{
    QColor bg, fg, muted, view, card, cardAlt, border, acc, acc2, accFg, hero, heroFg, heroMuted, chip;
};

Colors colors(bool dark);
QPalette palette(bool dark);
QString styleSheet(bool dark);

} // namespace Aurora
