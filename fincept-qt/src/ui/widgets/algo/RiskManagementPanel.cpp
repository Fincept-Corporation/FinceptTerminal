// src/ui/widgets/algo/RiskManagementPanel.cpp
#include "ui/widgets/algo/RiskManagementPanel.h"

#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/algo/AlgoCurrency.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::ui::algo {

namespace {

QWidget* rmp_group(QWidget* host, QLabel* label, QWidget* field) {
    auto* g = new QWidget(host);
    auto* h = new QHBoxLayout(g);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    label->setParent(g);
    field->setParent(g);
    label->setBuddy(field);
    h->addWidget(label);
    h->addWidget(field);
    return g;
}

QString rmp_number(double v, int decimals) {
    QString s = QString::number(v, 'f', decimals);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s;
}

} // namespace

RiskManagementPanel::RiskManagementPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("riskManagementPanel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    auto* host = new QWidget(this);
    auto* flow = new ui::FlowLayout(host);
    stop_loss_ = create_field(0, 50, 2.0, 2, 0.5, QStringLiteral(" %"), host);
    take_profit_ = create_field(0, 100, 5.0, 2, 0.5, QStringLiteral(" %"), host);
    trailing_stop_ = create_field(0, 50, 0, 2, 0.5, QStringLiteral(" %"), host);
    capital_pct_ = create_field(1, 100, 100, 0, 5, QStringLiteral(" %"), host);
    quantity_ = create_field(1, 100000, 1, 0, 1, QString(), host);
    max_order_ = create_field(0, 10000000, 0, 0, 1000, QString(), host);
    deploy_caption_ = new QLabel(host);
    deploy_caption_->setObjectName(QStringLiteral("algoNote"));
    for (const Field* f : {&stop_loss_, &take_profit_, &trailing_stop_, &capital_pct_})
        flow->addWidget(rmp_group(host, f->label, f->spin));
    flow->addWidget(deploy_caption_);
    for (const Field* f : {&quantity_, &max_order_})
        flow->addWidget(rmp_group(host, f->label, f->spin));
    root->addWidget(host);

    retranslateUi();
}

RiskManagementPanel::Field RiskManagementPanel::create_field(double min_val, double max_val, double default_val,
                                                             int decimals, double step, const QString& suffix,
                                                             QWidget* host) {
    Field f;
    f.label = new QLabel(host);
    f.label->setObjectName(QStringLiteral("algoFieldLabel"));
    f.spin = new QDoubleSpinBox(host);
    f.spin->setObjectName(QStringLiteral("algoField"));
    f.spin->setRange(min_val, max_val);
    f.spin->setDecimals(decimals);
    f.spin->setSingleStep(step);
    f.spin->setValue(default_val);
    f.spin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    f.spin->setKeyboardTracking(false);
    f.spin->setFocusPolicy(Qt::StrongFocus);
    if (!suffix.isEmpty())
        f.spin->setSuffix(suffix);
    connect(f.spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { emit values_changed(); });
    return f;
}

void RiskManagementPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void RiskManagementPanel::retranslateUi() {
    struct Text {
        Field* f;
        QString label;
        QString name;
        QString tip;
        QString special;
    };
    const Text texts[] = {
        {&stop_loss_, tr("STOP"), tr("Stop-loss percent"),
         tr("Stop-loss: close the position when it loses this much from the entry fill (a resting stop order "
            "checked against each bar's range; a gap fills at the open). Off = none."),
         tr("off")},
        {&take_profit_, tr("TARGET"), tr("Take-profit percent"),
         tr("Take-profit: close the position at this gain from the entry fill (a limit order, no slippage). "
            "Off = none."),
         tr("off")},
        {&trailing_stop_, tr("TRAIL"), tr("Trailing stop percent"),
         tr("Trailing stop: follows the best price reached before the bar by this distance; the tighter of the "
            "stop-loss and the trail applies. Off = none."),
         tr("off")},
        {&capital_pct_, tr("ALLOC"), tr("Capital allocated per entry, percent"),
         tr("Share of the capital (cash at the time) committed to each entry in the backtest; no leverage."),
         QString()},
        {&quantity_, tr("QTY"), tr("Deployment quantity"),
         tr("Units per order when deployed (paper or live). The backtest ignores it: it sizes each entry by ALLOC "
            "of the capital."),
         QString()},
        {&max_order_, tr("MAX ORDER"), tr("Deployment maximum order value"),
         tr("Largest order value a deployment may send, in the instrument's currency. The backtest ignores it. "
            "No limit = 0."),
         tr("no limit")},
    };
    for (const Text& t : texts) {
        t.f->label->setText(t.label);
        t.f->label->setToolTip(t.tip);
        t.f->spin->setAccessibleName(t.name);
        t.f->spin->setToolTip(t.tip);
        t.f->spin->setSpecialValueText(t.special);
    }
    deploy_caption_->setText(tr("DEPLOY ONLY:"));
    deploy_caption_->setToolTip(tr("Used by paper / live deployments; the backtest sizes positions by ALLOC"));
}

double RiskManagementPanel::stop_loss() const {
    return stop_loss_.spin->value();
}
double RiskManagementPanel::take_profit() const {
    return take_profit_.spin->value();
}
double RiskManagementPanel::trailing_stop() const {
    return trailing_stop_.spin->value();
}
double RiskManagementPanel::quantity() const {
    return quantity_.spin->value();
}
double RiskManagementPanel::max_order_value() const {
    return max_order_.spin->value();
}
double RiskManagementPanel::capital_pct() const {
    return capital_pct_.spin->value();
}

void RiskManagementPanel::set_values(double sl, double tp, double ts, double qty, double mov, double capital_pct) {
    stop_loss_.spin->setValue(sl);
    take_profit_.spin->setValue(tp);
    trailing_stop_.spin->setValue(ts);
    quantity_.spin->setValue(qty);
    max_order_.spin->setValue(mov);
    capital_pct_.spin->setValue(capital_pct);
}

void RiskManagementPanel::set_currency(const QString& ccy) {
    // The order value limit is price x quantity of the instrument: its quote currency.
    algo_bind_money_spin(max_order_.spin, ccy);
}

QString RiskManagementPanel::summary_text() const {
    auto pct = [](double v) { return v > 0 ? rmp_number(v, 2) + QStringLiteral(" %") : tr("off"); };
    return tr("stop %1 · target %2 · trail %3 · %4 % of capital per entry")
        .arg(pct(stop_loss()), pct(take_profit()), pct(trailing_stop()), rmp_number(capital_pct(), 0));
}

} // namespace fincept::ui::algo
