#include "screens/dashboard/DashboardToolBar.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>

namespace fincept::screens {

DashboardToolBar::DashboardToolBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet(QString(
        "background: %1; border-bottom: 2px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::AMBER));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    // ── Left: Terminal Identity ──
    auto* left = new QWidget;
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    auto* terminal_icon = new QLabel(QChar(0x25A0));  // ■
    terminal_icon->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER));
    ll->addWidget(terminal_icon);

    auto* brand = new QLabel("FINCEPT");
    brand->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; letter-spacing: 1px; background: transparent;").arg(ui::colors::AMBER));
    ll->addWidget(brand);

    auto* sub = new QLabel("TERMINAL");
    sub->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
    ll->addWidget(sub);

    // Separator
    auto* sep1 = new QLabel;
    sep1->setFixedSize(1, 16);
    sep1->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_MED));
    ll->addWidget(sep1);

    // Connection status
    status_dot_ = new QLabel;
    status_dot_->setFixedSize(6, 6);
    status_dot_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ui::colors::POSITIVE));
    ll->addWidget(status_dot_);

    status_text_ = new QLabel("LIVE");
    status_text_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(ui::colors::POSITIVE));
    ll->addWidget(status_text_);

    auto* sep2 = new QLabel;
    sep2->setFixedSize(1, 16);
    sep2->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_MED));
    ll->addWidget(sep2);

    // Clock
    auto* clock_icon = new QLabel(QChar(0x25CB));  // ○
    clock_icon->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::WARNING));
    ll->addWidget(clock_icon);

    clock_label_ = new QLabel;
    clock_label_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(ui::colors::WARNING));
    ll->addWidget(clock_label_);
    update_clock();

    auto* sep3 = new QLabel;
    sep3->setFixedSize(1, 16);
    sep3->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_MED));
    ll->addWidget(sep3);

    // Widget count
    widget_count_ = new QLabel("0 WIDGETS");
    widget_count_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(ui::colors::CYAN));
    ll->addWidget(widget_count_);

    hl->addWidget(left);
    hl->addStretch();

    // ── Right: Action buttons ──
    auto* right = new QWidget;
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);

    auto btn_style = [](const char* border_color, const char* text_color) {
        return QString(
            "QPushButton { background: transparent; border: 1px solid %1; color: %2; "
            "padding: 3px 10px; font-size: 9px; font-weight: bold; font-family: Consolas; }"
            "QPushButton:hover { border-color: %3; color: #e5e5e5; }")
            .arg(border_color, text_color, ui::colors::AMBER);
    };

    auto btn_primary = QString(
        "QPushButton { background: %1; border: 1px solid %1; color: %2; "
        "padding: 3px 10px; font-size: 9px; font-weight: bold; font-family: Consolas; }"
        "QPushButton:hover { background: #e8860a; }")
        .arg(ui::colors::AMBER, ui::colors::BG_BASE);

    auto btn_danger = QString(
        "QPushButton { background: transparent; border: 1px solid %1; color: %1; "
        "padding: 3px 10px; font-size: 9px; font-weight: bold; font-family: Consolas; }"
        "QPushButton:hover { border-color: %1; background: rgba(220,38,38,0.1); }")
        .arg(ui::colors::NEGATIVE);

    compact_btn_ = new QPushButton("COMPACT");
    compact_btn_->setStyleSheet(btn_style(ui::colors::BORDER_MED, ui::colors::TEXT_SECONDARY));
    connect(compact_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_compact_clicked);
    rl->addWidget(compact_btn_);

    pulse_btn_ = new QPushButton("PULSE");
    pulse_btn_->setStyleSheet(btn_style(ui::colors::BORDER_MED, ui::colors::TEXT_SECONDARY));
    connect(pulse_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_pulse_clicked);
    rl->addWidget(pulse_btn_);

    auto* sep4 = new QLabel;
    sep4->setFixedSize(1, 20);
    sep4->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_MED));
    rl->addWidget(sep4);

    auto* add_btn = new QPushButton("+ ADD");
    add_btn->setStyleSheet(btn_primary);
    connect(add_btn, &QPushButton::clicked, this, &DashboardToolBar::add_widget_clicked);
    rl->addWidget(add_btn);

    auto* save_btn = new QPushButton("SAVE");
    save_btn->setStyleSheet(btn_style(ui::colors::BORDER_MED, ui::colors::TEXT_SECONDARY));
    connect(save_btn, &QPushButton::clicked, this, &DashboardToolBar::save_layout_clicked);
    rl->addWidget(save_btn);

    auto* reset_btn = new QPushButton("RESET");
    reset_btn->setStyleSheet(btn_danger);
    connect(reset_btn, &QPushButton::clicked, this, &DashboardToolBar::reset_layout_clicked);
    rl->addWidget(reset_btn);

    hl->addWidget(right);

    // Clock timer
    connect(&clock_timer_, &QTimer::timeout, this, &DashboardToolBar::update_clock);
    clock_timer_.start(1000);
}

void DashboardToolBar::update_clock() {
    auto now = QDateTime::currentDateTimeUtc();
    clock_label_->setText(now.toString("yyyy-MM-dd HH:mm:ss") + " UTC");
}

void DashboardToolBar::set_widget_count(int count) {
    widget_count_->setText(QString("%1 WIDGETS").arg(count));
}

void DashboardToolBar::set_connected(bool connected) {
    connected_ = connected;
    if (connected) {
        status_dot_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ui::colors::POSITIVE));
        status_text_->setText("LIVE");
        status_text_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(ui::colors::POSITIVE));
    } else {
        status_dot_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ui::colors::NEGATIVE));
        status_text_->setText("OFFLINE");
        status_text_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(ui::colors::NEGATIVE));
    }
}

} // namespace fincept::screens
