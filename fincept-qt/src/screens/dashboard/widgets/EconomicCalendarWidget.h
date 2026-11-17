#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QComboBox>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QVector>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Economic Calendar Widget — consumes the DataHub topic
/// `econ:fincept:upcoming_events` (HTTP-backed by `MacroCalendarService`).
/// All cadence is owned by the hub scheduler.
class EconomicCalendarWidget : public BaseWidget {
    Q_OBJECT
  public:
    /// Config: {"country": ""|"@region"|"<ISO2>", "importance": 0|2|3}. Default
    /// = the user's MarketProfile countries, any importance.
    explicit EconomicCalendarWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    /// Drops low-priority columns as the tile narrows (see apply_column_visibility).
    void resizeEvent(QResizeEvent* e) override;

  private:
    void apply_styles();
    void clear_list();
    /// REF + CONS below 520 px tile width, FCST + PREV below 400 px are hidden
    /// (header and rows alike), so the EVENT column keeps room for the names.
    void apply_column_visibility();
    void show_status(const QString& text);
    void hub_subscribe();
    void hub_unsubscribe();
    void populate(const QJsonArray& events);

    QWidget* header_widget_ = nullptr;
    QFrame* header_sep_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QVector<QLabel*> header_labels_;
    // Row cells of the width-dependent columns (cleared with the rows).
    QVector<QLabel*> ref_cells_;
    QVector<QLabel*> fcst_cells_;
    QVector<QLabel*> cons_cells_;
    QVector<QLabel*> prev_cells_;
    int column_tier_ = -1; ///< 0 narrow, 1 medium, 2 wide — last applied by resizeEvent
    QJsonArray last_events_; // cached for theme-change re-populate
    bool hub_active_ = false;

    void fill_filters();
    QComboBox* country_combo_ = nullptr;
    QComboBox* importance_combo_ = nullptr;
    QString country_filter_ = QStringLiteral("@region"); ///< "" all, "@region" profile countries, else ISO2
    QString region_pin_; ///< config "market_region" (RegionPin.h): whose countries "@region" means
    int min_importance_ = 0;
    bool filling_filters_ = false;
};

} // namespace fincept::screens::widgets
