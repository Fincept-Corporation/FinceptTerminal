#include "screens/fno/OISubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/IntradayOIChart.h"
#include "screens/fno/MaxPainChart.h"
#include "screens/fno/MultiStrikeOIChart.h"
#include "screens/fno/OIBuildupTable.h"
#include "services/options/OptionChainService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QCoreApplication>
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
        s += "  " + QCoreApplication::translate("OISubTab", "(ATM)");
    return s;
}

} // namespace

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
                      .arg(colors::BG_BASE(), colors::TEXT_SECONDARY(), colors::BG_RAISED(), colors::TEXT_PRIMARY(),
                           colors::BORDER_DIM(), colors::AMBER()));

    setup_ui();

    connect(&fincept::services::options::OptionChainService::instance(),
            &fincept::services::options::OptionChainService::chain_published, this,
            [this](const OptionChain& chain) { on_chain_published({}, QVariant::fromValue(chain)); });
    subscribed_ = true;

    const auto& cached = fincept::services::options::OptionChainService::instance().last_chain();
    if (!cached.rows.isEmpty())
        on_chain_published({}, QVariant::fromValue(cached));
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
    picker_lbl_ = new QLabel(tr("STRIKE"), intraday_wrap);
    picker_lbl_->setObjectName("fnoOIPickerLabel");
    strike_combo_ = new QComboBox(intraday_wrap);
    strike_combo_->setAccessibleName(tr("Strike for intraday open-interest chart"));
    picker_row->addWidget(picker_lbl_);
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
    // Persist the strike VALUE, not the combo index: the index means nothing
    // across sessions (different ladder, different ATM) and restoring it would
    // silently chart a different strike than the one the user was watching.
    m["strike"] = selected_strike_;
    return m;
}

void OISubTab::restore_state(const QVariantMap& state) {
    const double strike = state.value("strike", 0.0).toDouble();
    if (strike <= 0)
        return;
    selected_strike_ = strike;
    // Applied when the next chain publish rebuilds the combo; if the chain is
    // already here, apply it immediately.
    if (!last_chain_.rows.isEmpty())
        rebuild_strike_combo(last_chain_);
}

void OISubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_)
        return;
    QPointer<OISubTab> self = this;
    fincept::datahub::DataHub::instance().subscribe_pattern(this, QStringLiteral("option:chain:*"),
                                                            [self](const QString& topic, const QVariant& v) {
                                                                if (!self)
                                                                    return;
                                                                self->on_chain_published(topic, v);
                                                            });
    subscribed_ = true;
}

void OISubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // §D3: pair the showEvent subscribe. Without this the `subscribed_` latch
    // never resets, and on_chain_published() has no visibility gate — so a
    // hidden tab kept driving oi_bars_->set_chain(), pain_chart_->set_chain()
    // and buildup_model()->set_chain() on every option-chain publish for the
    // rest of the process. These tabs live in a QStackedWidget, so one visit
    // armed it permanently.
    //
    // unsubscribe_pattern() rather than the blanket unsubscribe(this) so this
    // stays symmetric with the subscribe_pattern() above and can't collaterally
    // drop any concrete-topic subscription added here later (the sibling
    // MultiStraddleSubTab already has exactly that hazard).
    fincept::datahub::DataHub::instance().unsubscribe_pattern(this, QStringLiteral("option:chain:*"));
    subscribed_ = false;
}

void OISubTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void OISubTab::retranslateUi() {
    if (picker_lbl_)
        picker_lbl_->setText(tr("STRIKE"));
    if (strike_combo_)
        strike_combo_->setAccessibleName(tr("Strike for intraday open-interest chart"));
    // Rebuild the combo so the "(ATM)" suffix picks up the new language.
    // rebuild_strike_combo re-selects by strike value, so the user's pick is
    // preserved without touching indices.
    if (strike_combo_ && strike_combo_->count() > 0 && !last_chain_.rows.isEmpty())
        rebuild_strike_combo(last_chain_);
}

void OISubTab::on_chain_published(const QString& topic, const QVariant& v) {
    Q_UNUSED(topic);
    if (!v.canConvert<OptionChain>())
        return;
    last_chain_ = v.value<OptionChain>();

    oi_bars_->set_chain(last_chain_);
    pain_chart_->set_chain(last_chain_);
    buildup_->buildup_model()->set_chain(last_chain_);

    // Rebuild the strike combo whenever the ladder itself can have changed:
    // a new underlying OR a new expiry (each expiry has its own strike set),
    // or a differently-sized ladder on the same series (the chain window slides
    // as the ATM moves). On a plain refresh the combo is left alone so the
    // user's selection sticks.
    const bool ladder_changed = last_chain_.underlying != current_underlying_ ||
                                last_chain_.expiry != current_expiry_ ||
                                strike_combo_->count() != last_chain_.rows.size();
    if (ladder_changed) {
        current_underlying_ = last_chain_.underlying;
        current_expiry_ = last_chain_.expiry;
        rebuild_strike_combo(last_chain_);
    }
}

void OISubTab::rebuild_strike_combo(const OptionChain& chain) {
    QSignalBlocker block(strike_combo_);
    strike_combo_->clear();
    // The item's userData is the STRIKE, not the row index. A row index goes
    // stale the moment the chain republishes with a shifted window, which would
    // silently point the intraday OI chart at a different strike than the combo
    // label claims.
    int atm_idx = 0;
    int keep_idx = -1;
    for (int i = 0; i < chain.rows.size(); ++i) {
        const auto& r = chain.rows[i];
        strike_combo_->addItem(strike_label(r), QVariant(r.strike));
        if (r.is_atm)
            atm_idx = i;
        if (selected_strike_ > 0 && std::abs(r.strike - selected_strike_) < 1e-6)
            keep_idx = i;
    }
    if (chain.rows.isEmpty()) {
        intraday_->clear_subscription();
        return;
    }
    strike_combo_->setCurrentIndex(keep_idx >= 0 ? keep_idx : atm_idx);
    selected_strike_ = strike_combo_->currentData().toDouble();
    apply_strike_subscription();
}

void OISubTab::on_strike_combo_changed(int /*index*/) {
    if (strike_combo_)
        selected_strike_ = strike_combo_->currentData().toDouble();
    apply_strike_subscription();
}

void OISubTab::apply_strike_subscription() {
    if (last_chain_.rows.isEmpty() || !strike_combo_)
        return;
    const double strike = strike_combo_->currentData().toDouble();
    const fincept::services::options::OptionChainRow* row = nullptr;
    for (const auto& r : last_chain_.rows) {
        if (std::abs(r.strike - strike) < 1e-6) {
            row = &r;
            break;
        }
    }
    if (!row) {
        intraday_->clear_subscription();
        return;
    }
    intraday_->set_subscription(last_chain_.broker_id, row->ce_token, row->pe_token, QStringLiteral("1d"));
}

} // namespace fincept::screens::fno
