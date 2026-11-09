#pragma once
// KalshiPricesWidget — a handful of Kalshi prediction markets: YES price (last
// trade) and its 24 h change. Keyless: subscribes to
// `prediction:kalshi:market:<ticker>` (KalshiMarketProducer, public REST, one
// batched request per refresh). Mirrors PolymarketPriceWidget.
//
// Config (persisted):
//   { "entries": [ {"ticker": "KXPRESPARTY-2028-D", "label": "..."}, ... ] }
// A config without "entries" gets the default set; an empty array means the user
// removed every market.

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

class QLabel;

namespace fincept::services::prediction {
struct PredictionMarket;
} // namespace fincept::services::prediction

namespace fincept::screens::widgets {

class KalshiPricesWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit KalshiPricesWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct Entry {
        QString ticker;
        QString label; // optional; empty = take the market's own name
    };
    struct Row {
        QLabel* label = nullptr;
        QLabel* yes = nullptr;
        QLabel* change = nullptr;
    };
    struct Snapshot { // the fields of a PredictionMarket this tile shows
        QString question;
        QString yes_sub_title;
        QString status;
        QString result;
        double last = 0.0;
        double previous = 0.0;
        double bid = 0.0;
        double ask = 0.0;
    };

    static QVector<Entry> default_entries();
    /// Normalised ticker, or empty when it cannot be a Kalshi ticker.
    static QString normalize_ticker(const QString& raw);

    void apply_styles();
    void build_rows();
    void render_row(const QString& ticker);
    void hub_resubscribe();
    void hub_unsubscribe_all();
    void refresh_data();
    void on_market(const QString& ticker, const services::prediction::PredictionMarket& market);
    void on_market_error(const QString& ticker, const QString& error);
    /// After a burst of deliveries: loading progress, whole-tile error, status line.
    void update_summary();

    QVector<Entry> entries_;
    QWidget* rows_host_ = nullptr; // owns the rows/hint/status + the one stylesheet styling them
    QLabel* status_label_ = nullptr;
    QHash<QString, Row> rows_;       // ticker → widgets
    QHash<QString, Snapshot> cache_; // ticker → last good snapshot
    QHash<QString, QString> errors_; // ticker → last error ("[CODE] message")
    QSet<QString> resolved_;         // tickers answered (data or error) since the last subscribe
    QSet<QString> halted_;           // tickers dropped after [NOT_FOUND] until refresh / config change
    QStringList subscribed_topics_;
    QDateTime last_update_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
