// src/screens/portfolio/PortfolioFormat.h
//
// Presentation helpers shared by every Portfolio surface, so money, percent,
// date and absent-value rendering is identical across the ribbon, blotter and
// detail views. Everything goes through ui/formatting (locale grouping, the
// user's compact scheme, the em-dash placeholder for NaN / unknown).
//
// Also: PortfolioNoticeBar, the one loading / incomplete / error / retry strip
// the detail views put above their content.
#pragma once

#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAllocation.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>

#include <cmath>
#include <optional>

namespace fincept::screens::pfmt {

namespace uf = fincept::ui::formatting;
using portfolio::kAbsent;

inline QString tr_pf(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::screens::PortfolioFormat", text, disambiguation, n);
}

inline QString dash() {
    return uf::placeholder();
}

// ── Money ────────────────────────────────────────────────────────────────────
/// Full amount in `ccy` ("$1,234.50", "1.234,50 €", "¥2,845").
inline QString money(double v, const QString& ccy) {
    return uf::money(v, ccy);
}
/// Abbreviated per the user's scheme (Settings > Currency & Number Format; on
/// Automatic: lakh/crore for INR-family currencies, K/M/B otherwise).
inline QString money_c(double v, const QString& ccy) {
    return uf::money(v, ccy, uf::Compact::FromSettings);
}
/// Signed ("+$1,234.50") for P&L.
inline QString smoney(double v, const QString& ccy) {
    return uf::signed_money(v, ccy);
}
inline QString smoney_c(double v, const QString& ccy) {
    return uf::signed_money(v, ccy, uf::Compact::FromSettings);
}
/// A price in its own quote currency, decimals per instrument ("123.45p", "0.00001234").
inline QString price(double v, const QString& quote_ccy, const QString& symbol = {}) {
    const uf::InstrumentFmt inst = symbol.isEmpty() ? uf::InstrumentFmt{quote_ccy} : uf::instrument_for_symbol(symbol, quote_ccy);
    return uf::price(v, inst);
}
/// price() with its currency unit ("$351.71", "117.70p", "C$12.40"), for a column
/// that has no currency column or header naming the unit. Never converted.
inline QString price_sym(double v, const QString& quote_ccy, const QString& symbol = {}) {
    const uf::InstrumentFmt inst = symbol.isEmpty() ? uf::InstrumentFmt{quote_ccy} : uf::instrument_for_symbol(symbol, quote_ccy);
    return uf::price_with_symbol(v, inst);
}

// ── Numbers / percent ────────────────────────────────────────────────────────
inline QString num(double v, int dp = 2) {
    return uf::number(v, dp);
}
inline QString qty(double q) {
    return uf::quantity(q, 8);
}
/// Input in percent units (5.2 = 5.2 %), signed.
inline QString pct(double p, int dp = 2) {
    return uf::pct(p, dp, true);
}
/// Unsigned percent (weights, shares).
inline QString upct(double p, int dp = 1) {
    return uf::pct(p, dp, false);
}
/// Input a fraction (0.052 = 5.2 %).
inline QString fpct(double f, int dp = 2, bool signed_ = true) {
    return uf::fraction_pct(f, dp, signed_);
}
inline QString pp(double points, int dp = 2) {
    return uf::pp(points, dp, true);
}
inline QString opt_num(const std::optional<double>& v, int dp = 2) {
    return v.has_value() ? uf::number(*v, dp) : dash();
}
inline QString opt_pct(const std::optional<double>& v, int dp = 2, bool signed_ = true) {
    return v.has_value() ? uf::pct(*v, dp, signed_) : dash();
}
inline QString opt_fpct(const std::optional<double>& v, int dp = 2, bool signed_ = true) {
    return v.has_value() ? uf::fraction_pct(*v, dp, signed_) : dash();
}
inline double opt_or_nan(const std::optional<double>& v) {
    return v.has_value() ? *v : kAbsent;
}

// ── Dates ────────────────────────────────────────────────────────────────────
inline QDate iso_date(const QString& iso) {
    return QDate::fromString(iso.left(10), Qt::ISODate);
}
/// "YYYY-MM-DD..." as the locale's medium date; the raw text when unparsable.
inline QString date(const QString& iso, uf::DateStyle style = uf::DateStyle::Medium) {
    const QDate d = iso_date(iso);
    if (d.isValid())
        return uf::calendar_date(d, style);
    return iso.trimmed().isEmpty() ? dash() : iso;
}
inline QString date(const QDate& d, uf::DateStyle style = uf::DateStyle::Medium) {
    return d.isValid() ? uf::calendar_date(d, style) : dash();
}
/// Epoch ms of a calendar date at UTC midnight (TimeSeriesChart x with set_utc(true)).
inline double utc_ms(const QDate& d) {
    return static_cast<double>(QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch());
}
inline double utc_ms(const QString& iso) {
    const QDate d = iso_date(iso);
    return d.isValid() ? utc_ms(d) : kAbsent;
}

// ── Colours ──────────────────────────────────────────────────────────────────
/// Price / P&L direction: POSITIVE / NEGATIVE, flat = secondary, unknown = tertiary.
inline QColor dir_color(double v) {
    if (!std::isfinite(v))
        return QColor(ui::colors::TEXT_TERTIARY());
    if (v > 0)
        return QColor(ui::colors::POSITIVE());
    if (v < 0)
        return QColor(ui::colors::NEGATIVE());
    return QColor(ui::colors::TEXT_SECONDARY());
}
inline QString dir_css(double v) {
    return dir_color(v).name();
}

// ── Summary helpers ──────────────────────────────────────────────────────────
inline QString base_ccy(const portfolio::PortfolioSummary& s) {
    const QString b = s.base_currency.isEmpty() ? s.portfolio.currency : s.base_currency;
    // A portfolio stored without a currency is valued in the user's display currency (the
    // service resolves it the same way: portfolio::base_currency_of(), which logs it).
    return b.isEmpty() ? uf::display_currency() : b.toUpper();
}

/// At least one valued holding is in a currency other than the base.
inline bool has_foreign_holdings(const portfolio::PortfolioSummary& s) {
    const QString b = base_ccy(s);
    for (const auto& h : s.holdings)
        if (h.valued() && !h.currency.isEmpty() && h.currency.compare(b, Qt::CaseInsensitive) != 0)
            return true;
    return false;
}

/// Whether the views split unrealised P&L into price and FX parts: the user's
/// setting (Settings > Currency & Number Format, on by default) and a book that
/// holds something in a foreign currency.
inline bool fx_split_shown(const portfolio::PortfolioSummary& s) {
    return uf::FormatSettings::instance().fx_pnl_split() && has_foreign_holdings(s);
}

/// Why a holding's FX P&L is "—" (fx_pnl NaN): its cost has no trade-date rate.
inline QString fx_pnl_unknown_reason() {
    return tr_pf("Cost converted at today's rate: the trade-date FX rate is unknown, so the P&L cannot be split "
                 "into price and FX parts. Record the FX rate on the BUY rows to enable it.");
}

/// "≈ " when a converted total rests on a stale FX rate (R3: marked in the figure
/// itself, never by colour alone); "" otherwise.
inline QString approx_prefix(const portfolio::PortfolioSummary& s) {
    return s.fx_stale.isEmpty() ? QString() : QStringLiteral("≈ ");
}

/// "" when every FX rate behind the converted figures is fresh; otherwise one line
/// naming the stale currencies and the oldest rate's time. Stale rates are still
/// used (never dropped or replaced by 1.0): the figures stay in the base currency.
inline QString fx_stale_line(const portfolio::PortfolioSummary& s) {
    if (s.fx_stale.isEmpty())
        return {};
    QString t = tr_pf("FX rate stale: %1 → %2").arg(s.fx_stale.join(QStringLiteral(", ")), base_ccy(s));
    if (s.fx_stale_as_of.isValid())
        t += QStringLiteral(" — ") + tr_pf("oldest rate as of %1").arg(uf::date_time(s.fx_stale_as_of));
    t += QStringLiteral(". ") + tr_pf("Converted figures use the last known rate and are approximate (≈).");
    return t;
}

/// Valued holdings' total (cash excluded even when tracked).
inline double holdings_value(const portfolio::PortfolioSummary& s) {
    return s.cash_in_nav && std::isfinite(s.cash_value) ? s.total_market_value - s.cash_value : s.total_market_value;
}

/// "" when the valuation is complete; otherwise one line saying what is missing.
inline QString incomplete_line(const portfolio::PortfolioSummary& s) {
    if (s.complete)
        return {};
    const int missing = s.total_positions - s.valued_positions;
    QString t = tr_pf("Valuation incomplete: %1 of %2 positions valued", nullptr)
                    .arg(s.valued_positions)
                    .arg(s.total_positions);
    QStringList why;
    if (!s.unpriced.isEmpty())
        why << tr_pf("no live price: %1").arg(s.unpriced.join(QStringLiteral(", ")));
    if (!s.fx_missing.isEmpty())
        why << tr_pf("no FX rate yet: %1").arg(s.fx_missing.join(QStringLiteral(", ")));
    if (!why.isEmpty())
        t += QStringLiteral(" — ") + why.join(QStringLiteral("; "));
    if (missing > 0)
        t += QStringLiteral(". ") + tr_pf("Totals, weights and risk cover the valued positions only.");
    return t;
}

// ── Allocation vocabulary ────────────────────────────────────────────────────
inline QString dimension_name(portfolio::AllocationDimension d) {
    using AD = portfolio::AllocationDimension;
    switch (d) {
        case AD::Sector:
            return tr_pf("Sector");
        case AD::Industry:
            return tr_pf("Industry");
        case AD::AssetClass:
            return tr_pf("Asset class");
        case AD::Country:
            return tr_pf("Country");
        case AD::Region:
            return tr_pf("Region");
        case AD::Currency:
            return tr_pf("Currency");
        case AD::Exchange:
            return tr_pf("Exchange");
        case AD::MarketCap:
            return tr_pf("Market cap");
    }
    return {};
}

/// Display text of an allocation slice key ("north_america" → "North America", "" → "Unknown").
inline QString slice_label(portfolio::AllocationDimension d, const QString& key) {
    using AD = portfolio::AllocationDimension;
    if (key.isEmpty())
        return tr_pf("Unknown");
    if (d == AD::Region) {
        if (key == QLatin1String("north_america"))
            return tr_pf("North America");
        if (key == QLatin1String("uk"))
            return tr_pf("United Kingdom");
        if (key == QLatin1String("europe"))
            return tr_pf("Europe ex-UK");
        if (key == QLatin1String("japan"))
            return tr_pf("Japan");
        if (key == QLatin1String("china"))
            return tr_pf("China / Hong Kong");
        if (key == QLatin1String("india"))
            return tr_pf("India");
        if (key == QLatin1String("asia_pacific"))
            return tr_pf("Asia-Pacific ex-Japan");
        if (key == QLatin1String("emerging"))
            return tr_pf("Emerging markets");
    }
    if (d == AD::AssetClass) {
        if (key == QLatin1String("equity"))
            return tr_pf("Equity");
        if (key == QLatin1String("bond"))
            return tr_pf("Bonds");
        if (key == QLatin1String("commodity"))
            return tr_pf("Commodities");
        if (key == QLatin1String("crypto"))
            return tr_pf("Crypto");
        if (key == QLatin1String("cash"))
            return tr_pf("Cash & equivalents");
        if (key == QLatin1String("real_estate"))
            return tr_pf("Real estate");
        if (key == QLatin1String("other"))
            return tr_pf("Other");
    }
    if (d == AD::MarketCap) {
        if (key == QLatin1String("mega"))
            return tr_pf("Mega cap (≥ 200 bn USD)");
        if (key == QLatin1String("large"))
            return tr_pf("Large cap (10–200 bn)");
        if (key == QLatin1String("mid"))
            return tr_pf("Mid cap (2–10 bn)");
        if (key == QLatin1String("small"))
            return tr_pf("Small cap (300 m–2 bn)");
        if (key == QLatin1String("micro"))
            return tr_pf("Micro cap (< 300 m)");
    }
    if (d == AD::Currency)
        return QStringLiteral("%1 — %2").arg(key, ui::formatting::currency_name(key));
    return key;
}

// ── Notice strip ─────────────────────────────────────────────────────────────
/// One-line state strip for the top of a detail view: loading, incomplete
/// valuation (warning), error with a RETRY button, or hidden. No Q_OBJECT —
/// connect to retry_button()->clicked.
class PortfolioNoticeBar : public QFrame {
  public:
    enum class Kind { Hidden, Info, Loading, Warning, Error };

    explicit PortfolioNoticeBar(QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("pfNoticeBar"));
        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(10, 4, 8, 4);
        h->setSpacing(8);
        text_ = new QLabel(this);
        text_->setWordWrap(true);
        text_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(text_, 1);
        retry_ = new QPushButton(tr_pf("RETRY"), this);
        retry_->setCursor(Qt::PointingHandCursor);
        retry_->setVisible(false);
        h->addWidget(retry_);
        setVisible(false);
        // Restyled once an appearance change has settled (never inside theme_changed).
        QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
                         [this]() { restyle(); });
        restyle();
    }

    void set_notice(Kind kind, const QString& text, bool retry = false) {
        kind_ = kind;
        text_->setText(text);
        retry_->setVisible(retry && kind != Kind::Hidden);
        setVisible(kind != Kind::Hidden && !text.isEmpty());
        restyle();
    }
    void clear() { set_notice(Kind::Hidden, {}); }
    Kind kind() const { return kind_; }
    QPushButton* retry_button() const { return retry_; }
    QLabel* label() const { return text_; }

  private:
    /// Sets the stylesheet only when it changes: views re-render (and re-set their
    /// notice) on a currency / number-format change, which must not restyle anything.
    void restyle() {
        QColor accent;
        switch (kind_) {
            case Kind::Error:
                accent = QColor(ui::colors::DANGER());
                break;
            case Kind::Warning:
                accent = QColor(ui::colors::WARNING());
                break;
            case Kind::Loading:
            case Kind::Info:
            case Kind::Hidden:
                accent = QColor(ui::colors::INFO());
                break;
        }
        const QString css =
            QStringLiteral("#pfNoticeBar { background:%1; border-left:3px solid %2; border-bottom:1px solid %3; }"
                           "#pfNoticeBar QLabel { color:%4; font-size:%5px; background:transparent; }"
                           "#pfNoticeBar QPushButton { background:transparent; color:%2; border:1px solid %2;"
                           "  padding:2px 10px; font-size:%6px; font-weight:700; }"
                           "#pfNoticeBar QPushButton:hover { background:%2; color:%7; }")
                .arg(ui::colors::tint(accent, 0.12), accent.name(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
                     ui::fonts::px(-2), ui::fonts::px(-3), ui::colors::contrast_text_on(accent).name());
        if (styleSheet() != css)
            setStyleSheet(css);
    }

    Kind kind_ = Kind::Hidden;
    QLabel* text_ = nullptr;
    QPushButton* retry_ = nullptr;
};

} // namespace fincept::screens::pfmt
