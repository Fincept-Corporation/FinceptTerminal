// src/screens/equity_research/EquityFinancialsView.h
//
// The whole Financials tab UI, independent of the data service (EquityFinancialsTab feeds it):
//   header strip   company · reporting currency + scale · ANNUAL / QUARTERLY / TTM · units
//                  (reported / common size / YoY %) · fiscal year end · source + "as of" · EXPORT;
//   section bar    OVERVIEW · INCOME STATEMENT · BALANCE SHEET · CASH FLOW · RATIOS & QUALITY · PEERS,
//                  and the selected period (click a bar / header anywhere to select it);
//   KPI ribbon     twelve tiles for the selected period (value, change vs a year earlier with sign
//                  and arrow, five-period sparkline; a missing input says why);
//   body           loading skeleton | designed empty / error page (RETRY) | the sections.
// Sections are built on first show and refilled lazily: only the visible one is touched when
// the data, basis, units or selected period change.
#pragma once

#include "screens/equity_research/EquityFinancialsCommon.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;

namespace fincept::ui {
class EmptyState;
class KpiStrip;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

namespace er_ui {
class Segmented;
}

class FinOverviewPage;
class FinPeersPage;
class FinRatiosPage;
class FinStatementPage;

class EquityFinancialsView : public QWidget {
    Q_OBJECT
  public:
    explicit EquityFinancialsView(QWidget* parent = nullptr);

    /// No symbol yet.
    void show_idle();
    /// Nothing to show yet: skeleton with "Loading statements for SYMBOL…".
    void show_loading(const QString& symbol);
    /// Nothing on screen and the load failed. code "no_data" = an instrument without statements
    /// (ETF, index, currency, crypto): a designed page, no RETRY; anything else: error + RETRY.
    void show_error(const QString& symbol, const QString& message, const QString& code);
    /// The statements on screen stay; a notice says the refresh failed (with RETRY).
    void show_refresh_failed(const QString& message);
    /// New statements (or the same ones recomputed).
    void set_bundle(const FinBundlePtr& bundle);
    FinBundlePtr bundle() const { return bundle_; }

    /// The peer group as the Peers tab loads it (the subject included).
    void set_peers(const QVector<services::equity::PeerData>& peers);
    /// 0 none, 1 loading, 2 failed (with message).
    void set_peers_state(int state, const QString& message = {});

    /// Section routing hook: "overview" | "income" | "balance" | "cashflow" | "ratios" | "peers".
    void show_section(const QString& key);
    QString section() const { return section_; }

    /// Re-format everything from the data held (number / currency format changed).
    void refresh_format();

  signals:
    void retry_requested();
    void peers_requested();

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void retranslate();
    void set_body(QWidget* page);
    void set_basis(int basis);
    void set_units(int units);
    void select_period(int index);
    void render_header();
    void render_kpis();
    void refresh_current(bool data_changed);
    QWidget* ensure_page(const QString& key);
    void export_to(bool clipboard);
    void collect_export(QStringList& headers, QVector<QStringList>& rows);
    int latest() const;

    FinBundlePtr bundle_;
    FinViewState vs_;
    QString section_ = QStringLiteral("overview");
    QVector<services::equity::PeerData> peers_;
    int peers_state_ = 0;
    QString peers_message_;
    quint64 data_gen_ = 1;  ///< bumps on new data / basis / units / format
    quint64 sel_gen_ = 1;   ///< bumps on a period selection
    QHash<QString, quint64> page_data_gen_;
    QHash<QString, quint64> page_sel_gen_;

    // Header strip
    QWidget* header_ = nullptr;
    QLabel* symbol_lbl_ = nullptr;
    QLabel* name_lbl_ = nullptr;
    QLabel* unit_chip_ = nullptr;
    er_ui::Segmented* basis_seg_ = nullptr;
    er_ui::Segmented* units_seg_ = nullptr;
    QLabel* meta_lbl_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    // Section bar
    QWidget* section_bar_ = nullptr;
    er_ui::Segmented* section_seg_ = nullptr;
    QLabel* period_chip_ = nullptr;
    QPushButton* latest_btn_ = nullptr;
    // Notice
    QWidget* notice_ = nullptr;
    QLabel* notice_lbl_ = nullptr;
    QPushButton* notice_btn_ = nullptr;
    // Body
    QStackedWidget* body_ = nullptr;
    ui::LoadingSkeleton* skeleton_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    QWidget* content_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QStackedWidget* sections_ = nullptr;
    QHash<QString, QWidget*> pages_; ///< sections built so far (on first show)
};

} // namespace fincept::screens
