// src/screens/equity_research/EquityAnalysisTab.cpp
//
// The "Analysis" sub-tab interprets StockInfo into decisions rather than
// listing it (Overview already lists). Layout: a full-width analyst
// price-target gauge (hero) above a 3×2 grid of color-coded verdict cards
// (Valuation, Financial Health, Cash Flow, Profitability, Growth, Risk).
//
// Every rating is computed purely from StockInfo — no extra backend calls.
// The heuristics are absolute screening signals (not sector-adjusted), so
// rationales stay factual. i18n follows the existing pattern: static labels
// register an English source key in i18n_labels_; dynamic verdict/hero text
// is rebuilt via tr() each time on_info_loaded() runs, and retranslateUi()
// replays it on a language change.
#include "screens/equity_research/EquityAnalysisTab.h"

#include "services/equity/EquityResearchService.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPolygonF>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

// ── AnalysisPriceTargetGauge ───────────────────────────────────────────────────
// Custom-painted low—mean—high track with a clamped "now" marker. No Q_OBJECT
// (no signals/slots) so it needs no moc and is safe in the unity build. Numeric
// labels are drawn directly; the big price / upside / recommendation text lives
// in sibling QLabels owned by the tab.
class AnalysisPriceTargetGauge : public QWidget {
  public:
    explicit AnalysisPriceTargetGauge(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(88);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void set_data(double low, double mean, double high, double price, const QString& cur) {
        low_ = low;
        mean_ = mean;
        high_ = high;
        price_ = price;
        cur_ = cur;
        has_data_ = (low > 0.0 || mean > 0.0 || high > 0.0);
        update();
    }

    void clear_data() {
        has_data_ = false;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        if (!has_data_)
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const double w = width();
        const double margin = 12.0;
        double x_left = margin;
        double x_right = w - margin;
        if (x_right <= x_left + 1.0) {
            x_left = 2.0;
            x_right = w - 2.0;
        }
        const double y_track = std::round(height() * 0.52);
        const double track_h = 6.0;

        auto x_for = [&](double v) -> double {
            if (high_ <= low_)
                return (x_left + x_right) / 2.0;
            double t = (v - low_) / (high_ - low_);
            t = std::clamp(t, 0.0, 1.0);
            return x_left + (t * (x_right - x_left));
        };

        // Base track.
        QRectF track(x_left, y_track - (track_h / 2.0), x_right - x_left, track_h);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(ui::colors::BORDER_MED()));
        p.drawRoundedRect(track, 3, 3);

        // Filled portion from low → now (visualizes where price sits in range).
        if (price_ > 0.0) {
            double xp = x_for(price_);
            QRectF fill(x_left, y_track - (track_h / 2.0), xp - x_left, track_h);
            p.setBrush(QColor(ui::colors::CYAN()));
            p.drawRoundedRect(fill, 3, 3);
        }

        QFont small = font();
        small.setPointSizeF(std::max(10.0, small.pointSizeF() + 1.0));
        p.setFont(small);
        const QFontMetrics fm(small);

        auto money = [&](double v) -> QString {
            const int dec = v >= 1000.0 ? 0 : 2;
            return cur_ + QString::number(v, 'f', dec);
        };

        // Mean tick + label (above the track).
        if (mean_ > 0.0) {
            double xm = x_for(mean_);
            p.setPen(QPen(QColor(ui::colors::AMBER()), 1.5));
            p.drawLine(QPointF(xm, y_track - 9), QPointF(xm, y_track + 9));
            const QString t = QStringLiteral("mean ") + money(mean_);
            double tx = std::clamp(xm - (fm.horizontalAdvance(t) / 2.0), x_left, x_right - fm.horizontalAdvance(t));
            p.setPen(QColor(ui::colors::AMBER()));
            p.drawText(QPointF(tx, y_track - 12), t);
        }

        // Low / high labels (below the track ends).
        p.setPen(QColor(ui::colors::TEXT_TERTIARY()));
        if (low_ > 0.0)
            p.drawText(QPointF(x_left, y_track + 20), QStringLiteral("low ") + money(low_));
        if (high_ > 0.0) {
            const QString t = QStringLiteral("high ") + money(high_);
            p.drawText(QPointF(x_right - fm.horizontalAdvance(t), y_track + 20), t);
        }

        // "Now" marker: downward triangle sitting on the track.
        if (price_ > 0.0) {
            double xp = x_for(price_);
            QPolygonF tri;
            tri << QPointF(xp - 5, y_track - 10) << QPointF(xp + 5, y_track - 10) << QPointF(xp, y_track - 2);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(ui::colors::TEXT_PRIMARY()));
            p.drawPolygon(tri);
            // Beyond-range hint.
            if (high_ > low_ && (price_ < low_ || price_ > high_)) {
                p.setPen(QColor(ui::colors::WARNING()));
                const QString hint = price_ < low_ ? QStringLiteral("▼") : QStringLiteral("▲");
                p.drawText(QPointF(xp - (fm.horizontalAdvance(hint) / 2.0), y_track - 14), hint);
            }
        }
    }

  private:
    double low_ = 0.0;
    double mean_ = 0.0;
    double high_ = 0.0;
    double price_ = 0.0;
    QString cur_ = QStringLiteral("$");
    bool has_data_ = false;
};

// ── Panel helper ───────────────────────────────────────────────────────────────
// Class method (not a free function) so it can register the title label with the
// per-instance i18n map. retranslateUi() iterates the map and re-applies tr().

QFrame* EquityAnalysisTab::make_panel_(const char* title_key, const QString& accent_color) {
    auto* f = new QFrame;
    f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:4px; }")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(f);
    vl->setContentsMargins(18, 16, 18, 18);
    vl->setSpacing(12);

    auto* hdr = new QWidget(nullptr);
    hdr->setStyleSheet(QString("background:transparent; border:0; border-bottom:2px solid %1;").arg(accent_color));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(0, 0, 0, 8);
    hl->setSpacing(8);

    auto* bar = new QFrame;
    bar->setFixedSize(4, 16);
    bar->setStyleSheet(QString("background:%1; border:0; border-radius:0;").arg(accent_color));
    hl->addWidget(bar);

    auto* lbl = new QLabel(tr(title_key));
    lbl->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; letter-spacing:1px; "
                               "background:transparent; border:0;")
                           .arg(accent_color));
    hl->addWidget(lbl);
    hl->addStretch();
    i18n_labels_.insert(lbl, title_key);

    vl->addWidget(hdr);
    return f;
}

// ── EquityAnalysisTab ───────────────────────────────────────────────────────────

EquityAnalysisTab::EquityAnalysisTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this, &EquityAnalysisTab::on_info_loaded);
}

void EquityAnalysisTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    info_loaded_ = false;
    loading_overlay_->show_loading(tr("LOADING ANALYSIS…"));
}

void EquityAnalysisTab::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    loading_overlay_ = new ui::LoadingOverlay(this);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:0;");

    auto* content = new QWidget(this);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // Hero (full width).
    root->addWidget(build_hero_());

    // Verdict grid: 3 columns × 2 rows.
    auto* grid_host = new QWidget(nullptr);
    grid_host->setStyleSheet("background:transparent;");
    auto* grid = new QGridLayout(grid_host);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    struct DimSpec {
        Dim dim;
        const char* title;
        QString accent;
    };
    const DimSpec specs[kDimCount] = {
        {kValuation, QT_TR_NOOP("VALUATION"), ui::colors::CYAN()},
        {kHealth, QT_TR_NOOP("FINANCIAL HEALTH"), ui::colors::AMBER()},
        {kCashFlow, QT_TR_NOOP("CASH FLOW"), ui::colors::INFO()},
        {kProfitability, QT_TR_NOOP("PROFITABILITY"), ui::colors::POSITIVE()},
        {kGrowth, QT_TR_NOOP("GROWTH"), QStringLiteral("#a855f7")},
        {kRisk, QT_TR_NOOP("RISK / SENTIMENT"), ui::colors::WARNING()},
    };

    for (int i = 0; i < kDimCount; ++i) {
        auto* panel = make_panel_(specs[i].title, specs[i].accent);
        cards_[specs[i].dim] = build_verdict_card_(panel, specs[i].title, specs[i].accent);
        grid->addWidget(panel, i / 3, i % 3);
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    root->addWidget(grid_host, 1);

    scroll->setWidget(content);
    auto* ol = new QVBoxLayout(this);
    ol->setContentsMargins(0, 0, 0, 0);
    ol->addWidget(scroll);
}

QFrame* EquityAnalysisTab::build_hero_() {
    auto* panel = make_panel_(QT_TR_NOOP("ANALYST PRICE TARGET"), ui::colors::AMBER());
    auto* vl = static_cast<QVBoxLayout*>(panel->layout());

    // Headline row: now-price + upside, with the recommendation badge on the right.
    auto* head = new QWidget(nullptr);
    head->setStyleSheet("background:transparent;");
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(12);

    auto* left = new QVBoxLayout;
    left->setSpacing(2);
    hero_price_ = new QLabel(QStringLiteral("—"));
    hero_price_->setStyleSheet(QString("color:%1; font-size:26px; font-weight:800; font-family:monospace; "
                                       "background:transparent; border:0;")
                                   .arg(ui::colors::TEXT_PRIMARY()));
    hero_upside_ = new QLabel(QStringLiteral("—"));
    hero_upside_->setStyleSheet(QString("color:%1; font-size:15px; font-weight:700;"
                                        "background:transparent; border:0;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
    left->addWidget(hero_price_);
    left->addWidget(hero_upside_);
    hl->addLayout(left);
    hl->addStretch();

    auto* right = new QVBoxLayout;
    right->setSpacing(2);
    hero_reco_ = new QLabel(QStringLiteral("—"));
    hero_reco_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hero_reco_->setStyleSheet(QString("color:%1; font-size:18px; font-weight:800; letter-spacing:1px; "
                                      "background:transparent; border:0;")
                                  .arg(ui::colors::TEXT_PRIMARY()));
    hero_count_ = new QLabel(QString());
    hero_count_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hero_count_->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600; "
                                       "background:transparent; border:0;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    right->addWidget(hero_reco_);
    right->addWidget(hero_count_);
    hl->addLayout(right);

    vl->addWidget(head);

    gauge_ = new AnalysisPriceTargetGauge;
    vl->addWidget(gauge_);

    // No-coverage fallback (hidden unless there is no analyst data).
    hero_empty_ = new QLabel(tr("No analyst coverage available."));
    hero_empty_->setStyleSheet(QString("color:%1; font-size:13px; font-style:italic; "
                                       "background:transparent; border:0;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    hero_empty_->setVisible(false);
    vl->addWidget(hero_empty_);
    i18n_labels_.insert(hero_empty_, QT_TR_NOOP("No analyst coverage available."));

    return panel;
}

EquityAnalysisTab::VerdictCard EquityAnalysisTab::build_verdict_card_(QWidget* panel, const char* title_key,
                                                                      const QString& /*accent*/) {
    VerdictCard card;
    card.title_key = title_key;
    auto* vl = static_cast<QVBoxLayout*>(panel->layout());

    card.rating = new QLabel(QStringLiteral("—"));
    card.rating->setStyleSheet(QString("color:%1; font-size:26px; font-weight:800; letter-spacing:1px; "
                                       "background:transparent; border:0; padding:2px 0 6px 0;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(card.rating);

    // Metric lines grouped tightly together (their own box) so they read as a
    // block rather than drifting apart at the panel's wider spacing.
    auto* lines_box = new QWidget(nullptr);
    lines_box->setStyleSheet("background:transparent;");
    auto* lines_vl = new QVBoxLayout(lines_box);
    lines_vl->setContentsMargins(0, 0, 0, 0);
    lines_vl->setSpacing(6);
    for (auto& line : card.lines) {
        line = new QLabel(QString());
        line->setStyleSheet(QString("color:%1; font-size:15px; font-weight:600; font-family:monospace; "
                                    "background:transparent; border:0;")
                                .arg(ui::colors::TEXT_PRIMARY()));
        lines_vl->addWidget(line);
    }
    vl->addWidget(lines_box);

    card.rationale = new QLabel(QString());
    card.rationale->setWordWrap(true);
    card.rationale->setStyleSheet(QString("color:%1; font-size:13px; font-weight:500; line-height:140%; "
                                          "background:transparent; border:0; padding-top:4px;")
                                      .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(card.rationale);

    vl->addStretch();
    return card;
}

// ── Populate ────────────────────────────────────────────────────────────────────

void EquityAnalysisTab::on_info_loaded(services::equity::StockInfo info) {
    if (info.symbol != current_symbol_)
        return;
    cached_info_ = info;
    info_loaded_ = true;
    loading_overlay_->hide_loading();

    populate_hero_(info);

    apply_verdict_(cards_[kValuation], assess_valuation_(info));
    apply_verdict_(cards_[kHealth], assess_health_(info));
    apply_verdict_(cards_[kCashFlow], assess_cashflow_(info));
    apply_verdict_(cards_[kProfitability], assess_profitability_(info));
    apply_verdict_(cards_[kGrowth], assess_growth_(info));
    apply_verdict_(cards_[kRisk], assess_risk_(info));
}

void EquityAnalysisTab::populate_hero_(const services::equity::StockInfo& info) {
    const bool has_targets = info.analyst_count > 0 && (info.target_mean > 0.0 || info.target_high > 0.0);

    if (!has_targets) {
        gauge_->clear_data();
        gauge_->setVisible(false);
        hero_price_->setText(info.current_price > 0.0 ? fmt_money(info.current_price) : QStringLiteral("—"));
        hero_upside_->setVisible(false);
        hero_reco_->setText(QStringLiteral("—"));
        hero_reco_->setStyleSheet(QString("color:%1; font-size:18px; font-weight:800; letter-spacing:1px; "
                                          "background:transparent; border:0;")
                                      .arg(ui::colors::TEXT_TERTIARY()));
        hero_count_->setText(QString());
        hero_empty_->setVisible(true);
        return;
    }

    hero_empty_->setVisible(false);
    hero_upside_->setVisible(true);
    gauge_->setVisible(true);
    gauge_->set_data(info.target_low, info.target_mean, info.target_high, info.current_price, cur_symbol_());

    hero_price_->setText(info.current_price > 0.0 ? fmt_money(info.current_price) + tr(" now") : QStringLiteral("—"));

    if (info.current_price > 0.0 && info.target_mean > 0.0) {
        const double up = (info.target_mean - info.current_price) / info.current_price * 100.0;
        const QString sign = up >= 0.0 ? QStringLiteral("+") : QStringLiteral("-");
        hero_upside_->setText(QString("%1%2%  ").arg(sign).arg(std::abs(up), 0, 'f', 1) + tr("to mean target"));
        hero_upside_->setStyleSheet(QString("color:%1; font-size:15px; font-weight:700;"
                                            "background:transparent; border:0;")
                                        .arg(up >= 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    } else {
        hero_upside_->setText(tr("target range shown"));
        hero_upside_->setStyleSheet(QString("color:%1; font-size:15px; font-weight:700;"
                                            "background:transparent; border:0;")
                                        .arg(ui::colors::TEXT_TERTIARY()));
    }

    // Recommendation badge.
    const QString key = info.recommendation_key.toLower();
    QString label;
    Tone tone = Tone::Neutral;
    if (key == "strong_buy") {
        label = tr("STRONG BUY");
        tone = Tone::Good;
    } else if (key == "buy") {
        label = tr("BUY");
        tone = Tone::Good;
    } else if (key == "hold" || key == "neutral") {
        label = tr("HOLD");
        tone = Tone::Caution;
    } else if (key == "underperform" || key == "sell") {
        label = tr("SELL");
        tone = Tone::Bad;
    } else if (key == "strong_sell") {
        label = tr("STRONG SELL");
        tone = Tone::Bad;
    } else {
        label = key.isEmpty() ? QStringLiteral("—") : info.recommendation_key.toUpper();
        tone = Tone::Neutral;
    }
    hero_reco_->setText(label);
    hero_reco_->setStyleSheet(QString("color:%1; font-size:18px; font-weight:800; letter-spacing:1px; "
                                      "background:transparent; border:0;")
                                  .arg(color_for_(tone)));

    QString count = tr("%n analyst(s)", "", info.analyst_count);
    if (info.recommendation_mean > 0.0)
        count = QString("%1  ·  ").arg(QString::number(info.recommendation_mean, 'f', 1)) + count;
    hero_count_->setText(count);
}

void EquityAnalysisTab::apply_verdict_(const VerdictCard& card, const Verdict& v) {
    card.rating->setText(tr(v.rating_key));
    card.rating->setStyleSheet(QString("color:%1; font-size:26px; font-weight:800; letter-spacing:1px; "
                                       "background:transparent; border:0; padding:2px 0 6px 0;")
                                   .arg(color_for_(v.tone)));

    const QString lines[3] = {v.line1, v.line2, v.line3};
    for (int i = 0; i < 3; ++i) {
        card.lines[i]->setText(lines[i]);
        card.lines[i]->setVisible(!lines[i].isEmpty());
    }
    card.rationale->setText(v.rationale);
    card.rationale->setVisible(!v.rationale.isEmpty());
}

// ── Assessment ────────────────────────────────────────────────────────────────
// Pure functions of StockInfo. Thresholds are deliberately simple, transparent,
// and market-cap/sector-agnostic — screening signals, not investment advice.

EquityAnalysisTab::Verdict EquityAnalysisTab::assess_valuation_(const services::equity::StockInfo& s) const {
    Verdict v;
    const bool has_pe = s.pe_ratio > 0.0;
    const bool has_peg = s.peg_ratio > 0.0;
    if (!has_pe && !has_peg && s.forward_pe <= 0.0) {
        v.rating_key = QT_TR_NOOP("N/A");
        v.tone = Tone::NA;
        v.rationale = tr("No earnings-based valuation available (may be unprofitable).");
        return v;
    }

    // Prefer PEG (growth-adjusted); fall back to raw P/E.
    int score = 0; // -1 expensive, 0 fair, +1 cheap
    if (has_peg) {
        if (s.peg_ratio < 1.0)
            score = 1;
        else if (s.peg_ratio <= 2.0)
            score = 0;
        else
            score = -1;
    } else if (has_pe) {
        if (s.pe_ratio < 15.0)
            score = 1;
        else if (s.pe_ratio <= 30.0)
            score = 0;
        else
            score = -1;
    }

    if (score > 0) {
        v.rating_key = QT_TR_NOOP("UNDERVALUED");
        v.tone = Tone::Good;
    } else if (score == 0) {
        v.rating_key = QT_TR_NOOP("FAIRLY VALUED");
        v.tone = Tone::Caution;
    } else {
        v.rating_key = QT_TR_NOOP("EXPENSIVE");
        v.tone = Tone::Bad;
    }

    QString pe_line = QStringLiteral("P/E ") + (has_pe ? fmt(s.pe_ratio, 1) : QStringLiteral("—"));
    if (s.forward_pe > 0.0) {
        const QString arrow = (has_pe && s.forward_pe < s.pe_ratio) ? QStringLiteral(" ↓") : QString();
        pe_line += QString("  fwd %1%2").arg(fmt(s.forward_pe, 1), arrow);
    }
    v.line1 = pe_line;
    v.line2 = QStringLiteral("PEG ") + (has_peg ? fmt(s.peg_ratio, 2) : QStringLiteral("—"));
    if (s.price_to_book > 0.0)
        v.line2 += QString("  ·  P/B %1").arg(fmt(s.price_to_book, 1));
    if (s.ev_to_ebitda > 0.0)
        v.line3 = QString("EV/EBITDA %1").arg(fmt(s.ev_to_ebitda, 1));

    if (has_pe && s.forward_pe > 0.0 && s.forward_pe < s.pe_ratio)
        v.rationale = tr("Forward P/E below trailing — earnings expected to grow.");
    else if (score < 0)
        v.rationale = tr("Trades at a premium; priced for growth or quality.");
    else if (score > 0)
        v.rationale = tr("Low multiple relative to earnings/growth.");
    else
        v.rationale = tr("Valuation in line with broad-market norms.");
    return v;
}

EquityAnalysisTab::Verdict EquityAnalysisTab::assess_health_(const services::equity::StockInfo& s) const {
    Verdict v;
    if (s.total_cash <= 0.0 && s.total_debt <= 0.0) {
        v.rating_key = QT_TR_NOOP("N/A");
        v.tone = Tone::NA;
        v.rationale = tr("Balance-sheet cash/debt not reported.");
        return v;
    }

    const double net = s.total_cash - s.total_debt;
    v.line1 = tr("Cash %1").arg(fmt_large(s.total_cash));
    v.line2 = tr("Debt %1").arg(fmt_large(s.total_debt));
    v.line3 = tr("Net %1%2").arg(net >= 0.0 ? QStringLiteral("+") : QStringLiteral("")).arg(fmt_large(net));

    if (net > 0.0) {
        v.rating_key = QT_TR_NOOP("STRONG");
        v.tone = Tone::Good;
        v.rationale = tr("Net cash position — more cash than total debt.");
    } else {
        const double ratio = s.total_cash > 0.0 ? s.total_debt / s.total_cash : 999.0;
        if (ratio < 2.0) {
            v.rating_key = QT_TR_NOOP("STABLE");
            v.tone = Tone::Caution;
            v.rationale = tr("Manageable leverage relative to cash on hand.");
        } else {
            v.rating_key = QT_TR_NOOP("STRETCHED");
            v.tone = Tone::Bad;
            v.rationale = tr("Debt is high relative to available cash.");
        }
    }
    return v;
}

EquityAnalysisTab::Verdict EquityAnalysisTab::assess_cashflow_(const services::equity::StockInfo& s) const {
    Verdict v;
    if (s.free_cashflow == 0.0 && s.operating_cashflow == 0.0) {
        v.rating_key = QT_TR_NOOP("N/A");
        v.tone = Tone::NA;
        v.rationale = tr("Cash-flow figures not reported.");
        return v;
    }

    v.line1 = tr("FCF %1").arg(fmt_large(s.free_cashflow));
    v.line2 = tr("Op CF %1").arg(fmt_large(s.operating_cashflow));
    if (s.total_revenue > 0.0)
        v.line3 = tr("FCF margin %1").arg(fmt_pct(s.free_cashflow / s.total_revenue));

    if (s.free_cashflow > 0.0) {
        v.rating_key = QT_TR_NOOP("STRONG");
        v.tone = Tone::Good;
        v.rationale = tr("Generates positive free cash flow after capex.");
    } else if (s.operating_cashflow > 0.0) {
        v.rating_key = QT_TR_NOOP("REINVESTING");
        v.tone = Tone::Caution;
        v.rationale = tr("Operating cash is positive but FCF is negative (heavy investment).");
    } else {
        v.rating_key = QT_TR_NOOP("BURNING CASH");
        v.tone = Tone::Bad;
        v.rationale = tr("Operations are consuming cash.");
    }
    return v;
}

EquityAnalysisTab::Verdict EquityAnalysisTab::assess_profitability_(const services::equity::StockInfo& s) const {
    Verdict v;
    const bool any = s.roe != 0.0 || s.roa != 0.0 || s.profit_margins != 0.0;
    if (!any) {
        v.rating_key = QT_TR_NOOP("N/A");
        v.tone = Tone::NA;
        v.rationale = tr("Profitability ratios not reported.");
        return v;
    }

    v.line1 = QString("ROE %1   ROA %2").arg(fmt_pct(s.roe), fmt_pct(s.roa));
    v.line2 = tr("Net margin %1").arg(fmt_pct(s.profit_margins));
    if (s.operating_margins != 0.0)
        v.line3 = tr("Oper. margin %1").arg(fmt_pct(s.operating_margins));

    if (s.profit_margins < 0.0) {
        v.rating_key = QT_TR_NOOP("LOSS-MAKING");
        v.tone = Tone::Bad;
        v.rationale = tr("Currently unprofitable on a net basis.");
    } else if (s.roe > 0.20 && s.profit_margins > 0.15) {
        v.rating_key = QT_TR_NOOP("EXCELLENT");
        v.tone = Tone::Good;
        v.rationale = tr("High returns on equity and strong net margins.");
    } else if (s.roe > 0.10 || s.profit_margins > 0.08) {
        v.rating_key = QT_TR_NOOP("SOLID");
        v.tone = Tone::Good;
        v.rationale = tr("Healthy, consistent profitability.");
    } else {
        v.rating_key = QT_TR_NOOP("THIN");
        v.tone = Tone::Caution;
        v.rationale = tr("Profitable, but margins and returns are slim.");
    }
    return v;
}

EquityAnalysisTab::Verdict EquityAnalysisTab::assess_growth_(const services::equity::StockInfo& s) const {
    Verdict v;
    if (s.revenue_growth == 0.0 && s.earnings_growth == 0.0) {
        v.rating_key = QT_TR_NOOP("N/A");
        v.tone = Tone::NA;
        v.rationale = tr("Growth rates not reported.");
        return v;
    }

    v.line1 = tr("Revenue %1").arg(fmt_pct(s.revenue_growth));
    v.line2 = tr("Earnings %1").arg(fmt_pct(s.earnings_growth));

    const double g = s.revenue_growth != 0.0 ? s.revenue_growth : s.earnings_growth;
    if (g > 0.20) {
        v.rating_key = QT_TR_NOOP("HIGH GROWTH");
        v.tone = Tone::Good;
        v.rationale = tr("Top line expanding rapidly.");
    } else if (g >= 0.05) {
        v.rating_key = QT_TR_NOOP("MODERATE");
        v.tone = Tone::Caution;
        v.rationale = tr("Steady single-to-double-digit growth.");
    } else if (g >= -0.02) {
        v.rating_key = QT_TR_NOOP("FLAT");
        v.tone = Tone::Neutral;
        v.rationale = tr("Revenue is roughly unchanged year over year.");
    } else {
        v.rating_key = QT_TR_NOOP("DECLINING");
        v.tone = Tone::Bad;
        v.rationale = tr("Revenue is contracting year over year.");
    }
    return v;
}

EquityAnalysisTab::Verdict EquityAnalysisTab::assess_risk_(const services::equity::StockInfo& s) const {
    Verdict v;
    const bool has_beta = s.beta != 0.0;
    if (!has_beta && s.short_pct_of_float == 0.0 && s.week52_high <= 0.0) {
        v.rating_key = QT_TR_NOOP("N/A");
        v.tone = Tone::NA;
        v.rationale = tr("Risk metrics not reported.");
        return v;
    }

    if (has_beta)
        v.line1 = tr("Beta %1").arg(fmt(s.beta, 2));
    if (s.short_pct_of_float > 0.0)
        v.line2 = tr("Short %1 of float").arg(fmt_pct(s.short_pct_of_float));
    if (s.week52_high > s.week52_low && s.current_price > 0.0) {
        const double pos = (s.current_price - s.week52_low) / (s.week52_high - s.week52_low) * 100.0;
        v.line3 = tr("52w position %1%").arg(QString::number(std::clamp(pos, 0.0, 100.0), 'f', 0));
    }

    const bool high_short = s.short_pct_of_float > 0.10;
    if ((has_beta && s.beta > 1.5) || high_short) {
        v.rating_key = QT_TR_NOOP("ELEVATED");
        v.tone = Tone::Bad;
        v.rationale = high_short ? tr("Heavy short interest signals bearish positioning.")
                                 : tr("High beta — amplifies market moves.");
    } else if (has_beta && s.beta > 1.0) {
        v.rating_key = QT_TR_NOOP("MODERATE");
        v.tone = Tone::Caution;
        v.rationale = tr("Moves roughly in line with, or above, the market.");
    } else if (has_beta) {
        v.rating_key = QT_TR_NOOP("LOW");
        v.tone = Tone::Good;
        v.rationale = tr("Lower volatility than the broad market.");
    } else {
        // No beta — don't claim a volatility level we can't support.
        v.rating_key = QT_TR_NOOP("MODERATE");
        v.tone = Tone::Neutral;
        v.rationale = tr("Beta unavailable; based on limited risk signals.");
    }
    return v;
}

// ── Formatting ────────────────────────────────────────────────────────────────

QString EquityAnalysisTab::fmt(double v, int decimals) const {
    return v != 0.0 ? QString::number(v, 'f', decimals) : QStringLiteral("—");
}

QString EquityAnalysisTab::fmt_large(double v) {
    if (v == 0.0)
        return QStringLiteral("—");
    const bool neg = v < 0;
    const double a = std::abs(v);
    QString s;
    if (a >= 1e12)
        s = QString("%1T").arg(a / 1e12, 0, 'f', 2);
    else if (a >= 1e9)
        s = QString("%1B").arg(a / 1e9, 0, 'f', 2);
    else if (a >= 1e6)
        s = QString("%1M").arg(a / 1e6, 0, 'f', 1);
    else
        s = QString::number(a, 'f', 0);
    return neg ? "-" + s : s;
}

QString EquityAnalysisTab::fmt_pct(double v) const {
    return v != 0.0 ? QString("%1%").arg(v * 100.0, 0, 'f', 2) : QStringLiteral("—");
}

QString EquityAnalysisTab::cur_symbol_() const {
    const QString c = cached_info_.currency.toUpper();
    if (c == "INR")
        return QStringLiteral("₹");
    if (c == "EUR")
        return QStringLiteral("€");
    if (c == "GBP")
        return QStringLiteral("£");
    if (c == "JPY")
        return QStringLiteral("¥");
    if (c.isEmpty() || c == "USD")
        return QStringLiteral("$");
    return c + QStringLiteral(" ");
}

QString EquityAnalysisTab::fmt_money(double v) const {
    if (v == 0.0)
        return QStringLiteral("—");
    const int dec = std::abs(v) >= 1000.0 ? 0 : 2;
    return cur_symbol_() + QString::number(v, 'f', dec);
}

QString EquityAnalysisTab::color_for_(Tone t) const {
    switch (t) {
        case Tone::Good:
            return ui::colors::POSITIVE();
        case Tone::Caution:
            return ui::colors::AMBER();
        case Tone::Bad:
            return ui::colors::NEGATIVE();
        case Tone::Neutral:
            return ui::colors::TEXT_SECONDARY();
        case Tone::NA:
        default:
            return ui::colors::TEXT_TERTIARY();
    }
}

// ── Re-translation ──────────────────────────────────────────────────────────────
// Re-apply tr() to static labels, then replay cached info so all dynamic
// hero/verdict text picks up the new language too.

void EquityAnalysisTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityAnalysisTab::retranslateUi() {
    for (auto it = i18n_labels_.constBegin(); it != i18n_labels_.constEnd(); ++it)
        it.key()->setText(tr(it.value()));
    if (info_loaded_)
        on_info_loaded(cached_info_);
}

} // namespace fincept::screens
