#pragma once
#include "services/feeds/FeedTypes.h"

#include <QByteArray>
#include <QString>

namespace fincept::feeds {

/// Strip HTML tags + collapse whitespace + unescape common entities; cap length.
QString strip_html(const QString& in, int max_chars = 300);

/// Parse a feed timestamp through the RSS/Atom format cascade (optional explicit
/// `fmt`, then RFC2822, ISO, "ddd, dd MMM yyyy HH:mm:ss"). Returns epoch secs and
/// fills `display` ("MMM dd, HH:mm"); returns 0 if unparseable.
qint64 parse_feed_datetime(const QString& text, QString& display, const QString& fmt = {});

/// True if the body looks like an HTML page (access-denied / captcha), not a feed.
bool looks_like_html(const QByteArray& body);

/// Sniff the format from raw bytes (used when sub.format == Auto).
FeedFormat sniff_format(const QByteArray& body);

} // namespace fincept::feeds
