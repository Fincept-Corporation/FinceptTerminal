#include "screens/polymarket/PolymarketStatusBar.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;

PolymarketStatusBar::PolymarketStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(22);
    setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(14);

    // Brand
    brand_label_ = new QLabel("POLYMARKET");
    brand_label_->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 1.5px; "
                "background: transparent;")
            .arg(accent_.name()));
    hl->addWidget(brand_label_);

    // Divider
    auto* d1 = new QWidget;
    d1->setFixedSize(1, 12);
    d1->setStyleSheet(QString("background: %1;").arg(colors::BORDER_DIM()));
    hl->addWidget(d1);

    // Exchange status (Kalshi)
    exchange_status_ = new QLabel;
    exchange_status_->setVisible(false);
    exchange_status_->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; background: transparent;").arg(colors::POSITIVE()));
    hl->addWidget(exchange_status_);

    // Next session
    next_session_ = new QLabel;
    next_session_->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent;").arg(colors::TEXT_DIM()));
    next_session_->setVisible(false);
    hl->addWidget(next_session_);

    hl->addStretch(1);

    // View
    view_label_ = new QLabel("MARKETS");
    view_label_->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 600; background: transparent;")
            .arg(colors::TEXT_DIM()));
    hl->addWidget(view_label_);

    // Count
    count_label_ = new QLabel;
    count_label_->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent;").arg(colors::TEXT_DIM()));
    hl->addWidget(count_label_);

    // Divider
    auto* d2 = new QWidget;
    d2->setFixedSize(1, 12);
    d2->setStyleSheet(QString("background: %1;").arg(colors::BORDER_DIM()));
    hl->addWidget(d2);

    // Selected market (truncated)
    selected_label_ = new QLabel;
    selected_label_->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent; "
                "max-width: 360px;")
            .arg(accent_.name()));
    hl->addWidget(selected_label_);

    // Divider
    auto* d3 = new QWidget;
    d3->setFixedSize(1, 12);
    d3->setStyleSheet(QString("background: %1;").arg(colors::BORDER_DIM()));
    hl->addWidget(d3);

    // WS
    ws_label_ = new QLabel;
    ws_label_->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent;").arg(colors::TEXT_DIM()));
    hl->addWidget(ws_label_);
    set_ws_status(false);
}

void PolymarketStatusBar::set_view(const QString& view) {
    view_label_->setText(view);
}

void PolymarketStatusBar::set_count(int count, const QString& label) {
    count_label_->setText(QString::number(count) + " " + label);
}

void PolymarketStatusBar::set_selected(const QString& question) {
    QString q = question.left(60);
    if (question.size() > 60) q += "…";
    selected_label_->setText(q);
}

void PolymarketStatusBar::set_ws_status(bool connected) {
    if (connected) {
        ws_label_->setText("● LIVE");
        ws_label_->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: 700; background: transparent;")
                .arg(colors::POSITIVE()));
    } else {
        ws_label_->setText("○ OFF");
        ws_label_->setStyleSheet(
            QString("color: %1; font-size: 8px; background: transparent;").arg(colors::TEXT_DIM()));
    }
}

void PolymarketStatusBar::set_brand(const QString& name, const QColor& accent) {
    accent_ = accent;
    brand_label_->setText(name.toUpper());
    brand_label_->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 1.5px; "
                "background: transparent;")
            .arg(accent_.name()));
    selected_label_->setStyleSheet(
        QString("color: %1; font-size: 8px; background: transparent;").arg(accent_.name()));
}

void PolymarketStatusBar::set_exchange_status(const QString& status) {
    if (status.isEmpty()) {
        exchange_status_->setVisible(false);
        return;
    }
    const QString upper = status.toUpper();
    QString color = colors::POSITIVE();
    if (upper == QStringLiteral("PAUSED") || upper == QStringLiteral("MAINT"))
        color = colors::AMBER();
    else if (upper == QStringLiteral("CLOSED") || upper == QStringLiteral("OFF"))
        color = colors::NEGATIVE();

    exchange_status_->setText(upper);
    exchange_status_->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; background: transparent;"
                "padding: 0 4px; border: 1px solid %1;")
            .arg(color));
    exchange_status_->setVisible(true);
}

void PolymarketStatusBar::set_next_session(const QString& text) {
    if (text.isEmpty()) {
        next_session_->setVisible(false);
        return;
    }
    next_session_->setText(text);
    next_session_->setVisible(true);
}

} // namespace fincept::screens::polymarket
