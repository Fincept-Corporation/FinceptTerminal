#pragma once
#include "services/feeds/FeedTypes.h"

#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace fincept::feeds {

/// A source tag found by discovery, with a sample value to help the user choose.
struct DiscoveredField {
    QString path;   // tag path the parser understands (element / json key / csv header)
    QString sample; // first observed value (truncated), for preview
};

/// The structure auto-detected from one fetch of a feed — used to populate the
/// manual-mapping UI so the user just picks tags instead of typing paths.
struct DiscoveredSchema {
    bool ok = false;
    QString format;            // "rss" | "atom" | "xml" | "json" | "csv"
    QString item_selector;     // best-guess repeating record (XML element / JSON array path)
    QStringList item_options;  // candidate repeating records the user can switch to
    QVector<DiscoveredField> fields;
};

/// Stateless parser: raw bytes + a subscription -> feed items.
/// Auto mode sniffs the format and maps obvious fields; Manual mode applies sub.mapping.
class FeedScraper {
  public:
    static QVector<FeedItem> parse(const QByteArray& body, const FeedSubscription& sub);

    /// Probe one fetched body and report its repeating record + available tags so the
    /// manual-mapping UI can be filled in by selection rather than typing.
    static DiscoveredSchema discover(const QByteArray& body, const FeedSubscription& sub);

  private:
    static QVector<FeedItem> parse_rss_atom(const QByteArray& xml, const FeedSubscription& sub);
    static QVector<FeedItem> parse_json(const QByteArray& body, const FeedSubscription& sub);
    static QVector<FeedItem> parse_csv(const QByteArray& body, const FeedSubscription& sub);
    static QVector<FeedItem> parse_xml_manual(const QByteArray& xml, const FeedSubscription& sub);

    static DiscoveredSchema discover_xml(const QByteArray& xml);
    static DiscoveredSchema discover_json(const QByteArray& body, const FeedSubscription& sub);
    static DiscoveredSchema discover_csv(const QByteArray& body);
};

} // namespace fincept::feeds
