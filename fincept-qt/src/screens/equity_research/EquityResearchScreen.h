// src/screens/equity_research/EquityResearchScreen.h
#pragma once
#include "screens/IStatefulScreen.h"
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

class EquityResearchScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit EquityResearchScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "equity_research"; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_quote_loaded(services::equity::QuoteData quote);
    void on_info_loaded(services::equity::StockInfo info);
    void on_tab_changed(int index);

  private:
    void build_ui();
    QWidget* build_title_bar();
    QWidget* build_quote_bar();
    void update_quote_bar(const services::equity::QuoteData& q);
    void load_symbol(const QString& symbol);

    // Title bar
    QLabel* symbol_label_ = nullptr;

    // Quote bar
    QLabel* sym_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* vol_label_ = nullptr;
    QLabel* hl_label_ = nullptr;
    QLabel* mktcap_label_ = nullptr;
    QLabel* rec_label_ = nullptr;

    // Tabs
    QTabWidget* tab_widget_ = nullptr;
    EquityOverviewTab* overview_tab_ = nullptr;
    EquityFinancialsTab* financials_tab_ = nullptr;
    EquityAnalysisTab* analysis_tab_ = nullptr;
    EquityTechnicalsTab* technicals_tab_ = nullptr;
    EquityTalippTab* talipp_tab_ = nullptr;
    EquityPeersTab* peers_tab_ = nullptr;
    EquityNewsTab* news_tab_ = nullptr;

    QTimer* refresh_timer_ = nullptr;
    QString current_symbol_;
    QString current_currency_;
};

} // namespace fincept::screens
