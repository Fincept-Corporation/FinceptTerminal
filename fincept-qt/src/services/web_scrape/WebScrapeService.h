#pragma once
// WebScrapeService — fetches a user-configured URL and extracts tables from it
// (HTML <table>, JSON arrays/objects, CSV/TSV, XML/RSS/Atom).
//
// Used by the dashboard Web Scraper tile. One QNetworkAccessManager for every
// tile (it used to be one per tile); the response is bounded in size and
// time; parsing runs on a worker thread because a multi-megabyte page through
// the regex HTML parser could otherwise stall the UI for seconds.
//
// The URL is third-party and user-supplied (also settable via the MCP
// add-widget tool), so:
//   * only http/https, with a host and no embedded user:password,
//   * NoLessSafeRedirectPolicy (no https → http downgrade), ≤ 5 redirects,
//   * logs carry scheme://host/path only — never the query string, which is
//     where API keys usually live,
//   * HttpClient is deliberately NOT used: it attaches the signed-in user's
//     Fincept session token, which must never reach a third-party host.
//
// This is a one-shot, user-configured fetch (D2 exception): each tile has its
// own URL/headers and the result has no cross-consumer fan-out, so it is not a
// DataHub producer.

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace fincept::services {

/// A table of extracted data — one header row plus N data rows.
struct ScrapedTable {
    QString label; // "Table 1", "items[]", "<item> × 20" etc.
    QStringList headers;
    QVector<QStringList> rows;
};

struct WebScrapeRequest {
    QString url;
    QString user_agent;              ///< empty = service default
    QHash<QString, QString> headers; ///< extra request headers
    QString encoding;                ///< empty = Content-Type / <meta> / utf-8
    QString json_path;               ///< dot path into a JSON root ("data.items")
    QString force_format;            ///< "", "html", "json", "csv", "tsv", "xml"
};

struct WebScrapeResult {
    bool ok = false;
    QString error;  ///< user-facing reason when !ok
    QString format; ///< "HTML", "JSON", "CSV", "TSV", "XML" — empty when nothing parsed
    QVector<ScrapedTable> tables;
    qint64 bytes = 0;
    int http_status = 0;
};

class WebScrapeService : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(const WebScrapeResult&)>;

    static WebScrapeService& instance();

    /// Largest response body accepted, in bytes.
    static constexpr qint64 kMaxBodyBytes = 10 * 1024 * 1024;
    /// Inactivity timeout and hard wall-clock deadline for one fetch.
    static constexpr int kTransferTimeoutMs = 15000;
    static constexpr int kDeadlineMs = 30000;

    /// Validate a user-entered URL. Returns an empty string when acceptable,
    /// otherwise a user-facing reason.
    static QString validate_url(const QUrl& url);

    /// scheme://host/path — safe to log or show (no query, no credentials).
    static QString redacted(const QUrl& url);

    /// Start a fetch + parse. `cb` runs on the GUI thread, and only while
    /// `context` is alive. Returns a request id for cancel(); 0 when the
    /// request was rejected synchronously (cb has then already been called).
    quint64 scrape(const WebScrapeRequest& req, QObject* context, Callback cb);

    /// Abort a pending request. Its callback is never invoked.
    void cancel(quint64 id);

  private:
    explicit WebScrapeService(QObject* parent = nullptr);
    Q_DISABLE_COPY(WebScrapeService)

    struct Pending {
        QPointer<QNetworkReply> reply;
        QPointer<QObject> context;
        Callback cb;
        WebScrapeRequest req;
        bool too_large = false;
        bool deadline_hit = false;
    };

    void on_finished(quint64 id);
    void deliver(QPointer<QObject> context, const Callback& cb, const WebScrapeResult& result);

    QNetworkAccessManager* nam_ = nullptr;
    QHash<quint64, Pending> pending_;
    quint64 next_id_ = 1;
};

} // namespace fincept::services
