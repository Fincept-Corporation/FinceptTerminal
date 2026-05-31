#include "core/currency/CurrencyManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

namespace fincept::currency {

CurrencyManager& CurrencyManager::instance() {
    static CurrencyManager s;
    return s;
}

QVector<CurrencyInfo> CurrencyManager::available() {
    return {
        {"USD", "$", "US Dollar"},
        {"EUR", QString::fromUtf8("\xe2\x82\xac"), "Euro"},          // €
        {"GBP", QString::fromUtf8("\xc2\xa3"), "British Pound"},     // £
        {"JPY", QString::fromUtf8("\xc2\xa5"), "Japanese Yen"},      // ¥
        {"INR", QString::fromUtf8("\xe2\x82\xb9"), "Indian Rupee"},  // ₹
        {"CNY", QString::fromUtf8("\xc2\xa5"), "Chinese Yuan"},      // ¥
        {"CHF", "Fr", "Swiss Franc"},
        {"CAD", "$", "Canadian Dollar"},
        {"AUD", "$", "Australian Dollar"},
    };
}

QString CurrencyManager::symbol_for(const QString& code) {
    for (const auto& c : available())
        if (c.code == code)
            return c.symbol;
    return code; // unknown → show the code rather than a wrong glyph
}

void CurrencyManager::initialize() {
    const auto r = SettingsRepository::instance().get("general.currency", "USD");
    code_ = (r.is_ok() && !r.value().isEmpty()) ? r.value() : QStringLiteral("USD");
    LOG_INFO("Currency", QString("Preferred currency: %1").arg(code_));
}

void CurrencyManager::set_currency(const QString& code) {
    if (code == code_ || symbol_for(code) == code) // unchanged, or unknown code
        return;
    code_ = code;
    SettingsRepository::instance().set("general.currency", code, "general");
    LOG_INFO("Currency", QString("Currency \xe2\x86\x92 %1").arg(code)); // →
    emit currency_changed(code_, symbol());
}

} // namespace fincept::currency
