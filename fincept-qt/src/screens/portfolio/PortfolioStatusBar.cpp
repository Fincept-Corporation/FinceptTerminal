// src/screens/portfolio/PortfolioStatusBar.cpp
#include "screens/portfolio/PortfolioStatusBar.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QTimeZone>

namespace fincept::screens {

PortfolioStatusBar::PortfolioStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);
    setObjectName("portfolioStatusBar");
    setStyleSheet(QString("#portfolioStatusBar { background:%1; border-top:1px solid %2; }")
                      .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(0);

    auto make_label = [&](const char* color, bool bold = false) {
        auto* lbl = new QLabel;
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:%2; letter-spacing:0.3px;")
                               .arg(color)
                               .arg(bold ? 700 : 400));
        layout->addWidget(lbl);
        return lbl;
    };

    auto add_divider = [&]() {
        auto* sep = new QLabel(" | ");
        sep->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::BORDER_MED()));
        layout->addWidget(sep);
    };

    // Left section
    brand_label_ = make_label(ui::colors::AMBER, true);
    brand_label_->setText("FINCEPT");

    add_divider();

    version_label_ = make_label(ui::colors::TEXT_TERTIARY);
    version_label_->setText(tr("PORTFOLIO TERMINAL v4.0"));

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
    time_label_->setText(now.toString("hh:mm:ss"));
    tz_label_->setText(QString(" %1").arg(now.timeZone().abbreviation(now)));
}

void PortfolioStatusBar::set_portfolio_name(const QString& name) {
    portfolio_label_->setText(name.toUpper());
}

void PortfolioStatusBar::set_summary(const portfolio::PortfolioSummary& s) {
    last_summary_ = s;
    auto fmt = [](double v) { return QString::number(v, 'f', 2); };

    set_portfolio_name(s.portfolio.name);
    positions_label_->setText(tr("%1 positions").arg(s.total_positions));

    nav_label_->setText(tr("NAV %1 %2").arg(s.portfolio.currency, fmt(s.total_market_value)));

    double pnl = s.total_unrealized_pnl;
    pnl_label_->setText(tr("P&L %1%2").arg(pnl >= 0 ? "+" : "").arg(fmt(pnl)));
    pnl_label_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600;")
                                  .arg(pnl >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

void PortfolioStatusBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioStatusBar::retranslateUi() {
    if (version_label_)  version_label_->setText(tr("PORTFOLIO TERMINAL v4.0"));
    if (live_label_)     live_label_->setText("● " + tr("LIVE"));

    if (last_summary_.has_value()) {
        // Re-render dynamic strings from cached summary.
        set_summary(*last_summary_);
    } else if (positions_label_) {
        positions_label_->setText(tr("0 positions"));
    }
}

} // namespace fincept::screens
