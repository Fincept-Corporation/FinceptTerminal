#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "screens/dashboard/widgets/QuoteGrid.h"

#include <QJsonObject>

class QLabel;

namespace fincept::screens::widgets {

/// What a quote-list tile shows by default (from the user's MarketProfile).
enum class QuoteListKind { Indices, Forex, Crypto, Commodities, Custom, GlobalIndices };

/// Live quote-list tile: indices, FX, crypto, commodities, watchlist.
///
/// Rows come from the tile config or — when the config has no list — from the
/// current MarketProfile for `kind`, following region changes. The gear opens a
/// SymbolListDialog (symbol search, reorder, relabel, region defaults); the
/// edited list is persisted as {"symbols":[{"s":"^GSPC","l":"S&P 500","g":""}]}.
/// Legacy configs ({"symbols":["AAPL", ...]}) still load.
///
/// The tile subscribes `market:quote:<SYM>` (tracked: loading / error / stale
/// states) and `market:sparkline:<SYM>` (trend column) in showEvent() and drops
/// them in hideEvent() (P3 / D3). Rows update in place (QuoteGrid). A footer
/// line says where the list comes from and how old / delayed the quotes are.
class QuoteTableWidget : public BaseWidget {
    Q_OBJECT
  public:
    QuoteTableWidget(QuoteListKind kind, const QJsonObject& cfg, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

    void refresh_data();

    /// Region defaults for `kind` from the current MarketProfile, or from the
    /// `region_pin` profile for a tile pinned to a region (RegionPin.h).
    static QVector<QuoteGridEntry> profile_entries(QuoteListKind kind, const QString& region_pin = {});
    /// Parse a {"symbols": [...]} array (objects or plain strings).
    static QVector<QuoteGridEntry> entries_from_json(const QJsonObject& cfg);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_topic_errors_changed() override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

    /// Replace the rows and re-subscribe (when visible).
    void set_entries(const QVector<QuoteGridEntry>& entries);
    QuoteGrid* grid() const { return grid_; }
    QuoteListKind kind() const { return kind_; }
    /// Where the rows come from, for the footer ("United States defaults", "Watchlist: Tech").
    virtual QString source_text() const;
    /// Default rows when the config has none. Subclasses (watchlist) override.
    virtual QVector<QuoteGridEntry> default_entries() const { return profile_entries(kind_, region_pin_); }
    /// Footer + title refresh.
    void update_footer();
    void update_title();
    virtual QString base_title() const;

    bool follow_region_ = true;
    /// Config "market_region": the region this tile is pinned to ("" = follow the selector).
    QString region_pin_;
    /// Config "list_label": names a template's own list in the title and footer ("Asia-Pacific").
    QString list_label_;
    /// Config "list_title": replaces the whole title for a template's own list ("US RATES").
    QString list_title_;

  private:
    void hub_subscribe_all();
    void hub_unsubscribe_all();
    void apply_styles();

    QuoteListKind kind_;
    QuoteGrid* grid_ = nullptr;
    QLabel* footer_ = nullptr;
    QTimer* footer_timer_ = nullptr;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
