// src/screens/portfolio/PortfolioStatusBar.cpp
#include "screens/portfolio/PortfolioStatusBar.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QStyle>
#include <QTimeZone>

#include <algorithm>

namespace fincept::screens {

PortfolioStatusBar::PortfolioStatusBar(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(24);
    setObjectName("portfolioStatusBar");
    // The notes label is styled from here (property selector) rather than with
    // a per-update setStyleSheet on the label itself.
    setStyleSheet(QString("#portfolioStatusBar { background:%1; border-top:1px solid %2; }"
                          "#pfStatusNotes { color:%3; font-size:" + fincept::ui::fonts::px(-4) + "px; letter-spacing:0.3px; }"
                          "#pfStatusNotes[warn=\"true\"] { color:%4; font-weight:700; }")
                      .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY(),
                           ui::colors::WARNING()));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(0);

    auto make_label = [&](const char* color, bool bold = false) {
        auto* lbl = new QLabel;
        lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:%2; letter-spacing:0.3px;")
                               .arg(color)
                               .arg(bold ? 700 : 400));
        layout->addWidget(lbl);
        return lbl;
    };

    auto add_divider = [&]() {
        auto* sep = new QLabel(" | ");
        sep->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::BORDER_MED()));
        layout->addWidget(sep);
    };

    // Left section
    brand_label_ = make_label(ui::colors::AMBER, true);
    brand_label_->setText("FINCEPT");

    add_divider();

    // Was a hard-coded "PORTFOLIO TERMINAL v4.0" (stale version, no information).
    notes_label_ = new QLabel;
    notes_label_->setObjectName("pfStatusNotes");
    layout->addWidget(notes_label_);

    add_divider();

    portfolio_label_ = make_label(ui::colors::CYAN, true);
    portfolio_label_->setText("");

    add_divider();

    // Live indicator
    live_label_ = make_label(ui::colors::POSITIVE, true);
    live_label_->setText("\u25CF " + tr("LIVE"));

    add_divider();

    positions_label_ = make_label(ui::colors::TEXT_SECONDARY);
    positions_label_->setText(tr("0 positions"));

    layout->addStretch();

    // Right section
    nav_label_ = make_label(ui::colors::WARNING, true);

    add_divider();

    pnl_label_ = make_label(ui::colors::TEXT_PRIMARY);

    add_divider();

    time_label_ = make_label(ui::colors::CYAN, true);

    tz_label_ = make_label(ui::colors::AMBER, true);

    // Clock timer (P3: don't start in constructor)
    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &PortfolioStatusBar::update_clock);

    // Currency / number format changed: re-format the last summary's text (no restyle).
    ui::formatting::on_format_changed(this, [this]() { render_summary_text(); });
}

void PortfolioStatusBar::start_clock() {
    clock_timer_->start();
    update_clock();
}

void PortfolioStatusBar::stop_clock() {
    clock_timer_->stop();
}

void PortfolioStatusBar::update_clock() {
    auto now = QDateTime::currentDateTime();
    time_label_->setText(ui::formatting::time(now, /*with_seconds=*/true));
    // The timezone abbreviation is an expensive lookup and changes at most
    // twice a year — do not recompute + re-set it every single second.
    const QString abbrev = now.timeZone().abbreviation(now);
    if (abbrev != tz_abbrev_) {
        tz_abbrev_ = abbrev;
        tz_label_->setText(QString(" %1").arg(abbrev));
    }
}

void PortfolioStatusBar::set_feed_state(bool live, const QString& detail) {
    feed_live_ = live;
    if (!live_label_)
        return;
    live_label_->setText(QString("● ") + (live ? tr("LIVE") : tr("STALE")));
    live_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.3px;")
                                   .arg(live ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    live_label_->setToolTip(detail.isEmpty()
                                ? (live ? tr("Market data refreshed successfully.")
                                        : tr("The last refresh failed — values shown may be out of date."))
                                : detail);
}

void PortfolioStatusBar::set_portfolio_name(const QString& name) {
    portfolio_label_->setText(name.toUpper());
}

void PortfolioStatusBar::set_summary(const portfolio::PortfolioSummary& s) {
    last_summary_ = s;
    set_portfolio_name(s.portfolio.name);
    render_summary_text();

    const double pnl = s.total_unrealized_pnl;
    pnl_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:600;")
                                  .arg(pfmt::dir_css(pnl)));
}

void PortfolioStatusBar::render_summary_text() {
    if (!last_summary_.has_value())
        return;
    const auto& s = *last_summary_;
    const QString ccy = pfmt::base_ccy(s);
    positions_label_->setText(s.complete ? tr("%1 positions").arg(s.total_positions)
                                         : tr("%1 of %2 positions valued").arg(s.valued_positions).arg(s.total_positions));

    // "≈" when a conversion used a stale FX rate; "*" when the valuation is incomplete.
    nav_label_->setText(tr("NAV %1").arg(pfmt::approx_prefix(s) + pfmt::money(s.total_market_value, ccy)) +
                        (s.complete ? QString() : QStringLiteral(" *")));
    QString nav_tip = s.complete ? tr("Net asset value in %1, as of %2.").arg(ccy, pfmt::date(s.as_of))
                                 : pfmt::incomplete_line(s);
    if (!s.fx_stale.isEmpty())
        nav_tip += QLatin1Char('\n') + pfmt::fx_stale_line(s);
    nav_label_->setToolTip(nav_tip);

    render_notes();

    pnl_label_->setText(tr("P&L %1").arg(pfmt::approx_prefix(s) + pfmt::smoney(s.total_unrealized_pnl, ccy)));
}

void PortfolioStatusBar::set_analytics_note(const QString& text, const QString& tooltip, bool warn) {
    analytics_note_ = text;
    analytics_tip_ = tooltip;
    analytics_warn_ = warn;
    render_notes();
}

void PortfolioStatusBar::render_notes() {
    if (!notes_label_)
        return;
    QString text = analytics_note_;
    QString tip = analytics_tip_;
    bool warn = analytics_warn_;
    if (last_summary_.has_value() && !last_summary_->complete) {
        const auto& s = *last_summary_;
        const int missing = std::max(0, s.total_positions - s.valued_positions);
        text = tr("⚠ VALUATION INCOMPLETE — %n POSITION(S) NOT VALUED", "", missing);
        tip = pfmt::incomplete_line(s) + (s.fx_stale.isEmpty() ? QString() : QLatin1Char('\n') + pfmt::fx_stale_line(s)) +
              (analytics_tip_.isEmpty() ? QString() : QStringLiteral("\n\n") + analytics_tip_);
        warn = true;
    } else if (last_summary_.has_value() && !last_summary_->fx_stale.isEmpty()) {
        // A stale rate is still used, never hidden: say so beside the figures it moves.
        const auto& s = *last_summary_;
        text = tr("⚠ FX RATE STALE — %1").arg(s.fx_stale.join(QStringLiteral(", ")));
        tip = pfmt::fx_stale_line(s) + (analytics_tip_.isEmpty() ? QString() : QStringLiteral("\n\n") + analytics_tip_);
        warn = true;
    } else if (last_summary_.has_value() && !last_summary_->ledger_warnings.isEmpty()) {
        // Ledger notes (unmatched sells, missing trade FX) explain why realised figures may be partial.
        if (text.isEmpty())
            text = tr("%n LEDGER NOTE(S)", "", static_cast<int>(last_summary_->ledger_warnings.size()));
        tip = last_summary_->ledger_warnings.mid(0, 12).join(QLatin1Char('\n')) +
              (analytics_tip_.isEmpty() ? QString() : QStringLiteral("\n\n") + analytics_tip_);
    }
    notes_label_->setText(text);
    notes_label_->setToolTip(tip);
    if (notes_label_->property("warn").toBool() != warn) {
        notes_label_->setProperty("warn", warn);
        notes_label_->style()->unpolish(notes_label_);
        notes_label_->style()->polish(notes_label_);
    }
}

void PortfolioStatusBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioStatusBar::retranslateUi() {
    set_feed_state(feed_live_); // re-renders LIVE/STALE + its tooltip

    if (last_summary_.has_value()) {
        // Re-render dynamic strings from cached summary.
        set_summary(*last_summary_);
    } else if (positions_label_) {
        positions_label_->setText(tr("0 positions"));
    }
}

} // namespace fincept::screens
