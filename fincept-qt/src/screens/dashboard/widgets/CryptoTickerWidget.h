#pragma once
// CryptoTickerWidget — live crypto ticker strip for Kraken / HyperLiquid.
// Subscribes to `ws:<exchange>:ticker:<pair>` (push-only, coalesced 50ms).
// Ticks flow whenever the exchange WS is running — widget doesn't force
// the stream; it's a consumer only.
//
// Config (persisted):
//   { "exchange": "kraken" | "hyperliquid",
//     "pairs":    ["BTC/USD", "ETH/USD", ...] }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class QLabel;

namespace fincept::trading {
struct TickerData;
}

namespace fincept::screens::widgets {

class CryptoTickerWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit CryptoTickerWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct Row {
        QLabel* symbol = nullptr;
        QLabel* price = nullptr;
        QLabel* change = nullptr;
    };

    void build_rows();
    void apply_styles();
    void on_ticker(const QString& pair, const fincept::trading::TickerData& t);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QString exchange_;
    QStringList pairs_;
    QHash<QString, Row> rows_;
    QSet<QString> received_;  // pairs that have published at least once
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
