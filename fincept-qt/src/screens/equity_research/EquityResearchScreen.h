// src/screens/equity_research/EquityResearchScreen.h
#pragma once
#include "services/equity/EquityResearchModels.h"

#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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

class EquityResearchScreen : public QWidget {
    Q_OBJECT
  public:
    explicit EquityResearchScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_search_text_changed(const QString& text);
    void on_search_committed();
    void on_search_results_loaded(QVector<services::equity::SearchResult> results);
    void on_quote_loaded(services::equity::QuoteData quote);
    void on_tab_changed(int index);

  private:
    void build_ui();
    QWidget* build_search_bar();
    QWidget* build_quote_bar();
    void update_quote_bar(const services::equity::QuoteData& q);
    void load_symbol(const QString& symbol);

    // Search widgets
    QWidget*     search_container_ = nullptr;
    QLineEdit*   search_edit_      = nullptr;
    QListWidget* suggest_list_     = nullptr;

    // Quote bar
    QLabel* sym_label_    = nullptr;
    QLabel* price_label_  = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* vol_label_    = nullptr;
    QLabel* hl_label_     = nullptr;
    QLabel* mktcap_label_ = nullptr;
    QLabel* rec_label_    = nullptr;

    // Tabs
    QTabWidget*          tab_widget_     = nullptr;
    EquityOverviewTab*   overview_tab_   = nullptr;
    EquityFinancialsTab* financials_tab_ = nullptr;
    EquityAnalysisTab*   analysis_tab_   = nullptr;
    EquityTechnicalsTab* technicals_tab_ = nullptr;
    EquityTalippTab*     talipp_tab_     = nullptr;
    EquityPeersTab*      peers_tab_      = nullptr;
    EquityNewsTab*       news_tab_       = nullptr;

    QTimer*  refresh_timer_  = nullptr;
    QString  current_symbol_;

    QVector<services::equity::SearchResult> last_search_results_;
};

} // namespace fincept::screens
