#include "screens/fno/MultiStraddleSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/MultiStraddleChart.h"
#include "services/options/OISnapshotter.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::fno {

using fincept::services::options::OISample;
using fincept::services::options::OISnapshotter;
using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using namespace fincept::ui;

namespace {

struct TypeDef {
    const char* label;
    int wing_offset;       // 0 = straddle, ±N = strangle wings
};

constexpr TypeDef kTypeDefs[] = {
    {"Straddle (ATM)",  0},
    {"Strangle ±1",     1},
    {"Strangle ±2",     2},
    {"Strangle ±3",     3},
};

QString strike_label(const OptionChainRow& row) {
    QString s = QString::number(row.strike, 'f', row.strike < 100 ? 2 : 0);
    if (row.is_atm)
        s += "  (ATM)";
    return s;
}

/// Floor a millisecond timestamp to its minute boundary (in seconds).
[[maybe_unused]] qint64 floor_to_minute_secs(qint64 ms) {
    qint64 secs = ms / 1000;
    return (secs / 60) * 60;
}

}  // namespace

MultiStraddleSubTab::MultiStraddleSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoMultiStraddleTab");
    setStyleSheet(
        QString("#fnoMultiStraddleTab { background:%1; }"
                "#fnoMSHeader { background:%2; border-bottom:1px solid %3; }"
                "#fnoMSLabel { color:%4; font-size:9px; font-weight:700; "
                "                 letter-spacing:0.4px; background:transparent; }"
                "#fnoMSAdd { background:%5; color:%2; border:none; padding:5px 14px; "
                "             font-size:10px; font-weight:700; letter-spacing:0.4px; }"
                "#fnoMSAdd:hover { background:%6; color:%2; }"
                "#fnoMSAdd:disabled { background:%3; color:%4; }"
                "QComboBox { background:%2; color:%6; border:1px solid %3; padding:3px 8px; "
                "             font-size:11px; min-width:90px; }"
                "QComboBox:hover { border-color:%5; }"
                "QComboBox::drop-down { border:none; width:18px; }"
                "QComboBox QAbstractItemView { background:%1; color:%6; border:1px solid %3; "
                "                              selection-background-color:%5; }"
                "QListWidget { background:%1; color:%6; border:1px solid %3; }"
                "QListWidget::item { padding:5px 8px; border-bottom:1px solid %3; }"
                "QListWidget::item:selected { background:%7; color:%6; }")
            .arg(colors::BG_BASE(),         // %1
                 colors::BG_RAISED(),       // %2
                 colors::BORDER_DIM(),      // %3
                 colors::TEXT_SECONDARY(),  // %4
                 colors::AMBER(),           // %5
                 colors::TEXT_PRIMARY(),    // %6
                 colors::BG_HOVER()));      // %7

    setup_ui();
    connect(add_btn_, &QPushButton::clicked, this, &MultiStraddleSubTab::on_add_clicked);
    connect(selection_list_, &QListWidget::itemDoubleClicked, this,
            &MultiStraddleSubTab::on_selection_double_clicked);
}

MultiStraddleSubTab::~MultiStraddleSubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void MultiStraddleSubTab::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header strip ──────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("fnoMSHeader");
    auto* hlay = new QHBoxLayout(header);
    hlay->setContentsMargins(12, 8, 12, 8);
    hlay->setSpacing(8);

    auto add_kv = [&](const QString& label_text, QWidget* control) {
        auto* l = new QLabel(label_text.toUpper(), header);
        l->setObjectName("fnoMSLabel");
        hlay->addWidget(l);
        hlay->addWidget(control);
    };

    type_combo_ = new QComboBox(header);
    for (const auto& t : kTypeDefs)
        type_combo_->addItem(QString::fromLatin1(t.label));
    add_kv("Type", type_combo_);

    strike_combo_ = new QComboBox(header);
    add_kv("Anchor", strike_combo_);

    add_btn_ = new QPushButton("ADD", header);
    add_btn_->setObjectName("fnoMSAdd");
    add_btn_->setCursor(Qt::PointingHandCursor);
    add_btn_->setEnabled(false);
    hlay->addWidget(add_btn_);
    hlay->addStretch(1);
    root->addWidget(header);

    // ── Body: selections list (left) | chart (right) ──────────────────────
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);

    selection_list_ = new QListWidget(split);
    selection_list_->setToolTip("Double-click an entry to remove it.");
    chart_ = new MultiStraddleChart(split);

    split->addWidget(selection_list_);
    split->addWidget(chart_);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 4);
    root->addWidget(split, 1);
}

QVariantMap MultiStraddleSubTab::save_state() const { return {}; }
void MultiStraddleSubTab::restore_state(const QVariantMap& /*state*/) {}

void MultiStraddleSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (chain_subscribed_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    QPointer<MultiStraddleSubTab> self = this;
    hub.subscribe_pattern(this, QStringLiteral("option:chain:*"),
                          [self](const QString& /*topic*/, const QVariant& v) {
                              if (!self) return;
                              self->on_chain_published(v);
                          });
    chain_subscribed_ = true;
}

void MultiStraddleSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!chain_subscribed_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe_pattern(this, QStringLiteral("option:chain:*"));
    // Also drop any per-token oi:history subscriptions — they re-subscribe
    // when the user re-shows the tab and adds selections again.
    hub.unsubscribe(this);
    chain_subscribed_ = false;
}

void MultiStraddleSubTab::on_chain_published(const QVariant& v) {
    if (!v.canConvert<OptionChain>())
        return;
    last_chain_ = v.value<OptionChain>();
    if (last_chain_.underlying != current_underlying_) {
        current_underlying_ = last_chain_.underlying;
        rebuild_strike_combo(last_chain_);
        // Drop existing selections — they're stale for the new underlying.
        for (int i = selections_.size() - 1; i >= 0; --i)
            remove_selection(i);
    }
    if (strike_combo_->count() == 0)
        rebuild_strike_combo(last_chain_);
    add_btn_->setEnabled(strike_combo_->count() > 0);
}

void MultiStraddleSubTab::rebuild_strike_combo(const OptionChain& chain) {
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
}

void MultiStraddleSubTab::on_add_clicked() {
    if (last_chain_.rows.isEmpty())
        return;
    const int anchor_idx = strike_combo_->currentData().toInt();
    if (anchor_idx < 0 || anchor_idx >= last_chain_.rows.size())
        return;
    const int type_idx = type_combo_->currentIndex();
    if (type_idx < 0 || type_idx >= int(sizeof(kTypeDefs) / sizeof(kTypeDefs[0])))
        return;
    const int wing = kTypeDefs[type_idx].wing_offset;

    const int ce_idx = anchor_idx + wing;
    const int pe_idx = anchor_idx - wing;
    if (ce_idx < 0 || ce_idx >= last_chain_.rows.size() || pe_idx < 0
        || pe_idx >= last_chain_.rows.size()) {
        LOG_WARN("FnoMultiStraddle", "Selected wings fall outside the chain — pick a closer anchor or smaller wing.");
        return;
    }

    Selection sel;
    sel.broker = last_chain_.broker_id;
    sel.ce_token = last_chain_.rows[ce_idx].ce_token;
    sel.pe_token = last_chain_.rows[pe_idx].pe_token;
    if (sel.ce_token == 0 || sel.pe_token == 0) {
        LOG_WARN("FnoMultiStraddle", "Anchor row missing CE or PE token — pick a different strike.");
        return;
    }
    if (wing == 0) {
        sel.label = QString("Straddle %1").arg(last_chain_.rows[anchor_idx].strike, 0, 'f', 0);
    } else {
        sel.label = QString("Strangle %1C / %2P")
                        .arg(last_chain_.rows[ce_idx].strike, 0, 'f', 0)
                        .arg(last_chain_.rows[pe_idx].strike, 0, 'f', 0);
    }
    // Avoid duplicates — same (ce, pe, broker) is already in the list.
    for (const auto& existing : selections_) {
        if (existing.broker == sel.broker && existing.ce_token == sel.ce_token
            && existing.pe_token == sel.pe_token)
            return;
    }
    selections_.append(sel);

    auto* item = new QListWidgetItem(sel.label);
    item->setToolTip("Double-click to remove.");
    selection_list_->addItem(item);

    subscribe_token(sel.broker, sel.ce_token);
    subscribe_token(sel.broker, sel.pe_token);
    rebuild_chart();
}

void MultiStraddleSubTab::on_selection_double_clicked() {
    const int row = selection_list_->currentRow();
    if (row < 0)
        return;
    remove_selection(row);
}

void MultiStraddleSubTab::remove_selection(int index) {
    if (index < 0 || index >= selections_.size())
        return;
    const Selection sel = selections_.at(index);
    selections_.remove(index);
    delete selection_list_->takeItem(index);

    unsubscribe_token(sel.ce_token);
    unsubscribe_token(sel.pe_token);
    rebuild_chart();
}

void MultiStraddleSubTab::subscribe_token(const QString& broker, qint64 token) {
    if (token == 0)
        return;
    const int n = ++token_refcount_[token];
    if (n > 1)
        return;  // already subscribed via another selection
    const QString topic = OISnapshotter::history_topic(broker, token, QStringLiteral("1d"));
    topic_for_token_.insert(token, topic);
    QPointer<MultiStraddleSubTab> self = this;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, topic, [self, token](const QVariant& v) {
        if (!self) return;
        self->on_oi_history(token, v);
    });
    hub.request(topic, /*force*/ true);
}

void MultiStraddleSubTab::unsubscribe_token(qint64 token) {
    if (token == 0)
        return;
    auto it = token_refcount_.find(token);
    if (it == token_refcount_.end())
        return;
    if (--it.value() > 0)
        return;
    token_refcount_.erase(it);
    const QString topic = topic_for_token_.take(token);
    if (!topic.isEmpty())
        fincept::datahub::DataHub::instance().unsubscribe(this, topic);
    samples_by_token_.remove(token);
}

void MultiStraddleSubTab::on_oi_history(qint64 token, const QVariant& v) {
    if (!v.canConvert<QVector<OISample>>())
        return;
    samples_by_token_.insert(token, v.value<QVector<OISample>>());
    rebuild_chart();
}

void MultiStraddleSubTab::rebuild_chart() {
    QVector<MultiStraddleChart::Selection> out;
    out.reserve(selections_.size());
    for (const auto& sel : selections_) {
        const auto ce_it = samples_by_token_.constFind(sel.ce_token);
        const auto pe_it = samples_by_token_.constFind(sel.pe_token);
        if (ce_it == samples_by_token_.constEnd() || pe_it == samples_by_token_.constEnd())
            continue;
        // Index PE samples by ts_minute for O(N) join.
        QHash<qint64, double> pe_ltp_at;
        pe_ltp_at.reserve(pe_it->size());
        for (const auto& s : *pe_it)
            pe_ltp_at.insert(s.ts_minute, s.ltp);

        MultiStraddleChart::Selection chart_sel;
        chart_sel.label = sel.label;
        chart_sel.points.reserve(ce_it->size());
        for (const auto& ce : *ce_it) {
            const auto pe = pe_ltp_at.constFind(ce.ts_minute);
            if (pe == pe_ltp_at.constEnd())
                continue;
            MultiStraddleChart::Sample p;
            p.ts_secs = ce.ts_minute;
            p.premium = ce.ltp + pe.value();
            chart_sel.points.append(p);
        }
        // Ensure ascending by ts (samples_by_token comes ASC already, but
        // rebuild_chart can be called mid-update so be defensive).
        std::sort(chart_sel.points.begin(), chart_sel.points.end(),
                  [](const auto& a, const auto& b) { return a.ts_secs < b.ts_secs; });
        if (!chart_sel.points.isEmpty())
            out.append(chart_sel);
    }
    chart_->set_straddles(out);
}

} // namespace fincept::screens::fno
