#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/screener/ScreenerService.h"

#include <QJsonObject>

class QComboBox;
class QLabel;

namespace fincept::ui {
class DataTable;
}

namespace fincept::screens::widgets {

/// Stock screener tile — runs region-aware screens through ScreenerService (the
/// Yahoo screener, same engine as the Screener screen) over the user's
/// MarketProfile region: largest companies, value, dividend, quality, growth,
/// low beta; plus Yahoo's predefined strategy screens when the region is the US.
/// Results show price, change, market cap (in its own currency) and the screen's
/// metric, sortable by column. Config: {"screen": "<id>"}.
class ScreenerWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit ScreenerWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;

  private:
    void fill_screens();
    void run(bool force);
    void apply_result(const services::ScreenerResult& r);
    void update_title();

    QComboBox* screen_combo_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QLabel* footer_ = nullptr;
    QString screen_id_ = QStringLiteral("largest");
    QString region_pin_; ///< config "market_region" (RegionPin.h); "" = follow the selector
    QString pending_key_;
    bool filling_ = false;
    bool has_rows_ = false;
};

} // namespace fincept::screens::widgets
