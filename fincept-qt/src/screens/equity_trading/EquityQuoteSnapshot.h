#pragma once
// EquityQuoteSnapshot — what the right column shows instead of an empty market-depth
// ladder when no broker depth exists (public / paper mode): the selected instrument's
// delayed public data at a glance — last price and change, previous close and open,
// the day's and the 52-week range as bars with the last price marked, volume against
// its 3-month average, the venue's session state and the quote's "as of" time and
// delay. A one-line note says depth needs a live broker. Compact (~9 text lines) so
// the order ticket below gets the height.

#include "services/markets/MarketDataTypes.h"

#include <QString>
#include <QWidget>

class QLabel;
class QGridLayout;

namespace fincept::screens::equity {

class EquityRangeBar;

class EquityQuoteSnapshot : public QWidget {
    Q_OBJECT
  public:
    explicit EquityQuoteSnapshot(QWidget* parent = nullptr);

    /// `title` names the instrument ("RELIANCE · NSE"); `digits` < 0 = from the quote.
    void set_quote(const QString& title, const services::QuoteData& q, qint64 received_ms, int digits = -1);
    /// No data for `title` yet (or a reason it has none).
    void set_waiting(const QString& title, const QString& message);
    /// The venue's session line, e.g. "NSE · CLOSED · opens Mon 09:15 IST".
    void set_session(const QString& text, bool open);
    /// The line under the title about market depth (empty hides it).
    void set_depth_note(const QString& text);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void apply_style();
    void retranslateUi();
    QString fmt_price(double v) const;

    QLabel* title_ = nullptr;
    QLabel* badge_ = nullptr; // "DELAYED"
    QLabel* depth_note_ = nullptr;
    QLabel* last_ = nullptr;
    QLabel* change_ = nullptr;
    QLabel* cap_prev_ = nullptr;
    QLabel* prev_ = nullptr;
    QLabel* cap_open_ = nullptr;
    QLabel* open_ = nullptr;
    QLabel* cap_day_ = nullptr;
    EquityRangeBar* day_bar_ = nullptr;
    QLabel* cap_52w_ = nullptr;
    EquityRangeBar* year_bar_ = nullptr;
    QLabel* cap_vol_ = nullptr;
    QLabel* vol_ = nullptr;
    QLabel* session_ = nullptr;
    QLabel* asof_ = nullptr;
    QString currency_;
    QString symbol_; // for FX-pair detection (no unit, pip decimals)
    int digits_ = 2;
    bool session_open_ = false;
    QString applied_sheet_;

    // Last set_quote() arguments: a currency / number-format change re-renders from them.
    bool has_quote_ = false;
    QString last_title_;
    services::QuoteData last_quote_;
    qint64 last_rx_ms_ = 0;
    int last_digits_ = -1;
};

} // namespace fincept::screens::equity
