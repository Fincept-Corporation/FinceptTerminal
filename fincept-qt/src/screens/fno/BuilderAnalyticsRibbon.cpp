#include "screens/fno/BuilderAnalyticsRibbon.h"

#include "screens/fno/FnoFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace fincept::screens::fno {

using fincept::services::options::Strategy;
using fincept::services::options::StrategyAnalytics;
using namespace fincept::ui;

namespace {

namespace fmt = fincept::ui::formatting;

/// A Greek (not money): signed, in the user's number locale; ±∞ for an unbounded value.
QString bar_fmt_signed(double v, int decimals = 2) {
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("+∞") : QStringLiteral("−∞");
    return fmt::signed_number(v, decimals);
}

/// A payoff bound, signed ("+₹1,240", "-₹820"); "+∞" / "−∞" when unlimited.
QString bar_fmt_bound(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("+∞") : QStringLiteral("−∞");
    return fno_signed_money(v, ccy);
}

const char* bar_tone(double v) {
    return v > 0 ? "positive" : (v < 0 ? "negative" : "neutral");
}

void add_kv(QHBoxLayout* row, QLabel*& value_out, QLabel*& key_out, const QString& key, QWidget* parent) {
    auto* k = new QLabel(key.toUpper(), parent);
    k->setObjectName("fnoRbnKey");
    auto* v = new QLabel(QString::fromUtf8("—"), parent);
    v->setObjectName("fnoRbnVal");
    v->setProperty("pnl", "neutral");
    row->addWidget(k);
    row->addWidget(v);
    row->addSpacing(12);
    value_out = v;
    key_out = k;
}

void set_pnl_color(QLabel* lbl, const QString& pnl_class) {
    lbl->setProperty("pnl", pnl_class);
    lbl->style()->unpolish(lbl);
    lbl->style()->polish(lbl);
}

} // namespace

BuilderAnalyticsRibbon::BuilderAnalyticsRibbon(QWidget* parent) : QWidget(parent), ccy_(fno_ccy()) {
    setObjectName("fnoBuilderRibbon");
    setStyleSheet(
        QString("#fnoBuilderRibbon { background:%1; border-bottom:1px solid %2; }"
                "#fnoRbnKey { color:%3; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; "
                "  letter-spacing:0.6px; background:transparent; }"
                "#fnoRbnVal { color:%4; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; background:transparent; }"
                "QLabel[pnl=\"positive\"] { color: %5; }"
                "QLabel[pnl=\"negative\"] { color: %6; }"
                "QLabel[pnl=\"neutral\"]  { color: %4; }")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM(), colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(),
                 colors::POSITIVE(), colors::NEGATIVE()));
    setup_ui();
    // A currency / number-format change re-formats what is on show; nothing is recomputed.
    fmt::on_format_changed(this, [this]() { render_from_cache(); });
}

void BuilderAnalyticsRibbon::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Row 1
    // 22px tall at the default font size; grows with larger fonts instead of clipping.
    auto* row1_w = new QWidget(this);
    row1_w->setMinimumHeight(22);
    row1_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* row1 = new QHBoxLayout(row1_w);
    row1->setContentsMargins(6, 2, 6, 2);
    row1->setSpacing(0);

    add_kv(row1, lbl_premium_, key_premium_, tr("Premium"), row1_w);
    add_kv(row1, lbl_max_profit_, key_max_profit_, tr("Max Profit"), row1_w);
    add_kv(row1, lbl_max_loss_, key_max_loss_, tr("Max Loss"), row1_w);
    add_kv(row1, lbl_breakevens_, key_breakevens_, tr("Breakevens"), row1_w);
    row1->addStretch(1);
    root->addWidget(row1_w);

    // Row 2
    auto* row2_w = new QWidget(this);
    row2_w->setMinimumHeight(22);
    row2_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* row2 = new QHBoxLayout(row2_w);
    row2->setContentsMargins(6, 2, 6, 2);
    row2->setSpacing(0);

    add_kv(row2, lbl_pop_, key_pop_, tr("POP"), row2_w);
    add_kv(row2, lbl_delta_, key_delta_, tr("Delta"), row2_w);
    add_kv(row2, lbl_gamma_, key_gamma_, tr("Gamma"), row2_w);
    add_kv(row2, lbl_theta_, key_theta_, tr("Theta"), row2_w);
    add_kv(row2, lbl_vega_, key_vega_, tr("Vega"), row2_w);
    add_kv(row2, lbl_margin_, key_margin_, tr("Margin"), row2_w);
    row2->addStretch(1);
    root->addWidget(row2_w);

    // Units are the single biggest source of misreading on a Greeks strip.
    apply_unit_tooltips();
}

void BuilderAnalyticsRibbon::set_currency(const QString& ccy) {
    const QString c = ccy.trimmed().isEmpty() ? fno_ccy() : ccy.trimmed();
    if (c == ccy_)
        return;
    ccy_ = c;
    render_from_cache();
}

void BuilderAnalyticsRibbon::apply_unit_tooltips() {
    // The strategy's currency unit as the user's symbol style prints it ("₹", "INR", "$").
    const QString unit = fmt::currency_unit(ccy_);
    for (QLabel* l : {key_vega_, lbl_vega_})
        if (l)
            l->setToolTip(tr("Position vega: change in strategy value (%1) for a 1-point (1%) move in implied "
                             "volatility.")
                              .arg(unit));
    for (QLabel* l : {key_theta_, lbl_theta_})
        if (l)
            l->setToolTip(tr("Position theta: change in strategy value (%1) per calendar day.").arg(unit));
    for (QLabel* l : {key_delta_, lbl_delta_})
        if (l)
            l->setToolTip(tr("Position delta: change in strategy value (%1) per 1-point move in the underlying.")
                              .arg(unit));
    for (QLabel* l : {key_premium_, lbl_premium_})
        if (l)
            l->setToolTip(tr("Net premium in %1: Dr = net debit (paid), Cr = net credit (received).").arg(unit));
}

void BuilderAnalyticsRibbon::update_from(const Strategy& s, const StrategyAnalytics& a) {
    Q_UNUSED(s);
    last_analytics_ = a;
    has_analytics_ = true;
    render_analytics();
}

void BuilderAnalyticsRibbon::render_analytics() {
    if (!has_analytics_)
        return;
    const StrategyAnalytics& a = last_analytics_;

    // Dr / Cr are the debit / credit markers of a net premium (not crores).
    const QString premium = fno_money(std::abs(a.premium_paid), ccy_);
    lbl_premium_->setText(a.premium_paid > 0   ? tr("%1  Dr", "net debit: premium paid").arg(premium)
                          : a.premium_paid < 0 ? tr("%1  Cr", "net credit: premium received").arg(premium)
                                               : premium);
    set_pnl_color(lbl_premium_, a.premium_paid > 0 ? "negative" : a.premium_paid < 0 ? "positive" : "neutral");

    // Signed, so the direction never rests on the colour alone.
    lbl_max_profit_->setText(bar_fmt_bound(a.max_profit, ccy_));
    set_pnl_color(lbl_max_profit_, bar_tone(a.max_profit));

    lbl_max_loss_->setText(bar_fmt_bound(a.max_loss, ccy_));
    set_pnl_color(lbl_max_loss_, bar_tone(a.max_loss));

    if (a.breakevens.isEmpty()) {
        lbl_breakevens_->setText(QString::fromUtf8("—"));
    } else {
        // Underlying levels (index points / share prices), not amounts of money.
        QStringList parts;
        const int show = qMin(a.breakevens.size(), 4);
        for (int i = 0; i < show; ++i)
            parts.append(fmt::number(a.breakevens[i], 0));
        if (a.breakevens.size() > 4)
            parts.append(QString("(+%1)").arg(a.breakevens.size() - 4));
        lbl_breakevens_->setText(parts.join(" / "));
    }
    set_pnl_color(lbl_breakevens_, "neutral");

    lbl_pop_->setText(fmt::pct(a.pop * 100.0, 1, false));
    set_pnl_color(lbl_pop_, "neutral");

    if (a.combined.valid) {
        lbl_delta_->setText(bar_fmt_signed(a.combined.delta, 2));
        set_pnl_color(lbl_delta_, a.combined.delta >= 0 ? "positive" : "negative");

        lbl_gamma_->setText(bar_fmt_signed(a.combined.gamma, 4));
        set_pnl_color(lbl_gamma_, "neutral");

        lbl_theta_->setText(bar_fmt_signed(a.combined.theta, 1));
        set_pnl_color(lbl_theta_, a.combined.theta >= 0 ? "positive" : "negative");

        // combined.vega is in OptionGreeks' "per 1.00 σ" convention (Σ vega × lots × lot
        // size). The figure a trader reads as "vega" is rupees per ONE vol point (1% IV),
        // i.e. a hundredth of that — shown raw it was 100× too large.
        lbl_vega_->setText(bar_fmt_signed(a.combined.vega / 100.0, 1));
        set_pnl_color(lbl_vega_, "neutral");
    } else {
        for (auto* l : {lbl_delta_, lbl_gamma_, lbl_theta_, lbl_vega_}) {
            l->setText(QString::fromUtf8("—"));
            set_pnl_color(l, "neutral");
        }
    }
}

void BuilderAnalyticsRibbon::clear() {
    has_analytics_ = false;
    has_margin_ = false;
    for (auto* l : {lbl_premium_, lbl_max_profit_, lbl_max_loss_, lbl_breakevens_, lbl_pop_, lbl_delta_, lbl_gamma_,
                    lbl_theta_, lbl_vega_, lbl_margin_}) {
        l->setText(QString::fromUtf8("—"));
        set_pnl_color(l, "neutral");
    }
}

void BuilderAnalyticsRibbon::set_margin(double value, bool estimated) {
    last_margin_ = value;
    margin_estimated_ = estimated;
    has_margin_ = true;
    render_margin();
}

void BuilderAnalyticsRibbon::render_margin() {
    if (!has_margin_)
        return;
    // "~" marks a paper / heuristic estimate rather than a broker figure.
    const QString value = fno_money(last_margin_, ccy_);
    lbl_margin_->setText(margin_estimated_ ? tr("~ %1", "estimated amount").arg(value) : value);
    set_pnl_color(lbl_margin_, margin_estimated_ ? "neutral" : "positive");
}

void BuilderAnalyticsRibbon::clear_margin() {
    has_margin_ = false;
    lbl_margin_->setText(QString::fromUtf8("—"));
    set_pnl_color(lbl_margin_, "neutral");
}

void BuilderAnalyticsRibbon::render_from_cache() {
    apply_unit_tooltips();
    render_analytics();
    render_margin();
}

void BuilderAnalyticsRibbon::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void BuilderAnalyticsRibbon::retranslateUi() {
    // Keys mirror add_kv's .toUpper() rendering. Value cells carry data only.
    if (key_premium_)
        key_premium_->setText(tr("Premium").toUpper());
    if (key_max_profit_)
        key_max_profit_->setText(tr("Max Profit").toUpper());
    if (key_max_loss_)
        key_max_loss_->setText(tr("Max Loss").toUpper());
    if (key_breakevens_)
        key_breakevens_->setText(tr("Breakevens").toUpper());
    if (key_pop_)
        key_pop_->setText(tr("POP").toUpper());
    if (key_delta_)
        key_delta_->setText(tr("Delta").toUpper());
    if (key_gamma_)
        key_gamma_->setText(tr("Gamma").toUpper());
    if (key_theta_)
        key_theta_->setText(tr("Theta").toUpper());
    if (key_vega_)
        key_vega_->setText(tr("Vega").toUpper());
    if (key_margin_)
        key_margin_->setText(tr("Margin").toUpper());
    apply_unit_tooltips();
    // Dr / Cr and the estimate marker are translated too.
    render_analytics();
    render_margin();
}

} // namespace fincept::screens::fno
