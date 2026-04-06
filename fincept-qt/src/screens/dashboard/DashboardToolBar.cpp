#include "screens/dashboard/DashboardToolBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>

namespace fincept::screens {

static QString lbl_ss(const QString& color, bool bold = false) {
    return QString("color:%1;background:transparent;%2")
        .arg(color, bold ? "font-weight:bold;" : "");
}

static QString btn_ss(const QString& border, const QString& color) {
    return QString("QPushButton{background:%1;border:1px solid %2;color:%3;padding:0 10px;font-weight:bold;}"
                   "QPushButton:hover{background:%4;color:%5;border-color:%6;}")
        .arg(ui::colors::BG_RAISED(), border, color,
             ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_BRIGHT());
}

DashboardToolBar::DashboardToolBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto* left = new QWidget;
    left->setStyleSheet("background:transparent;");
    auto* ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);

    auto make_sep = [&](QBoxLayout* layout) {
        auto* s = new QLabel("|");
        s->setStyleSheet(lbl_ss(ui::colors::BORDER_MED()));
        layout->addWidget(s);
    };

    auto* brand = new QLabel("FINCEPT");
    brand->setStyleSheet(lbl_ss(ui::colors::AMBER(), true) + "letter-spacing:1px;");
    ll->addWidget(brand);

    auto* sub = new QLabel("TERMINAL");
    sub->setStyleSheet(lbl_ss(ui::colors::TEXT_TERTIARY(), true));
    ll->addWidget(sub);

    make_sep(ll);

    status_text_ = new QLabel("LIVE");
    status_text_->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
    ll->addWidget(status_text_);

    make_sep(ll);

    clock_label_ = new QLabel;
    clock_label_->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));
    ll->addWidget(clock_label_);
    update_clock();

    make_sep(ll);

    widget_count_ = new QLabel("0 WIDGETS");
    widget_count_->setStyleSheet(lbl_ss(ui::colors::CYAN(), true));
    ll->addWidget(widget_count_);

    hl->addWidget(left);
    hl->addStretch();

    auto* right = new QWidget;
    right->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);

    compact_btn_ = new QPushButton("COMPACT");
    compact_btn_->setFixedHeight(20);
    compact_btn_->setStyleSheet(btn_ss(ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY()));
    connect(compact_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_compact_clicked);
    rl->addWidget(compact_btn_);

    pulse_btn_ = new QPushButton("PULSE");
    pulse_btn_->setFixedHeight(20);
    pulse_btn_->setStyleSheet(btn_ss(ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY()));
    connect(pulse_btn_, &QPushButton::clicked, this, &DashboardToolBar::toggle_pulse_clicked);
    rl->addWidget(pulse_btn_);

    make_sep(rl);

    auto* add_btn = new QPushButton("+ ADD");
    add_btn->setFixedHeight(20);
    add_btn->setStyleSheet(btn_ss(ui::colors::AMBER_DIM(), ui::colors::AMBER()));
    connect(add_btn, &QPushButton::clicked, this, &DashboardToolBar::add_widget_clicked);
    rl->addWidget(add_btn);

    auto* save_btn = new QPushButton("SAVE");
    save_btn->setFixedHeight(20);
    save_btn->setStyleSheet(btn_ss(ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY()));
    connect(save_btn, &QPushButton::clicked, this, &DashboardToolBar::save_layout_clicked);
    rl->addWidget(save_btn);

    auto* reset_btn = new QPushButton("RESET");
    reset_btn->setFixedHeight(20);
    reset_btn->setStyleSheet(btn_ss(ui::colors::NEGATIVE(), ui::colors::NEGATIVE()));
    connect(reset_btn, &QPushButton::clicked, this, &DashboardToolBar::reset_layout_clicked);
    rl->addWidget(reset_btn);

    hl->addWidget(right);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { refresh_theme(); });

    connect(&clock_timer_, &QTimer::timeout, this, &DashboardToolBar::update_clock);
    clock_timer_.start(1000);

    refresh_theme();
}

void DashboardToolBar::refresh_theme() {
    setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
}

void DashboardToolBar::update_clock() {
    clock_label_->setText(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss") + " UTC");
}

void DashboardToolBar::set_widget_count(int count) {
    widget_count_->setText(QString("%1 WIDGETS").arg(count));
}

void DashboardToolBar::set_connected(bool connected) {
    connected_ = connected;
    status_text_->setText(connected ? "LIVE" : "OFFLINE");
    status_text_->setStyleSheet(
        lbl_ss(connected ? ui::colors::POSITIVE() : ui::colors::NEGATIVE(), true));
}

} // namespace fincept::screens
