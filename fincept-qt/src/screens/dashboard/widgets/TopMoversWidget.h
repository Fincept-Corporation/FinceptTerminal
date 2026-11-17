#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "screens/dashboard/widgets/DashboardScreens.h"
#include "services/markets/MarketDataTypes.h"

#include <QHash>
#include <QJsonObject>

class QButtonGroup;
class QLabel;
class QTimer;

namespace fincept::ui {
class DataTable;
}

namespace fincept::screens::widgets {

/// Top movers tile — GAINERS / LOSERS / MOST ACTIVE.
///
/// Default ("region" universe): the user's MarketProfile region large caps from
/// the Yahoo screener (ScreenerService, sorted server-side), refreshed every
/// 5 minutes while visible. "List" universe (legacy {"symbols": [...]} configs
/// keep it): the tile's own symbols ranked from their live quotes. The number of
/// rows follows the tile height. Config: {"universe": "region"|"list",
/// "symbols": [...], "tab": 0..2}.
class TopMoversWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit TopMoversWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;
    void on_topic_errors_changed() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct Row {
        QString symbol;
        QString name;
        QString currency;
        double price = services::kMarketDataAbsent;
        double change_pct = services::kMarketDataAbsent;
        double volume = services::kMarketDataAbsent;
        double rel_volume = services::kMarketDataAbsent;
        double market_cap = services::kMarketDataAbsent;
        QString mcap_currency;
    };
    void run_screen(bool force);
    void apply_result(const services::ScreenerResult& r);
    void subscribe_list();
    void render();
    int visible_rows() const;
    void update_title();
    dash_screens::MoverKind kind() const;

    ui::DataTable* table_ = nullptr;
    QButtonGroup* tabs_ = nullptr;
    QLabel* footer_ = nullptr;
    QTimer* refresh_timer_ = nullptr;

    bool list_mode_ = false;
    QString region_pin_; ///< config "market_region" (RegionPin.h); "" = follow the selector
    QStringList symbols_;
    int tab_ = 0;
    QString pending_key_;
    QVector<Row> rows_;
    QHash<QString, services::QuoteData> quotes_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
