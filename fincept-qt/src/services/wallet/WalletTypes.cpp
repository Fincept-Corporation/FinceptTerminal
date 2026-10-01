#include "services/wallet/WalletTypes.h"

#include <cmath>

namespace fincept::wallet {

double TokenHolding::ui_amount() const noexcept {
    if (amount_raw.isEmpty())
        return 0.0;
    // amount_raw is an unsigned 64-bit atomic amount; toLongLong() fails (and
    // the holding silently reads 0) above 2^63-1 — reachable for 9-decimal
    // meme tokens with trillion-scale supply. A double parse covers the whole
    // u64 range and the display only needs ~15 significant digits anyway.
    bool ok = false;
    const double raw = amount_raw.toDouble(&ok);
    if (!ok || raw < 0.0)
        return 0.0;
    return raw / std::pow(10.0, decimals);
}

const TokenHolding* WalletBalance::fncpt_holding() const noexcept {
    const auto fncpt = QString::fromLatin1(kFncptMint);
    for (const auto& t : tokens) {
        if (t.mint == fncpt)
            return &t;
    }
    return nullptr;
}

double WalletBalance::fncpt_ui() const noexcept {
    const auto* h = fncpt_holding();
    return h ? h->ui_amount() : 0.0;
}

int WalletBalance::fncpt_decimals() const noexcept {
    const auto* h = fncpt_holding();
    // Pump.fun mints default to 6 decimals; if we don't hold the token we
    // still need to scale balance amounts correctly (e.g. for Settings'
    // "1,000 $FNCPT" threshold), so fall back to 6.
    return (h && h->decimals > 0) ? h->decimals : 6;
}

} // namespace fincept::wallet
