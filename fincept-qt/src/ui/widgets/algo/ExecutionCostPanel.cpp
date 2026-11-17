// src/ui/widgets/algo/ExecutionCostPanel.cpp
#include "ui/widgets/algo/ExecutionCostPanel.h"

#include "ui/theme/Theme.h"
#include "ui/widgets/algo/AlgoCurrency.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::ui::algo {

namespace ex = fincept::algo::execution;

ExecutionCostPanel::ExecutionCostPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("executionCostPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // The host's panel header names the section; this label only carries the translated title
    // for hosts that want it (not shown here).
    header_ = new QLabel(this);
    header_->setVisible(false);

    auto* top = new QGridLayout();
    top->setHorizontalSpacing(8);
    top->setVerticalSpacing(4);
    top->setColumnStretch(1, 1);

    side_label_ = new QLabel(this);
    side_label_->setObjectName(QStringLiteral("algoFieldLabel"));
    side_combo_ = new QComboBox(this);
    side_combo_->setObjectName(QStringLiteral("algoField"));
    side_combo_->addItem(QString(), QStringLiteral("BUY"));
    side_combo_->addItem(QString(), QStringLiteral("SELL"));
    top->addWidget(side_label_, 0, 0);
    top->addWidget(side_combo_, 0, 1);

    preset_label_ = new QLabel(this);
    preset_label_->setObjectName(QStringLiteral("algoFieldLabel"));
    preset_combo_ = new QComboBox(this);
    preset_combo_->setObjectName(QStringLiteral("algoField"));
    preset_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    preset_combo_->setMinimumContentsLength(18);
    top->addWidget(preset_label_, 1, 0);
    top->addWidget(preset_combo_, 1, 1);
    layout->addLayout(top);

    note_ = new QLabel(this);
    note_->setObjectName(QStringLiteral("algoNote"));
    note_->setWordWrap(true);
    layout->addWidget(note_);

    comm_pct_ = make_spin(5, 4, 0.01, QStringLiteral(" %"));
    comm_unit_ = make_spin(100, 4, 0.001, QString());
    comm_order_ = make_spin(10000, 2, 1, QString());
    comm_min_ = make_spin(10000, 2, 1, QString());
    comm_max_ = make_spin(1000000, 2, 1, QString());
    buy_tax_ = make_spin(5, 4, 0.01, QStringLiteral(" %"));
    sell_tax_ = make_spin(5, 4, 0.01, QStringLiteral(" %"));
    slip_bps_ = make_spin(500, 1, 0.5, QStringLiteral(" bp"));
    slip_atr_ = make_spin(5, 3, 0.01, QStringLiteral(" ×"));
    lot_ = make_spin(1000000, 4, 1, QString());
    lot_->setMinimum(0.0001);
    lot_->setValue(1);
    borrow_ = make_spin(100, 2, 0.25, QStringLiteral(" %"));
    rf_ = make_spin(60, 2, 0.25, QStringLiteral(" %"));

    auto label = [this]() {
        auto* l = new QLabel(this);
        l->setObjectName(QStringLiteral("algoFieldLabel"));
        return l;
    };
    comm_pct_label_ = label();
    comm_unit_label_ = label();
    comm_order_label_ = label();
    comm_min_label_ = label();
    comm_max_label_ = label();
    buy_tax_label_ = label();
    sell_tax_label_ = label();
    slip_bps_label_ = label();
    slip_atr_label_ = label();
    lot_label_ = label();
    borrow_label_ = label();
    rf_label_ = label();

    // Three label / field pairs a row (the fields are a few characters wide).
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);
    for (int c : {1, 3, 5})
        grid->setColumnStretch(c, 1);
    const QList<QPair<QLabel*, QDoubleSpinBox*>> pairs = {
        {comm_pct_label_, comm_pct_}, {comm_unit_label_, comm_unit_}, {comm_order_label_, comm_order_},
        {comm_min_label_, comm_min_}, {comm_max_label_, comm_max_},   {lot_label_, lot_},
        {buy_tax_label_, buy_tax_},   {sell_tax_label_, sell_tax_},   {slip_bps_label_, slip_bps_},
        {slip_atr_label_, slip_atr_}, {borrow_label_, borrow_},       {rf_label_, rf_}};
    for (int i = 0; i < pairs.size(); ++i) {
        pairs[i].first->setBuddy(pairs[i].second);
        pairs[i].second->setObjectName(QStringLiteral("algoField"));
        grid->addWidget(pairs[i].first, i / 3, (i % 3) * 2);
        grid->addWidget(pairs[i].second, i / 3, (i % 3) * 2 + 1);
    }
    layout->addLayout(grid);

    fill_presets();
    retranslateUi();

    connect(preset_combo_, QOverload<int>::of(&QComboBox::activated), this, &ExecutionCostPanel::on_preset_activated);
    connect(side_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!loading_)
            emit changed();
    });
    for (QDoubleSpinBox* s : {comm_pct_, comm_unit_, comm_order_, comm_min_, comm_max_, buy_tax_, sell_tax_, slip_bps_,
                              slip_atr_, lot_, borrow_})
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { on_field_edited(); });
    connect(rf_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!loading_)
            emit changed();
    });
    apply_preset(QStringLiteral("none"));
    user_chose_ = false;
}

void ExecutionCostPanel::set_currency(const QString& ccy) {
    // Flat commissions are charged in the instrument's quote currency (percentages, bp and
    // the lot size have no currency).
    for (QDoubleSpinBox* s : {comm_unit_, comm_order_, comm_min_, comm_max_})
        algo_bind_money_spin(s, ccy);
}

QDoubleSpinBox* ExecutionCostPanel::make_spin(double max, int decimals, double step, const QString& suffix) {
    auto* s = new QDoubleSpinBox(this);
    s->setRange(0, max);
    s->setDecimals(decimals);
    s->setSingleStep(step);
    if (!suffix.isEmpty())
        s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    return s;
}

void ExecutionCostPanel::fill_presets() {
    QSignalBlocker block(preset_combo_);
    const QString keep = preset_id_;
    preset_combo_->clear();
    for (const auto& p : ex::presets())
        preset_combo_->addItem(p.label, p.id);
    preset_combo_->addItem(tr("Custom"), QStringLiteral("custom"));
    const int idx = preset_combo_->findData(keep);
    preset_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

QString ExecutionCostPanel::summary_text() const {
    const fincept::algo::CostModel m = cost_model();
    QStringList parts;
    parts << (entry_side() == QLatin1String("SELL") ? tr("short entries") : tr("long entries"));
    if (preset_id_ == QLatin1String("none") || m.is_zero()) {
        parts << tr("no costs (GROSS)");
    } else {
        if (preset_id_ != QLatin1String("custom"))
            parts << ex::preset(preset_id_).label.section(QStringLiteral(" - "), 0, 0).section(QLatin1Char('('), 0, 0).trimmed();
        else
            parts << tr("custom costs");
        const bool commission = m.commission_pct > 0 || m.commission_per_unit > 0 || m.commission_per_order > 0 ||
                                m.commission_min > 0;
        if (!commission)
            parts << tr("commission-free");
        else if (m.commission_pct > 0)
            parts << tr("%1 % commission").arg(QString::number(m.commission_pct, 'g', 4));
        else if (m.commission_per_unit > 0)
            parts << tr("%1 / unit commission").arg(QString::number(m.commission_per_unit, 'g', 4));
        else
            parts << tr("%1 / order commission").arg(QString::number(std::max(m.commission_per_order, m.commission_min), 'g', 6));
        if (m.slippage_bps > 0 || m.slippage_atr_frac > 0)
            parts << tr("%1 bps slippage").arg(QString::number(m.slippage_bps, 'g', 4)) +
                         (m.slippage_atr_frac > 0 ? tr(" + %1 × ATR").arg(QString::number(m.slippage_atr_frac, 'g', 3)) : QString());
        if (m.lot_size != 1.0)
            parts << tr("lot %1").arg(QString::number(m.lot_size, 'g', 6));
    }
    parts << tr("signals fill at the next bar's open");
    if (risk_free_pct() > 0)
        parts << tr("risk-free %1 %").arg(QString::number(risk_free_pct(), 'g', 4));
    return parts.join(QStringLiteral(" · "));
}

QComboBox* ExecutionCostPanel::take_side_combo() {
    side_label_->setVisible(false);
    return side_combo_;
}

void ExecutionCostPanel::update_note() {
    if (preset_id_ == QLatin1String("custom")) {
        note_->setText(tr("Custom cost model. Values are per order, in the instrument's quote currency (percentages "
                          "of traded value)."));
        return;
    }
    note_->setText(ex::preset(preset_id_).note);
}

void ExecutionCostPanel::on_preset_activated(int index) {
    const QString id = preset_combo_->itemData(index).toString();
    if (id == QLatin1String("custom")) {
        preset_id_ = id;
        user_chose_ = true;
        update_note();
        emit changed();
        return;
    }
    apply_preset(id);
    user_chose_ = true;
    emit changed();
}

void ExecutionCostPanel::apply_preset(const QString& preset_id) {
    fincept::algo::CostModel m = ex::preset(preset_id).model;
    set_cost_model(m);
}

void ExecutionCostPanel::suggest_for(const QString& venue_or_country, bool intraday) {
    if (user_chose_)
        return;
    const QString id = ex::default_preset_for(venue_or_country, intraday);
    if (id == preset_id_)
        return;
    apply_preset(id);
    emit changed();
}

void ExecutionCostPanel::on_field_edited() {
    if (loading_)
        return;
    user_chose_ = true;
    if (preset_id_ != QLatin1String("custom")) {
        preset_id_ = QStringLiteral("custom");
        QSignalBlocker block(preset_combo_);
        preset_combo_->setCurrentIndex(preset_combo_->findData(preset_id_));
        update_note();
    }
    emit changed();
}

fincept::algo::CostModel ExecutionCostPanel::cost_model() const {
    fincept::algo::CostModel m;
    m.preset = preset_id_;
    m.commission_pct = comm_pct_->value();
    m.commission_per_unit = comm_unit_->value();
    m.commission_per_order = comm_order_->value();
    m.commission_min = comm_min_->value();
    m.commission_max = comm_max_->value();
    m.buy_tax_pct = buy_tax_->value();
    m.sell_tax_pct = sell_tax_->value();
    m.slippage_bps = slip_bps_->value();
    m.slippage_atr_frac = slip_atr_->value();
    m.lot_size = lot_->value() > 0 ? lot_->value() : 1.0;
    m.borrow_pct_annual = borrow_->value();
    return m;
}

void ExecutionCostPanel::set_cost_model(const fincept::algo::CostModel& m) {
    loading_ = true;
    comm_pct_->setValue(m.commission_pct);
    comm_unit_->setValue(m.commission_per_unit);
    comm_order_->setValue(m.commission_per_order);
    comm_min_->setValue(m.commission_min);
    comm_max_->setValue(m.commission_max);
    buy_tax_->setValue(m.buy_tax_pct);
    sell_tax_->setValue(m.sell_tax_pct);
    slip_bps_->setValue(m.slippage_bps);
    slip_atr_->setValue(m.slippage_atr_frac);
    lot_->setValue(m.lot_size > 0 ? m.lot_size : 1.0);
    borrow_->setValue(m.borrow_pct_annual);
    // An unknown preset id (or a saved model that no longer matches its preset)
    // is shown as Custom so the label never claims values it does not have.
    QString id = m.preset.isEmpty() ? QStringLiteral("custom") : m.preset;
    if (id != QLatin1String("custom")) {
        const auto p = ex::preset(id);
        if (p.id != id || p.model != m)
            id = QStringLiteral("custom");
    }
    preset_id_ = id;
    {
        QSignalBlocker block(preset_combo_);
        const int idx = preset_combo_->findData(preset_id_);
        preset_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    update_note();
    loading_ = false;
}

QString ExecutionCostPanel::entry_side() const {
    return side_combo_->currentData().toString();
}

void ExecutionCostPanel::set_entry_side(const QString& side) {
    loading_ = true;
    const int idx = side_combo_->findData(side.trimmed().toUpper() == QLatin1String("SELL") ? QStringLiteral("SELL")
                                                                                            : QStringLiteral("BUY"));
    side_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    loading_ = false;
}

double ExecutionCostPanel::risk_free_pct() const {
    return rf_->value();
}

void ExecutionCostPanel::set_risk_free_pct(double pct) {
    loading_ = true;
    rf_->setValue(pct);
    loading_ = false;
}

void ExecutionCostPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        fill_presets();
        retranslateUi();
        update_note();
    }
    QWidget::changeEvent(event);
}

void ExecutionCostPanel::retranslateUi() {
    header_->setText(tr("EXECUTION & COSTS"));
    side_label_->setText(tr("Entry side"));
    side_combo_->setItemText(0, tr("Long — buy to enter"));
    side_combo_->setItemText(1, tr("Short — sell to enter"));
    side_combo_->setToolTip(tr("Direction of the entry signal. Backtests and deployments use the same side; a short "
                               "needs a product / account that allows shorting."));
    preset_label_->setText(tr("Cost preset"));
    preset_combo_->setToolTip(tr("Approximate fee schedules per market — starting points to edit, not a broker's "
                                 "actual charges."));
    comm_pct_label_->setText(tr("Commission"));
    comm_pct_->setToolTip(tr("Commission as a percentage of traded value"));
    comm_unit_label_->setText(tr("Per unit"));
    comm_unit_->setToolTip(tr("Commission per share / contract, in the quote currency"));
    comm_order_label_->setText(tr("Per order"));
    comm_order_->setToolTip(tr("Flat commission per order, in the quote currency"));
    comm_min_label_->setText(tr("Minimum"));
    comm_min_->setToolTip(tr("Minimum commission per order"));
    comm_max_label_->setText(tr("Cap"));
    comm_max_->setToolTip(tr("Maximum commission per order (0 = no cap)"));
    comm_max_->setSpecialValueText(tr("none"));
    buy_tax_label_->setText(tr("Buy taxes"));
    buy_tax_->setToolTip(tr("Taxes and exchange / regulatory fees on buys, % of value (stamp duty, STT, …)"));
    sell_tax_label_->setText(tr("Sell taxes"));
    sell_tax_->setToolTip(tr("Taxes and exchange / regulatory fees on sells, % of value (SEC fee, STT, …)"));
    slip_bps_label_->setText(tr("Slippage"));
    slip_bps_->setToolTip(tr("Adverse slippage on market fills, basis points of price (1 bp = 0.01 %)"));
    slip_atr_label_->setText(tr("+ ATR"));
    slip_atr_->setToolTip(tr("Extra adverse slippage as a fraction of the 14-bar ATR (volatility-scaled)"));
    lot_label_->setText(tr("Lot size"));
    lot_->setToolTip(tr("Orders are multiples of this. Below 1 allows fractional units (0.0001 for crypto)."));
    borrow_label_->setText(tr("Borrow"));
    borrow_->setToolTip(tr("Stock-borrow fee on short positions, % per year"));
    rf_label_->setText(tr("Risk-free"));
    rf_->setToolTip(tr("Annual risk-free rate used by Sharpe and Sortino"));
}

} // namespace fincept::ui::algo
