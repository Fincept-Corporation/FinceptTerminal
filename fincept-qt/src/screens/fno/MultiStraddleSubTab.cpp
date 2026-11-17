#include "screens/fno/MultiStraddleSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/FnoFormat.h"
#include "screens/fno/MultiStraddleChart.h"
#include "services/options/OISnapshotter.h"
#include "services/options/OptionChainService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QComboBox>
#include <QCoreApplication>
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
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens::fno {

using fincept::services::options::OISample;
using fincept::services::options::OISnapshotter;
using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using namespace fincept::ui;

namespace {

struct TypeDef {
    const char* label;
    int wing_offset; // 0 = straddle, ±N = strangle wings
};

constexpr TypeDef kTypeDefs[] = {
    {"Straddle (ATM)", 0},
    {"Strangle ±1", 1},
    {"Strangle ±2", 2},
    {"Strangle ±3", 3},
};

/// Translated label for the straddle/strangle type combo at index i (0..3).
QString type_label_text(int i) {
    switch (i) {
        case 0:
            return QCoreApplication::translate("MultiStraddleSubTab", "Straddle (ATM)");
        case 1:
            return QCoreApplication::translate("MultiStraddleSubTab", "Strangle ±1");
        case 2:
            return QCoreApplication::translate("MultiStraddleSubTab", "Strangle ±2");
        case 3:
            return QCoreApplication::translate("MultiStraddleSubTab", "Strangle ±3");
        default:
            return {};
    }
}

QString strike_label(const OptionChainRow& row) {
    QString s = fincept::services::options::format_strike(row.strike);
    if (row.is_atm)
        s += "  " + QCoreApplication::translate("MultiStraddleSubTab", "(ATM)");
    return s;
}

/// Floor a millisecond timestamp to its minute boundary (in seconds).
[[maybe_unused]] qint64 floor_to_minute_secs(qint64 ms) {
    qint64 secs = ms / 1000;
    return (secs / 60) * 60;
}

/// Key of one (underlying, expiry) series — selections belong to it.
QString msstraddle_series_key(const QString& underlying, const QString& expiry) {
    return underlying + QLatin1Char('|') + expiry;
}

} // namespace

MultiStraddleSubTab::MultiStraddleSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoMultiStraddleTab");
    setStyleSheet(QString("#fnoMultiStraddleTab { background:%1; }"
                          "#fnoMSHeader { background:%2; border-bottom:1px solid %3; }"
                          "#fnoMSLabel { color:%4; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; "
                          "                 letter-spacing:0.4px; background:transparent; }"
                          "#fnoMSAdd { background:%5; color:%8; border:none; padding:5px 14px; "
                          "             font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.4px; }"
                          "#fnoMSAdd:hover { background:%6; color:%2; }"
                          "#fnoMSAdd:disabled { background:%3; color:%4; }"
                          "QComboBox { background:%2; color:%6; border:1px solid %3; padding:3px 8px; "
                          "             font-size:" + fincept::ui::fonts::px(-3) + "px; min-width:90px; }"
                          "QComboBox:hover { border-color:%5; }"
                          "QComboBox::drop-down { border:none; width:18px; }"
                          "QComboBox QAbstractItemView { background:%1; color:%6; border:1px solid %3; "
                          "                              selection-background-color:%5; selection-color:%8; }"
                          "QListWidget { background:%1; color:%6; border:1px solid %3; }"
                          "QListWidget::item { padding:5px 8px; border-bottom:1px solid %3; }"
                          "QListWidget::item:selected { background:%7; color:%6; }")
                      .arg(colors::BG_BASE(),        // %1
                           colors::BG_RAISED(),      // %2
                           colors::BORDER_DIM(),     // %3
                           colors::TEXT_SECONDARY(), // %4
                           colors::AMBER(),          // %5
                           colors::TEXT_PRIMARY(),   // %6
                           colors::BG_HOVER(),       // %7
                           colors::TEXT_ON_ACCENT())); // %8

    setup_ui();
    connect(add_btn_, &QPushButton::clicked, this, &MultiStraddleSubTab::on_add_clicked);
    connect(selection_list_, &QListWidget::itemDoubleClicked, this, &MultiStraddleSubTab::on_selection_double_clicked);
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

    auto add_kv = [&](const QString& label_text, QLabel*& label_out, QWidget* control) {
        auto* l = new QLabel(label_text.toUpper(), header);
        l->setObjectName("fnoMSLabel");
        hlay->addWidget(l);
        hlay->addWidget(control);
        label_out = l;
    };

    type_combo_ = new QComboBox(header);
    for (int i = 0; i < int(sizeof(kTypeDefs) / sizeof(kTypeDefs[0])); ++i)
        type_combo_->addItem(type_label_text(i));
    add_kv(tr("Type"), type_label_, type_combo_);

    strike_combo_ = new QComboBox(header);
    add_kv(tr("Anchor"), anchor_label_, strike_combo_);

    add_btn_ = new QPushButton(tr("ADD"), header);
    add_btn_->setObjectName("fnoMSAdd");
    add_btn_->setCursor(Qt::PointingHandCursor);
    add_btn_->setEnabled(false);
    type_combo_->setAccessibleName(tr("Straddle or strangle type"));
    strike_combo_->setAccessibleName(tr("Anchor strike"));
    add_btn_->setAccessibleName(tr("Add this straddle to the chart"));
    setTabOrder(type_combo_, strike_combo_);
    setTabOrder(strike_combo_, add_btn_);
    hlay->addWidget(add_btn_);
    add_status_ = new QLabel(header);
    add_status_->setObjectName("fnoMSLabel");
    add_status_->setStyleSheet(QString("color:%1;").arg(colors::NEGATIVE()));
    hlay->addWidget(add_status_, 1);
    hlay->addStretch(1);
    root->addWidget(header);

    // ── Body: selections list (left) | chart (right) — or a skeleton / page ─
    body_ = new QStackedWidget(this);
    content_ = new QWidget(body_);
    auto* content_lay = new QVBoxLayout(content_);
    content_lay->setContentsMargins(0, 0, 0, 0);
    content_lay->setSpacing(0);
    auto* split = new QSplitter(Qt::Horizontal, content_);
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);

    selection_list_ = new QListWidget(split);
    selection_list_->setToolTip(tr("Double-click an entry to remove it."));
    chart_stack_ = new QStackedWidget(split);
    chart_ = new MultiStraddleChart(chart_stack_);
    chart_stack_->addWidget(chart_);
    chart_wait_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Chart, chart_stack_);
    chart_wait_->set_message(tr("Loading intraday premium…"));
    chart_stack_->addWidget(chart_wait_);
    chart_note_ = new fincept::ui::EmptyState(chart_stack_);
    chart_note_->set_compact(true);
    chart_stack_->addWidget(chart_note_);
    connect(chart_note_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
        if (selections_.isEmpty()) {
            // ADD ATM STRADDLE
            for (const auto& r : last_chain_.rows)
                if (r.is_atm) {
                    const QString why = add_selection(r.strike, 0);
                    add_status_->setText(why);
                    break;
                }
            return;
        }
        // REFRESH: ask for the selections' history again.
        waited_out_ = false;
        const auto sels = selections_;
        for (const auto& s : sels) {
            auto& hub = fincept::datahub::DataHub::instance();
            for (qint64 tok : {s.ce_token, s.pe_token})
                if (const QString topic = topic_for_token_.value(tok); !topic.isEmpty())
                    hub.request(topic, /*force*/ true);
        }
        if (wait_timer_)
            wait_timer_->start();
        update_chart_area();
    });

    split->addWidget(selection_list_);
    split->addWidget(chart_stack_);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 4);
    content_lay->addWidget(split);
    body_->addWidget(content_);
    skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Chart, body_);
    skeleton_->set_message(fno_chain_loading_caption());
    body_->addWidget(skeleton_);
    page_ = new fincept::ui::EmptyState(body_);
    connect(page_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
        if (!page_is_feed_ || feed_req_.ok) {
            emit open_chain_requested();
        } else if (feed_req_.retry || !feed_req_.link.valid()) {
            update_body(); // check again (secure storage readable now?)
        } else {
            fincept::ui::open_setup_link(feed_req_.link, this);
        }
    });
    connect(page_, &fincept::ui::EmptyState::secondary_clicked, this, &MultiStraddleSubTab::open_chain_requested);
    body_->addWidget(page_);
    root->addWidget(body_, 1);

    // History that never comes (market closed, nothing recorded yet) must not leave
    // a skeleton spinning: after the wait the chart area says so.
    wait_timer_ = new QTimer(this);
    wait_timer_->setSingleShot(true);
    wait_timer_->setInterval(10'000);
    connect(wait_timer_, &QTimer::timeout, this, [this]() {
        waited_out_ = true;
        update_chart_area();
    });

    // No chain_published connection: it fired for every chain assembly for the
    // widget's lifetime (hidden or not) and double-handled each publish once the
    // hub subscription below was armed. Live updates come from showEvent().
    const auto& cached = fincept::services::options::OptionChainService::instance().last_chain();
    if (!cached.rows.isEmpty())
        on_chain_published(QVariant::fromValue(cached));
    update_body();
}

QVariantMap MultiStraddleSubTab::save_state() const {
    QVariantMap m;
    if (current_underlying_.isEmpty() || selections_.isEmpty())
        return m;
    QVariantList items;
    for (const auto& s : selections_)
        items.append(QVariantMap{{QStringLiteral("strike"), s.anchor_strike}, {QStringLiteral("wing"), s.wing}});
    m[QStringLiteral("series")] = msstraddle_series_key(current_underlying_, current_expiry_);
    m[QStringLiteral("items")] = items;
    return m;
}

void MultiStraddleSubTab::restore_state(const QVariantMap& state) {
    pending_series_ = state.value(QStringLiteral("series")).toString();
    pending_items_ = state.value(QStringLiteral("items")).toList();
    if (!last_chain_.rows.isEmpty())
        seed_selections();
}

void MultiStraddleSubTab::set_chain_status(FnoChainStatus status, const QString& reason) {
    chain_status_ = status;
    chain_reason_ = reason;
    update_body();
}

void MultiStraddleSubTab::set_external_chain(const OptionChain& chain) {
    apply_chain(chain);
}

void MultiStraddleSubTab::update_body() {
    if (!body_)
        return;
    page_is_feed_ = false;
    if (!last_chain_.rows.isEmpty() && last_chain_.broker_id == fno_free_source()) {
        // The delayed chain is a snapshot: no minute prices to sum into a premium line.
        const FnoFeedPage page = fno_broker_feed_page(
            tr("Multi-Straddle needs a broker feed"),
            tr("Straddle and strangle premiums are charted from minute-by-minute prices, which only a broker's "
               "live chain records — the delayed US chain is a snapshot."));
        feed_req_ = page.status;
        page_is_feed_ = true;
        page_->apply(page.spec);
        body_->setCurrentWidget(page_);
        return;
    }
    if (!last_chain_.rows.isEmpty()) {
        body_->setCurrentWidget(content_);
        update_chart_area();
        return;
    }
    // Loading — or the Chain tab already shows a chain this tab is about to receive.
    if (chain_status_ != FnoChainStatus::Unavailable) {
        body_->setCurrentWidget(skeleton_);
        return;
    }
    page_->apply(fno_no_chain_spec(
        tr("Multi-Straddle"),
        tr("Intraday premium of straddles and strangles on the chain's underlying, side by side."), chain_reason_,
        fincept::ui::LoadingSkeleton::Shape::Chart));
    body_->setCurrentWidget(page_);
}

void MultiStraddleSubTab::update_chart_area() {
    if (!chart_stack_)
        return;
    if (selections_.isEmpty()) {
        if (wait_timer_)
            wait_timer_->stop();
        fincept::ui::EmptyState::Spec s;
        s.glyph = QStringLiteral("∿");
        s.title = tr("No straddles on the chart");
        s.message = tr("Pick a type and an anchor strike above and press ADD — or start from the at-the-money "
                       "straddle.");
        s.primary = tr("ADD ATM STRADDLE");
        s.illustration = true;
        s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Chart;
        chart_note_->apply(s);
        chart_stack_->setCurrentWidget(chart_note_);
        return;
    }
    bool any_points = false;
    bool all_answered = true;
    for (const auto& sel : selections_) {
        const auto ce = samples_by_token_.constFind(sel.ce_token);
        const auto pe = samples_by_token_.constFind(sel.pe_token);
        if (ce == samples_by_token_.constEnd() || pe == samples_by_token_.constEnd())
            all_answered = false;
        else if (!ce->isEmpty() && !pe->isEmpty())
            any_points = true;
    }
    if (any_points) {
        if (wait_timer_)
            wait_timer_->stop();
        chart_stack_->setCurrentWidget(chart_);
        return;
    }
    if (!all_answered && !waited_out_) {
        chart_stack_->setCurrentWidget(chart_wait_);
        if (wait_timer_ && !wait_timer_->isActive() && isVisible())
            wait_timer_->start();
        return;
    }
    if (wait_timer_)
        wait_timer_->stop();
    fincept::ui::EmptyState::Spec s;
    s.glyph = QStringLiteral("○");
    s.glyph_tone = fincept::ui::EmptyState::Tone::Neutral;
    s.title = tr("No intraday premium recorded yet");
    s.message = tr("Prices are sampled once a minute while the chain is open; the lines fill in as the session "
                   "trades. Outside market hours there may be nothing to draw.");
    s.primary = tr("REFRESH");
    s.illustration = true;
    s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Chart;
    chart_note_->apply(s);
    chart_stack_->setCurrentWidget(chart_note_);
}

void MultiStraddleSubTab::seed_selections() {
    // Only the visible tab subscribes to history (showEvent seeds when it appears).
    if (!isVisible() || last_chain_.rows.isEmpty() || last_chain_.broker_id == fno_free_source())
        return;
    const QString series = msstraddle_series_key(last_chain_.underlying, last_chain_.expiry);
    if (seeded_series_.contains(series))
        return;
    seeded_series_.insert(series);
    if (!pending_items_.isEmpty() && pending_series_ == series) {
        // The user's own selections from the last session.
        const QVariantList items = std::exchange(pending_items_, {});
        pending_series_.clear();
        for (const QVariant& v : items) {
            const QVariantMap m = v.toMap();
            add_selection(m.value(QStringLiteral("strike")).toDouble(), m.value(QStringLiteral("wing")).toInt());
        }
        if (!selections_.isEmpty())
            return;
    }
    pending_items_.clear();
    pending_series_.clear();
    if (!selections_.isEmpty() || fno_is_us_source(last_chain_.broker_id))
        return; // Databento: the user adds what to chart (its intraday history is not recorded)
    // Default: the ATM straddle, once per series, so the chart opens with a line.
    for (const auto& r : last_chain_.rows) {
        if (r.is_atm) {
            add_selection(r.strike, 0);
            break;
        }
    }
}

void MultiStraddleSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    seed_selections(); // a chain that arrived while hidden: restore / add the ATM straddle now
    update_chart_area();
    if (chain_subscribed_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    QPointer<MultiStraddleSubTab> self = this;
    hub.subscribe_pattern(this, QStringLiteral("option:chain:*"), [self](const QString& /*topic*/, const QVariant& v) {
        if (!self)
            return;
        self->on_chain_published(v);
    });
    chain_subscribed_ = true;
    // Catch up on a snapshot published while this tab was hidden.
    const auto& cached = fincept::services::options::OptionChainService::instance().last_chain();
    if (!cached.rows.isEmpty() && cached.timestamp_ms != last_chain_.timestamp_ms)
        on_chain_published(QVariant::fromValue(cached));
}

void MultiStraddleSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (wait_timer_)
        wait_timer_->stop(); // no timer runs while hidden
    // §D3: pair the showEvent subscribe. The `chain_subscribed_` latch never
    // reset and on_chain_published() has no visibility gate, so a hidden tab
    // kept rebuilding on every option-chain publish for the rest of the
    // process.
    //
    // unsubscribe_pattern(), not the blanket unsubscribe(this): this tab also
    // owns per-token `subscribe()` entries created by subscribe_token(), whose
    // lifetime is tracked by token_refcount_ / topic_for_token_ and released
    // by remove_selection(). Dropping those here would leave the refcounts
    // populated with no subscription behind them, so subscribe_token() would
    // short-circuit on re-show and the selections' OI history would never come
    // back.
    fincept::datahub::DataHub::instance().unsubscribe_pattern(this, QStringLiteral("option:chain:*"));
    chain_subscribed_ = false;
}

void MultiStraddleSubTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MultiStraddleSubTab::retranslateUi() {
    if (type_label_)
        type_label_->setText(tr("Type").toUpper());
    if (anchor_label_)
        anchor_label_->setText(tr("Anchor").toUpper());
    if (add_btn_)
        add_btn_->setText(tr("ADD"));
    if (selection_list_)
        selection_list_->setToolTip(tr("Double-click an entry to remove it."));

    // Re-translate the fixed type combo items, preserving the selection.
    if (type_combo_) {
        const int keep = type_combo_->currentIndex();
        for (int i = 0; i < type_combo_->count(); ++i)
            type_combo_->setItemText(i, type_label_text(i));
        if (keep >= 0 && keep < type_combo_->count())
            type_combo_->setCurrentIndex(keep);
    }
    // Rebuild the strike combo so the "(ATM)" suffix re-translates.
    // rebuild_strike_combo re-selects by strike value, so the anchor sticks.
    if (strike_combo_ && strike_combo_->count() > 0 && !last_chain_.rows.isEmpty())
        rebuild_strike_combo(last_chain_);
}

void MultiStraddleSubTab::on_chain_published(const QVariant& v) {
    if (!v.canConvert<OptionChain>())
        return;
    // Every option:chain:* publish arrives here — keep only the Chain tab's series
    // (a late publish of the previous one would wipe the user's selections).
    const OptionChain incoming = v.value<OptionChain>();
    if (!fincept::services::options::OptionChainService::instance().is_focus_chain(incoming))
        return;
    apply_chain(incoming);
}

void MultiStraddleSubTab::apply_chain(const OptionChain& chain) {
    last_chain_ = chain;
    // Premiums are in the chain's currency (rupees on NSE, dollars on Databento's US options).
    if (chart_)
        chart_->set_currency(fno_chain_ccy(last_chain_.broker_id));
    const bool series_changed =
        last_chain_.underlying != current_underlying_ || last_chain_.expiry != current_expiry_;
    if (series_changed) {
        current_underlying_ = last_chain_.underlying;
        current_expiry_ = last_chain_.expiry;
        rebuild_strike_combo(last_chain_);
        // Drop existing selections — their tokens belong to the previous series.
        for (int i = selections_.size() - 1; i >= 0; --i)
            remove_selection(i);
    } else if (strike_combo_->count() != last_chain_.rows.size()) {
        // Same series, resized ladder (the chain window slides with the ATM).
        // Rebuild so the anchor list matches the rows we index into; existing
        // selections are token-keyed and stay valid.
        rebuild_strike_combo(last_chain_);
    }
    add_btn_->setEnabled(strike_combo_->count() > 0 && last_chain_.broker_id != fno_free_source());
    seed_selections();
    update_body();
}

void MultiStraddleSubTab::rebuild_strike_combo(const OptionChain& chain) {
    QSignalBlocker block(strike_combo_);
    // Preserve the user's anchor by STRIKE across rebuilds; a row index would
    // point at a different strike as soon as the ladder window shifts.
    const double keep_strike = strike_combo_->count() > 0 ? strike_combo_->currentData().toDouble() : 0.0;
    strike_combo_->clear();
    int atm_idx = 0;
    int keep_idx = -1;
    for (int i = 0; i < chain.rows.size(); ++i) {
        const auto& r = chain.rows[i];
        strike_combo_->addItem(strike_label(r), QVariant(r.strike));
        if (r.is_atm)
            atm_idx = i;
        if (keep_strike > 0 && std::abs(r.strike - keep_strike) < 1e-6)
            keep_idx = i;
    }
    if (chain.rows.isEmpty())
        return;
    strike_combo_->setCurrentIndex(keep_idx >= 0 ? keep_idx : atm_idx);
}

void MultiStraddleSubTab::on_add_clicked() {
    if (last_chain_.rows.isEmpty())
        return;
    const int type_idx = type_combo_->currentIndex();
    if (type_idx < 0 || type_idx >= int(sizeof(kTypeDefs) / sizeof(kTypeDefs[0])))
        return;
    add_status_->setText(add_selection(strike_combo_->currentData().toDouble(), kTypeDefs[type_idx].wing_offset));
}

QString MultiStraddleSubTab::add_selection(double anchor_strike, int wing) {
    if (last_chain_.rows.isEmpty())
        return tr("No chain loaded yet.");
    // Resolve the anchor by strike value, then derive the wings from its
    // position in the *current* ladder.
    int anchor_idx = -1;
    for (int i = 0; i < last_chain_.rows.size(); ++i) {
        if (std::abs(last_chain_.rows[i].strike - anchor_strike) < 1e-6) {
            anchor_idx = i;
            break;
        }
    }
    if (anchor_idx < 0) {
        LOG_WARN("FnoMultiStraddle", "Anchor strike is no longer in the chain — reselect it.");
        return tr("That anchor strike is no longer in the chain. Pick another.");
    }

    const int ce_idx = anchor_idx + wing;
    const int pe_idx = anchor_idx - wing;
    if (ce_idx < 0 || ce_idx >= last_chain_.rows.size() || pe_idx < 0 || pe_idx >= last_chain_.rows.size()) {
        LOG_WARN("FnoMultiStraddle", "Selected wings fall outside the chain — pick a closer anchor or smaller wing.");
        // Surface it — a silent no-op on a button press reads as a broken UI.
        return tr("The ±%1 wings fall outside the loaded strike range. "
                  "Pick a strike nearer the middle of the chain, or a narrower strangle.")
            .arg(wing);
    }

    Selection sel;
    sel.broker = last_chain_.broker_id;
    sel.ce_token = last_chain_.rows[ce_idx].ce_token;
    sel.pe_token = last_chain_.rows[pe_idx].pe_token;
    sel.anchor_strike = last_chain_.rows[anchor_idx].strike;
    sel.wing = wing;
    if (sel.ce_token == 0 || sel.pe_token == 0) {
        LOG_WARN("FnoMultiStraddle", "Anchor row missing CE or PE token — pick a different strike.");
        return tr("This provider published no instrument token for one side of that strike, "
                  "so its intraday history can't be fetched. Pick another strike.");
    }
    if (wing == 0) {
        // The shared strike label: a 2.5-step stock strike (1272.5) keeps its fraction.
        sel.label = tr("Straddle %1").arg(fincept::services::options::format_strike(last_chain_.rows[anchor_idx].strike));
    } else {
        sel.label = tr("Strangle %1C / %2P")
                        .arg(fincept::services::options::format_strike(last_chain_.rows[ce_idx].strike),
                             fincept::services::options::format_strike(last_chain_.rows[pe_idx].strike));
    }
    // Avoid duplicates — same (ce, pe, broker) is already in the list.
    for (const auto& existing : selections_) {
        if (existing.broker == sel.broker && existing.ce_token == sel.ce_token && existing.pe_token == sel.pe_token)
            return tr("%1 is already on the chart.").arg(sel.label);
    }
    selections_.append(sel);

    auto* item = new QListWidgetItem(sel.label);
    item->setToolTip(tr("Double-click to remove."));
    selection_list_->addItem(item);

    waited_out_ = false;
    subscribe_token(sel.broker, sel.ce_token);
    subscribe_token(sel.broker, sel.pe_token);
    rebuild_chart();
    return QString();
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
    update_chart_area();
}

void MultiStraddleSubTab::subscribe_token(const QString& broker, qint64 token) {
    if (token == 0)
        return;
    const int n = ++token_refcount_[token];
    if (n > 1)
        return; // already subscribed via another selection
    const QString topic = OISnapshotter::history_topic(broker, token, QStringLiteral("1d"));
    topic_for_token_.insert(token, topic);
    QPointer<MultiStraddleSubTab> self = this;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, topic, [self, token](const QVariant& v) {
        if (!self)
            return;
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
    update_chart_area();
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
