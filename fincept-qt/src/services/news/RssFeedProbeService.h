#pragma once
// RssFeedProbeService — "does this URL serve a usable RSS/Atom feed?"
//
// Used by the RSS feed editor / manager dialogs (which used to own a
// QNetworkAccessManager each). Deliberately NOT HttpClient: that client attaches
// the user's Fincept session token, which must never be sent to third-party feed
// hosts. One reused QNAM, GUI thread only; the XML sniffing runs on a worker.

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QNetworkAccessManager;

namespace fincept::services {

struct RssProbeResult {
    enum class Verdict { Feed, Html, NotFeed, NetworkError };
    Verdict verdict = Verdict::NetworkError;
    int http_status = 0;
    QString error;            // transport / HTTP error text (NetworkError)
    QString format;           // "RSS 2.0", "Atom", "RSS 1.0 (RDF)"
    QString title;            // channel / feed title
    QString root_element;     // first element seen (diagnostics for NotFeed)
    int item_count = 0;       // <item> / <entry> elements
    qint64 bytes = 0;
    qint64 newest_item_secs = 0; // newest pubDate / updated (unix secs), 0 = none parsed
    QString final_url;        // after redirects
    bool ok() const { return verdict == Verdict::Feed; }
};

class RssFeedProbeService : public QObject {
    Q_OBJECT
  public:
    using ProbeCallback = std::function<void(const RssProbeResult&)>;

    static RssFeedProbeService& instance();

    /// Fetch `url` (http/https only, 8 s transfer timeout, 8 MB cap) and classify
    /// it. `cb` runs on the GUI thread, and only while `context` is alive.
    void probe(const QString& url, QObject* context, ProbeCallback cb);

    /// One-line human summary, e.g. "✓ RSS 2.0 · Reuters Business · 30 items ·
    /// newest 2 h ago · 45 KB". Flags feeds whose newest item is over a week old.
    static QString describe(const RssProbeResult& r);

  private:
    RssFeedProbeService();
    QNetworkAccessManager* nam_ = nullptr;
};

} // namespace fincept::services
