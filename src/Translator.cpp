#include "Translator.h"
#include <QHash>
#include <QLocale>

const QHash<QString, QString> &polishTexts();

namespace {
// Polish plural: 1 utwór, 2-4 utwory (not 12-14), 5+ utworów
QString pl(int n, const char *one, const char *few, const char *many)
{
    if (n == 1) return QString::fromUtf8(one);
    const int d = n % 10, h = n % 100;
    if (d >= 2 && d <= 4 && !(h >= 12 && h <= 14)) return QString::fromUtf8(few);
    return QString::fromUtf8(many);
}
}

QString AppTranslator::translate(const char *, const char *sourceText, const char *, int n) const
{
    const QString src = QString::fromUtf8(sourceText);
    if (src == QStringLiteral("%1 · %n track(s)"))
        return QStringLiteral("%1 · %n ") + pl(n, "utwór", "utwory", "utworów");
    if (src == QLatin1String("%n playlist(s), %1 entries"))
        return QStringLiteral("%n ") + pl(n, "playlista", "playlisty", "playlist") + QStringLiteral(", pozycji: %1");
    return polishTexts().value(src);
}

bool usePolish(const QString &setting)
{
    if (setting == QLatin1String("pl")) return true;
    if (setting == QLatin1String("en")) return false;
    return QLocale::system().language() == QLocale::Polish;
}
