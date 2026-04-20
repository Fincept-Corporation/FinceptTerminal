#pragma once

#include "screens/polymarket/ExchangePresentation.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Top command bar: view pills, category filters, search, sort, refresh, WS status.
/// Visual identity (accent colors, view pill set, category display mode)
/// is driven by the installed ExchangePresentation so Polymarket and Kalshi
/// render distinctly even though the widget tree is shared.
class PolymarketCommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketCommandBar(QWidget* parent = nullptr);

    /// Install the per-exchange presentation profile. Rebuilds view pills,
    /// swaps the category row between chip layout and dropdown, and applies
    /// the accent color across active-state styling.
    void set_presentation(const ExchangePresentation& p);

    /// Populate the category filter row. `tags` is exchange-agnostic —
    /// each entry is displayed verbatim and passed back through the
    /// category_changed signal. "ALL" is always prepended automatically.
    void set_categories(const QStringList& tags);
    void set_active_view(const QString& view);
    void set_active_category(const QString& cat);
    void set_loading(bool loading);
    void set_ws_status(bool connected);
    void set_market_count(int count);
    void set_search_text(const QString& text) { if (search_input_) search_input_->setText(text); }

    /// Populate the exchange dropdown. `ids` are adapter ids from
    /// PredictionExchangeRegistry (e.g. ["polymarket", "kalshi"]); `labels`
    /// are the corresponding display names. The selected entry controls
    /// which adapter the screen pulls data from.
    void set_exchanges(const QStringList& ids, const QStringList& labels, const QString& active_id);

    /// Update the account chip shown next to the exchange combo.
    /// `connected == false` renders "⚪ No account" (clickable CTA);
    /// `connected == true` renders "🟢 <label>" in green.
    void set_account_status(bool connected, const QString& label);

  signals:
    void view_changed(const QString& view);
    void category_changed(const QString& category);
    void search_submitted(const QString& query);
    void sort_changed(const QString& sort_by);
    void refresh_clicked();
    void exchange_changed(const QString& exchange_id);
    void account_clicked();

  private:
    void build_ui();
    void rebuild_view_pills();
    void apply_accent();
    void rebuild_categories();  // called when presentation or tag set changes

    QComboBox* exchange_combo_ = nullptr;
    QPushButton* account_chip_ = nullptr;
    QWidget* view_pills_container_ = nullptr;
    QList<QPushButton*> view_btns_;
    QWidget* category_container_ = nullptr;
    QComboBox* category_combo_ = nullptr;  // used in CategoryMode::ComboBox
    QLineEdit* search_input_ = nullptr;
    QComboBox* sort_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* ws_indicator_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* title_label_ = nullptr;

    QString active_view_ = "MARKETS";
    QString active_category_ = "ALL";

    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();
    QStringList current_tags_;
};

} // namespace fincept::screens::polymarket
