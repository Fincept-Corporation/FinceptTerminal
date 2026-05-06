#include "screens/fno/OISubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/IntradayOIChart.h"
#include "screens/fno/MaxPainChart.h"
#include "screens/fno/MultiStrikeOIChart.h"
#include "screens/fno/OIBuildupTable.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPointer>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using namespace fincept::ui;

namespace {

QString strike_label(const OptionChainRow& row) {
    QString s = QString::number(row.strike, 'f', row.strike < 100 ? 2 : 0);
    if (row.is_atm)
        s += "  (ATM)";
    return s;
}

}  // namespace

OISubTab::OISubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoOITab");
    setStyleSheet(QString("#fnoOITab { background:%1; }"
                          "#fnoOIPickerLabel { color:%2; font-size:9px; font-weight:700; "
                          "                     letter-spacing:0.4px; background:transparent; }"
                          "QComboBox { background:%3; color:%4; border:1px solid %5; padding:3px 8px; "
                          "             font-size:11px; min-width:120px; }"
                          "QComboBox:hover { border-color:%6; }"
                          "QComboBox::drop-down { border:none; width:18px; }"
                          "QComboBox QAbstractItemView { background:%1; color:%4; border:1px solid %5; "
                          "                              selection-background-color:%6; }")
                      .arg(colors::BG_BASE(),
                           colors::TEXT_SECONDARY(),
                           colors::BG_RAISED(),
                           colors::TEXT_PRIMARY(),
                           colors::BORDER_DIM(),
                           colors::AMBER()));

    setup_ui();
}

OISubTab::~OISubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void OISubTab::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* outer = new QSplitter(Qt::Vertical, this);
    outer->setHandleWidth(1);
    outer->setChildrenCollapsible(false);

    // ── Top row: Multi-strike OI bars | Max pain chart ────────────────────
    auto* top = new QSplitter(Qt::Horizontal, outer);
    top->setHandleWidth(1);
    top->setChildrenCollapsible(false);
    oi_bars_ = new MultiStrikeOIChart(top);
    pain_chart_ = new MaxPainChart(top);
    top->addWidget(oi_bars_);
    top->addWidget(pain_chart_);
    top->setStretchFactor(0, 3);
    top->setStretchFactor(1, 2);

    // ── Bottom row: Buildup table | (strike combo + intraday chart) ───────
    auto* bottom = new QSplitter(Qt::Horizontal, outer);
    bottom->setHandleWidth(1);
    bottom->setChildrenCollapsible(false);

    buildup_ = new OIBuildupTable(bottom);

    auto* intraday_wrap = new QWidget(bottom);
    auto* intraday_lay = new QVBoxLayout(intraday_wrap);
    intraday_lay->setContentsMargins(8, 6, 8, 0);
    intraday_lay->setSpacing(4);
    auto* picker_row = new QHBoxLayout();
    picker_row->setContentsMargins(0, 0, 0, 0);
    picker_row->setSpacing(8);
    auto* picker_lbl = new QLabel("STRIKE", intraday_wrap);
    picker_lbl->setObjectName("fnoOIPickerLabel");
    strike_combo_ = new QComboBox(intraday_wrap);
    picker_row->addWidget(picker_lbl);
    picker_row->addWidget(strike_combo_);
    picker_row->addStretch(1);
    intraday_lay->addLayout(picker_row);
    intraday_ = new IntradayOIChart(intraday_wrap);
    intraday_lay->addWidget(intraday_, 1);

    bottom->addWidget(buildup_);
    bottom->addWidget(intraday_wrap);
    bottom->setStretchFactor(0, 3);
    bottom->setStretchFactor(1, 2);

    outer->addWidget(top);
    outer->addWidget(bottom);
    outer->setStretchFactor(0, 1);
    outer->setStretchFactor(1, 1);

    root->addWidget(outer);

    connect(strike_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OISubTab::on_strike_combo_changed);
}

QVariantMap OISubTab::save_state() const {
    QVariantMap m;
    m["strike_index"] = strike_combo_ ? strike_combo_->currentIndex() : -1;
    return m;
}

void OISubTab::restore_state(const QVariantMap& state) {
    Q_UNUSED(state);
    // Strike combo content is rebuilt from chain on first publish — restore
    // happens implicitly when the user re-navigates and the chain arrives.
}

void OISubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_)
        return;
    QPointer<OISubTab> self = this;
    fincept::datahub::DataHub::instance().subscribe_pattern(
        this, QStringLiteral("option:chain:*"),
        [self](const QString& topic, const QVariant& v) {
            if (!self) return;
            self->on_chain_published(topic, v);
        });
    subscribed_ = true;
}

void OISubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!subscribed_)
        return;
    fincept::datahub::DataHub::instance().unsubscribe_pattern(this, QStringLiteral("option:chain:*"));
    subscribed_ = false;
}

void OISubTab::on_chain_published(const QString& topic, const QVariant& v) {
    Q_UNUSED(topic);
    if (!v.canConvert<OptionChain>())
        return;
    last_chain_ = v.value<OptionChain>();

    oi_bars_->set_chain(last_chain_);
    pain_chart_->set_chain(last_chain_);
    buildup_->buildup_model()->set_chain(last_chain_);

    // Rebuild strike combo whenever the underlying changes (different strike
    // ladder). For chain refreshes on the same underlying, leave the combo
    // alone so the user's selection sticks.
    if (last_chain_.underlying != current_underlying_) {
        current_underlying_ = last_chain_.underlying;
        rebuild_strike_combo(last_chain_);
    }
    // Strike list might be empty if the publish came in with rows but combo
    // hadn't been seeded yet (e.g. underlying just changed mid-publish).
    if (strike_combo_->count() == 0)
        rebuild_strike_combo(last_chain_);
}

void OISubTab::rebuild_strike_combo(const OptionChain& chain) {
    QSignalBlocker block(strike_combo_);
    strike_combo_->clear();
    int atm_idx = 0;
    for (int i = 0; i < chain.rows.size(); ++i) {
        const auto& r = chain.rows[i];
        strike_combo_->addItem(strike_label(r), QVariant(int(i)));
        if (r.is_atm)
            atm_idx = i;
    }
    if (chain.rows.isEmpty())
        return;
    strike_combo_->setCurrentIndex(atm_idx);
    apply_strike_subscription();
}

void OISubTab::on_strike_combo_changed(int /*index*/) {
    apply_strike_subscription();
}

void OISubTab::apply_strike_subscription() {
    if (last_chain_.rows.isEmpty() || !strike_combo_)
        return;
    const int row_idx = strike_combo_->currentData().toInt();
    if (row_idx < 0 || row_idx >= last_chain_.rows.size()) {
        intraday_->clear_subscription();
        return;
    }
    const auto& row = last_chain_.rows.at(row_idx);
    intraday_->set_subscription(last_chain_.broker_id, row.ce_token, row.pe_token, QStringLiteral("1d"));
}

} // namespace fincept::screens::fno
