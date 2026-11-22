#include "CoverArt.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QStandardPaths>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <thread>

namespace {

QString localPath(const QUrl &url)
{
    QUrl u = url;
    u.setFragment(QString());                 // cue sheet tracks: file:///x.cue#N
    return u.isLocalFile() ? u.toLocalFile() : QString();
}

QString cacheDir()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                      + QStringLiteral("/linux-aurora-player/covers");
    QDir().mkpath(d);
    return d;
}

QString cacheFile(const QString &audioPath)
{
    const QByteArray h = QCryptographicHash::hash(audioPath.toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(h);
}

QString folderPicture(const QString &audioPath)
{
    const QDir dir = QFileInfo(audioPath).absoluteDir();
    static const QStringList preferred = {"cover", "folder", "front", "album", "albumart", "albumartsmall"};
    const QStringList files = dir.entryList({"*.jpg", "*.jpeg", "*.png", "*.webp", "*.JPG", "*.JPEG", "*.PNG"},
                                            QDir::Files | QDir::Readable);
    for (const QString &want : preferred)
        for (const QString &f : files)
            if (QFileInfo(f).completeBaseName().toLower() == want) return dir.filePath(f);
    for (const QString &f : files)
        if (f.toLower().startsWith(QLatin1String("albumart"))) return dir.filePath(f);
    return {};
}

QString embeddedPicture(const QString &audioPath)
{
    const QString cached = cacheFile(audioPath);
    if (QFileInfo::exists(cached)) return QFileInfo(cached).size() > 0 ? cached : QString();
    QString result;
    GError *err = nullptr;
    GstDiscoverer *disc = gst_discoverer_new(5 * GST_SECOND, &err);
    if (disc) {
        const QByteArray uri = QUrl::fromLocalFile(audioPath).toEncoded();
        if (GstDiscovererInfo *info = gst_discoverer_discover_uri(disc, uri.constData(), nullptr)) {
            if (const GstTagList *tags = gst_discoverer_info_get_tags(info)) {
                GstSample *sample = nullptr;
                if (gst_tag_list_get_sample(tags, GST_TAG_IMAGE, &sample)
                    || gst_tag_list_get_sample(tags, GST_TAG_PREVIEW_IMAGE, &sample)) {
                    GstMapInfo map;
                    GstBuffer *buf = gst_sample_get_buffer(sample);
                    if (buf && gst_buffer_map(buf, &map, GST_MAP_READ)) {
                        QFile f(cached);
                        if (f.open(QIODevice::WriteOnly)) {
                            f.write(reinterpret_cast<const char *>(map.data), qint64(map.size));
                            result = cached;
                        }
                        gst_buffer_unmap(buf, &map);
                    }
                    gst_sample_unref(sample);
                }
            }
            g_object_unref(info);
        }
        g_object_unref(disc);
    }
    if (err) g_error_free(err);
    if (result.isEmpty()) QFile(cached).open(QIODevice::WriteOnly);   // empty marker: "no embedded picture"
    return result;
}

} // namespace

QString CoverArt::findCached(const QUrl &trackUrl)
{
    const QString p = localPath(trackUrl);
    if (p.isEmpty()) return {};
    const QString folder = folderPicture(p);
    if (!folder.isEmpty()) return folder;
    const QString c = cacheFile(p);
    return QFileInfo(c).size() > 0 ? c : QString();
}

void CoverArt::findAsync(const QUrl &trackUrl, QObject *context, std::function<void(const QString &)> done)
{
    const QString p = localPath(trackUrl);
    if (p.isEmpty()) { done({}); return; }
    QPointer<QObject> ctx(context);
    std::thread([p, ctx, done] {
        QString pic = folderPicture(p);
        if (pic.isEmpty() && !p.endsWith(QLatin1String(".cue"), Qt::CaseInsensitive)) pic = embeddedPicture(p);
        if (ctx) QMetaObject::invokeMethod(ctx.data(), [done, pic] { done(pic); }, Qt::QueuedConnection);
    }).detach();
}
