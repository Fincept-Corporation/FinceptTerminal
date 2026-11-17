#pragma once
// AltPanels.h — the workbench panels of the Alternative Investments screen, beside the analyzer
// catalogue. The screen shows a mode strip (ANALYZERS | PRIVATE MARKETS | REAL ESTATE | ...) over a
// stack; a panel is created by its factory the first time its mode is chosen (never at screen
// construction) and lives under the stack from then on.
//
// Every panel follows the terminal's rules: never an empty view (remembered inputs -> a local run on
// first show, or a designed preview where a network source is involved), nothing on the GUI thread
// (Python through services::alt::AltInvestmentsService, SQL in QtConcurrent), a reply drawn only under
// the view that asked for it, colours / fonts from ui::colors / ui::fonts, numbers through
// ui::formatting, COPY / CSV / IMAGE + ui::AddToReportButton above each result.

class QWidget;

namespace fincept::screens::alt {

/// A3 — fund book: dated calls / distributions / NAV per fund, IRR / TVPI / DPI / RVPI, J-curve,
/// PME against a public index, commitment pacing, the whole book (AltPrivateMarketsPanel.cpp).
QWidget* make_private_markets_panel(QWidget* parent);

/// A6 — multi-year property pro forma with financing and exit, exit cap x rent growth sensitivity,
/// and a listed-REIT comparison (AltRealEstatePanel.cpp).
QWidget* make_real_estate_panel(QWidget* parent);

/// A5 — a fund / liquid-alternative return series against a benchmark (AltFundAnalyticsPanel.cpp).
QWidget* make_fund_analytics_panel(QWidget* parent);

/// A9 — the alternatives sleeve inside a portfolio: allocation, liquidity ladder, unfunded cover
/// (AltAllocationPanel.cpp).
QWidget* make_allocation_panel(QWidget* parent);

/// A10 — private credit analytics and due-diligence checklists (AltDueDiligencePanel.cpp).
QWidget* make_due_diligence_panel(QWidget* parent);

} // namespace fincept::screens::alt
