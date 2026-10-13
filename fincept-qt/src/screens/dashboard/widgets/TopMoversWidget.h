#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"
#include "ui/tables/DataTable.h"

#include <QHash>
#include <QPushButton>

#include <algorithm>

namespace fincept::screens::widgets {

/// Gainers / losers ranked within a symbol UNIVERSE (configurable via the gear
/// icon; default MarketDataService::mover_symbols()). It is not a whole-market
/// scan — the tile says "among N tracked" so it never reads like one.
///
/// Subscribes to `market:quote:<sym>` on the DataHub for the universe. The row
/// cache is re-sorted into `all_quotes_` on every delivery burst.
class TopMoversWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit TopMoversWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void refresh_data();
    void show_tab(bool gainers);

    void hub_subscribe_all();
    void hub_unsubscribe_all();
    /// Recompute `all_quotes_` from `row_cache_`, sort, and redraw the tab.
    void rebuild_from_cache();

    ui::DataTable* table_ = nullptr;
    QLabel* universe_label_ = nullptr; ///< "ranked among N tracked" / empty-state line
    QPushButton* gainers_btn_ = nullptr;
    QPushButton* losers_btn_ = nullptr;
    QVector<services::QuoteData> all_quotes_;
    bool showing_gainers_ = true;
    /// Guards the tab-button stylesheet reparse — show_tab() runs on every
    /// hub delivery but the button CSS only changes with tab/theme.
    bool tab_style_applied_ = false;

    QHash<QString, services::QuoteData> row_cache_;
    QStringList symbols_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
