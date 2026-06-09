#include "screens/fno/ChainSubTab.h"

#include "core/logging/Logger.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/FnoHeaderBar.h"
#include "screens/fno/OptionChainModel.h"
#include "screens/fno/OptionChainTable.h"
#include "services/databento/DatabentoService.h"
#include "services/options/OptionChainService.h"
#include "trading/AccountManager.h"
#include "trading/UnifiedTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QSet>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHideEvent>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens::fno {

using namespace fincept::ui;
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

ChainSubTab::ChainSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoChainTab");
    setStyleSheet(QStringLiteral("#fnoChainTab { background:%1; } "
                                 "#fnoEmptyLabel { color:%2; font-size:12px; background:transparent; }")
                      .arg(colors::BG_BASE(), colors::TEXT_SECONDARY()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    header_ = new FnoHeaderBar(this);
    root->addWidget(header_);

    // Body uses a QStackedLayout so we can flip between the table and an
    // empty/error label without rebuilding either widget.
    auto* body_wrap = new QWidget(this);
    auto* body_stack = new QStackedLayout(body_wrap);
    body_stack->setContentsMargins(0, 0, 0, 0);
    body_stack->setStackingMode(QStackedLayout::StackOne);

    table_ = new OptionChainTable(body_wrap);
    body_stack->addWidget(table_);

    empty_label_ = new QLabel(tr("Connect a broker to load F&O data."), body_wrap);
    empty_label_->setObjectName("fnoEmptyLabel");
    empty_label_->setAlignment(Qt::AlignCenter);
    body_stack->addWidget(empty_label_);
    body_stack->setCurrentWidget(empty_label_);

    root->addWidget(body_wrap, 1);

    // ── Wire signals ──────────────────────────────────────────────────────
    connect(header_, &FnoHeaderBar::broker_changed, this, &ChainSubTab::on_broker_changed);
    connect(header_, &FnoHeaderBar::underlying_changed, this, &ChainSubTab::on_underlying_changed);
    connect(header_, &FnoHeaderBar::expiry_changed, this, &ChainSubTab::on_expiry_changed);
    connect(header_, &FnoHeaderBar::refresh_requested, this, &ChainSubTab::on_refresh_clicked);
    connect(table_, &OptionChainTable::order_requested, this, &ChainSubTab::on_order_requested);

    connect(&InstrumentService::instance(), &InstrumentService::refresh_done,
            this, [this](const QString& broker_id) {
                if (broker_id == header_->broker_id())
                    rebuild_picker_for_broker(broker_id, true);
            });
    connect(&InstrumentService::instance(), &InstrumentService::refresh_failed,
            this, [this](const QString& broker_id, const QString& error) {
                if (broker_id == header_->broker_id())
                    show_empty_state(tr("Failed to load %1 instruments: %2").arg(broker_id, error));
            });

    // Populate broker picker. Prefer a live/connected trading broker; Databento
    // is listed (last) but only auto-selected when no broker account exists —
    // we never auto-open Databento while a broker is connected.
    QTimer::singleShot(0, this, [this]() {
        static const QString kDatabento = QStringLiteral("databento");
        QStringList broker_ids;
        QString connected_broker;  // first account in ConnectionState::Connected
        QString active_broker;     // first active broker account (fallback)
        for (const auto& acc : AccountManager::instance().active_accounts()) {
            if (!broker_ids.contains(acc.broker_id))
                broker_ids.append(acc.broker_id);
            if (active_broker.isEmpty())
                active_broker = acc.broker_id;
            if (connected_broker.isEmpty() &&
                AccountManager::instance().connection_state(acc.account_id) ==
                    fincept::trading::ConnectionState::Connected)
                connected_broker = acc.broker_id;
        }
        broker_ids.append(kDatabento);  // always available, listed last

        // Auto-select: sticky real-broker choice → connected → any active → Databento.
        QString prefer;
        if (!last_broker_.isEmpty() && last_broker_ != kDatabento && broker_ids.contains(last_broker_))
            prefer = last_broker_;
        if (prefer.isEmpty())
            prefer = connected_broker;
        if (prefer.isEmpty())
            prefer = active_broker;
        if (prefer.isEmpty())
            prefer = kDatabento;

        header_->set_brokers(broker_ids, prefer);
        rebuild_picker_for_broker(prefer, /*keep_selection*/ true);
    });
}

ChainSubTab::~ChainSubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

QVariantMap ChainSubTab::save_state() const {
    QVariantMap m;
    m["broker"] = header_->broker_id();
    m["underlying"] = header_->underlying();
    m["expiry"] = header_->expiry();
    return m;
}

void ChainSubTab::restore_state(const QVariantMap& state) {
    last_broker_ = state.value("broker").toString();
    last_underlying_ = state.value("underlying").toString();
    last_expiry_ = state.value("expiry").toString();
    // Picker repopulation in the deferred lambda above will pick these up.
}

void ChainSubTab::request_underlying(const QString& underlying) {
    if (!header_ || underlying.isEmpty())
        return;
    last_underlying_ = underlying;  // sticky across picker rebuilds
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
    resubscribe();
}

void ChainSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    is_visible_ = false;
    auto& hub = fincept::datahub::DataHub::instance();
    if (!active_topic_.isEmpty()) {
        hub.unsubscribe(this, active_topic_);
        active_topic_.clear();
    }
    if (!tick_pattern_.isEmpty()) {
        hub.unsubscribe_pattern(this, tick_pattern_);
        tick_pattern_.clear();
    }
}

void ChainSubTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ChainSubTab::retranslateUi() {
    // The empty label carries dynamic, state-dependent status text (loading,
    // error, etc.) that refreshes on the next state change — like GovData's
    // status label, those are not re-applied here. Only the default idle
    // "connect a broker" message (shown when no broker is selected) is safe
    // to re-translate in place. The FnoHeaderBar child retranslates itself.
    if (empty_label_ && header_ && header_->broker_id().isEmpty())
        empty_label_->setText(tr("Connect a broker to load F&O data."));
}

void ChainSubTab::on_broker_changed(const QString& broker_id) {
    LOG_INFO("FnoChain", QString("on_broker_changed: '%1'").arg(broker_id));
    last_broker_ = broker_id;
    rebuild_picker_for_broker(broker_id, /*keep_selection*/ true);
}

void ChainSubTab::on_underlying_changed(const QString& underlying) {
    last_underlying_ = underlying;
    rebuild_expiries_for_underlying(header_->broker_id(), underlying, /*keep_selection*/ true);

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
        QStringLiteral("NIFTY"), QStringLiteral("BANKNIFTY"),
        QStringLiteral("FINNIFTY"), QStringLiteral("MIDCPNIFTY"),
    };
    fincept::SymbolRef ref;
    ref.symbol = underlying;
    if (header_->broker_id() == QStringLiteral("databento")) {
        ref.asset_class = QStringLiteral("equity");
        ref.exchange = QStringLiteral("US");
    } else {
        ref.asset_class = kIndexNames.contains(underlying) ? QStringLiteral("index")
                                                           : QStringLiteral("equity");
        ref.exchange = QStringLiteral("NSE");
    }
    fincept::SymbolContext::instance().set_group_symbol(link_group, ref, this);
}

void ChainSubTab::on_expiry_changed(const QString& expiry) {
    last_expiry_ = expiry;
    if (is_visible_)
        resubscribe();
}

void ChainSubTab::on_refresh_clicked() {
    const QString topic = current_topic();
    if (topic.isEmpty())
        return;
    fincept::datahub::DataHub::instance().request(topic, /*force*/ true);
}

void ChainSubTab::rebuild_picker_for_broker(const QString& broker_id, bool keep_selection) {
    LOG_INFO("FnoChain", QString("rebuild_picker_for_broker: broker='%1' keep=%2").arg(broker_id).arg(keep_selection));
    if (broker_id.isEmpty()) {
        LOG_WARN("FnoChain", "rebuild_picker_for_broker: empty broker_id — showing 'Select a broker'");
        show_empty_state(tr("Select a broker."));
        return;
    }
    if (broker_id == QStringLiteral("databento")) {
        if (!fincept::DatabentoService::instance().has_api_key()) {
            show_empty_state(tr("Databento selected — enter your API key in Settings > Credentials to load US options data."));
            header_->set_underlyings({});
            header_->set_expiries({});
            return;
        }
        auto unders = OptionChainService::instance().list_underlyings(QStringLiteral("databento"));
        if (unders.isEmpty()) {
            show_empty_state(tr("Databento configuration error."));
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
    LOG_INFO("FnoChain", QString("rebuild_picker_for_broker: is_loaded('%1')=%2").arg(broker_id).arg(loaded));
    if (!loaded) {
        show_empty_state(tr("Loading %1 instruments...").arg(broker_id));
        header_->set_underlyings({});
        header_->set_expiries({});

        QPointer<ChainSubTab> self = this;
        InstrumentService::instance().load_from_db_async(broker_id, [self, broker_id](int count) {
            if (!self) {
                LOG_WARN("FnoChain", QString("load_from_db_async callback: widget destroyed, broker='%1'").arg(broker_id));
                return;
            }
            LOG_INFO("FnoChain", QString("load_from_db_async callback: broker='%1' count=%2").arg(broker_id).arg(count));
            if (count > 0) {
                self->rebuild_picker_for_broker(broker_id, true);
                return;
            }
            QString account_id = find_account_for_broker(broker_id);
            LOG_INFO("FnoChain", QString("find_account_for_broker('%1') → '%2'").arg(broker_id, account_id));
            if (account_id.isEmpty()) {
                self->show_empty_state(ChainSubTab::tr("No account configured for %1. Connect one in Equity Trading.").arg(broker_id));
                return;
            }
            self->show_empty_state(ChainSubTab::tr("Downloading %1 instruments from broker...").arg(broker_id));
            auto creds = trading::AccountManager::instance().load_credentials(account_id);
            InstrumentService::instance().refresh(broker_id, creds);
        });
        return;
    }
    auto unders = OptionChainService::instance().list_underlyings(broker_id);
    LOG_INFO("FnoChain", QString("list_underlyings('%1') → %2 items").arg(broker_id).arg(unders.size()));
    if (unders.isEmpty()) {
        show_empty_state(tr("No NFO instruments cached for %1.").arg(broker_id));
        header_->set_underlyings({});
        header_->set_expiries({});
        return;
    }
    QString prefer;
    if (keep_selection && unders.contains(last_underlying_))
        prefer = last_underlying_;
    else
        prefer = unders.contains("NIFTY") ? "NIFTY" : unders.first();
    LOG_INFO("FnoChain", QString("Setting underlyings for '%1', prefer='%2' (%3 items)")
                             .arg(broker_id, prefer).arg(unders.size()));
    header_->set_underlyings(unders, prefer);
    LOG_INFO("FnoChain", "set_underlyings done, calling rebuild_expiries_for_underlying");
    rebuild_expiries_for_underlying(broker_id, prefer, keep_selection);
    LOG_INFO("FnoChain", "rebuild_expiries_for_underlying done");
}

void ChainSubTab::rebuild_expiries_for_underlying(const QString& broker_id, const QString& underlying,
                                                  bool keep_selection) {
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
        show_empty_state(tr("Loading expiries for %1 from Databento...").arg(underlying));
        QPointer<ChainSubTab> self = this;
        OptionChainService::instance().list_databento_expiries(
            underlying,
            [self, underlying, keep_selection](const QStringList& exps) {
                if (!self) return;
                if (exps.isEmpty()) {
                    self->show_empty_state(
                        ChainSubTab::tr("No expiries found for %1. Check Databento API key and OPRA access.").arg(underlying));
                    return;
                }
                QString prefer;
                if (keep_selection && exps.contains(self->last_expiry_))
                    prefer = self->last_expiry_;
                else
                    prefer = exps.first();
                self->header_->set_expiries(exps, prefer);
                self->hide_empty_state();
                if (self->is_visible_)
                    self->resubscribe();
            });
        return;
    }

    LOG_INFO("FnoChain", QString("rebuild_expiries: calling list_expiries('%1','%2')").arg(broker_id, underlying));
    auto exps = OptionChainService::instance().list_expiries(broker_id, underlying);
    LOG_INFO("FnoChain", QString("list_expiries('%1','%2') → %3 items").arg(broker_id, underlying).arg(exps.size()));
    if (exps.isEmpty()) {
        show_empty_state(tr("No expiries cached for %1.").arg(underlying));
        header_->set_expiries({});
        return;
    }
    QString prefer;
    if (keep_selection && exps.contains(last_expiry_))
        prefer = last_expiry_;
    else
        prefer = exps.first();  // nearest expiry
    LOG_INFO("FnoChain", QString("Setting expiries for '%1/%2', prefer='%3', visible=%4")
                             .arg(broker_id, underlying, prefer).arg(is_visible_));
    header_->set_expiries(exps, prefer);
    if (is_visible_)
        resubscribe();
}

void ChainSubTab::resubscribe() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!active_topic_.isEmpty()) {
        hub.unsubscribe(this, active_topic_);
        active_topic_.clear();
    }
    if (!tick_pattern_.isEmpty()) {
        hub.unsubscribe_pattern(this, tick_pattern_);
        tick_pattern_.clear();
    }
    const QString topic = current_topic();
    if (topic.isEmpty()) {
        show_empty_state(tr("Pick a broker, underlying, and expiry."));
        return;
    }
    hide_empty_state();
    active_topic_ = topic;

    QPointer<ChainSubTab> self = this;
    hub.subscribe(this, topic, [self](const QVariant& v) {
        if (!self)
            return;
        if (!v.canConvert<OptionChain>())
            return;
        OptionChain chain = v.value<OptionChain>();
        self->table_->chain_model()->set_chain(chain);
        self->header_->update_from_chain(chain);
        self->hide_empty_state();
    });
    hub.subscribe_errors(this, topic, [self](const QString& err) {
        if (!self)
            return;
        self->show_empty_state(ChainSubTab::tr("Chain unavailable: %1").arg(err));
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

    LOG_INFO("FnoChain", QString("Subscribed to %1").arg(topic));
}

void ChainSubTab::show_empty_state(const QString& message) {
    if (empty_label_) {
        empty_label_->setText(message);
        if (auto* lay = qobject_cast<QStackedLayout*>(empty_label_->parentWidget()->layout()))
            lay->setCurrentWidget(empty_label_);
    }
    if (header_)
        header_->clear_ribbon();
}

void ChainSubTab::hide_empty_state() {
    if (table_ && empty_label_)
        if (auto* lay = qobject_cast<QStackedLayout*>(empty_label_->parentWidget()->layout()))
            lay->setCurrentWidget(table_);
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
    Q_UNUSED(strike);

    const QString broker = header_->broker_id();
    if (broker.isEmpty() || broker == QStringLiteral("databento")) {
        QMessageBox::warning(this, tr("Place Order"),
                             tr("Connect a trading broker to place F&O orders."));
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
    for (const auto& r : chain.rows) {
        if (is_call && r.ce_token == token) {
            sym = r.ce_symbol; lot = r.lot_size; ltp = r.ce_quote.ltp; break;
        }
        if (!is_call && r.pe_token == token) {
            sym = r.pe_symbol; lot = r.lot_size; ltp = r.pe_quote.ltp; break;
        }
    }
    if (sym.isEmpty()) {
        QMessageBox::warning(this, tr("Place Order"), tr("Could not resolve the contract for this strike."));
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
    for (const auto& suf : {QStringLiteral("-CE"), QStringLiteral("-PE"), QStringLiteral("-FUT"),
                            QStringLiteral("-EQ")}) {
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
    hdr->setStyleSheet(QStringLiteral("font-weight:700;font-size:13px;color:%1;")
                           .arg(is_buy ? colors::POSITIVE() : colors::NEGATIVE()));
    form->addRow(hdr);

    auto* type_combo = new QComboBox(&dlg);
    type_combo->addItems({tr("Market"), tr("Limit")});
    form->addRow(tr("Type"), type_combo);

    auto* lots_spin = new QSpinBox(&dlg);
    lots_spin->setRange(1, 100000);
    lots_spin->setValue(1);
    form->addRow(tr("Qty (lots)"), lots_spin);

    auto* qty_label = new QLabel(&dlg);
    qty_label->setStyleSheet(QStringLiteral("color:%1;font-size:11px;").arg(colors::TEXT_SECONDARY()));
    auto update_qty = [qty_label, lots_spin, lot]() {
        qty_label->setText(ChainSubTab::tr("= %1 qty  (lot size %2)").arg(lots_spin->value() * lot).arg(lot));
    };
    update_qty();
    connect(lots_spin, qOverload<int>(&QSpinBox::valueChanged), &dlg, [update_qty](int) { update_qty(); });
    form->addRow(QString(), qty_label);

    auto* px_spin = new QDoubleSpinBox(&dlg);
    px_spin->setRange(0.05, 1.0e8);
    px_spin->setDecimals(2);
    px_spin->setValue(ltp > 0 ? ltp : 0.05);
    form->addRow(tr("Limit price"), px_spin);

    auto* product_combo = new QComboBox(&dlg);
    product_combo->addItems({tr("NRML (positional)"), tr("MIS (intraday)")});
    form->addRow(tr("Product"), product_combo);

    // Market needs no price — hide the limit row unless Limit is selected.
    auto sync_type = [form, px_spin](int idx) { form->setRowVisible(px_spin, idx == 1); };
    connect(type_combo, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, sync_type);
    sync_type(type_combo->currentIndex());

    auto* ctx = new QLabel(
        QStringLiteral("%1  ·  LTP %2  ·  [%3]").arg(acct_name).arg(ltp, 0, 'f', 2).arg(is_paper ? tr("PAPER") : tr("LIVE")),
        &dlg);
    ctx->setStyleSheet(QStringLiteral("color:%1;font-size:11px;").arg(colors::TEXT_SECONDARY()));
    form->addRow(ctx);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton* ok = buttons->addButton(is_buy ? tr("Place Buy") : tr("Place Sell"), QDialogButtonBox::AcceptRole);
    ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const bool is_market = (type_combo->currentIndex() == 0);
    UnifiedOrder order;
    // Live placement needs the broker-native symbol; paper stores the bare symbol
    // so the position marks to market against the live quote feed.
    order.symbol = is_paper ? bare_sym : order_symbol;
    order.exchange = order_exchange;
    order.side = is_buy ? OrderSide::Buy : OrderSide::Sell;
    order.order_type = is_market ? OrderType::Market : OrderType::Limit;
    order.quantity = double(lots_spin->value()) * double(lot);
    order.price = is_market ? ltp : px_spin->value();  // ltp lets paper fill a market order; live ignores it
    order.product_type = (product_combo->currentIndex() == 1) ? ProductType::Intraday : ProductType::Margin;
    order.validity = QStringLiteral("DAY");
    order.instrument_token = QString::number(token);
    LOG_INFO("posdbg", QString("FNO order placed: sym='%1' (bare='%2' native='%3') qty=%4 %5 acct=%6 mode=%7")
                           .arg(order.symbol, bare_sym, order_symbol)
                           .arg(order.quantity)
                           .arg(is_buy ? "BUY" : "SELL", account_id, is_paper ? "paper" : "live"));

    auto show_result = [this](const UnifiedOrderResponse& result) {
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

    // Live placement hits the broker REST synchronously — run off the UI thread.
    QPointer<ChainSubTab> self = this;
    (void)QtConcurrent::run([self, account_id, order]() {
        auto result = UnifiedTrading::instance().place_order(account_id, order);
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
