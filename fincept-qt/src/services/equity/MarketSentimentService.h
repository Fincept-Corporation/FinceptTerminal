#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>

namespace fincept::services::equity {

class MarketSentimentService : public QObject {
    Q_OBJECT
  public:
    static MarketSentimentService& instance();

    void fetch_snapshot(const QString& symbol, int days = 7, bool force = false);
    bool is_configured() const;

  signals:
    void snapshot_loaded(QString symbol, fincept::services::equity::MarketSentimentSnapshot snapshot);
    void error_occurred(QString context, QString message);

  private:
    explicit MarketSentimentService(QObject* parent = nullptr);
    Q_DISABLE_COPY(MarketSentimentService)

    struct ConnectionConfig {
        bool configured = false;
        QString api_key;
    };

    ConnectionConfig load_connection() const;

    QNetworkRequest build_request(const QUrl& url, const QString& api_key) const;

    QNetworkAccessManager* nam_ = nullptr;
    quint64 active_request_id_ = 0;

    static constexpr int kCacheTtlSec = 600;
};

} // namespace fincept::services::equity
