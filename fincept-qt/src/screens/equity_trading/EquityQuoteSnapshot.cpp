// EquityQuoteSnapshot.cpp — see header.
#include "screens/equity_trading/EquityQuoteSnapshot.h"

#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"

#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::equity {

namespace fmt = fincept::ui::formatting;

// A low—high range with the last price marked. Painted from theme tokens at paint
// time, so a theme switch needs only a repaint (which the restyle pass triggers).
class EquityRangeBar : public QWidget {
  public:
    explicit EquityRangeBar(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, this,
                [this](const fincept::ui::ThemeTokens&) { update(); });
    }
    void set_range(double low, double high, double last, const QString& low_text, const QString& high_text) {
        low_ = low;
        high_ = high;
        last_ = last;
        low_text_ = low_text;
        high_text_ = high_text;
        update();
    }
    QSize sizeHint() const override {
        const QFontMetrics fm(fincept::ui::fonts::font(-4));
        return {120, fm.height() + 10};
    }
    QSize minimumSizeHint() const override { return {60, sizeHint().height()}; }

  protected:
    void paintEvent(QPaintEvent*) override {
        namespace c = fincept::ui::colors;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QFont f = fincept::ui::fonts::font(-4);
        p.setFont(f);
        const QFontMetrics fm(f);
        const int text_y = fm.ascent();
        const int bar_y = fm.height() + 4;
        p.setPen(QColor(c::TEXT_SECONDARY()));
        if (!low_text_.isEmpty())
            p.drawText(0, text_y, low_text_);
        if (!high_text_.isEmpty())
            p.drawText(width() - fm.horizontalAdvance(high_text_), text_y, high_text_);
        const QRectF track(0, bar_y, width(), 3);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(c::BORDER_MED()));
        p.drawRoundedRect(track, 1.5, 1.5);
        if (!(high_ > low_) || !std::isfinite(last_))
            return;
        const double t = std::clamp((last_ - low_) / (high_ - low_), 0.0, 1.0);
        const double x = t * (width() - 1);
        p.setBrush(QColor(c::AMBER()));
        p.drawRoundedRect(QRectF(0, bar_y, x, 3), 1.5, 1.5);
        p.setBrush(QColor(c::TEXT_PRIMARY()));
        p.drawEllipse(QPointF(std::max(3.0, std::min(x, width() - 4.0)), bar_y + 1.5), 3.5, 3.5);
    }

  private:
    double low_ = 0, high_ = 0, last_ = NAN;
    QString low_text_, high_text_;
};

EquityQuoteSnapshot::EquityQuoteSnapshot(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("eqQuoteSnapshot"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    // Shared panel header: "QUOTE" + the DELAYED state chip.
    auto* header = new fincept::ui::PanelHeader(tr("Quote"), this);
    badge_ = new QLabel(this);
    badge_->setObjectName(QStringLiteral("eqSnapBadge"));
    header->add_control(badge_);
    outer->addWidget(header);
    auto* root = new QVBoxLayout;
    root->setContentsMargins(10, 6, 10, 8);
    root->setSpacing(4);
    outer->addLayout(root);

    // The instrument ("RELIANCE · NSE"); never widens the column (clipped, full text in the tooltip).
    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("eqSnapTitle"));
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    root->addWidget(title_);

    depth_note_ = new QLabel(this);
    depth_note_->setObjectName(QStringLiteral("eqSnapNote"));
    depth_note_->setWordWrap(true);
    depth_note_->hide();
    root->addWidget(depth_note_);

    auto* price_row = new QHBoxLayout;
    price_row->setSpacing(8);
    last_ = new QLabel(QStringLiteral("--"), this);
    last_->setObjectName(QStringLiteral("eqSnapLast"));
    change_ = new QLabel(this);
    change_->setObjectName(QStringLiteral("eqSnapChange"));
    price_row->addWidget(last_);
    price_row->addWidget(change_);
    price_row->addStretch(1);
    root->addLayout(price_row);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(3);
    auto cap = [this](QLabel*& out) {
        out = new QLabel(this);
        out->setObjectName(QStringLiteral("eqSnapCap"));
        return out;
    };
    auto val = [this](QLabel*& out) {
        out = new QLabel(QStringLiteral("--"), this);
        out->setObjectName(QStringLiteral("eqSnapVal"));
        out->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return out;
    };
    grid->addWidget(cap(cap_prev_), 0, 0);
    grid->addWidget(val(prev_), 0, 1);
    grid->addWidget(cap(cap_open_), 0, 2);
    grid->addWidget(val(open_), 0, 3);
    grid->addWidget(cap(cap_day_), 1, 0);
    day_bar_ = new EquityRangeBar(this);
    grid->addWidget(day_bar_, 1, 1, 1, 3);
    grid->addWidget(cap(cap_52w_), 2, 0);
    year_bar_ = new EquityRangeBar(this);
    grid->addWidget(year_bar_, 2, 1, 1, 3);
    grid->addWidget(cap(cap_vol_), 3, 0);
    grid->addWidget(val(vol_), 3, 1, 1, 3);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    root->addLayout(grid);

    session_ = new QLabel(this);
    session_->setObjectName(QStringLiteral("eqSnapSession"));
    session_->setWordWrap(true);
    root->addWidget(session_);
    asof_ = new QLabel(this);
    asof_->setObjectName(QStringLiteral("eqSnapAsOf"));
    asof_->setWordWrap(true);
    root->addWidget(asof_);

    retranslateUi();
    apply_style();
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this,
            [this]() { apply_style(); });
    // Currency / number-format change: re-render the last quote held (no fetch).
    fmt::on_format_changed(this, [this]() {
        if (has_quote_)
            set_quote(last_title_, last_quote_, last_rx_ms_, last_digits_);
    });
    setAccessibleName(tr("Market snapshot"));
}

void EquityQuoteSnapshot::retranslateUi() {
    badge_->setText(tr("DELAYED"));
    cap_prev_->setText(tr("PREV CLOSE"));
    cap_open_->setText(tr("OPEN"));
    cap_day_->setText(tr("DAY RANGE"));
    cap_52w_->setText(tr("52W RANGE"));
    cap_vol_->setText(tr("VOLUME"));
}

void EquityQuoteSnapshot::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityQuoteSnapshot::apply_style() {
    namespace c = fincept::ui::colors;
    // Markers %1..%14, one argument each, in order.
    const QString sheet =
        QStringLiteral("#eqQuoteSnapshot{background:%1;border-bottom:1px solid %2;}"
                       "#eqQuoteSnapshot QLabel{background:transparent;font-family:%3;}"
                       "QLabel#eqSnapTitle{color:%4;font-weight:700;font-size:%7px;}"
                       "QLabel#eqSnapBadge{color:%5;border:1px solid %5;padding:0 4px;font-weight:700;font-size:%8px;}"
                       "QLabel#eqSnapNote{color:%6;font-size:%9px;}"
                       "QLabel#eqSnapLast{color:%4;font-weight:700;font-size:%10px;}"
                       "QLabel#eqSnapChange{color:%6;font-weight:700;font-size:%7px;}"
                       "QLabel#eqSnapChange[dir=\"up\"]{color:%11;}"
                       "QLabel#eqSnapChange[dir=\"down\"]{color:%12;}"
                       "QLabel#eqSnapCap{color:%6;font-size:%8px;font-weight:600;}"
                       "QLabel#eqSnapVal{color:%4;font-size:%13px;}"
                       "QLabel#eqSnapSession{color:%6;font-size:%9px;}"
                       "QLabel#eqSnapSession[open=\"true\"]{color:%14;}"
                       "QLabel#eqSnapAsOf{color:%6;font-size:%9px;}")
            .arg(c::BG_SURFACE(), c::BORDER_DIM(), QString(fincept::ui::fonts::DATA_FAMILY), c::TEXT_PRIMARY(), c::WARNING(),
                 c::TEXT_SECONDARY())
            .arg(fincept::ui::fonts::px(-2))
            .arg(fincept::ui::fonts::px(-5))
            .arg(fincept::ui::fonts::px(-4))
            .arg(fincept::ui::fonts::px(3))
            .arg(c::POSITIVE(), c::NEGATIVE())
            .arg(fincept::ui::fonts::px(-3))
            .arg(c::SUCCESS());
    if (sheet != applied_sheet_) {
        applied_sheet_ = sheet;
        setStyleSheet(sheet);
    }
}

namespace {
// Direction / state as a dynamic property: a re-polish only when it changes.
void eqqs_set_state(QLabel* label, const char* name, const QString& value) {
    if (label->property(name).toString() == value)
        return;
    label->setProperty(name, value);
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

QString EquityQuoteSnapshot::fmt_price(double v) const {
    if (!std::isfinite(v) || v <= 0.0)
        return fmt::placeholder();
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol_, currency_);
    inst.decimals = digits_;
    return fmt::price(v, inst);
}

void EquityQuoteSnapshot::set_waiting(const QString& title, const QString& message) {
    has_quote_ = false;
    title_->setText(title);
    title_->setToolTip(title);
    last_->setText(fmt::placeholder());
    change_->setText(message);
    eqqs_set_state(change_, "dir", QString());
    prev_->setText(fmt::placeholder());
    open_->setText(fmt::placeholder());
    vol_->setText(fmt::placeholder());
    day_bar_->set_range(0, 0, NAN, {}, {});
    year_bar_->set_range(0, 0, NAN, {}, {});
    asof_->clear();
}

void EquityQuoteSnapshot::set_quote(const QString& title, const services::QuoteData& q, qint64 received_ms,
                                    int digits) {
    if (&q != &last_quote_) { // kept for a format-change re-render (which passes these members back)
        last_title_ = title;
        last_quote_ = q;
        last_rx_ms_ = received_ms;
        last_digits_ = digits;
    }
    has_quote_ = true;
    currency_ = q.currency;
    symbol_ = q.symbol;
    digits_ = digits >= 0 ? digits : (q.price_digits >= 0 ? q.price_digits : -1);
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol_, currency_);
    inst.decimals = digits_;
    title_->setText(title);
    title_->setToolTip(title);
    // The last price in its own currency; an index level or FX rate gets no unit.
    const bool money = fmt::is_money_quote(q.quote_type, q.symbol);
    last_->setText(std::isfinite(q.price) && q.price > 0.0
                       ? (money ? fmt::price_with_symbol(q.price, inst) : fmt::price(q.price, inst))
                       : fmt::placeholder());

    // Direction by arrow AND colour (never colour alone).
    const QString arrow = q.change > 0.0 ? QStringLiteral("\xE2\x96\xB2 ") : (q.change < 0.0 ? QStringLiteral("\xE2\x96\xBC ") : QString());
    const QString chg = std::isfinite(q.change)
                            ? arrow + fmt::price_change(q.change, q.price, inst) +
                                  QStringLiteral(" (") + fmt::pct(q.change_pct) + QStringLiteral(")")
                            : fmt::placeholder();
    change_->setText(chg);
    eqqs_set_state(change_, "dir",
                   q.change > 0.0   ? QStringLiteral("up")
                   : q.change < 0.0 ? QStringLiteral("down")
                                    : QString());

    prev_->setText(fmt_price(q.previous_close));
    open_->setText(fmt_price(q.open));
    day_bar_->set_range(q.low, q.high, q.price, fmt_price(q.low), fmt_price(q.high));
    year_bar_->set_range(q.fifty_two_week_low, q.fifty_two_week_high, q.price, fmt_price(q.fifty_two_week_low),
                         fmt_price(q.fifty_two_week_high));

    QString vol = q.volume > 0.0 ? fmt::volume(q.volume) : fmt::placeholder();
    if (q.volume > 0.0 && std::isfinite(q.average_volume) && q.average_volume > 0.0)
        vol += tr("  (%1\xC3\x97 3-month avg)").arg(q.volume / q.average_volume, 0, 'f', 2);
    vol_->setText(vol);

    // As of: the exchange time of the last trade, in the exchange's zone, + the delay.
    QString asof;
    if (q.market_time > 0) {
        QDateTime t = QDateTime::fromSecsSinceEpoch(q.market_time, QTimeZone::UTC);
        const QTimeZone tz(q.exchange_timezone.toUtf8());
        if (tz.isValid())
            t = t.toTimeZone(tz);
        asof = tr("Last trade %1 %2").arg(t.toString(QStringLiteral("ddd HH:mm")), tz.isValid() ? tz.abbreviation(t) : QStringLiteral("UTC"));
    }
    if (q.exchange_delay_min > 0)
        asof += (asof.isEmpty() ? QString() : QStringLiteral(" \xC2\xB7 ")) + tr("delayed %1 min").arg(q.exchange_delay_min);
    else if (q.exchange_delay_min == 0)
        asof += (asof.isEmpty() ? QString() : QStringLiteral(" \xC2\xB7 ")) + tr("real-time feed");
    if (received_ms > 0)
        asof += (asof.isEmpty() ? QString() : QStringLiteral(" \xC2\xB7 ")) +
                tr("updated %1").arg(QDateTime::fromMSecsSinceEpoch(received_ms).toString(QStringLiteral("HH:mm:ss")));
    asof_->setText(asof);
}

void EquityQuoteSnapshot::set_session(const QString& text, bool open) {
    session_open_ = open;
    session_->setText(text);
    eqqs_set_state(session_, "open", open ? QStringLiteral("true") : QString());
}

void EquityQuoteSnapshot::set_depth_note(const QString& text) {
    depth_note_->setText(text);
    depth_note_->setVisible(!text.isEmpty());
}

} // namespace fincept::screens::equity
