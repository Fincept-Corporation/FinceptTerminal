#pragma once
// PolymarketPriceWidget — live prediction-market price strip. Subscribes to
// `polymarket:price:<asset_id>` (push-only). Payload is a bare double (0..1
// probability). Provide a human label per id.
//
// Config (persisted):
//   { "entries": [ {"asset_id": "...", "label": "..."}, ... ] }
// A config without "entries" (a fresh tile) is filled once, on the first show,
// with the most-traded open markets from Polymarket's free public Gamma API
// (one GET, never on a hidden tile), then written back with config_changed().
// A config with "entries": [] (the user cleared the list) stays empty.

#include "core/result/Result.h"
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/polymarket/PolymarketTypes.h"

#include <QHash>
#include <QJsonDocument>
#include <QSet>
#include <QString>
#include <QVector>

class QLabel;

namespace fincept::screens::widgets {

class PolymarketPriceWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PolymarketPriceWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

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
        QString asset_id;
        QString label;
    };
    struct Row {
        QLabel* label = nullptr;
        QLabel* pct = nullptr;
    };

    void apply_styles();
    void build_rows();
    void on_price(const QString& asset_id, double price);
    /// Order-book update: shows the book midpoint (see the .cpp for the rule).
    void on_book(const QString& asset_id, const services::polymarket::OrderBook& book);
    void hub_resubscribe();
    void hub_unsubscribe_all();
    /// Fresh tile: fetch the most-traded open markets (Gamma /markets by volume)
    /// and make them the tile's list. A RETRY on failure calls it again.
    void resolve_defaults();
    void on_defaults(quint64 request_id, const Result<QJsonDocument>& reply);
    void show_defaults_failure(const QString& reason);

    QVector<Entry> entries_;
    bool defaults_pending_ = false;  ///< config had no "entries": the list is still to be chosen
    bool defaults_inflight_ = false; ///< the Gamma request is out (skeleton on screen)
    bool defaults_failed_ = false;   ///< it failed: wait for RETRY, never re-fetch by itself
    quint64 defaults_req_ = 0;       ///< id of the request whose answer counts (stale ones dropped)
    QWidget* rows_host_ = nullptr; // owns the rows/hint + the one stylesheet styling them
    QHash<QString, Row> rows_;     // asset_id → widgets
    QSet<QString> ws_tokens_;      // tokens this tile already asked the socket to stream
    QSet<QString> traded_;         // tokens that have printed a last-trade price
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
