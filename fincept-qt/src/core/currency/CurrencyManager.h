#pragma once
// CurrencyManager — owns the user's preferred display currency.
// Cloned from core/i18n/LanguageManager: singleton, persisted via
// SettingsRepository ("general.currency"), broadcasts currency_changed.
// Symbol-only: changing currency never converts values, it only changes
// the symbol shown on denomination-agnostic surfaces (calculators).
// Market-data surfaces pass an explicit intrinsic currency and are unaffected.

#include <QObject>
#include <QString>
#include <QVector>

namespace fincept::currency {

struct CurrencyInfo {
    QString code;   // ISO 4217, e.g. "USD"
    QString symbol; // display glyph, e.g. "$"
    QString name;   // human label for the settings combo
};

class CurrencyManager : public QObject {
    Q_OBJECT
  public:
    static CurrencyManager& instance();

    /// All selectable currencies (single source of truth for the combo).
    static QVector<CurrencyInfo> available();

    /// Symbol for a given code; falls back to the code itself if unknown.
    static QString symbol_for(const QString& code);

    /// Read the persisted choice from SettingsRepository (default "USD").
    /// Call once on startup before windows are shown.
    void initialize();

    QString code() const { return code_; }
    QString symbol() const { return symbol_for(code_); }

    /// Switch currency: persist + emit. No-op if unchanged or unknown code.
    void set_currency(const QString& code);

  signals:
    void currency_changed(const QString& code, const QString& symbol);

  private:
    CurrencyManager() = default;
    QString code_ = "USD";
};

} // namespace fincept::currency
