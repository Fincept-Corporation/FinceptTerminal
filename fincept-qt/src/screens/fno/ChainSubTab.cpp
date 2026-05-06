#include "screens/fno/ChainSubTab.h"

#include "core/logging/Logger.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/FnoHeaderBar.h"
#include "screens/fno/OptionChainModel.h"
#include "screens/fno/OptionChainTable.h"
#include "services/options/OptionChainService.h"
#include "trading/AccountManager.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QSet>

#include <QHideEvent>
#include <QShowEvent>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::fno {

using namespace fincept::ui;
using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainService;
using fincept::trading::AccountManager;
using fincept::trading::BrokerAccount;
using fincept::trading::InstrumentService;

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

    empty_label_ = new QLabel("Connect a broker to load F&O data.", body_wrap);
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

    // Populate broker picker from connected accounts. Defer one tick so the
    // AccountManager is fully wired even if we're constructed during startup.
    QTimer::singleShot(0, this, [this]() {
        QStringList broker_ids;
        for (const auto& acc : AccountManager::instance().active_accounts()) {
            if (!broker_ids.contains(acc.broker_id))
                broker_ids.append(acc.broker_id);
        }
        if (broker_ids.isEmpty()) {
            show_empty_state("No broker accounts. Connect one in Equity Trading first.");
            return;
        }
        const QString prefer = !last_broker_.isEmpty() && broker_ids.contains(last_broker_)
                                   ? last_broker_
                                   : broker_ids.first();
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
    if (!active_topic_.isEmpty()) {
        fincept::datahub::DataHub::instance().unsubscribe(this, active_topic_);
        active_topic_.clear();
    }
}

void ChainSubTab::on_broker_changed(const QString& broker_id) {
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
    ref.asset_class = kIndexNames.contains(underlying) ? QStringLiteral("index")
                                                       : QStringLiteral("equity");
    ref.exchange = QStringLiteral("NSE");
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
    if (broker_id.isEmpty()) {
        show_empty_state("Select a broker.");
        return;
    }
    if (!InstrumentService::instance().is_loaded(broker_id)) {
        show_empty_state(QString("Instruments not yet loaded for %1. Switch to Equity Trading "
                                 "to trigger a refresh, or wait for the cache to populate.")
                             .arg(broker_id));
        header_->set_underlyings({});
        header_->set_expiries({});
        return;
    }
    auto unders = OptionChainService::instance().list_underlyings(broker_id);
    if (unders.isEmpty()) {
        show_empty_state("No NFO instruments cached for " + broker_id + ".");
        header_->set_underlyings({});
        header_->set_expiries({});
        return;
    }
    QString prefer;
    if (keep_selection && unders.contains(last_underlying_))
        prefer = last_underlying_;
    else
        prefer = unders.contains("NIFTY") ? "NIFTY" : unders.first();
    header_->set_underlyings(unders, prefer);
    rebuild_expiries_for_underlying(broker_id, prefer, keep_selection);
}

void ChainSubTab::rebuild_expiries_for_underlying(const QString& broker_id, const QString& underlying,
                                                  bool keep_selection) {
    auto exps = OptionChainService::instance().list_expiries(broker_id, underlying);
    if (exps.isEmpty()) {
        show_empty_state(QString("No expiries cached for %1.").arg(underlying));
        header_->set_expiries({});
        return;
    }
    QString prefer;
    if (keep_selection && exps.contains(last_expiry_))
        prefer = last_expiry_;
    else
        prefer = exps.first();  // nearest expiry
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
    const QString topic = current_topic();
    if (topic.isEmpty()) {
        show_empty_state("Pick a broker, underlying, and expiry.");
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
        self->show_empty_state("Chain unavailable: " + err);
    });

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

} // namespace fincept::screens::fno
