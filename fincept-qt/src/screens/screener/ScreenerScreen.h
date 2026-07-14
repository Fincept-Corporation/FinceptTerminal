#pragma once
#include "services/markets/MarketDataService.h"

#include <QHash>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace fincept::screens {

/// Full-screen Stock Screener.
///
/// Reuses the dashboard `ScreenerWidget` data path — subscribes to
/// `market:quote:<sym>` on the DataHub for a broad large-cap basket, caches
/// each delivery, and sorts/filters client-side. Presents the result as a
/// full-width table with a symbol/name search box and a sort selector.
///
/// Hub lifecycle follows P3/D3: subscribe in `showEvent`, unsubscribe in
/// `hideEvent`, so the producer pauses when the screen isn't visible.
class ScreenerScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ScreenerScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void apply_styles();
    void retranslate();

    void hub_subscribe_all();
    void hub_unsubscribe_all();
    /// Recompute `all_quotes_` from `row_cache_` (in basket order) then filter.
    void rebuild_from_cache();
    void apply_filter();
    void render_rows(const QVector<services::QuoteData>& rows);
    void refresh_now();

    QLineEdit* search_ = nullptr;
    QComboBox* sort_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QLabel* subtitle_lbl_ = nullptr;
    QLabel* count_lbl_ = nullptr;
    QWidget* header_bar_ = nullptr;
    QTableWidget* table_ = nullptr;

    QHash<QString, services::QuoteData> row_cache_;
    QVector<services::QuoteData> all_quotes_;
    bool hub_active_ = false;
};

} // namespace fincept::screens
