#pragma once
// Equity Price Ribbon — dense single-line: [symbol] price, change (sign + arrow),
// bid/ask, spread, high/low/volume. It sits in the command bar and never widens it:
// its minimum is price + change; the other fields give way right to left (volume
// first, bid last) when the bar is narrow, and come back when there is room.

#include <QLabel>
#include <QWidget>

namespace fincept::screens::equity {

class EquityTickerBar : public QWidget {
    Q_OBJECT
  public:
    explicit EquityTickerBar(QWidget* parent = nullptr);

    void set_symbol(const QString& symbol);
    // Fallback price currency (ISO code, e.g. the exchange's or the broker's) used
    // until set_price_format() names the quote's own; empty = prices without a unit.
    void set_currency(const QString& currency_code) { currency_ = currency_code; }
    void update_quote(double ltp, double change, double change_pct, double high, double low, double volume, double bid,
                      double ask);
    // Back to "--" everywhere (the symbol stays). Used when the price source
    // changes, so one source's numbers never sit under the other's label.
    void clear_quote();
    // A source-level state shown in place of the price fields, e.g. "NO LIVE QUOTES ·
    // Tradejini" for a broker that supplies no prices; `detail` (the source's own
    // reason) is its tooltip. An empty `text` hides it and brings the fields back.
    void set_notice(const QString& text, const QString& detail = {});
    // Price currency as quoted ("USD", "GBp") and decimals (-1 = from the value):
    // prices print through ui::formatting (pence as "123.45p", FX with pips). An
    // empty currency falls back to set_currency()'s code.
    void set_price_format(const QString& currency, int digits);
    // Public (delayed) data has no bid / ask: hide those fields instead of "--".
    void set_bid_ask_visible(bool visible);
    // The symbol label (off when a symbol box beside the ribbon already names it).
    void set_symbol_visible(bool visible);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void set_price_fields_visible(bool visible);
    // Show the optional fields that fit the width given, in priority order.
    void refit();
    bool field_wanted(const QLabel* l) const;
    QString px(double v) const;
    // The quote's currency when known, else the fallback (set_currency).
    QString effective_currency() const { return price_currency_.isEmpty() ? currency_ : price_currency_; }
    // Re-format the last quote after a Settings > Currency & Number Format change.
    void repaint_last_quote();

    QLabel* symbol_label_ = nullptr;
    QLabel* notice_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* bid_label_ = nullptr;
    QLabel* ask_label_ = nullptr;
    QLabel* spread_label_ = nullptr;
    QLabel* high_label_ = nullptr;
    QLabel* low_label_ = nullptr;
    QLabel* volume_label_ = nullptr;

    bool last_positive_ = true;
    int last_sign_ = 2; // -1 / 0 / +1 of the last change painted (2 = none yet)
    QString currency_;       // set_currency(): fallback ISO code
    QString price_currency_; // set_price_format()
    // Last quote painted (repaint_last_quote); ltp <= 0 = none.
    struct LastQuote {
        double ltp = 0, change = 0, change_pct = 0, high = 0, low = 0, volume = 0, bid = 0, ask = 0;
    } last_;
    int price_digits_ = -1;
    bool bid_ask_visible_ = true;
    bool fields_visible_ = true;
    bool symbol_visible_ = true;
};

} // namespace fincept::screens::equity
