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
    return v >= 0 ? "+" + s : s;  // QString::number prefixes minus for negatives
}

QString fmt_currency(double v) {
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("∞") : QStringLiteral("−∞");
    return QLocale(QLocale::English).toString(v, 'f', 0);
}

QWidget* make_kv(QWidget* parent, QLabel*& value_out, const QString& key) {
    auto* wrap = new QWidget(parent);
    auto* lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    auto* k = new QLabel(key.toUpper(), wrap);
    k->setObjectName("fnoBldrKey");
    auto* v = new QLabel("—", wrap);
    v->setObjectName("fnoBldrValue");
    lay->addWidget(k);
    lay->addWidget(v);
    value_out = v;
    return wrap;
}

}  // namespace

BuilderAnalyticsRibbon::BuilderAnalyticsRibbon(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoBuilderRibbon");
    setStyleSheet(QString(
        "#fnoBuilderRibbon { background:%1; border-bottom:1px solid %2; }"
        "#fnoBldrKey { color:%3; font-size:8px; font-weight:700; letter-spacing:0.6px; background:transparent; }"
        "#fnoBldrValue { color:%4; font-size:13px; font-weight:700; background:transparent; }"
        "#fnoBldrValuePos { color:%5; font-size:13px; font-weight:700; background:transparent; }"
        "#fnoBldrValueNeg { color:%6; font-size:13px; font-weight:700; background:transparent; }")
                      .arg(colors::BG_RAISED(), colors::BORDER_DIM(), colors::TEXT_SECONDARY(),
                           colors::TEXT_PRIMARY(), colors::POSITIVE(), colors::NEGATIVE()));
    setup_ui();
}

void BuilderAnalyticsRibbon::setup_ui() {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(20);

    lay->addWidget(make_kv(this, lbl_premium_, "Premium"));
    lay->addWidget(make_kv(this, lbl_max_profit_, "Max Profit"));
    lay->addWidget(make_kv(this, lbl_max_loss_, "Max Loss"));
    lay->addWidget(make_kv(this, lbl_breakevens_, "Breakevens"));
    lay->addWidget(make_kv(this, lbl_pop_, "POP"));
    lay->addWidget(make_kv(this, lbl_delta_, "Delta"));
    lay->addWidget(make_kv(this, lbl_gamma_, "Gamma"));
    lay->addWidget(make_kv(this, lbl_theta_, "Theta"));
    lay->addWidget(make_kv(this, lbl_vega_, "Vega"));
    lay->addWidget(make_kv(this, lbl_margin_, "Margin"));
    lay->addStretch(1);
}

void BuilderAnalyticsRibbon::update_from(const Strategy& s, const StrategyAnalytics& a) {
    Q_UNUSED(s);

    // Premium — sign-coloured. Positive = paid (debit), negative = received.
    lbl_premium_->setText(fmt_currency(std::abs(a.premium_paid)) +
                          (a.premium_paid > 0 ? "  Dr" : a.premium_paid < 0 ? "  Cr" : ""));
    lbl_premium_->setObjectName(a.premium_paid > 0 ? "fnoBldrValueNeg"
                                : a.premium_paid < 0 ? "fnoBldrValuePos" : "fnoBldrValue");

    lbl_max_profit_->setText(fmt_currency(a.max_profit));
    lbl_max_profit_->setObjectName("fnoBldrValuePos");
    lbl_max_loss_->setText(fmt_currency(a.max_loss));
    lbl_max_loss_->setObjectName("fnoBldrValueNeg");

    if (a.breakevens.isEmpty()) {
        lbl_breakevens_->setText("—");
    } else {
        QStringList parts;
        for (double be : a.breakevens)
            parts.append(QString::number(be, 'f', 0));
        lbl_breakevens_->setText(parts.join(" / "));
    }
    lbl_breakevens_->setObjectName("fnoBldrValue");

    lbl_pop_->setText(QString::number(a.pop * 100.0, 'f', 1) + "%");
    lbl_pop_->setObjectName("fnoBldrValue");

    if (a.combined.valid) {
        lbl_delta_->setText(fmt_signed(a.combined.delta, 2));
        lbl_delta_->setObjectName(a.combined.delta >= 0 ? "fnoBldrValuePos" : "fnoBldrValueNeg");
        lbl_gamma_->setText(fmt_signed(a.combined.gamma, 4));
        lbl_gamma_->setObjectName("fnoBldrValue");
        lbl_theta_->setText(fmt_signed(a.combined.theta, 1));
        lbl_theta_->setObjectName(a.combined.theta >= 0 ? "fnoBldrValuePos" : "fnoBldrValueNeg");
        lbl_vega_->setText(fmt_signed(a.combined.vega, 1));
        lbl_vega_->setObjectName("fnoBldrValue");
    } else {
        for (auto* l : {lbl_delta_, lbl_gamma_, lbl_theta_, lbl_vega_}) {
            l->setText("—");
            l->setObjectName("fnoBldrValue");
        }
    }

    // Margin cell stays whatever set_margin() most recently set; don't
    // clobber it on every analytics refresh.

    // Re-evaluate stylesheet because objectName changed.
    for (auto* l : {lbl_premium_, lbl_max_profit_, lbl_max_loss_, lbl_breakevens_, lbl_pop_,
                    lbl_delta_, lbl_gamma_, lbl_theta_, lbl_vega_, lbl_margin_}) {
        l->style()->unpolish(l);
        l->style()->polish(l);
    }
}

void BuilderAnalyticsRibbon::clear() {
    for (auto* l : {lbl_premium_, lbl_max_profit_, lbl_max_loss_, lbl_breakevens_, lbl_pop_,
                    lbl_delta_, lbl_gamma_, lbl_theta_, lbl_vega_, lbl_margin_}) {
        l->setText("—");
        l->setObjectName("fnoBldrValue");
        l->style()->unpolish(l);
        l->style()->polish(l);
    }
}

void BuilderAnalyticsRibbon::set_margin(double value, bool estimated) {
    const QString prefix = estimated ? QStringLiteral("~ ₹ ") : QStringLiteral("₹ ");
    lbl_margin_->setText(prefix + fmt_currency(value));
    lbl_margin_->setObjectName(estimated ? "fnoBldrValue" : "fnoBldrValuePos");
    lbl_margin_->style()->unpolish(lbl_margin_);
    lbl_margin_->style()->polish(lbl_margin_);
}

void BuilderAnalyticsRibbon::clear_margin() {
    lbl_margin_->setText("—");
    lbl_margin_->setObjectName("fnoBldrValue");
    lbl_margin_->style()->unpolish(lbl_margin_);
    lbl_margin_->style()->polish(lbl_margin_);
}

} // namespace fincept::screens::fno
