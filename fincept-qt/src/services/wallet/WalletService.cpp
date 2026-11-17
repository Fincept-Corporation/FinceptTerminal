#include "services/wallet/WalletService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "services/wallet/ConnectWalletDialog.h"
#include "services/wallet/PumpFunSwapService.h"
#include "services/wallet/SignTransactionDialog.h"
#include "services/wallet/SolanaRpcClient.h"
#include "services/wallet/TokenPriceProducer.h"
#include "services/wallet/WalletActivityProducer.h"
#include "services/wallet/WalletBalanceProducer.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QPointer>
#include <QWidget>

namespace fincept::wallet {

namespace {
// Persisted SecureStorage keys. See plan §4 for rationale.
constexpr const char* kKeyPubkey = "wallet.solana.pubkey";
constexpr const char* kKeyLabel = "wallet.solana.label";
constexpr const char* kKeyConnectedAt = "wallet.solana.connected_at";
} // namespace

WalletService& WalletService::instance() {
    static WalletService s;
    return s;
}

WalletService::WalletService() {
    balance_producer_ = new WalletBalanceProducer(this);
    price_producer_ = new TokenPriceProducer(this);
    swap_service_ = new PumpFunSwapService(this);
    activity_producer_ = new WalletActivityProducer(this);
}

WalletService::~WalletService() = default;

void WalletService::ensure_registered_with_hub() {
    if (hub_registered_) {
        return;
    }
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(balance_producer_);
    hub.register_producer(price_producer_);
    hub.register_producer(activity_producer_);
    // PumpFunSwapService is one-shot (user-initiated build_swap call); it
    // doesn't expose any hub topics and is not registered as a Producer.
    // See CLAUDE.md D2 exception clause.

    fincept::datahub::TopicPolicy balance_policy;
    balance_policy.ttl_ms = 30 * 1000;
    balance_policy.min_interval_ms = 10 * 1000;
    balance_policy.refresh_timeout_ms = 15 * 1000;
    hub.set_policy_pattern(QStringLiteral("wallet:balance:*"), balance_policy);

    fincept::datahub::TopicPolicy price_policy;
    price_policy.ttl_ms = 60 * 1000;
    price_policy.min_interval_ms = 30 * 1000;
    price_policy.refresh_timeout_ms = 15 * 1000;
    price_policy.coalesce_within_ms = 250; // batch concurrent mint subscribes
    // Phase 2 §2A.5: generalised topic family for any token's USD price.
    hub.set_policy_pattern(QStringLiteral("market:price:token:*"), price_policy);
    // Backward-compat alias kept for one phase. Removed in Phase 3.
    hub.set_policy(QStringLiteral("market:price:fncpt"), price_policy);

    // Phase 2 §2B: parsed activity feed.
    fincept::datahub::TopicPolicy activity_policy;
    activity_policy.ttl_ms = 30 * 1000;
    activity_policy.min_interval_ms = 10 * 1000;
    activity_policy.refresh_timeout_ms = 15 * 1000;
    hub.set_policy_pattern(QStringLiteral("wallet:activity:*"), activity_policy);

    hub_registered_ = true;
    LOG_DEBUG("WalletService", "Registered with DataHub (wallet:balance:*, market:price:token:*, "
                               "market:price:fncpt[deprecated alias], wallet:activity:*)");
}

void WalletService::restore_from_storage() {
    const SecretRead pk_res = SecureStorage::instance().read(QString::fromLatin1(kKeyPubkey));
    const bool failing = pk_res.failed();
    LOG_TRANSITION("WalletService", QStringLiteral("restore_read"), failing, failing ? LogLevel::Warn : LogLevel::Info,
                   failing ? logf("Saved wallet could not be read; shown as unreadable, nothing written",
                                  {{"key", kKeyPubkey}, {"kind", pk_res.error_name()}})
                           : QStringLiteral("Saved wallet readable again"));
    if (failing) {
        // Not "no wallet": say so (the Crypto Center shows it with a retry) and keep
        // the saved row untouched — connecting a wallet anew is the user's choice.
        restore_error_ = tr("Could not read the saved wallet: %1.").arg(pk_res.reason());
        emit restore_failed(restore_error_);
        return;
    }
    restore_error_.clear();
    if (!pk_res.found() || pk_res.value.isEmpty()) {
        return;
    }
    WalletState s;
    s.pubkey_b58 = pk_res.value;

    // Label and timestamp are display-only: an unreadable one stays empty / 0.
    const SecretRead lbl_res = SecureStorage::instance().read(QString::fromLatin1(kKeyLabel));
    if (lbl_res.found()) {
        s.label = lbl_res.value;
    }
    const SecretRead ts_res = SecureStorage::instance().read(QString::fromLatin1(kKeyConnectedAt));
    if (ts_res.found()) {
        bool ok = false;
        const auto v = ts_res.value.toLongLong(&ok);
        if (ok) {
            s.connected_at_ms = v;
        }
    }
    s.soft_connected = true;
    set_state(std::move(s));
    LOG_INFO("WalletService", logf("Wallet restored", {{"wallet", log_mask(state_.pubkey_b58)}, {"label", state_.label}}));
    emit wallet_connected(state_.pubkey_b58, state_.label);
}

void WalletService::retry_restore() {
    if (state_.is_connected())
        return;
    restore_from_storage();
}

void WalletService::connect_with_dialog(QWidget* parent) {
    auto* dlg = new ConnectWalletDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<WalletService> self = this;
    connect(dlg, &ConnectWalletDialog::connected, this, [self](QString pubkey, QString label) {
        if (!self) {
            return;
        }
        WalletState s;
        s.pubkey_b58 = std::move(pubkey);
        s.label = std::move(label);
        s.connected_at_ms = QDateTime::currentMSecsSinceEpoch();
        s.soft_connected = false;
        self->set_state(std::move(s));
        self->restore_error_.clear(); // the user connected a wallet: it replaces the unreadable one
        self->persist_state();
        LOG_INFO("WalletService",
                 logf("Wallet connected", {{"wallet", log_mask(self->state_.pubkey_b58)}, {"label", self->state_.label}}));
        emit self->wallet_connected(self->state_.pubkey_b58, self->state_.label);
    });
    connect(dlg, &ConnectWalletDialog::failed, this, [self](QString reason) {
        if (!self) {
            return;
        }
        LOG_WARN("WalletService", logf("Wallet connect failed", {{"reason", reason}}));
        emit self->connect_failed(reason);
    });
    dlg->open();
}

void WalletService::disconnect() {
    if (!state_.is_connected()) {
        return;
    }
    LOG_INFO("WalletService", logf("Wallet disconnected", {{"wallet", log_mask(state_.pubkey_b58)}}));
    clear_persisted_state();
    state_ = {};
    emit wallet_disconnected();
}

void WalletService::set_state(WalletState s) {
    state_ = std::move(s);
}

void WalletService::persist_state() {
    auto& store = SecureStorage::instance();
    store.store(QString::fromLatin1(kKeyPubkey), state_.pubkey_b58);
    store.store(QString::fromLatin1(kKeyLabel), state_.label);
    store.store(QString::fromLatin1(kKeyConnectedAt), QString::number(state_.connected_at_ms));
}

void WalletService::clear_persisted_state() {
    auto& store = SecureStorage::instance();
    store.remove(QString::fromLatin1(kKeyPubkey));
    store.remove(QString::fromLatin1(kKeyLabel));
    store.remove(QString::fromLatin1(kKeyConnectedAt));
}

void WalletService::set_balance_mode(bool stream) {
    const auto wanted = stream ? BalanceMode::Stream : BalanceMode::Poll;
    if (!balance_producer_) {
        return;
    }
    if (balance_producer_->mode() == wanted) {
        return;
    }
    SecureStorage::instance().store(QStringLiteral("wallet.balance_mode"),
                                    stream ? QStringLiteral("stream") : QStringLiteral("poll"));
    balance_producer_->set_mode(wanted);
    LOG_DEBUG("WalletService", QStringLiteral("balance mode set to %1").arg(stream ? "stream" : "poll"));

    // Force a refresh so subscribers see fresh data via the new path.
    if (state_.is_connected()) {
        const auto topic = QStringLiteral("wallet:balance:%1").arg(state_.pubkey_b58);
        fincept::datahub::DataHub::instance().request(topic, /*force=*/true);
    }
    emit balance_mode_changed(stream);
}

bool WalletService::balance_mode_is_stream() const noexcept {
    return balance_producer_ && balance_producer_->mode() == BalanceMode::Stream;
}

void WalletService::force_balance_refresh() {
    if (!balance_producer_ || !state_.is_connected()) {
        return;
    }
    balance_producer_->force_refresh(state_.pubkey_b58);
}

void WalletService::sign_and_send(const QString& tx_base64, const QString& dialog_title, const QString& dialog_lede,
                                  QWidget* parent, std::function<void(Result<QString>)> cb) {
    if (tx_base64.isEmpty()) {
        if (cb)
            cb(Result<QString>::err("empty_tx"));
        return;
    }
    if (!state_.is_connected()) {
        if (cb)
            cb(Result<QString>::err("not_connected"));
        return;
    }

    auto* dlg = new SignTransactionDialog(tx_base64, dialog_title, dialog_lede, parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QPointer<SignTransactionDialog> guard = dlg;
    connect(dlg, &QDialog::accepted, this, [this, guard, cb]() {
        if (!cb || !guard)
            return;
        const QString result = guard->signature();

        // resources/wallet/swap.html has two outcomes. A wallet that exposes
        // signAndSendTransaction broadcasts the tx itself and the page posts the
        // base58 signature. A wallet that only exposes signTransaction cannot
        // broadcast, so the page posts "BYTES:<base64 signed tx>" and leaves the
        // forwarding to us ("caller forwards via RPC" in the page source). Nothing
        // in the C++ side handled that prefix, so the raw "BYTES:…" string was
        // handed back as if it were a signature: callers polled
        // getSignatureStatuses with it for a minute and reported a timeout while
        // the signed transaction was never sent.
        static const QString kSignedBytesPrefix = QStringLiteral("BYTES:");
        if (!result.startsWith(kSignedBytesPrefix)) {
            cb(Result<QString>::ok(result));
            return;
        }
        const QString signed_b64 = result.mid(kSignedBytesPrefix.size());
        if (signed_b64.isEmpty()) {
            cb(Result<QString>::err("empty_signed_tx"));
            return;
        }
        LOG_INFO("WalletService", "wallet returned a signed transaction without sending — broadcasting via RPC");
        auto* rpc = new SolanaRpcClient(this);
        rpc->reload_endpoint();
        rpc->send_transaction(signed_b64, [rpc, cb](Result<QString> sent) {
            rpc->deleteLater();
            cb(std::move(sent));
        });
    });
    connect(dlg, &QDialog::rejected, this, [guard, cb]() {
        if (!cb || !guard)
            return;
        const auto reason = guard->failure_reason();
        cb(Result<QString>::err(reason.isEmpty() ? std::string("rejected") : reason.toStdString()));
    });
    dlg->open();
}

} // namespace fincept::wallet
