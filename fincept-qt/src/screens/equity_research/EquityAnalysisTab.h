// src/screens/equity_research/EquityAnalysisTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QHash>
#include <QLabel>
#include <QVector>
#include <QWidget>

#include <array>
#include <cmath>
#include <cstdint>

class QFrame;

namespace fincept::screens {

// Custom-painted analyst price-target gauge (defined in the .cpp, no Q_OBJECT).
class AnalysisPriceTargetGauge;

// EquityAnalysisTab — the "Analysis" sub-tab of Equity Research.
//
// Unlike Overview (which lists raw fundamentals), this tab *interprets* the
// same StockInfo into decisions: an analyst price-target gauge plus six
// color-coded verdict cards. All ratings are computed purely from StockInfo —
// no extra backend calls. Heuristics are absolute screening signals (not
// sector-adjusted); rationales stay factual rather than prescriptive.
class EquityAnalysisTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityAnalysisTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  private slots:
    void on_info_loaded(services::equity::StockInfo info);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    // ── Verdict model ─────────────────────────────────────────────────────────
    enum class Tone : std::uint8_t { Good, Caution, Bad, Neutral, NA };

    struct Verdict {
        const char* rating_key = "—"; ///< QT_TR_NOOP literal chosen at assess time
        Tone tone = Tone::NA;
        QString line1;
        QString line2;
        QString line3;
        QString rationale; ///< already tr()'d at build time
    };

    // One verdict card's live labels (scaffolding built once in build_ui).
    struct VerdictCard {
        const char* title_key = nullptr;
        QLabel* rating = nullptr;
        std::array<QLabel*, 3> lines{nullptr, nullptr, nullptr};
        QLabel* rationale = nullptr;
    };

    enum Dim : std::uint8_t { kValuation, kHealth, kCashFlow, kProfitability, kGrowth, kRisk, kDimCount };

    // ── Build ───────────────────────────────────────────────────────────────
    void build_ui();
    QFrame* build_hero_();
    VerdictCard build_verdict_card_(QWidget* parent_grid_cell, const char* title_key, const QString& accent);
    void retranslateUi();

    QFrame* make_panel_(const char* title_key, const QString& accent_color);

    // ── Populate ──────────────────────────────────────────────────────────────
    void populate_hero_(const services::equity::StockInfo& info);
    void apply_verdict_(const VerdictCard& card, const Verdict& v);

    // ── Assessment (pure, from StockInfo) ──────────────────────────────────────
    Verdict assess_valuation_(const services::equity::StockInfo& s) const;
    Verdict assess_health_(const services::equity::StockInfo& s) const;
    Verdict assess_cashflow_(const services::equity::StockInfo& s) const;
    Verdict assess_profitability_(const services::equity::StockInfo& s) const;
    Verdict assess_growth_(const services::equity::StockInfo& s) const;
    Verdict assess_risk_(const services::equity::StockInfo& s) const;

    // ── Formatting ─────────────────────────────────────────────────────────────
    QString fmt(double v, int decimals = 2) const;
    static QString fmt_large(double v);
    QString fmt_pct(double v) const; ///< v is a fraction (0.31 → "31.00%")
    QString fmt_money(double v) const; ///< currency-symbol prefixed price
    QString cur_symbol_() const;       ///< $/₹/€/£ from cached_info_.currency
    QString color_for_(Tone t) const;

    // ── State ──────────────────────────────────────────────────────────────────
    QHash<QLabel*, const char*> i18n_labels_; ///< static label → English source key
    services::equity::StockInfo cached_info_;
    bool info_loaded_ = false;
    QString current_symbol_;

    // Hero
    AnalysisPriceTargetGauge* gauge_ = nullptr;
    QLabel* hero_price_ = nullptr;    ///< "$182.40 now"
    QLabel* hero_upside_ = nullptr;   ///< "+18.3% to mean target"
    QLabel* hero_reco_ = nullptr;     ///< recommendation badge
    QLabel* hero_count_ = nullptr;    ///< "34 analysts"
    QLabel* hero_empty_ = nullptr;    ///< "No analyst coverage" fallback

    // Verdict cards (one per Dim)
    std::array<VerdictCard, kDimCount> cards_{};

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
