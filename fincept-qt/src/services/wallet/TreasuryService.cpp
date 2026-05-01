#include "services/wallet/TreasuryService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/SolanaRpcClient.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QPointer>
#include <QVariant>

#include <atomic>

namespace fincept::wallet {

namespace {

constexpr const char* kTopicReserves = "treasury:reserves";
constexpr const char* kTopicRunway   = "treasury:runway";

constexpr const char* kKeyTreasuryPubkey  = "fincept.treasury_pubkey";
constexpr const char* kKeyMonthlyOpexUsd  = "fincept.treasury_monthly_opex_usd";
constexpr const char* kKeyMultisigLabel   = "fincept.treasury_multisig_label";
constexpr const char* kKeyMultisigUrl     = "fincept.treasury_multisig_url";

// Defaults used when keys aren't configured. Conservative — better to
// over-state runway pressure than to under-state it.
constexpr double kDefaultMonthlyOpexUsd = 100'000.0;
constexpr const char* kDefaultMultisigLabel = "3-of-5";
constexpr const char* kDefaultMultisigUrl   = "https://app.squads.so/";

// Fetch the live SOL price by peeking the hub. Returns 0 if unavailable.
double sol_price_usd_from_hub() {
    auto& hub = fincept::datahub::DataHub::instance();
    const auto v = hub.peek(QStringLiteral("market:price:token:") +
                            QString::fromLatin1(kWrappedSolMint));
    if (v.canConvert<TokenPrice>()) {
        const auto p = v.value<TokenPrice>();
        if (p.valid) return p.usd;
    }
    return 0.0;
}

double monthly_opex_usd_or_default() {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kKeyMonthlyOpexUsd));
    if (r.is_ok()) {
        bool ok = false;
        const double v = r.value().toDouble(&ok);
        if (ok && v > 0.0) return v;
    }
    return kDefaultMonthlyOpexUsd;
}

QString multisig_label_or_default() {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kKeyMultisigLabel));
    if (r.is_ok() && !r.value().trimmed().isEmpty()) return r.value();
    return QString::fromLatin1(kDefaultMultisigLabel);
}

QString multisig_url_or_default(const QString& pubkey) {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kKeyMultisigUrl));
    if (r.is_ok() && !r.value().trimmed().isEmpty()) return r.value();
    // Default: Squads vault page for the pubkey, when known. Otherwise the
    // generic Squads landing.
    if (!pubkey.isEmpty()) {
        return QStringLiteral("https://app.squads.so/squads/") + pubkey;
    }
    return QString::fromLatin1(kDefaultMultisigUrl);
}

} // namespace

TreasuryService::TreasuryService(QObject* parent) : QObject(parent) {
    rpc_ = new SolanaRpcClient(this);
}

TreasuryService::~TreasuryService() = default;

QStringList TreasuryService::topic_patterns() const {
    return QStringList{
        QString::fromLatin1(kTopicReserves),
        QString::fromLatin1(kTopicRunway),
    };
}

QString TreasuryService::usdc_mint() {
    // USDC on Solana mainnet. Pinned here rather than in WalletTypes because
    // it's only meaningful in the treasury context for now.
    return QStringLiteral("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v");
}

void TreasuryService::refresh(const QStringList& topics) {
    auto pk = SecureStorage::instance().retrieve(QString::fromLatin1(kKeyTreasuryPubkey));
    const QString treasury_pubkey = pk.is_ok() ? pk.value().trimmed() : QString();

    const bool reserves_requested = topics.contains(QLatin1String(kTopicReserves));
    const bool runway_requested   = topics.contains(QLatin1String(kTopicRunway));

    if (treasury_pubkey.isEmpty()) {
        // Log once per process — see BuybackBurnService for the rationale.
        static std::atomic<bool> once{false};
        if (!once.exchange(true)) {
            LOG_INFO("Treasury",
                     "no fincept.treasury_pubkey configured — serving mock data");
        } else {
            LOG_DEBUG("Treasury", "mock-mode refresh");
        }
        if (reserves_requested) publish_mock_reserves();
        if (runway_requested) {
            // Mock runway uses the same total as the mock reserves.
            const double mock_total = (8.34 * 84.28) + 1'840'000.0; // hand-tuned
            publish_mock_runway(mock_total);
        }
        return;
    }

    // Real path — one fetch covers both topics, since runway is derived from
    // reserves. We always run the reserves fetch when either is requested.
    refresh_real(treasury_pubkey);
}

// ── Real fetch path ────────────────────────────────────────────────────────

void TreasuryService::refresh_real(const QString& treasury_pubkey) {
    rpc_->reload_endpoint();

    QPointer<TreasuryService> self = this;
    rpc_->get_sol_balance(treasury_pubkey,
        [self, treasury_pubkey](Result<quint64> sol_r) {
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (sol_r.is_err()) {
            hub.publish_error(QString::fromLatin1(kTopicReserves),
                              QString::fromStdString(sol_r.error()));
            return;
        }
        const quint64 sol_lamports = sol_r.value();

        // Now fetch the USDC ATA balance.
        self->rpc_->get_token_balance(treasury_pubkey, usdc_mint(),
            [self, treasury_pubkey, sol_lamports](
                Result<std::vector<SplTokenBalance>> usdc_r) {
            if (!self) return;
            auto& hub = fincept::datahub::DataHub::instance();
            double usdc_amount = 0.0;
            if (usdc_r.is_ok()) {
                for (const auto& b : usdc_r.value()) {
                    usdc_amount += b.ui_amount();
                }
            } // soft-fail on USDC: empty ATA is normal, we report 0

            const double sol_price = sol_price_usd_from_hub();
            const double sol_ui = static_cast<double>(sol_lamports) / 1e9;
            const double total_usd = (sol_ui * sol_price) + usdc_amount;

            TreasuryReserves r;
            r.pubkey_b58       = treasury_pubkey;
            r.sol_lamports     = sol_lamports;
            r.usdc_amount      = usdc_amount;
            r.sol_usd_price    = sol_price;
            r.total_usd        = total_usd;
            r.multisig_label   = multisig_label_or_default();
            r.multisig_url     = multisig_url_or_default(treasury_pubkey);
            r.ts_ms            = QDateTime::currentMSecsSinceEpoch();
            r.is_mock          = false;
            hub.publish(QString::fromLatin1(kTopicReserves), QVariant::fromValue(r));

            // Derive runway from the same total — single source of truth.
            self->publish_runway(total_usd, /*is_mock=*/false);
        });
    });
}

// ── Mock paths ─────────────────────────────────────────────────────────────

void TreasuryService::publish_mock_reserves() {
    // Hand-tuned numbers consistent with the mock buyback epoch.
    const double sol_price = 84.28;
    const double sol_ui    = 234.0;
    const double usdc      = 1'840'000.0;
    TreasuryReserves r;
    r.pubkey_b58       = QStringLiteral("FinceptTreasuryMockVaultXXXXXXXXXXXXXXXXXXXX");
    r.sol_lamports     = static_cast<quint64>(sol_ui * 1e9);
    r.usdc_amount      = usdc;
    r.sol_usd_price    = sol_price;
    r.total_usd        = (sol_ui * sol_price) + usdc;
    r.multisig_label   = multisig_label_or_default();
    r.multisig_url     = multisig_url_or_default(QString());
    r.ts_ms            = QDateTime::currentMSecsSinceEpoch();
    r.is_mock          = true;
    fincept::datahub::DataHub::instance().publish(
        QString::fromLatin1(kTopicReserves), QVariant::fromValue(r));
}

void TreasuryService::publish_mock_runway(double total_usd_for_runway) {
    publish_runway(total_usd_for_runway, /*is_mock=*/true);
}

void TreasuryService::publish_runway(double total_usd, bool is_mock) {
    const double opex = monthly_opex_usd_or_default();
    TreasuryRunway w;
    w.total_usd        = total_usd;
    w.monthly_opex_usd = opex;
    w.months           = (opex > 0.0) ? (total_usd / opex) : 0.0;
    w.ts_ms            = QDateTime::currentMSecsSinceEpoch();
    w.is_mock          = is_mock;
    fincept::datahub::DataHub::instance().publish(
        QString::fromLatin1(kTopicRunway), QVariant::fromValue(w));
}

} // namespace fincept::wallet
