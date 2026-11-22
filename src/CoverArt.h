#pragma once
// Album covers: a picture next to the audio file (cover.jpg, folder.jpg, front.png, AlbumArt*.jpg, ...)
// or the picture embedded in the tags (ID3 APIC, FLAC PICTURE, MP4 covr) read through GStreamer.
// Embedded pictures are written once to ~/.cache/linux-aurora-player/covers so that MPRIS can pass
// them to GNOME as a file:// URL and later lookups are instant.
#include <QString>
#include <QUrl>
#include <functional>

class QObject;

namespace CoverArt {

// Fast, no tag reading: folder picture or an already cached embedded picture. Empty when none.
QString findCached(const QUrl &trackUrl);

// Full lookup in a background thread (reads the tags if needed); 'done' runs in the GUI thread
// on 'context' with the picture path (empty when the track has no cover).
void findAsync(const QUrl &trackUrl, QObject *context, std::function<void(const QString &)> done);

} // namespace CoverArt
