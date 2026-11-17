// src/screens/equity_research/EquityFinancialsCommon.h
//
// Shared pieces of the Financials tab (no Q_OBJECT here):
//   * FinBundle — everything computed from one statements payload, built OFF the GUI thread
//     (fin_bundle()): the three period bases' analyses and as-reported statement tables;
//   * FinViewState — what every section shows (basis, units, selected period, number scale);
//   * text helpers on top of ui::formatting (amounts in the chosen scale with the user's
//     negative style, money, percentages, multiples, days, "NM" / "—" with the reason);
//   * the tab's one stylesheet and the panel frame (ui::PanelHeader + body).
#pragma once

#include "screens/equity_research/EquityFinancialsAnalytics.h"
#include "screens/equity_research/EquityStatementModel.h"
#include "services/equity/EquityResearchModels.h"

#include <QString>
#include <QVector>

#include <memory>

class QFrame;
class QLabel;
class QVBoxLayout;
class QWidget;

namespace fincept::ui {
class PanelHeader;
}

namespace fincept::screens {

enum FinBasis { kFinAnnual = 0, kFinQuarterly = 1, kFinTtm = 2 };
enum FinUnits { kFinReported = 0, kFinCommonSize = 1, kFinGrowth = 2 };

/// Everything computed from one statements payload (immutable once built; shared by pointer).
struct FinBundle {
    QString symbol;
    services::equity::FinancialsData data;
    services::equity::StockInfo info;
    fin_an::Analysis an[3];          ///< by FinBasis
    fin_model::Table stmt[3][3];     ///< [FinBasis][fin_model::Statement], as reported
    QVector<fin_an::PeriodInfo> stmt_periods[3][3];
    bool has_quarters = false;
    qint64 fetched_at = 0; ///< epoch seconds (0 = unknown)
    bool stale = false;    ///< the last-known-good copy replayed while a refresh runs
};
using FinBundlePtr = std::shared_ptr<const FinBundle>;

/// Builds the bundle (pure; safe on a worker thread). `info` supplies the sector (template,
/// Altman variant) and, when the listing and reporting currencies agree, the market value.
FinBundlePtr fin_bundle(const services::equity::FinancialsData& data, const services::equity::StockInfo& info);

/// The reporting currency (major ISO code) of the statements: info.financial_currency, else the
/// quote's major currency; empty when unknown.
QString fin_reporting_currency(const services::equity::StockInfo& info);

struct FinViewState {
    int basis = kFinAnnual;
    int units = kFinReported;
    int selected = -1;  ///< period index in the basis' analysis
    QString ccy;        ///< reporting currency (major ISO)
    double scale = 1e6; ///< statement cells are divided by this
    int decimals = 0;
    QString scale_word; ///< "millions", "crore", … (translated)
};

/// Picks the scale for the statement cells from the bundle's largest amounts and the user's
/// compact scheme (K / M / B or lakh / crore). GUI thread (reads the format settings).
void fin_choose_scale(FinViewState& vs, const FinBundle& b);

// ── Text ─────────────────────────────────────────────────────────────────────────────────────
/// A statement cell: |v| / scale with the scale's decimals; negatives per the user's negative
/// style ("(1,234)" or "-1,234"); "—" for NaN.
QString fin_amount(double v, const FinViewState& vs);
/// Abbreviated money in `ccy`: "$478.8B" (the user's compact scheme).
QString fin_money(double v, const QString& ccy);
/// A value of any analytics unit.
QString fin_value(double v, fin_an::Unit unit, const QString& ccy);
/// A change: Percent → "+1.0 pp", Money / PerShare → "+15.1%", Multiple → "+0.10x", Days → "+3 d".
QString fin_change(double change, fin_an::Unit unit);
/// The text for a value that could not be computed: "NM" when the reason says so, else "—".
QString fin_missing_text(const QString& note);
/// "+12.3%" from a fraction (always signed).
QString fin_pct_signed(double fraction, int decimals = 1);
/// Arrow for a signed change: "▲" / "▼" / "".
QString fin_arrow(double change);

// ── Look ─────────────────────────────────────────────────────────────────────────────────────
/// The tab's stylesheet (object-name selectors, tokens only), applied once on the tab root and
/// re-derived after an appearance change has settled.
QString fin_stylesheet();

/// A panel: QFrame#finPanel with a ui::PanelHeader on top and a body layout (margins `margin`).
struct FinPanel {
    QFrame* frame = nullptr;
    ui::PanelHeader* header = nullptr;
    QVBoxLayout* body = nullptr;
};
FinPanel fin_panel(const QString& title, QWidget* parent, int margin = 8);

/// A dim one-line caption under a chart (QLabel#finCaption, word-wrapped).
QLabel* fin_caption(QWidget* parent);

} // namespace fincept::screens
