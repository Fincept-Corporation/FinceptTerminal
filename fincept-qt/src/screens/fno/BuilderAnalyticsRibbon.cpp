#include "screens/fno/BuilderAnalyticsRibbon.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLocale>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace fincept::screens::fno {

using fincept::services::options::Strategy;
using fincept::services::options::StrategyAnalytics;
using namespace fincept::ui;

namespace {

QString fmt_signed(double v, int decimals = 2) {
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("+∞") : QStringLiteral("−∞");
    const QString s = QString::number(v, 'f', decimals);
    return v >= 0 ? "+" + s : s;
}

QString fmt_currency(double v) {
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("∞") : QStringLiteral("−∞");
    return QLocale(QLocale::English).toString(v, 'f', 0);
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

}  // namespace

BuilderAnalyticsRibbon::BuilderAnalyticsRibbon(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoBuilderRibbon");
    setStyleSheet(QString(
        "#fnoBuilderRibbon { background:%1; border-bottom:1px solid %2; }"
        "#fnoRbnKey { color:%3; font-size:9px; font-weight:700; "
        "  letter-spacing:0.6px; background:transparent; }"
        "#fnoRbnVal { color:%4; font-size:11px; font-weight:700; background:transparent; }"
        "QLabel[pnl=\"positive\"] { color: #16a34a; }"
        "QLabel[pnl=\"negative\"] { color: #dc2626; }"
        "QLabel[pnl=\"neutral\"]  { color: %4; }")
                      .arg(colors::BG_RAISED(), colors::BORDER_DIM(),
                           colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY()));
    setup_ui();
}

void BuilderAnalyticsRibbon::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Row 1
    auto* row1_w = new QWidget(this);
    row1_w->setFixedHeight(22);
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
    row2_w->setFixedHeight(22);
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
}

void BuilderAnalyticsRibbon::update_from(const Strategy& s, const StrategyAnalytics& a) {
    Q_UNUSED(s);

    lbl_premium_->setText(fmt_currency(std::abs(a.premium_paid)) +
                          (a.premium_paid > 0 ? "  Dr" : a.premium_paid < 0 ? "  Cr" : ""));
    set_pnl_color(lbl_premium_, a.premium_paid > 0 ? "negative"
                                : a.premium_paid < 0 ? "positive" : "neutral");

    lbl_max_profit_->setText(fmt_currency(a.max_profit));
    set_pnl_color(lbl_max_profit_, "positive");

    lbl_max_loss_->setText(fmt_currency(a.max_loss));
    set_pnl_color(lbl_max_loss_, "negative");

    if (a.breakevens.isEmpty()) {
        lbl_breakevens_->setText(QString::fromUtf8("—"));
    } else {
        QStringList parts;
        const int show = qMin(a.breakevens.size(), 4);
        for (int i = 0; i < show; ++i)
            parts.append(QString::number(a.breakevens[i], 'f', 0));
        if (a.breakevens.size() > 4)
            parts.append(QString("(+%1)").arg(a.breakevens.size() - 4));
        lbl_breakevens_->setText(parts.join(" / "));
    }
    set_pnl_color(lbl_breakevens_, "neutral");

    lbl_pop_->setText(QString::number(a.pop * 100.0, 'f', 1) + "%");
    set_pnl_color(lbl_pop_, "neutral");

    if (a.combined.valid) {
        lbl_delta_->setText(fmt_signed(a.combined.delta, 2));
        set_pnl_color(lbl_delta_, a.combined.delta >= 0 ? "positive" : "negative");

        lbl_gamma_->setText(fmt_signed(a.combined.gamma, 4));
        set_pnl_color(lbl_gamma_, "neutral");

        lbl_theta_->setText(fmt_signed(a.combined.theta, 1));
        set_pnl_color(lbl_theta_, a.combined.theta >= 0 ? "positive" : "negative");

        lbl_vega_->setText(fmt_signed(a.combined.vega, 1));
        set_pnl_color(lbl_vega_, "neutral");
    } else {
        for (auto* l : {lbl_delta_, lbl_gamma_, lbl_theta_, lbl_vega_}) {
            l->setText(QString::fromUtf8("—"));
            set_pnl_color(l, "neutral");
        }
    }
}

void BuilderAnalyticsRibbon::clear() {
    for (auto* l : {lbl_premium_, lbl_max_profit_, lbl_max_loss_, lbl_breakevens_, lbl_pop_,
                    lbl_delta_, lbl_gamma_, lbl_theta_, lbl_vega_, lbl_margin_}) {
        l->setText(QString::fromUtf8("—"));
        set_pnl_color(l, "neutral");
    }
}

void BuilderAnalyticsRibbon::set_margin(double value, bool estimated) {
    const QString prefix = estimated ? QStringLiteral("~ ₹ ") : QStringLiteral("₹ ");
    lbl_margin_->setText(prefix + fmt_currency(value));
    set_pnl_color(lbl_margin_, estimated ? "neutral" : "positive");
}

void BuilderAnalyticsRibbon::clear_margin() {
    lbl_margin_->setText(QString::fromUtf8("—"));
    set_pnl_color(lbl_margin_, "neutral");
}

void BuilderAnalyticsRibbon::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void BuilderAnalyticsRibbon::retranslateUi() {
    // Keys mirror add_kv's .toUpper() rendering. Value cells carry data only.
    if (key_premium_)     key_premium_->setText(tr("Premium").toUpper());
    if (key_max_profit_)  key_max_profit_->setText(tr("Max Profit").toUpper());
    if (key_max_loss_)    key_max_loss_->setText(tr("Max Loss").toUpper());
    if (key_breakevens_)  key_breakevens_->setText(tr("Breakevens").toUpper());
    if (key_pop_)         key_pop_->setText(tr("POP").toUpper());
    if (key_delta_)       key_delta_->setText(tr("Delta").toUpper());
    if (key_gamma_)       key_gamma_->setText(tr("Gamma").toUpper());
    if (key_theta_)       key_theta_->setText(tr("Theta").toUpper());
    if (key_vega_)        key_vega_->setText(tr("Vega").toUpper());
    if (key_margin_)      key_margin_->setText(tr("Margin").toUpper());
}

} // namespace fincept::screens::fno
