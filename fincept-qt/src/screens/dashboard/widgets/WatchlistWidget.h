#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

class QComboBox;

namespace fincept::screens::widgets {

/// Watchlist tile — bound to one of the user's saved watchlists (Watchlist
/// screen / WatchlistRepository, reloaded on `watchlist.updated`) or to a list
/// kept in this tile. Config: {"watchlist_id": "<id>"} or {"symbols": [...]}
/// (the legacy {"symbols": ["AAPL", ...]} still loads). A tile with no config
/// binds to the default saved watchlist, else shows the region's large caps.
class WatchlistWidget : public QuoteTableWidget {
    Q_OBJECT
  public:
    explicit WatchlistWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    QVector<QuoteGridEntry> default_entries() const override;
    QString source_text() const override;
    QString base_title() const override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void reload_sources();
    void bind(const QString& watchlist_id);
    /// An empty list (a fresh "Default" watchlist) gets "<list> is empty" with
    /// OPEN WATCHLIST / SHOW REGION LARGE CAPS instead of a blank grid.
    void update_empty_state();
    /// SHOW REGION LARGE CAPS: unbind and follow the region's large caps.
    void follow_region_large_caps();

    QComboBox* source_ = nullptr;
    QString watchlist_id_;   ///< bound saved watchlist; empty = this tile's list
    QString watchlist_name_;
    /// The user chose the region's large caps over the default saved watchlist
    /// (SHOW REGION LARGE CAPS, or "This tile's list" while it follows the
    /// region); persisted as {"list_source": "region"} so a restart does not
    /// bind the (empty) default watchlist again.
    bool region_chosen_ = false;
    bool filling_ = false;
};

} // namespace fincept::screens::widgets
