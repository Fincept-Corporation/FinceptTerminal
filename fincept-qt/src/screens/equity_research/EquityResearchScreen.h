#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/equity/EquityResearchModels.h"

#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

class EquityOverviewTab;
class EquityFinancialsTab;
class EquityAnalysisTab;
class EquityTechnicalsTab;
class EquityTalippTab;
class EquityPeersTab;
class EquityNewsTab;
class EquitySentimentTab;
class EquityValuationTab;

class EquityResearchScreen : public QWidget, public IStatefulScreen, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit EquityResearchScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "equity_research"; }

    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_quote_loaded(services::equity::QuoteData quote);
    void on_info_loaded(services::equity::StockInfo info);
    void on_tab_changed(int index);
    void on_download_csv_clicked();
    void on_financials_loaded(services::equity::FinancialsData data);

  private:
    void build_ui();
    QWidget* build_title_bar();
    QWidget* build_quote_bar();
    void update_quote_bar(const services::equity::QuoteData& q);
    void load_symbol(const QString& symbol);
    void retranslateUi();
    void hub_subscribe_broker_quote();
    void hub_unsubscribe_broker_quote();

    QLabel* title_label_  = nullptr;
    QLabel* symbol_label_ = nullptr;
    QLabel* hint_label_   = nullptr;

    QLabel* sym_label_    = nullptr;
    QLabel* price_label_  = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* vol_label_    = nullptr;
    QLabel* hl_label_     = nullptr;
    QLabel* mktcap_label_ = nullptr;
    QLabel* rec_label_    = nullptr;

    QTabWidget*          tab_widget_     = nullptr;
    EquityOverviewTab*   overview_tab_   = nullptr;
    EquityFinancialsTab* financials_tab_ = nullptr;
    EquityAnalysisTab*   analysis_tab_   = nullptr;
    EquityTechnicalsTab* technicals_tab_ = nullptr;
    EquityTalippTab*     talipp_tab_     = nullptr;
    EquityPeersTab*      peers_tab_      = nullptr;
    EquityNewsTab*       news_tab_       = nullptr;
    EquitySentimentTab*  sentiment_tab_  = nullptr;
    EquityValuationTab*  valuation_tab_  = nullptr;

    QTimer*  refresh_timer_    = nullptr;
    QString  current_symbol_;
    QString  current_currency_;
    bool     hub_broker_active_ = false;
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens