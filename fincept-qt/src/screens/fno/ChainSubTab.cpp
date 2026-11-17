#include "screens/fno/ChainSubTab.h"

#include "trading/OrderThreadPool.h"
#include "core/logging/Logger.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/FnoFormat.h"
#include "screens/fno/FnoHeaderBar.h"
#include "screens/fno/OptionChainModel.h"
#include "screens/fno/OptionChainTable.h"
#include "services/databento/DatabentoService.h"
#include "services/options/DerivMarketData.h"
#include "services/options/FreeOptionChainService.h"
#include "services/options/OptionChainService.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/OrderSourceCapture.h"
#include "trading/UnifiedTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::fno {

using namespace fincept::ui;
using fincept::services::options::FreeOptionChainService;
using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainService;
using fincept::trading::AccountManager;
using fincept::trading::BrokerAccount;
using fincept::trading::InstrumentService;

static QString find_account_for_broker(const QString& broker_id) {
    auto accounts = AccountManager::instance().list_accounts(broker_id);
    for (const auto& acct : accounts)
        if (acct.is_active)
            return acct.account_id;
    return accounts.isEmpty() ? QString{} : accounts[0].account_id;
}

namespace {

namespace fcmarket = fincept::services::options::market;

/// Underlyings offered as quick-pick chips on the free delayed US source (any US-listed
/// optionable ticker works through the symbol box; these are the liquid defaults).
/// Plain US tickers — index roots without Yahoo's caret (fno_us_chain_symbol). The
/// first is the opening default.
const QStringList& fnochain_free_underlyings() {
    static const QStringList k = {
        QStringLiteral("SPY"),  QStringLiteral("QQQ"),  QStringLiteral("IWM"),   QStringLiteral("DIA"),
        QStringLiteral("SPX"),  QStringLiteral("NDX"),  QStringLiteral("RUT"),   QStringLiteral("AAPL"),
        QStringLiteral("MSFT"), QStringLiteral("NVDA"), QStringLiteral("AMZN"),  QStringLiteral("META"),
        QStringLiteral("GOOGL"), QStringLiteral("TSLA"), QStringLiteral("AMD"),  QStringLiteral("NFLX"),
    };
    return k;
}

/// OCC-style contract code of a US listed option ("SPY261120C00450000"): the leg's
/// identity in the Builder and the chain's menus (the free feed has no broker token).
QString fnochain_occ_symbol(const QString& underlying, const QDate& expiry, bool is_call, double strike) {
    QString root = underlying.trimmed().toUpper();
    if (root.startsWith(QLatin1Char('^')))
        root = root.mid(1);
    return root + expiry.toString(QStringLiteral("yyMMdd")) + QLatin1Char(is_call ? 'C' : 'P') +
           QStringLiteral("%1").arg(qint64(std::llround(strike * 1000.0)), 8, 10, QLatin1Char('0'));
}

double fnochain_finite(double v) {
    return std::isfinite(v) ? v : 0.0;
}

/// Strike that minimises the option writers' payout at expiry (max pain).
double fnochain_max_pain(const QVector<fincept::services::options::OptionChainRow>& rows) {
    double best = 0, best_pain = std::numeric_limits<double>::infinity();
    for (const auto& s : rows) {
        double pain = 0;
        for (const auto& r : rows) {
            pain += double(r.ce_quote.oi) * std::max(0.0, s.strike - r.strike);
            pain += double(r.pe_quote.oi) * std::max(0.0, r.strike - s.strike);
        }
        if (pain < best_pain) {
            best_pain = pain;
            best = s.strike;
        }
    }
    return best;
}

/// One expiry of the free delayed chain in the F&O screen's chain model: one row per
/// strike within ±30 % of spot (the whole ladder when that leaves too few), US
/// contract multiplier 100, quotes / OI / volume from the feed, IV and Greeks from
/// the local analysis (vega per 1.00 vol, as OptionGreeks expects), synthetic
/// non-zero tokens so the table's leg clicks and the Builder work. Pure; < 1 ms.
OptionChain fnochain_from_free(const fcmarket::FreeChain& fc, int e, const QString& underlying) {
    using fincept::services::options::OptionChainRow;
    using fincept::services::options::OptionGreeks;
    using fincept::services::options::UnderlyingKind;
    OptionChain c;
    c.broker_id = fno_free_source();
    c.underlying = underlying;
    if (e < 0 || e >= fc.expiries.size())
        return c;
    const fcmarket::FreeExpiry& ex = fc.expiries.at(e);
    c.expiry = ex.date.toString(Qt::ISODate);
    c.kind = fno_is_us_index_root(underlying) ? UnderlyingKind::Index : UnderlyingKind::Stock;
    c.spot = fnochain_finite(fc.spot);
    c.spot_change = fnochain_finite(fc.change);
    c.spot_change_pct = fnochain_finite(fc.change_pct);
    const QDateTime at = fc.quote_time.isValid() ? fc.quote_time
                                                 : (fc.fetched_at.isValid() ? fc.fetched_at
                                                                            : QDateTime::currentDateTimeUtc());
    c.timestamp_ms = at.toMSecsSinceEpoch();

    const double ref = c.spot > 0 ? c.spot : fnochain_finite(ex.forward);
    const int end = std::min<int>(ex.end, int(fc.quotes.size()));
    // ±30 % window, unless that leaves fewer than 8 strikes (illiquid names).
    bool windowed = ref > 0;
    if (windowed) {
        int inside = 0;
        double last_strike = -1;
        for (int i = ex.begin; i < end; ++i) {
            const double k = fc.quotes.at(i).strike;
            if (k >= ref * 0.7 && k <= ref * 1.3 && std::abs(k - last_strike) > 1e-9) {
                ++inside;
                last_strike = k;
            }
        }
        windowed = inside >= 8;
    }

    QVector<OptionChainRow> rows;
    rows.reserve((end - ex.begin) / 2 + 1);
    for (int i = ex.begin; i < end; ++i) {
        const fcmarket::FreeOptionQuote& q = fc.quotes.at(i);
        if (windowed && (q.strike < ref * 0.7 || q.strike > ref * 1.3))
            continue;
        if (rows.isEmpty() || std::abs(rows.last().strike - q.strike) > 1e-9) {
            OptionChainRow r;
            r.strike = q.strike;
            r.lot_size = 100; // US equity / index option multiplier
            rows.append(r);
        }
        OptionChainRow& r = rows.last();
        fincept::trading::BrokerQuote bq;
        bq.symbol = fnochain_occ_symbol(underlying, ex.date, q.is_call, q.strike);
        bq.bid = fnochain_finite(q.bid);
        bq.ask = fnochain_finite(q.ask);
        const double last = fnochain_finite(q.last);
        bq.ltp = last > 0 ? last : fnochain_finite(q.mid);
        bq.volume = double(q.volume);
        bq.oi = q.open_interest;
        bq.timestamp = c.timestamp_ms;
        const double iv = std::isfinite(q.iv) && q.iv > 0 ? q.iv : fnochain_finite(q.feed_iv);
        OptionGreeks g;
        if (std::isfinite(q.delta)) {
            g.delta = q.delta;
            g.gamma = fnochain_finite(q.gamma);
            g.theta = fnochain_finite(q.theta);
            g.vega = fnochain_finite(q.vega) * 100.0; // feed: per vol point; model: per 1.00 vol
            g.valid = true;
        }
        if (q.is_call) {
            r.ce_quote = bq;
            r.ce_symbol = bq.symbol;
            r.ce_iv = iv;
            r.ce_greeks = g;
        } else {
            r.pe_quote = bq;
            r.pe_symbol = bq.symbol;
            r.pe_iv = iv;
            r.pe_greeks = g;
        }
    }

    // Synthetic tokens (never 0 — the table treats 0 as "no contract"); unique per chain.
    const qint64 base = qint64(e + 1) * 1'000'000;
    double best_gap = std::numeric_limits<double>::infinity();
    int atm = -1;
    for (int i = 0; i < rows.size(); ++i) {
        OptionChainRow& r = rows[i];
        if (!r.ce_symbol.isEmpty())
            r.ce_token = base + qint64(i) * 2 + 1;
        if (!r.pe_symbol.isEmpty())
            r.pe_token = base + qint64(i) * 2 + 2;
        c.total_ce_oi += double(r.ce_quote.oi);
        c.total_pe_oi += double(r.pe_quote.oi);
        const double gap = std::abs(r.strike - ref);
        if (ref > 0 && gap < best_gap) {
            best_gap = gap;
            atm = i;
        }
    }
    if (atm >= 0) {
        rows[atm].is_atm = true;
        c.atm_strike = rows[atm].strike;
    }
    c.pcr = c.total_ce_oi > 0 ? c.total_pe_oi / c.total_ce_oi : 0.0;
    c.max_pain = std::isfinite(ex.max_pain) && ex.max_pain > 0 ? ex.max_pain : fnochain_max_pain(rows);
    c.rows = std::move(rows);
    return c;
}

} // namespace

ChainSubTab::ChainSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoChainTab");
    setStyleSheet(QString("#fnoChainTab { background:%1; }").arg(colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    header_ = new FnoHeaderBar(this);
    root->addWidget(header_);

    // Free delayed US chain: any US-listed stock / ETF / index through the symbol box,
    // the liquid defaults as chips (shown only while that source is picked).
    build_free_bar(this);
    root->addWidget(free_bar_);

    // Body: the table, a skeleton in its shape while a chain loads, and a designed
    // page (setup / error / note) — flipped without rebuilding any of them.
    auto* body_wrap = new QWidget(this);
    body_stack_ = new QStackedLayout(body_wrap);
    body_stack_->setContentsMargins(0, 0, 0, 0);
    body_stack_->setStackingMode(QStackedLayout::StackOne);

    table_ = new OptionChainTable(body_wrap);
    body_stack_->addWidget(table_);

    skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, body_wrap);
    skeleton_->set_row_count(12);
    skeleton_->set_message(tr("Finding an options source…"));
    body_stack_->addWidget(skeleton_);

    state_ = new fincept::ui::EmptyState(body_wrap);
    connect(state_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
        if (auto fn = state_primary_)
            fn();
    });
    connect(state_, &fincept::ui::EmptyState::secondary_clicked, this, [this]() {
        if (auto fn = state_secondary_)
            fn();
    });
    body_stack_->addWidget(state_);
    body_stack_->setCurrentWidget(skeleton_);

    root->addWidget(body_wrap, 1);

    // ── Wire signals ──────────────────────────────────────────────────────
    connect(header_, &FnoHeaderBar::broker_changed, this, &ChainSubTab::on_broker_changed);
    connect(header_, &FnoHeaderBar::underlying_changed, this, &ChainSubTab::on_underlying_changed);
    connect(header_, &FnoHeaderBar::expiry_changed, this, &ChainSubTab::on_expiry_changed);
    connect(header_, &FnoHeaderBar::refresh_requested, this, &ChainSubTab::on_refresh_clicked);
    connect(table_, &OptionChainTable::order_requested, this, &ChainSubTab::on_order_requested);

    connect(&InstrumentService::instance(), &InstrumentService::refresh_done, this, [this](const QString& broker_id) {
        if (broker_id == header_->broker_id())
            rebuild_picker_for_broker(broker_id, true);
    });
    connect(&InstrumentService::instance(), &InstrumentService::refresh_failed, this,
            [this](const QString& broker_id, const QString& error) {
                if (broker_id != header_->broker_id())
                    return;
                show_error(tr("Could not load the %1 instrument list").arg(broker_id),
                           tr("The broker's contract list did not download, so its option chains cannot be "
                              "assembled."),
                           [this, broker_id]() { rebuild_picker_for_broker(broker_id, true); }, error);
            });

    // Populate the source picker. Prefer a live/connected trading broker; with none,
    // the free delayed US chain (Databento only when the user picked it before and
    // has its key) — we never auto-open Databento while a broker is connected.
    QTimer::singleShot(0, this, [this]() { refresh_broker_list(); });
}

void ChainSubTab::refresh_broker_list() {
    static const QString kDatabento = QStringLiteral("databento");
    const QString kFree = fno_free_source();
    QStringList broker_ids;
    QString connected_broker; // first account in ConnectionState::Connected
    QString active_broker;    // first active broker account (fallback)
    for (const auto& acc : AccountManager::instance().active_accounts()) {
        if (!broker_ids.contains(acc.broker_id))
            broker_ids.append(acc.broker_id);
        if (active_broker.isEmpty())
            active_broker = acc.broker_id;
        if (connected_broker.isEmpty() && AccountManager::instance().connection_state(acc.account_id) ==
                                              fincept::trading::ConnectionState::Connected)
            connected_broker = acc.broker_id;
    }
    broker_ids.append(kFree);      // free, delayed, no key — always available
    broker_ids.append(kDatabento); // always available, listed last

    // Re-run on every screen show: a broker connected after this tab was built used
    // to stay missing until restart. When the current selection is still listed it
    // is kept as-is (no picker rebuild, no resubscribe) — only the list grows.
    const QString current = header_->broker_id();
    if (!current.isEmpty() && broker_ids.contains(current)) {
        header_->set_brokers(broker_ids, current);
        return;
    }

    // Auto-select: sticky choice (not Databento) → connected → any active → Databento
    // when it was the last choice and has its key → the free delayed US chain.
    QString prefer;
    if (!last_broker_.isEmpty() && last_broker_ != kDatabento && broker_ids.contains(last_broker_))
        prefer = last_broker_;
    if (prefer.isEmpty())
        prefer = connected_broker;
    if (prefer.isEmpty())
        prefer = active_broker;
    if (prefer.isEmpty() && last_broker_ == kDatabento && fincept::DatabentoService::instance().has_api_key())
        prefer = kDatabento;
    if (prefer.isEmpty())
        prefer = kFree;

    header_->set_brokers(broker_ids, prefer);
    rebuild_picker_for_broker(prefer, /*keep_selection*/ true);
}

ChainSubTab::~ChainSubTab() {
    cancel_free_fetch();
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

QVariantMap ChainSubTab::save_state() const {
    QVariantMap m;
    m["broker"] = header_->broker_id();
    m["underlying"] = header_->underlying();
    m["expiry"] = header_->expiry();
    if (!last_free_underlying_.isEmpty())
        m["free_underlying"] = last_free_underlying_;
    return m;
}

void ChainSubTab::restore_state(const QVariantMap& state) {
    last_broker_ = state.value("broker").toString();
    last_underlying_ = state.value("underlying").toString();
    last_expiry_ = state.value("expiry").toString();
    last_free_underlying_ = state.value("free_underlying").toString();
    if (last_free_underlying_.isEmpty() && last_broker_ == fno_free_source())
        last_free_underlying_ = last_underlying_;
    // Sessions saved before the free chain took plain tickers stored "^SPX".
    last_free_underlying_ = fno_us_chain_symbol(last_free_underlying_);
    // Picker repopulation in the deferred lambda above will pick these up.
}

QString ChainSubTab::active_source() const {
    return header_ ? header_->broker_id() : QString();
}

void ChainSubTab::use_free_source() {
    if (header_ && !header_->select_broker(fno_free_source()))
        refresh_broker_list();
}

void ChainSubTab::request_underlying(const QString& underlying) {
    if (!header_ || underlying.isEmpty())
        return;
    if (header_->broker_id() == fno_free_source()) {
        // The free chain lists plain US tickers ("^SPX" from a linked screen is "SPX").
        // Only a symbol already offered is followed: a linked non-US ticker must not
        // replace the chain with a "no listed options" page.
        header_->select_underlying(fno_us_chain_symbol(underlying));
        return;
    }
    last_underlying_ = underlying; // sticky across picker rebuilds
    // If the combo already has it, flip — the underlying_changed signal
    // wires through to on_underlying_changed which rebuilds expiries +
    // resubscribes. If the symbol isn't in the broker's NFO list yet,
    // select_underlying is a no-op and we silently ignore the link event.
    header_->select_underlying(underlying);
}

QString ChainSubTab::active_underlying() const {
    return header_ ? header_->underlying() : QString();
}

void ChainSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    is_visible_ = true;
    // Already live for the current selection (the screen stayed up while another sub-tab
    // was in front): nothing to redo. Re-subscribing would idle the topic for an instant
    // and tear down the live WS feed for no reason.
    if (!active_topic_.isEmpty() && active_topic_ == current_topic())
        return;
    resubscribe();
}

void ChainSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // Switching to another F&O sub-tab hides this page but leaves the screen up — keep
    // the chain flowing for the tabs that read it. A hide of the whole screen is
    // handled here too (the screen is already not visible) and by on_screen_hidden().
    is_visible_ = owning_screen_visible();
    if (!is_visible_)
        drop_subscription();
}

bool ChainSubTab::owning_screen_visible() const {
    for (const QWidget* w = parentWidget(); w; w = w->parentWidget()) {
        if (w->objectName() == QLatin1String("fnoScreen"))
            return w->isVisible();
    }
    return false;
}

void ChainSubTab::on_screen_shown() {
    refresh_broker_list(); // pick up a broker connected since the last visit
    // Normally this tab's own showEvent has already subscribed; this covers the case
    // where another sub-tab (restored session) is the visible one.
    if (is_visible_)
        return;
    is_visible_ = true;
    resubscribe();
}

void ChainSubTab::on_screen_hidden() {
    is_visible_ = false;
    drop_subscription();
}

void ChainSubTab::drop_subscription() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!active_topic_.isEmpty()) {
        hub.unsubscribe(this, active_topic_);
        hub.unsubscribe_errors(this, active_topic_);
        active_topic_.clear();
    }
    if (!tick_pattern_.isEmpty()) {
        hub.unsubscribe_pattern(this, tick_pattern_);
        tick_pattern_.clear();
    }
    // The free chain is fetched, not subscribed: leaving cancels a fetch in flight
    // (it starts again on the next show; the service's copy makes that instant).
    cancel_free_fetch();
}

void ChainSubTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ChainSubTab::retranslateUi() {
    // The designed pages carry state-dependent text that is rebuilt on the next state
    // change; the source names in the picker follow on the next screen show. The
    // FnoHeaderBar child retranslates itself.
    if (free_bar_label_)
        free_bar_label_->setText(tr("UNDERLYING"));
    if (free_chips_label_)
        free_chips_label_->setText(tr("QUICK PICKS"));
    if (free_box_) {
        free_box_->set_placeholder(tr("Any US stock, ETF or index"));
        free_box_->setToolTip(tr("Search any US-listed stock, ETF or index with listed options (delayed Cboe "
                                 "quotes). Enter also tries a ticker the search does not know, e.g. XSP or DJX."));
    }
    for (QPushButton* chip : free_chips_)
        chip->setToolTip(tr("Open the delayed %1 option chain").arg(chip->property("fno_symbol").toString()));
}

void ChainSubTab::on_broker_changed(const QString& broker_id) {
    LOG_DEBUG("FnoChain", QString("on_broker_changed: '%1'").arg(broker_id));
    last_broker_ = broker_id;
    rebuild_picker_for_broker(broker_id, /*keep_selection*/ true);
}

void ChainSubTab::on_underlying_changed(const QString& underlying) {
    last_underlying_ = underlying;
    const QString source = header_->broker_id();
    if (source == fno_free_source()) {
        // Another symbol: its own chain (memory copy when fresh, else fetched). It
        // becomes the remembered symbol (last_free_underlying_) once its chain loads.
        sync_free_bar();
        header_->set_expiries({});
        if (is_visible_)
            start_free_fetch(false);
        else
            show_loading(tr("Loading the delayed %1 option chain (Cboe)…").arg(underlying));
    } else {
        rebuild_expiries_for_underlying(source, underlying, /*keep_selection*/ true);
    }

    // Broadcast to the parent FnoScreen's symbol-link group so other
    // Yellow-group panels (watchlist, equity research, etc.) follow the
    // F&O underlying change. Walks up the parent chain looking for an
    // IGroupLinked screen — if found and its group != None, publish.
    if (underlying.isEmpty())
        return;
    fincept::SymbolGroup link_group = fincept::SymbolGroup::None;
    for (QObject* p = parent(); p; p = p->parent()) {
        if (auto* gl = dynamic_cast<fincept::IGroupLinked*>(p)) {
            link_group = gl->group();
            break;
        }
    }
    if (link_group == fincept::SymbolGroup::None)
        return;
    static const QSet<QString> kIndexNames = {
        QStringLiteral("NIFTY"),
        QStringLiteral("BANKNIFTY"),
        QStringLiteral("FINNIFTY"),
        QStringLiteral("MIDCPNIFTY"),
    };
    fincept::SymbolRef ref;
    ref.symbol = underlying;
    if (fno_is_us_source(source)) {
        // An index root ("SPX") links as the Yahoo index the other screens read ("^SPX").
        const bool index = fno_is_us_index_root(underlying);
        if (index)
            ref.symbol = fno_us_index_yahoo(underlying);
        ref.asset_class = index ? QStringLiteral("index") : QStringLiteral("equity");
        ref.exchange = QStringLiteral("US");
    } else {
        ref.asset_class = kIndexNames.contains(underlying) ? QStringLiteral("index") : QStringLiteral("equity");
        ref.exchange = QStringLiteral("NSE");
    }
    fincept::SymbolContext::instance().set_group_symbol(link_group, ref, this);
}

void ChainSubTab::on_expiry_changed(const QString& expiry) {
    last_expiry_ = expiry;
    if (header_->broker_id() == fno_free_source()) {
        // Every expiry of the free chain is already held: convert, no fetch.
        if (free_chain_ && FreeOptionChainService::key_of(free_chain_symbol_) ==
                               FreeOptionChainService::key_of(header_->underlying()))
            render_free_expiry();
        return;
    }
    if (is_visible_)
        resubscribe();
}

void ChainSubTab::on_refresh_clicked() {
    if (header_->broker_id() == fno_free_source()) {
        start_free_fetch(/*force*/ true);
        return;
    }
    const QString topic = current_topic();
    if (topic.isEmpty())
        return;
    fincept::datahub::DataHub::instance().request(topic, /*force*/ true);
}

void ChainSubTab::rebuild_picker_for_broker(const QString& broker_id, bool keep_selection) {
    LOG_DEBUG("FnoChain", QString("rebuild_picker_for_broker: broker='%1' keep=%2").arg(broker_id).arg(keep_selection));
    // The previous broker's chain topic must not stay subscribed while this picker is
    // rebuilt: several paths below end in an empty state ("Loading instruments…",
    // "Select a broker", no NFO data) without re-subscribing, and the next publish on
    // the OLD topic (WS refresh, Greeks republish) would flip the table back to the old
    // broker's chain under the new broker's name. Every success path re-subscribes
    // through resubscribe() when the tab is visible.
    drop_subscription();
    // The free source picks its underlying in the symbol strip (any US ticker); the
    // broker / Databento sources keep the header's Underlying picker.
    set_free_bar_visible(broker_id == fno_free_source());
    if (broker_id != fno_free_source()) {
        clear_free_shown();
        header_->set_delayed_source(QString());
    }
    if (broker_id.isEmpty()) {
        LOG_WARN("FnoChain", fincept::logf("Chain picker rebuild skipped", {{"reason", "no source selected"}}));
        fincept::ui::EmptyState::Spec s;
        s.glyph = QStringLiteral("◇");
        s.title = tr("Pick an options source");
        s.message = tr("Choose a broker for its live NSE / BSE chain, or open the free delayed US chain — no "
                       "broker or key needed.");
        s.primary = tr("USE FREE US CHAIN");
        s.illustration = true;
        s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Table;
        show_state(s, [this]() { use_free_source(); });
        return;
    }
    if (broker_id == fno_free_source()) {
        rebuild_picker_for_free_source(keep_selection);
        return;
    }
    if (broker_id == QStringLiteral("databento")) {
        if (!fincept::DatabentoService::instance().has_api_key()) {
            header_->set_underlyings({});
            header_->set_expiries({});
            const auto st = fincept::ui::check_requirements(
                {fincept::ui::Requirement::api_key(QStringLiteral("DATABENTO_API_KEY"), QStringLiteral("Databento"))});
            if (!st.ok) {
                show_requirement(st, [this]() { rebuild_picker_for_broker(QStringLiteral("databento"), true); },
                                 /*offer_free*/ true);
            } else {
                show_error(tr("Databento key not readable"),
                           tr("A Databento key is listed but could not be used to load US options."),
                           [this]() { rebuild_picker_for_broker(QStringLiteral("databento"), true); });
            }
            return;
        }
        auto unders = OptionChainService::instance().list_underlyings(QStringLiteral("databento"));
        if (unders.isEmpty()) {
            show_error(tr("Databento is not configured"),
                       tr("No US underlyings are set up for the Databento source."),
                       [this]() { rebuild_picker_for_broker(QStringLiteral("databento"), true); });
            return;
        }
        QString prefer;
        if (keep_selection && unders.contains(last_underlying_))
            prefer = last_underlying_;
        else
            prefer = unders.contains(QStringLiteral("SPY")) ? QStringLiteral("SPY") : unders.first();
        header_->set_underlyings(unders, prefer);
        rebuild_expiries_for_underlying(QStringLiteral("databento"), prefer, keep_selection);
        return;
    }
    const bool loaded = InstrumentService::instance().is_loaded(broker_id);
    LOG_DEBUG("FnoChain", QString("rebuild_picker_for_broker: is_loaded('%1')=%2").arg(broker_id).arg(loaded));
    if (!loaded) {
        show_loading(tr("Loading %1 instruments…").arg(broker_id));
        header_->set_underlyings({});
        header_->set_expiries({});

        QPointer<ChainSubTab> self = this;
        InstrumentService::instance().load_from_db_async(broker_id, [self, broker_id](int count) {
            if (!self) {
                LOG_WARN("FnoChain",
                         QString("load_from_db_async callback: widget destroyed, broker='%1'").arg(broker_id));
                return;
            }
            LOG_INFO("FnoChain",
                     QString("load_from_db_async callback: broker='%1' count=%2").arg(broker_id).arg(count));
            if (self->header_->broker_id() != broker_id)
                return; // the user picked another source meanwhile
            if (count > 0) {
                self->rebuild_picker_for_broker(broker_id, true);
                return;
            }
            const auto retry = [self, broker_id]() {
                if (self)
                    self->rebuild_picker_for_broker(broker_id, true);
            };
            QString account_id = find_account_for_broker(broker_id);
            LOG_DEBUG("FnoChain", QString("find_account_for_broker('%1') → '%2'").arg(broker_id, account_id));
            if (account_id.isEmpty()) {
                // The terminal's broker requirement: "No <broker> account connected" + OPEN
                // ACCOUNTS, or — stored credentials unreadable — RETRY, never "connect".
                auto st = fincept::ui::check_requirements({fincept::ui::Requirement::broker(broker_id, broker_id)});
                if (st.ok) {
                    st.ok = false;
                    st.title = ChainSubTab::tr("No %1 account configured").arg(broker_id);
                    st.message = ChainSubTab::tr("Add a %1 account in Equity Trading > Accounts to load its chain.")
                                     .arg(broker_id);
                    st.action_text = ChainSubTab::tr("OPEN ACCOUNTS");
                    st.link = fincept::ui::SetupLink{QStringLiteral("equity_trading"), QStringLiteral("accounts"),
                                                     broker_id};
                }
                self->show_requirement(st, retry, /*offer_free*/ true);
                return;
            }
            QString read_error;
            auto creds = trading::AccountManager::instance().load_credentials(account_id, &read_error);
            if (!read_error.isEmpty()) {
                // Unreadable stored credentials: say so and offer RETRY (a re-read) —
                // the account may well be set up, so never ask to connect it again.
                fincept::ui::RequirementStatus st;
                st.ok = false;
                st.retry = true;
                st.title = ChainSubTab::tr("Could not read secure storage");
                st.message = ChainSubTab::tr("%1 Your %2 credentials may already be saved — nothing needs to be "
                                             "entered again.")
                                 .arg(read_error, broker_id);
                st.action_text = ChainSubTab::tr("RETRY");
                self->show_requirement(st, retry, /*offer_free*/ true);
                return;
            }
            self->show_loading(ChainSubTab::tr("Downloading %1 instruments from the broker…").arg(broker_id));
            InstrumentService::instance().refresh(broker_id, creds);
        });
        return;
    }
    auto unders = OptionChainService::instance().list_underlyings(broker_id);
    LOG_DEBUG("FnoChain", QString("list_underlyings('%1') → %2 items").arg(broker_id).arg(unders.size()));
    if (unders.isEmpty()) {
        header_->set_underlyings({});
        header_->set_expiries({});
        fincept::ui::EmptyState::Spec s = fincept::ui::EmptyState::no_data_spec(
            tr("No F&O contracts for %1").arg(broker_id),
            tr("The %1 instrument list has no NFO option contracts cached yet.").arg(broker_id), tr("RETRY"));
        s.secondary = tr("USE FREE US CHAIN");
        s.illustration = true;
        s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Table;
        show_state(s, [this, broker_id]() { rebuild_picker_for_broker(broker_id, true); },
                   [this]() { use_free_source(); });
        return;
    }
    QString prefer;
    if (keep_selection && unders.contains(last_underlying_))
        prefer = last_underlying_;
    else
        prefer = unders.contains("NIFTY") ? "NIFTY" : unders.first();
    LOG_DEBUG("FnoChain",
              QString("Setting underlyings for '%1', prefer='%2' (%3 items)").arg(broker_id, prefer).arg(unders.size()));
    header_->set_underlyings(unders, prefer);
    LOG_DEBUG("FnoChain", "set_underlyings done, calling rebuild_expiries_for_underlying");
    rebuild_expiries_for_underlying(broker_id, prefer, keep_selection);
    LOG_DEBUG("FnoChain", "rebuild_expiries_for_underlying done");
}

void ChainSubTab::rebuild_expiries_for_underlying(const QString& broker_id, const QString& underlying,
                                                  bool keep_selection) {
    if (broker_id == fno_free_source()) {
        // The free chain carries every expiry: they come with the fetch.
        if (is_visible_)
            start_free_fetch(false);
        return;
    }
    if (broker_id == QStringLiteral("databento")) {
        auto cached = OptionChainService::instance().list_expiries(QStringLiteral("databento"), underlying);
        if (!cached.isEmpty()) {
            QString prefer;
            if (keep_selection && cached.contains(last_expiry_))
                prefer = last_expiry_;
            else
                prefer = cached.first();
            header_->set_expiries(cached, prefer);
            if (is_visible_)
                resubscribe();
            return;
        }
        header_->set_expiries({});
        show_loading(tr("Loading expiries for %1 from Databento…").arg(underlying));
        QPointer<ChainSubTab> self = this;
        OptionChainService::instance().list_databento_expiries(
            underlying, [self, underlying, keep_selection](const QStringList& exps) {
                if (!self)
                    return;
                if (exps.isEmpty()) {
                    self->show_error(ChainSubTab::tr("No expiries for %1").arg(underlying),
                                     ChainSubTab::tr("Databento returned no expiries. Check the Databento API key "
                                                     "and that it includes OPRA access."),
                                     [self, underlying]() {
                                         if (self)
                                             self->rebuild_expiries_for_underlying(QStringLiteral("databento"),
                                                                                   underlying, true);
                                     });
                    return;
                }
                QString prefer;
                if (keep_selection && exps.contains(self->last_expiry_))
                    prefer = self->last_expiry_;
                else
                    prefer = exps.first();
                self->header_->set_expiries(exps, prefer);
                if (self->is_visible_)
                    self->resubscribe();
                else
                    self->show_loading(ChainSubTab::tr("Loading %1 %2 chain…").arg(underlying, prefer));
            });
        return;
    }

    LOG_DEBUG("FnoChain", QString("rebuild_expiries: calling list_expiries('%1','%2')").arg(broker_id, underlying));
    auto exps = OptionChainService::instance().list_expiries(broker_id, underlying);
    LOG_DEBUG("FnoChain", QString("list_expiries('%1','%2') → %3 items").arg(broker_id, underlying).arg(exps.size()));
    if (exps.isEmpty()) {
        header_->set_expiries({});
        show_state(fincept::ui::EmptyState::no_data_spec(
                       tr("No expiries for %1").arg(underlying),
                       tr("The %1 instrument list has no open expiries for %2.").arg(broker_id, underlying),
                       tr("RETRY")),
                   [this, broker_id, underlying]() { rebuild_expiries_for_underlying(broker_id, underlying, true); });
        return;
    }
    QString prefer;
    if (keep_selection && exps.contains(last_expiry_))
        prefer = last_expiry_;
    else
        prefer = exps.first(); // nearest expiry
    LOG_DEBUG("FnoChain", QString("Setting expiries for '%1/%2', prefer='%3', visible=%4")
                              .arg(broker_id, underlying, prefer)
                              .arg(is_visible_));
    header_->set_expiries(exps, prefer);
    if (is_visible_)
        resubscribe();
}

void ChainSubTab::resubscribe() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (header_->broker_id() == fno_free_source()) {
        // The free chain is fetched, not a hub topic: release any broker topic still
        // held, keep a fetch already in flight for this symbol (start_free_fetch).
        if (!active_topic_.isEmpty()) {
            hub.unsubscribe(this, active_topic_);
            hub.unsubscribe_errors(this, active_topic_);
            active_topic_.clear();
        }
        if (!tick_pattern_.isEmpty()) {
            hub.unsubscribe_pattern(this, tick_pattern_);
            tick_pattern_.clear();
        }
        start_free_fetch(false);
        return;
    }
    // Also drops the error-channel subscription: re-adding subscribe_errors() on every
    // resubscribe (each expiry / underlying change, every re-show) stacked one more
    // error handler per call on the same topic.
    drop_subscription();
    const QString topic = current_topic();
    if (topic.isEmpty()) {
        show_note(tr("Pick an underlying and an expiry"),
                  tr("The chain loads as soon as the broker's underlyings and expiries are listed."));
        return;
    }
    // Loading state: hide_empty_state() used to flip straight to the table, so
    // between subscribing and the first publish the user stared at an empty grid
    // with no indication anything was in flight. Keep the skeleton visible until
    // rows actually arrive. A snapshot already rendered for THIS topic (hidden and
    // re-shown tab) stays on screen; one left over from a different underlying /
    // expiry must not — it would sit under the new picker selection (and feed the
    // right-click order / Builder paths) until the first publish landed.
    if (table_ && holds_rows_for(topic)) {
        hide_empty_state();
    } else {
        if (table_ && table_->chain_model()->rowCount() > 0)
            table_->chain_model()->set_chain(OptionChain{});
        show_loading(tr("Loading %1 %2 chain…").arg(header_->underlying(), header_->expiry()));
    }
    active_topic_ = topic;
    // The sibling tabs (Builder / OI / Screener / Multi-Straddle) follow THIS series.
    OptionChainService::instance().set_focus_topic(topic);
    table_->set_orders_enabled(true);

    QPointer<ChainSubTab> self = this;
    hub.subscribe(this, topic, [self, topic](const QVariant& v) {
        if (!self)
            return;
        // A late delivery for a topic we've since left must not overwrite the new one.
        if (self->active_topic_ != topic)
            return;
        if (!v.canConvert<OptionChain>())
            return;
        OptionChain chain = v.value<OptionChain>();
        self->table_->chain_model()->set_chain(chain);
        self->header_->update_from_chain(chain);
        self->hide_empty_state();
    });
    hub.subscribe_errors(this, topic, [self, topic](const QString& err) {
        if (!self || self->active_topic_ != topic)
            return;
        // Last-known-good stays visible (P11): a transient refresh failure used to wipe
        // a perfectly usable chain and the ribbon. The header's freshness label turns
        // amber/red as the snapshot ages, which is the honest signal; only a topic that
        // has never loaded gets the error screen.
        if (self->holds_rows_for(topic)) {
            LOG_WARN("FnoChain", QString("Refresh failed for %1 — keeping last snapshot: %2").arg(topic, err));
            return;
        }
        self->show_error(ChainSubTab::tr("Chain unavailable"),
                         ChainSubTab::tr("The %1 chain for %2 %3 could not be loaded.")
                             .arg(self->header_->broker_id(), self->header_->underlying(), self->header_->expiry()),
                         [self]() {
                             if (self)
                                 self->resubscribe();
                         },
                         err);
    });

    // Live per-leg ticks (WS fast path): the producer fans each strike's quote
    // out on `option:tick:<broker>:<token>`. Patch just that cell in place via
    // OptionChainModel::update_leg_quote — no full-table rebuild per tick.
    const QString broker = header_->broker_id();
    if (!broker.isEmpty()) {
        tick_pattern_ = QStringLiteral("option:tick:") + broker + QStringLiteral(":*");
        hub.subscribe_pattern(this, tick_pattern_, [self](const QString& t, const QVariant& v) {
            if (!self || !v.canConvert<fincept::trading::BrokerQuote>())
                return;
            const qint64 token = t.section(QLatin1Char(':'), -1).toLongLong();
            if (token == 0)
                return;
            self->table_->chain_model()->update_leg_quote(token, v.value<fincept::trading::BrokerQuote>());
        });
    }

    // Cold-start: ask producer for an immediate refresh so the user doesn't
    // wait for the next scheduler tick. Honours per-producer rate limit.
    hub.request(topic, /*force*/ false);

    LOG_DEBUG("FnoChain", QString("Subscribed to %1").arg(topic));
}

bool ChainSubTab::holds_rows_for(const QString& topic) const {
    if (!table_ || table_->chain_model()->rowCount() <= 0)
        return false;
    const auto& held = table_->chain_model()->chain();
    return OptionChainService::instance().chain_topic(held.broker_id, held.underlying, held.expiry) == topic;
}

// ── Free delayed US chain ──────────────────────────────────────────────────

void ChainSubTab::rebuild_picker_for_free_source(bool keep_selection) {
    QStringList unders = fnochain_free_underlyings();
    QString prefer = keep_selection ? last_free_underlying_ : QString();
    if (prefer.isEmpty())
        prefer = unders.first(); // SPY
    if (!unders.contains(prefer))
        unders.prepend(prefer); // the user's own last symbol
    header_->set_underlyings(unders, prefer);
    last_free_underlying_ = prefer;
    sync_free_bar();
    if (!(free_chain_ &&
          FreeOptionChainService::key_of(free_chain_symbol_) == FreeOptionChainService::key_of(prefer)))
        header_->set_expiries({});
    if (is_visible_)
        start_free_fetch(false);
    else
        show_loading(tr("Loading the delayed %1 option chain (Cboe)…").arg(prefer));
}

void ChainSubTab::cancel_free_fetch() {
    if (free_req_ == 0)
        return;
    FreeOptionChainService::instance().cancel(free_req_);
    free_req_ = 0;
    free_req_symbol_.clear();
}

void ChainSubTab::start_free_fetch(bool force) {
    const QString symbol = header_->underlying().trimmed();
    if (symbol.isEmpty()) {
        rebuild_picker_for_free_source(true);
        return;
    }
    // One fetch per symbol at a time: the screen's and this page's show both ask.
    if (!force && free_req_ != 0 && FreeOptionChainService::key_of(free_req_symbol_) ==
                                        FreeOptionChainService::key_of(symbol))
        return;
    cancel_free_fetch();

    // The free chain is read by a Python script (deriv_market.py).
    const auto py = fincept::ui::check_requirements({fincept::ui::Requirement::python()});
    if (!py.ok) {
        show_requirement(py, [this]() { start_free_fetch(true); }, /*offer_free*/ false);
        return;
    }

    // The chain already held for this symbol (BACK TO after a symbol without options):
    // back on screen at once, labelled with its own quote time, while a newer copy loads.
    const bool held = free_chain_ && FreeOptionChainService::key_of(free_chain_symbol_) ==
                                         FreeOptionChainService::key_of(symbol);
    if (held && status_ != FnoChainStatus::Ready) {
        apply_free_expiries();
        render_free_expiry();
    }
    const bool on_screen = held && table_->chain_model()->rowCount() > 0 &&
                           table_->chain_model()->chain().broker_id == fno_free_source() &&
                           FreeOptionChainService::key_of(table_->chain_model()->chain().underlying) ==
                               FreeOptionChainService::key_of(symbol);
    if (!on_screen)
        show_loading(tr("Loading the delayed %1 option chain (Cboe)…").arg(symbol));
    // The sibling tabs follow the free series from now on (broker publishes are not it).
    OptionChainService::instance().set_focus_topic(
        OptionChainService::instance().chain_topic(fno_free_source(), symbol, header_->expiry()));

    QPointer<ChainSubTab> self = this;
    free_req_symbol_ = symbol;
    free_req_ = FreeOptionChainService::instance().fetch(
        symbol, QStringLiteral("auto"), force, this, [self, symbol](const FreeOptionChainService::Reply& r) {
            if (!self || self->header_->broker_id() != fno_free_source() ||
                FreeOptionChainService::key_of(self->header_->underlying()) != FreeOptionChainService::key_of(symbol))
                return; // the picker moved on
            using Stage = FreeOptionChainService::Stage;
            if (r.stage != Stage::Saved) { // Saved is followed by Fresh / Failed
                self->free_req_ = 0;
                self->free_req_symbol_.clear();
            }
            if (r.stage == Stage::Failed) {
                if (r.status == QLatin1String("cancelled"))
                    return;
                // This symbol's chain on screen (a saved / earlier copy): it stays,
                // labelled with its own quote time. Rows of ANOTHER symbol (the one
                // before a switch) do not count — that switch failed.
                const bool on_screen_now =
                    self->free_chain_ && self->table_->chain_model()->rowCount() > 0 &&
                    self->table_->chain_model()->chain().broker_id == fno_free_source() &&
                    FreeOptionChainService::key_of(self->table_->chain_model()->chain().underlying) ==
                        FreeOptionChainService::key_of(symbol) &&
                    self->status_ == FnoChainStatus::Ready;
                if (on_screen_now) {
                    LOG_WARN("FnoChain", fincept::logf("Delayed chain refresh failed — keeping the last copy",
                                              {{"symbol", symbol}, {"status", r.status}}));
                    return;
                }
                LOG_INFO("FnoChain", fincept::logf("Delayed chain unavailable for symbol",
                                          {{"symbol", symbol}, {"status", r.status}}));
                self->show_free_failure(symbol, r.status, r.message);
                return;
            }
            if (!r.chain)
                return;
            self->free_chain_ = r.chain;
            self->free_chain_symbol_ = symbol;
            self->last_free_underlying_ = fno_us_chain_symbol(symbol); // remembered (save_state)
            self->apply_free_expiries();
            self->render_free_expiry();
        });
}

void ChainSubTab::show_free_failure(const QString& symbol, const QString& status, const QString& message) {
    const bool unlisted = status == QLatin1String("no_options") || status == QLatin1String("bad_symbol");
    // The last symbol whose chain loaded: still held here (and in the service's memory),
    // so BACK TO puts it on screen at once.
    const QString back = free_chain_ && FreeOptionChainService::key_of(free_chain_symbol_) !=
                                            FreeOptionChainService::key_of(symbol)
                             ? fno_us_chain_symbol(free_chain_symbol_)
                             : QString();
    fincept::ui::EmptyState::Spec s;
    s.glyph = QStringLiteral("◇");
    s.glyph_tone = unlisted ? fincept::ui::EmptyState::Tone::Warning : fincept::ui::EmptyState::Tone::Danger;
    if (unlisted) {
        s.title = tr("No listed options for %1").arg(symbol);
        s.message = message.isEmpty() ? tr("The free delayed feed (Cboe, Yahoo Finance as the fallback) lists no "
                                           "options for %1. Options trade on US-listed stocks, ETFs and indices.")
                                            .arg(symbol)
                                      : message;
    } else {
        s.title = tr("No option chain for %1").arg(symbol);
        s.message = tr("The free delayed source (Cboe, with Yahoo Finance as the fallback) returned no chain for "
                       "%1 — it may have no listed options, or the feed did not answer. RETRY asks again.")
                        .arg(symbol);
        s.technical = message;
    }
    s.note = tr("Search any US stock, ETF or index in the strip above, or pick a quick pick.");
    s.note_tone = fincept::ui::EmptyState::Tone::Info;
    s.illustration = true;
    s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Table;

    std::function<void()> primary;
    std::function<void()> secondary;
    const std::function<void()> retry = [this]() { start_free_fetch(true); };
    if (!back.isEmpty()) {
        s.primary = tr("BACK TO %1").arg(back);
        s.primary_tooltip = tr("Show the %1 chain again").arg(back);
        primary = [this, back]() { choose_free_underlying(back); };
    } else if (FreeOptionChainService::key_of(symbol) != QLatin1String("SPY")) {
        s.primary = tr("OPEN SPY");
        s.primary_tooltip = tr("Show the SPY chain");
        primary = [this]() { choose_free_underlying(QStringLiteral("SPY")); };
    }
    if (!unlisted || s.primary.isEmpty()) {
        if (s.primary.isEmpty()) {
            s.primary = tr("RETRY");
            primary = retry;
        } else {
            s.secondary = tr("RETRY");
            secondary = retry;
        }
    }
    show_state(s, std::move(primary), std::move(secondary));
}

void ChainSubTab::choose_free_underlying(const QString& symbol) {
    const QString sym = fno_us_chain_symbol(symbol);
    if (sym.isEmpty() || !header_ || header_->broker_id() != fno_free_source())
        return;
    if (FreeOptionChainService::key_of(sym) == FreeOptionChainService::key_of(header_->underlying())) {
        sync_free_bar();
        // The same symbol again: a page without its chain (a failure) asks again.
        if (status_ == FnoChainStatus::Unavailable && is_visible_)
            start_free_fetch(true);
        return;
    }
    // The hidden header combo carries the selection: the quick picks plus this symbol.
    QStringList unders = fnochain_free_underlyings();
    if (!unders.contains(sym))
        unders.prepend(sym);
    header_->set_underlyings(unders, sym); // no signal: on_underlying_changed runs once, below
    on_underlying_changed(sym);
}

void ChainSubTab::build_free_bar(QWidget* parent) {
    free_bar_ = new QWidget(parent);
    free_bar_->setObjectName(QStringLiteral("fnoFreeBar"));
    // Selector-based sheet, set before the children exist.
    free_bar_->setStyleSheet(
        QString("#fnoFreeBar { background:%1; border-bottom:1px solid %2; }"
                "#fnoFreeBarLabel { color:%3; font-weight:700; letter-spacing:0.6px; background:transparent; "
                "font-size:%6px; }"
                "QPushButton#fnoFreeChip { background:transparent; color:%4; border:1px solid %2; padding:2px 8px; "
                "font-weight:700; font-size:%7px; }"
                "QPushButton#fnoFreeChip:hover { border-color:%5; }"
                "QPushButton#fnoFreeChip[active=\"true\"] { color:%5; border-color:%5; }")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM(), colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(),
                 colors::AMBER())
            .arg(fincept::ui::fonts::px(-6))
            .arg(fincept::ui::fonts::px(-5)));
    auto* lay = new QHBoxLayout(free_bar_);
    lay->setContentsMargins(12, 6, 12, 6);
    lay->setSpacing(8);

    free_bar_label_ = new QLabel(tr("UNDERLYING"), free_bar_);
    free_bar_label_->setObjectName(QStringLiteral("fnoFreeBarLabel"));
    lay->addWidget(free_bar_label_, 0, Qt::AlignVCenter);

    free_box_ = new fincept::ui::SymbolSearchBox(free_bar_);
    free_box_->set_compact(true);
    free_box_->set_clear_on_pick(false);
    free_box_->set_raw_text_allowed(true); // index roots the search may not know (XSP, DJX)
    fincept::services::ResolveOptions ro;
    ro.home_country = QStringLiteral("US");
    ro.ctx.allowed_classes = {fincept::core::instrument::AssetClass::Equity,
                              fincept::core::instrument::AssetClass::Etf,
                              fincept::core::instrument::AssetClass::Index};
    free_box_->set_options(ro);
    // Shared with the Derivatives tab: the same US option underlyings come back.
    free_box_->set_recent_key(QStringLiteral("derivatives.underlying"));
    free_box_->set_row_filter([](const fincept::core::instrument::Instrument& inst) -> SymbolSearchBox::RowCheck {
        // Listed options here are US options: a listing on another venue has none.
        const QString country = inst.country.trimmed().toUpper();
        const bool suffixed = !inst.symbol.startsWith(QLatin1Char('^')) && inst.symbol.contains(QLatin1Char('.'));
        if ((!country.isEmpty() && country != QLatin1String("US")) || suffixed)
            return {SymbolSearchBox::RowState::Disabled, ChainSubTab::tr("no US-listed options")};
        return SymbolSearchBox::RowState::Pickable;
    });
    free_box_->setMinimumWidth(QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')) * 14);
    free_box_->setAccessibleName(tr("Underlying symbol"));
    connect(free_box_, &SymbolSearchBox::instrument_picked, this,
            [this](const fincept::core::instrument::Instrument& inst) {
                choose_free_underlying(inst.symbol.isEmpty() ? inst.display : inst.symbol);
            });
    connect(free_box_, &SymbolSearchBox::text_committed_raw, this, [this](const QString& text) {
        // Not in the symbol index: tried as a ticker (the feed decides; an unknown one
        // gets the "No listed options" page with BACK TO the last chain).
        const QString sym = fno_us_chain_symbol(text);
        if (fno_is_us_ticker_text(sym))
            choose_free_underlying(sym);
    });
    lay->addWidget(free_box_, 0, Qt::AlignVCenter);
    lay->addSpacing(8);

    free_chips_label_ = new QLabel(tr("QUICK PICKS"), free_bar_);
    free_chips_label_->setObjectName(QStringLiteral("fnoFreeBarLabel"));
    lay->addWidget(free_chips_label_, 0, Qt::AlignVCenter);

    // Chips wrap onto more lines when the width runs out (1280 px, large fonts).
    auto* chips = new QWidget(free_bar_);
    chips->setAccessibleName(tr("Quick picks"));
    auto* flow = new fincept::ui::FlowLayout(chips);
    for (const QString& sym : fnochain_free_underlyings()) {
        auto* chip = new QPushButton(sym, chips);
        chip->setObjectName(QStringLiteral("fnoFreeChip"));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setProperty("fno_symbol", sym);
        chip->setProperty("active", false);
        chip->setAccessibleName(tr("Underlying %1").arg(sym));
        connect(chip, &QPushButton::clicked, this, [this, sym]() { choose_free_underlying(sym); });
        flow->addWidget(chip);
        free_chips_.append(chip);
    }
    lay->addWidget(chips, 1);
    retranslateUi(); // placeholder and tooltips
    free_bar_->setVisible(false);
}

void ChainSubTab::set_free_bar_visible(bool visible) {
    if (header_)
        header_->set_underlying_picker_visible(!visible);
    if (free_bar_ && free_bar_->isVisibleTo(this) != visible)
        free_bar_->setVisible(visible);
}

void ChainSubTab::sync_free_bar() {
    if (!free_bar_ || !header_ || header_->broker_id() != fno_free_source())
        return;
    const QString current = header_->underlying();
    const QString key = FreeOptionChainService::key_of(current);
    // The box shows the ticker the chain is listed under ("SPX" after picking ^GSPC).
    if (free_box_ && free_box_->text() != current)
        free_box_->set_text(current);
    for (QPushButton* chip : free_chips_) {
        const bool active = !key.isEmpty() && FreeOptionChainService::key_of(chip->property("fno_symbol").toString()) == key;
        if (chip->property("active").toBool() == active)
            continue;
        chip->setProperty("active", active);
        chip->style()->unpolish(chip);
        chip->style()->polish(chip);
    }
}

void ChainSubTab::apply_free_expiries() {
    if (!free_chain_)
        return;
    QStringList exps;
    exps.reserve(free_chain_->expiries.size());
    QString nearest; // the first expiry at least a day out (0-DTE IVs are noise)
    for (const auto& ex : free_chain_->expiries) {
        const QString iso = ex.date.toString(Qt::ISODate);
        exps.append(iso);
        if (nearest.isEmpty() && ex.days >= 1)
            nearest = iso;
    }
    if (exps.isEmpty())
        return;
    QString prefer = header_->expiry();
    if (!exps.contains(prefer))
        prefer = exps.contains(last_expiry_) ? last_expiry_ : (nearest.isEmpty() ? exps.first() : nearest);
    header_->set_expiries(exps, prefer);
    last_expiry_ = prefer;
}

void ChainSubTab::render_free_expiry() {
    if (!free_chain_)
        return;
    const QString exp = header_->expiry();
    int e = -1;
    for (int i = 0; i < free_chain_->expiries.size(); ++i) {
        if (free_chain_->expiries.at(i).date.toString(Qt::ISODate) == exp) {
            e = i;
            break;
        }
    }
    if (e < 0) {
        show_note(tr("Pick an expiry"), tr("The delayed chain lists its expiries in the Expiry picker."));
        return;
    }
    const OptionChain chain = fnochain_from_free(*free_chain_, e, header_->underlying());
    if (chain.rows.isEmpty()) {
        fincept::ui::EmptyState::Spec s = fincept::ui::EmptyState::no_data_spec(
            tr("No quotes for %1 %2").arg(header_->underlying(), exp),
            tr("The delayed feed listed this expiry without any quoted contracts. Pick another expiry."));
        show_state(s, {});
        return;
    }
    const QString topic = OptionChainService::instance().chain_topic(chain.broker_id, chain.underlying, chain.expiry);
    OptionChainService::instance().set_focus_topic(topic);
    active_topic_ = topic; // a re-show of the page keeps this chain (showEvent)
    table_->set_orders_enabled(false, tr("Delayed quotes (Cboe) are for analysis — orders need a broker's live "
                                         "chain (Equity Trading > Accounts)."));
    table_->chain_model()->set_chain(chain);
    header_->set_delayed_source(free_chain_->source_label.isEmpty() ? tr("Cboe delayed quotes")
                                                                    : free_chain_->source_label);
    header_->update_from_chain(chain);
    // The other sub-tabs get the chain before the "ready" status, so none of them
    // flashes its "needs a chain" page in between.
    free_shown_ = chain;
    emit free_chain_changed(free_shown_);
    hide_empty_state();
}

void ChainSubTab::clear_free_shown() {
    cancel_free_fetch();
    if (free_shown_.rows.isEmpty() && free_shown_.broker_id.isEmpty())
        return;
    free_shown_ = OptionChain{};
    if (table_ && table_->chain_model()->chain().broker_id == fno_free_source())
        table_->chain_model()->set_chain(OptionChain{});
    emit free_chain_changed(free_shown_);
}

// ── Body pages ─────────────────────────────────────────────────────────────

void ChainSubTab::set_status(FnoChainStatus status, const QString& reason) {
    if (status == status_ && reason == status_reason_)
        return;
    status_ = status;
    status_reason_ = reason;
    emit chain_status_changed(status_, status_reason_);
}

void ChainSubTab::show_loading(const QString& caption) {
    skeleton_->set_message(caption);
    body_stack_->setCurrentWidget(skeleton_);
    if (header_)
        header_->clear_ribbon();
    set_status(FnoChainStatus::Loading, caption);
}

void ChainSubTab::show_state(const fincept::ui::EmptyState::Spec& spec, std::function<void()> primary,
                             std::function<void()> secondary) {
    state_->apply(spec);
    state_primary_ = std::move(primary);
    state_secondary_ = std::move(secondary);
    body_stack_->setCurrentWidget(state_);
    if (header_)
        header_->clear_ribbon();
    set_status(FnoChainStatus::Unavailable, spec.title);
}

void ChainSubTab::show_note(const QString& title, const QString& message) {
    fincept::ui::EmptyState::Spec s;
    s.glyph = QStringLiteral("◇");
    s.glyph_tone = fincept::ui::EmptyState::Tone::Neutral;
    s.title = title;
    s.message = message;
    s.illustration = true;
    s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Table;
    show_state(s, {});
}

void ChainSubTab::show_error(const QString& title, const QString& message, std::function<void()> retry,
                             const QString& details) {
    show_state(fincept::ui::EmptyState::error_spec(message, title, tr("RETRY"), details), std::move(retry));
}

void ChainSubTab::show_requirement(const fincept::ui::RequirementStatus& status, std::function<void()> retry,
                                   bool offer_free) {
    fincept::ui::EmptyState::Spec s = fincept::ui::EmptyState::needs_setup_spec(status);
    std::function<void()> primary;
    if (status.retry || !status.link.valid()) {
        if (s.primary.isEmpty())
            s.primary = tr("RETRY");
        primary = std::move(retry);
    } else {
        const fincept::ui::SetupLink link = status.link;
        QPointer<ChainSubTab> self = this;
        primary = [self, link]() {
            if (self)
                fincept::ui::open_setup_link(link, self);
        };
    }
    std::function<void()> secondary;
    if (offer_free) {
        s.secondary = tr("USE FREE US CHAIN");
        s.secondary_tooltip = tr("Cboe delayed quotes for US options — no broker or key needed");
        secondary = [this]() { use_free_source(); };
    }
    show_state(s, std::move(primary), std::move(secondary));
}

void ChainSubTab::hide_empty_state() {
    if (table_ && body_stack_)
        body_stack_->setCurrentWidget(table_);
    set_status(FnoChainStatus::Ready, QString());
}

QString ChainSubTab::current_topic() const {
    const QString broker = header_->broker_id();
    const QString under = header_->underlying();
    const QString exp = header_->expiry();
    if (broker.isEmpty() || under.isEmpty() || exp.isEmpty())
        return {};
    return OptionChainService::instance().chain_topic(broker, under, exp);
}

void ChainSubTab::on_order_requested(qint64 token, double strike, bool is_call, bool is_buy) {
    using namespace fincept::trading;

    const QString broker = header_->broker_id();
    if (broker == fno_free_source()) {
        // Delayed US quotes are never an order source (no broker, no live price).
        QMessageBox::information(this, tr("Place Order"),
                                 tr("These are delayed US option quotes (Cboe) for analysis. Orders need a broker's "
                                    "live chain — connect one in Equity Trading > Accounts and pick it as the "
                                    "source."));
        return;
    }
    if (broker.isEmpty() || broker == QStringLiteral("databento")) {
        QMessageBox::warning(this, tr("Place Order"), tr("Connect a trading broker to place F&O orders."));
        return;
    }
    const QString account_id = find_account_for_broker(broker);
    if (account_id.isEmpty()) {
        QMessageBox::warning(this, tr("Place Order"),
                             tr("No connected %1 account. Connect one in Equity Trading.").arg(broker));
        return;
    }

    // Resolve the leg's contract details from the live chain snapshot.
    const auto& chain = table_->chain_model()->chain();
    QString sym;
    int lot = 0;
    double ltp = 0;
    double resolved_strike = 0;
    for (const auto& r : chain.rows) {
        if (is_call && r.ce_token == token) {
            sym = r.ce_symbol;
            lot = r.lot_size;
            ltp = r.ce_quote.ltp;
            resolved_strike = r.strike;
            break;
        }
        if (!is_call && r.pe_token == token) {
            sym = r.pe_symbol;
            lot = r.lot_size;
            ltp = r.pe_quote.ltp;
            resolved_strike = r.strike;
            break;
        }
    }
    if (sym.isEmpty()) {
        QMessageBox::warning(this, tr("Place Order"), tr("Could not resolve the contract for this strike."));
        return;
    }
    // Cross-check: the strike the user right-clicked must be the strike the
    // token resolves to. They can only disagree if the chain republished with a
    // different ladder between the click and this handler, or if the provider
    // reused a token — either way, placing the order would trade a strike the
    // user never selected.
    if (std::abs(resolved_strike - strike) > 1e-6) {
        QMessageBox::warning(this, tr("Place Order"),
                             tr("The chain changed while the order was being prepared "
                                "(you picked %1, the contract now resolves to %2). Nothing was sent — "
                                "please pick the strike again.")
                                 .arg(fincept::services::options::format_strike(strike),
                                      fincept::services::options::format_strike(resolved_strike)));
        return;
    }

    // Broker-native symbol/exchange. Fyers chain symbols are already native
    // ("NSE:...CE") and pass straight through place_order's to_fyers_sym; other
    // brokers map via InstrumentService (normalized symbol → brsymbol/brexchange).
    QString order_symbol = sym;
    QString order_exchange = QStringLiteral("NFO");
    if (auto inst = InstrumentService::instance().find(sym, QStringLiteral("NFO"), broker)) {
        if (!inst->brsymbol.isEmpty())
            order_symbol = inst->brsymbol;
        if (!inst->brexchange.isEmpty())
            order_exchange = inst->brexchange;
        if (inst->lot_size > 0)
            lot = inst->lot_size;
    }
    // Fyers' options-chain-v3 omits lot size (chain row lot is 0) and its
    // "NSE:...CE" symbol won't match the master's normalized key — so backfill
    // the lot from the underlying's NFO instruments (all share one lot size).
    if (lot <= 0) {
        const auto insts = InstrumentService::instance().find_options_for_underlying(
            broker, header_->underlying(), header_->expiry(), QStringLiteral("NFO"));
        for (const auto& in : insts) {
            if (in.lot_size > 0) {
                lot = in.lot_size;
                break;
            }
        }
    }
    if (lot <= 0) {
        QMessageBox::warning(this, tr("Place Order"), tr("Could not resolve the lot size for %1.").arg(sym));
        return;
    }

    // Canonical bare symbol for the quote/position layer (matches the equity
    // convention + the WS-published symbol): strip the "NSE:" exchange prefix and
    // any segment suffix. Paper positions are stored under this so they mark to
    // market against the live broker quote feed (broker:<id>:<acct>:quote:<bare>).
    QString bare_sym = order_symbol;
    if (const int c = bare_sym.indexOf(QLatin1Char(':')); c >= 0)
        bare_sym = bare_sym.mid(c + 1);
    for (const auto& suf :
         {QStringLiteral("-CE"), QStringLiteral("-PE"), QStringLiteral("-FUT"), QStringLiteral("-EQ")}) {
        if (bare_sym.endsWith(suf)) {
            bare_sym.chop(int(suf.size()));
            break;
        }
    }

    const BrokerAccount acct = AccountManager::instance().get_account(account_id);
    const bool is_paper = (acct.trading_mode != QStringLiteral("live"));
    const QString acct_name = acct.display_name.isEmpty() ? account_id : acct.display_name;

    // ── Order ticket (editable: type, lots, price, product) ──────────────────
    QDialog dlg(this);
    dlg.setWindowTitle(is_buy ? tr("Buy %1").arg(sym) : tr("Sell %1").arg(sym));
    auto* form = new QFormLayout(&dlg);

    auto* hdr = new QLabel(QStringLiteral("%1  %2").arg(is_buy ? tr("BUY") : tr("SELL"), sym), &dlg);
    hdr->setStyleSheet(QString("font-weight:700;font-size:" + fincept::ui::fonts::px(-1) + "px;color:%1;")
                           .arg(is_buy ? colors::POSITIVE() : colors::NEGATIVE()));
    form->addRow(hdr);

    auto* type_combo = new QComboBox(&dlg);
    type_combo->addItems({tr("Market"), tr("Limit")});
    type_combo->setAccessibleName(tr("Order type"));
    form->addRow(tr("Type"), type_combo);

    auto* lots_spin = new QSpinBox(&dlg);
    // 10 000 lots is already far past any retail F&O limit; the old 100 000
    // ceiling let one keypress build a basket two orders of magnitude larger
    // than intended.
    lots_spin->setRange(1, 10000);
    lots_spin->setValue(1);
    lots_spin->setAccessibleName(tr("Quantity in lots"));
    form->addRow(tr("Qty (lots)"), lots_spin);

    auto* qty_label = new QLabel(&dlg);
    qty_label->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(colors::TEXT_SECONDARY()));
    auto update_qty = [qty_label, lots_spin, lot]() {
        qty_label->setText(ChainSubTab::tr("= %1 qty  (lot size %2)")
                               .arg(fincept::ui::formatting::quantity(double(lots_spin->value()) * double(lot)),
                                    fincept::ui::formatting::quantity(lot)));
    };
    update_qty();
    connect(lots_spin, qOverload<int>(&QSpinBox::valueChanged), &dlg, [update_qty](int) { update_qty(); });
    form->addRow(QString(), qty_label);

    auto* px_spin = new QDoubleSpinBox(&dlg);
    px_spin->setRange(0.05, 1.0e8);
    px_spin->setDecimals(2);
    px_spin->setValue(ltp > 0 ? ltp : 0.05);
    px_spin->setAccessibleName(tr("Limit price per share"));
    // The ticket names its currency: the chain's (an Indian broker's NSE chain: rupees).
    const QString ticket_ccy = fno_chain_ccy(broker);
    fincept::ui::formatting::bind_currency_affix(px_spin, ticket_ccy);
    form->addRow(tr("Limit price"), px_spin);

    auto* product_combo = new QComboBox(&dlg);
    product_combo->addItems({tr("NRML (positional)"), tr("MIS (intraday)")});
    product_combo->setAccessibleName(tr("Product type"));
    form->addRow(tr("Product"), product_combo);

    // Market needs no price — hide the limit row unless Limit is selected.
    auto sync_type = [form, px_spin](int idx) { form->setRowVisible(px_spin, idx == 1); };
    connect(type_combo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, sync_type);
    sync_type(type_combo->currentIndex());

    auto* ctx = new QLabel(QStringLiteral("%1  ·  %2  ·  [%3]")
                               .arg(acct_name)
                               .arg(tr("LTP %1 at ticket open")
                                        .arg(fincept::ui::formatting::price_with_symbol(ltp, fno_price_fmt(ticket_ccy))))
                               .arg(is_paper ? tr("PAPER") : tr("LIVE")),
                           &dlg);
    // A live F&O order is real leveraged money — the mode must not read as
    // secondary text the way the paper case can.
    ctx->setStyleSheet(is_paper ? QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(colors::TEXT_SECONDARY())
                                : QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;").arg(colors::NEGATIVE()));
    form->addRow(ctx);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton* ok = buttons->addButton(is_buy ? tr("Place Buy") : tr("Place Sell"), QDialogButtonBox::AcceptRole);
    ok->setAccessibleName(is_buy ? tr("Place buy order") : tr("Place sell order"));
    ok->setProperty("fincept_agent_risk", "order");
    // Live orders require a deliberate click; paper keeps Return as a shortcut.
    if (is_paper) {
        ok->setDefault(true);
    } else if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setDefault(true);
        ok->setAutoDefault(false);
    }
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    // Keyboard traversal through the ticket in reading order.
    dlg.setTabOrder(type_combo, lots_spin);
    dlg.setTabOrder(lots_spin, px_spin);
    dlg.setTabOrder(px_spin, product_combo);
    dlg.setTabOrder(product_combo, buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // Re-read the premium from the live chain at SUBMIT time. `ltp` was captured
    // when the ticket opened; an option quote goes stale in seconds, and for a
    // paper market order that value *is* the fill price. Filling a ticket the
    // user left open for a minute at the old premium books a fictitious P&L.
    double fill_ltp = ltp;
    {
        const auto& live = table_->chain_model()->chain();
        for (const auto& r : live.rows) {
            if (is_call && r.ce_token == token) {
                if (r.ce_quote.ltp > 0)
                    fill_ltp = r.ce_quote.ltp;
                break;
            }
            if (!is_call && r.pe_token == token) {
                if (r.pe_quote.ltp > 0)
                    fill_ltp = r.pe_quote.ltp;
                break;
            }
        }
    }

    const bool is_market = (type_combo->currentIndex() == 0);
    UnifiedOrder order;
    // Live placement needs the broker-native symbol; paper stores the bare symbol
    // so the position marks to market against the live quote feed.
    order.symbol = is_paper ? bare_sym : order_symbol;
    order.exchange = order_exchange;
    order.side = is_buy ? OrderSide::Buy : OrderSide::Sell;
    order.order_type = is_market ? OrderType::Market : OrderType::Limit;
    order.quantity = double(lots_spin->value()) * double(lot);
    // fill_ltp lets paper fill a market order at the current premium; live ignores it.
    order.price = is_market ? fill_ltp : px_spin->value();
    order.product_type = (product_combo->currentIndex() == 1) ? ProductType::Intraday : ProductType::Margin;
    order.validity = QStringLiteral("DAY");
    order.instrument_token = QString::number(token);
    // Who placed it, captured after the ticket dialog on the GUI thread (the live order
    // runs on a worker): the agent's while an agent placed it, else the ticket.
    order.source = fincept::trading::capture_order_source(QStringLiteral("ticket"));
    LOG_AUDIT("Orders", "Order submitted",
              {{"venue", broker},
               {"account", fincept::log_mask(account_id)},
               {"mode", is_paper ? "paper" : "live"},
               {"symbol", order.symbol},
               {"side", is_buy ? "buy" : "sell"},
               {"type", is_market ? "market" : "limit"},
               {"qty", order.quantity},
               {"price", order.price},
               {"screen", "fno_chain"},
               {"source", order.source}});

    // The outcome of every placement goes to the audit log too (paper and live).
    const QString audit_mode = is_paper ? QStringLiteral("paper") : QStringLiteral("live");
    const auto audit_result = [broker, audit_mode, sym = order.symbol,
                               src = order.source](const UnifiedOrderResponse& result) {
        if (result.success)
            LOG_AUDIT("Orders", "Order accepted",
                      {{"venue", broker}, {"mode", audit_mode}, {"symbol", sym}, {"order_id", result.order_id},
                       {"screen", "fno_chain"}, {"source", src}});
        else
            LOG_AUDIT("Orders", "Order rejected",
                      {{"venue", broker}, {"mode", audit_mode}, {"symbol", sym}, {"reason", result.message},
                       {"screen", "fno_chain"}, {"source", src}});
    };
    auto show_result = [this, audit_result](const UnifiedOrderResponse& result) {
        audit_result(result);
        if (result.success)
            QMessageBox::information(this, tr("Place Order"), tr("Order placed: %1").arg(result.order_id));
        else
            QMessageBox::warning(this, tr("Place Order"), tr("Order failed: %1").arg(result.message));
    };

    if (is_paper) {
        // Paper placement is a fast local DB write + immediate fill — main thread.
        show_result(UnifiedTrading::instance().place_order(account_id, order));
        return;
    }

    // Semi-Auto ("require approval") accounts: queue to the Action Center like
    // every other live-order path (equity screen, basket/broadcast dialogs, algo
    // engine, MCP / workflow tools) instead of sending the order directly.
    if (ActionCenter::instance().should_queue(account_id, QStringLiteral("placeorder"))) {
        const QString pending_id = ActionCenter::instance().queue_order(account_id, QStringLiteral("placeorder"),
                                                                        ActionCenter::serialize_unified_order(order));
        if (pending_id.isEmpty())
            QMessageBox::warning(this, tr("Place Order"), tr("Could not queue the order for approval — nothing was sent."));
        else
            QMessageBox::information(this, tr("Place Order"),
                                     tr("Order queued for approval in the Action Center (not sent yet)."));
        return;
    }

    // Live placement hits the broker REST synchronously — run off the UI thread.
    QPointer<ChainSubTab> self = this;
    (void)QtConcurrent::run(&order_thread_pool(), [self, account_id, order, audit_result]() {
        auto result = UnifiedTrading::instance().place_order(account_id, order);
        audit_result(result); // also when the screen is gone
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                if (result.success)
                    QMessageBox::information(self, ChainSubTab::tr("Place Order"),
                                             ChainSubTab::tr("Order placed: %1").arg(result.order_id));
                else
                    QMessageBox::warning(self, ChainSubTab::tr("Place Order"),
                                         ChainSubTab::tr("Order failed: %1").arg(result.message));
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::screens::fno
