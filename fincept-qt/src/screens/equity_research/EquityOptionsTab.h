// src/screens/equity_research/EquityOptionsTab.h
#pragma once
#include "screens/equity_research/EquityDataTab.h"
#include "services/fincept_data/FinceptDataModels.h"

class QComboBox;

namespace fincept::ui {
class BarChart;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

namespace er_ui {
class Segmented;
}


/// OPTIONS — the listed options chain of the research symbol from the Fincept
/// /data API (Cboe delayed quotes with Greeks; Yahoo Finance fallback without).
///
/// One expiration at a time, calls | strike | puts around the money: the chain is
/// asked with `strikes_around_atm` (±10 strikes by default) and paged (100
/// contracts per page, LOAD MORE for the next), so a full chain is never fetched.
/// The expiration list comes with the first answer (no separate call).
class EquityOptionsTab : public EquityDataTab {
    Q_OBJECT
  public:
    explicit EquityOptionsTab(QWidget* parent = nullptr);
    ~EquityOptionsTab() override;

  protected:
    void clear_view() override;
    void request(bool refresh) override;
    QString loading_text() const override;
    QString not_found_text(const services::fincept_data::DataError& e) const override;
    void retranslate() override;
    bool apply_remembered(const services::fincept_data::DataResult& r) override;
    /// The strike band: a remembered chain is per symbol and band.
    QString remember_variant() const override;
    /// Only a first page is kept (a later page alone is not the tab's view).
    bool rememberable(const services::fincept_data::DataResult& r) const override;

  private:
    void build_ui();
    void load_page(int page, bool refresh);
    void apply_answer(const services::fincept_data::DataResult& r, bool append);
    void populate_expirations();
    void populate_underlying(const services::fincept_data::DataResult& r);
    void populate_table();
    void update_footer();
    /// OI / volume by strike, IV smile, put/call ratios, max pain, expected move.
    void update_analytics();
    QStringList headers() const;

    QComboBox* expiry_combo_ = nullptr;
    QComboBox* strikes_combo_ = nullptr;
    QLabel* expiry_caption_ = nullptr;
    QLabel* strikes_caption_ = nullptr;
    QLabel* underlying_label_ = nullptr;
    QLabel* feed_label_ = nullptr;
    QLabel* calls_caption_ = nullptr;
    QLabel* puts_caption_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* footer_label_ = nullptr;
    QLabel* analytics_label_ = nullptr;
    er_ui::Segmented* oi_mode_ = nullptr;
    ui::BarChart* oi_chart_ = nullptr;
    ui::TimeSeriesChart* iv_chart_ = nullptr;
    QPushButton* more_btn_ = nullptr;

    QString expiration_;     ///< asked for; empty = the nearest (the server picks)
    int strikes_ = 10;       ///< strikes below and above the spot
    services::fincept_data::OptionChain chain_; ///< pages loaded so far for chain_.expiration
    services::fincept_data::Pagination pagination_;
    services::fincept_data::DataResult last_answer_;
};

} // namespace fincept::screens
