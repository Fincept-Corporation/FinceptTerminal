#pragma once
// CurrencyManager — the old display-currency API, now a facade over
// ui/formatting: the display currency is FormatSettings::display_currency()
// (Settings > Currency & Number Format, persisted by FormatSettingsStore), and
// symbols come from the one currency table (CurrencyTable.h) in the user's
// symbol style. Kept so existing callers (cur:: helpers, calculators) compile
// and follow the new setting; new code should use ui/formatting directly
// (fmt::display_currency(), fmt::currency_unit(), fmt::money()).
//
// Symbol-only, as before: changing the display currency never converts the
// values these callers print. Market-data surfaces pass an explicit intrinsic
// currency and are unaffected.

#include <QObject>
#include <QString>
#include <QVector>

namespace fincept::currency {

struct CurrencyInfo {
    QString code;   // ISO 4217, e.g. "USD"
    QString symbol; // display unit in the user's style, e.g. "$"
    QString name;   // translated name for a combo
};

class CurrencyManager : public QObject {
    Q_OBJECT
  public:
    static CurrencyManager& instance();

    /// Every selectable display currency: the table's fiat currencies (no
    /// minor units, crypto or metals), sorted by code.
    static QVector<CurrencyInfo> available();

    /// Display unit for a code in the user's symbol style
    /// (fmt::currency_unit()); the code itself when unknown.
    static QString symbol_for(const QString& code);

    /// Connects to FormatSettings so currency_changed() follows the setting.
    /// Reads nothing (FormatSettingsStore loads the setting). Idempotent.
    void initialize();

    /// The effective display currency (FormatSettings::display_currency()).
    QString code() const;
    /// Its unit in the user's symbol style.
    QString symbol() const;

    /// Sets the explicit display currency (FormatSettings::set_display_currency()).
    /// No-op if unchanged or unknown; currency_changed() follows through FormatSettings.
    void set_currency(const QString& code);

  signals:
    /// The display currency or its printed unit changed.
    void currency_changed(const QString& code, const QString& symbol);

  private:
    CurrencyManager() = default;
    void on_format_changed();

    bool initialized_ = false;
    QString last_code_;
    QString last_symbol_;
};

} // namespace fincept::currency
