#include "core/currency/CurrencyManager.h"

#include "core/logging/Logger.h"
#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/FormatSettings.h"

#include <algorithm>

namespace fincept::currency {

namespace uifmt = fincept::ui::formatting;

CurrencyManager& CurrencyManager::instance() {
    static CurrencyManager s;
    return s;
}

QVector<CurrencyInfo> CurrencyManager::available() {
    QVector<CurrencyInfo> out;
    for (const QString& code : uifmt::known_currency_codes()) {
        uifmt::CurrencySpec spec;
        if (!uifmt::find_currency(code, &spec) || spec.minor_divisor != 1 || spec.crypto || spec.metal)
            continue;
        out.append({spec.code, uifmt::currency_unit(spec.code), spec.name});
    }
    std::sort(out.begin(), out.end(), [](const CurrencyInfo& a, const CurrencyInfo& b) { return a.code < b.code; });
    return out;
}

QString CurrencyManager::symbol_for(const QString& code) {
    return code.trimmed().isEmpty() ? QString() : uifmt::currency_unit(code);
}

void CurrencyManager::initialize() {
    if (initialized_)
        return;
    initialized_ = true;
    last_code_ = code();
    last_symbol_ = symbol();
    connect(&uifmt::FormatSettings::instance(), &uifmt::FormatSettings::changed, this,
            &CurrencyManager::on_format_changed);
    LOG_DEBUG("Currency", QString("Display currency follows FormatSettings: %1").arg(last_code_));
}

QString CurrencyManager::code() const {
    return uifmt::FormatSettings::instance().display_currency();
}

QString CurrencyManager::symbol() const {
    return uifmt::currency_unit();
}

void CurrencyManager::set_currency(const QString& code) {
    if (!uifmt::FormatSettings::instance().set_display_currency(code))
        LOG_WARN("Currency", QString("Unknown display currency '%1' ignored").arg(code));
}

void CurrencyManager::on_format_changed() {
    const QString c = code();
    const QString s = symbol();
    if (c == last_code_ && s == last_symbol_)
        return;
    last_code_ = c;
    last_symbol_ = s;
    LOG_INFO("Currency", QString("Display currency \xe2\x86\x92 %1 (%2)").arg(c, s)); // →
    emit currency_changed(c, s);
}

} // namespace fincept::currency
