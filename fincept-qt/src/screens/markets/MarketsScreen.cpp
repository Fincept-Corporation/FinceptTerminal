#include "screens/markets/MarketsScreen.h"

#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::screens {

static QString lbl_ss(const QString& color, bool bold = false) {
    return QString("color:%1;background:transparent;%2")
        .arg(color, bold ? "font-weight:bold;" : "");
}

MarketsScreen::MarketsScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    header_widget_   = build_header();
    controls_widget_ = build_controls();
    root->addWidget(header_widget_);
    root->addWidget(controls_widget_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:%1;}"
                "QScrollBar:vertical{width:6px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%2;border-radius:3px;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));

    auto* content = new QWidget;
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(6, 6, 6, 6);
    cvl->setSpacing(2);

    auto make_section_hdr = [&](const QString& title, const QString& count_text) {
        auto* row = new QWidget;
        row->setFixedHeight(28);
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 4, 0, 0);
        hl->setSpacing(10);
        auto* lbl = new QLabel(title);
        lbl->setStyleSheet(lbl_ss(ui::colors::AMBER(), true) + "letter-spacing:1.5px;");
        hl->addWidget(lbl);
        auto* cnt = new QLabel(count_text);
        cnt->setStyleSheet(lbl_ss(ui::colors::TEXT_DIM()));
        hl->addWidget(cnt);
        hl->addStretch();
        return row;
    };

    auto make_sep = [&]() {
        auto* s = new QFrame;
        s->setFixedHeight(1);
        s->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
        return s;
    };

    int g_count = services::MarketDataService::default_global_markets().size();
    cvl->addWidget(make_section_hdr("GLOBAL MARKETS", QString("·  %1 instruments").arg(g_count)));
    cvl->addWidget(make_sep());

    auto* gg = new QGridLayout;
    gg->setSpacing(6);
    gg->setContentsMargins(0, 6, 0, 4);
    auto globals = services::MarketDataService::default_global_markets();
    for (int i = 0; i < globals.size(); i++) {
        auto* p = new MarketPanel(globals[i].category, globals[i].tickers, false);
        global_panels_.append(p);
        gg->addWidget(p, i / 3, i % 3);
    }
    cvl->addLayout(gg);

    int r_count = services::MarketDataService::default_regional_markets().size();
    cvl->addWidget(make_section_hdr("REGIONAL MARKETS", QString("·  %1 regions").arg(r_count)));
    cvl->addWidget(make_sep());

    auto* rg = new QGridLayout;
    rg->setSpacing(4);
    rg->setContentsMargins(0, 4, 0, 4);
    auto regionals = services::MarketDataService::default_regional_markets();
    for (int i = 0; i < regionals.size(); i++) {
        QStringList syms;
        for (const auto& t : regionals[i].tickers)
            syms << t.symbol;
        auto* p = new MarketPanel(regionals[i].region, syms, true);
        regional_panels_.append(p);
        rg->addWidget(p, 0, i);
    }
    cvl->addLayout(rg);
    cvl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto_refresh_timer_ = new QTimer(this);
    auto_refresh_timer_->setInterval(update_interval_ms_);
    connect(auto_refresh_timer_, &QTimer::timeout, this, &MarketsScreen::refresh_all);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

void MarketsScreen::refresh_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    if (header_widget_)
        header_widget_->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    if (controls_widget_)
        controls_widget_->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
}

void MarketsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (auto_update_ && auto_refresh_timer_)
        auto_refresh_timer_->start();
    // Only refresh if no data yet or data is stale (> 5 min old)
    bool needs_refresh = !last_refresh_time_.isValid() ||
                         last_refresh_time_.secsTo(QDateTime::currentDateTime()) >= kMinRefreshIntervalSec;
    if (needs_refresh)
        refresh_all();
}

void MarketsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (auto_refresh_timer_)
        auto_refresh_timer_->stop();
}

QWidget* MarketsScreen::build_header() {
    auto* w = new QWidget;
    w->setFixedHeight(40);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(14, 0, 14, 0);
    h->setSpacing(0);

    auto* brand = new QLabel("FINCEPT");
    brand->setStyleSheet(lbl_ss(ui::colors::AMBER(), true));
    h->addWidget(brand);
    auto* sub = new QLabel("  MARKETS");
    sub->setStyleSheet(lbl_ss(ui::colors::TEXT_TERTIARY()));
    h->addWidget(sub);

    auto* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedSize(1, 20);
    sep1->setStyleSheet(QString("background:%1;margin:0 16px;").arg(ui::colors::BORDER_MED()));
    h->addWidget(sep1);

    auto* session_dot = new QLabel("●");
    auto* session_lbl = new QLabel("NYSE  OPEN");
    session_dot->setStyleSheet(lbl_ss(ui::colors::POSITIVE()));
    session_lbl->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
    h->addWidget(session_dot);
    h->addWidget(new QLabel(" "));
    h->addWidget(session_lbl);

    auto update_session = [session_dot, session_lbl]() {
        QDateTime utc = QDateTime::currentDateTimeUtc();
        int day = utc.date().dayOfWeek();
        QDateTime et = utc.addSecs(-4 * 3600);
        int hhmm = et.time().hour() * 100 + et.time().minute();
        bool weekday = (day >= 1 && day <= 5);
        QString label, color;
        if (!weekday || hhmm < 400 || hhmm >= 2000) {
            label = "NYSE  CLOSED";     color = ui::colors::TEXT_TERTIARY();
        } else if (hhmm < 930) {
            label = "NYSE  PRE-MKT";   color = ui::colors::AMBER();
        } else if (hhmm < 1600) {
            label = "NYSE  OPEN";      color = ui::colors::POSITIVE();
        } else {
            label = "NYSE  AFTER-HRS"; color = ui::colors::INFO();
        }
        session_dot->setStyleSheet(lbl_ss(color));
        session_lbl->setStyleSheet(lbl_ss(color, true));
        session_lbl->setText(label);
    };
    update_session();
    auto* session_timer = new QTimer(w);
    session_timer->setInterval(60000);
    connect(session_timer, &QTimer::timeout, w, update_session);
    session_timer->start();

    h->addStretch();

    auto* ny_lbl  = new QLabel; ny_lbl ->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));
    auto* lon_lbl = new QLabel; lon_lbl->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));
    auto* tok_lbl = new QLabel; tok_lbl->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));

    auto update_clocks = [ny_lbl, lon_lbl, tok_lbl]() {
        QDateTime utc = QDateTime::currentDateTimeUtc();
        auto fmt = [](const QDateTime& dt, const QString& name) {
            return QString("%1  %2").arg(name, dt.toString("HH:mm"));
        };
        ny_lbl ->setText(fmt(utc.addSecs(-4 * 3600), "NY"));
        lon_lbl->setText(fmt(utc.addSecs( 1 * 3600), "LON"));
        tok_lbl->setText(fmt(utc.addSecs( 9 * 3600), "TOK"));
    };
    update_clocks();
    auto* clock_timer = new QTimer(w);
    clock_timer->setInterval(1000);
    connect(clock_timer, &QTimer::timeout, w, update_clocks);
    clock_timer->start();

    auto* dot1 = new QLabel("   ·   "); dot1->setStyleSheet(lbl_ss(ui::colors::BORDER_MED()));
    auto* dot2 = new QLabel("   ·   "); dot2->setStyleSheet(lbl_ss(ui::colors::BORDER_MED()));
    h->addWidget(ny_lbl);
    h->addWidget(dot1);
    h->addWidget(lon_lbl);
    h->addWidget(dot2);
    h->addWidget(tok_lbl);

    return w;
}

QWidget* MarketsScreen::build_controls() {
    auto* w = new QWidget;
    w->setFixedHeight(32);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(14, 0, 14, 0);
    h->setSpacing(4);

    auto make_btn = [](const QString& key, const QString& label) {
        auto* btn = new QPushButton(QString("[%1] %2").arg(key, label));
        btn->setFixedHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 8px;font-weight:bold;}"
                    "QPushButton:hover{background:%4;color:%5;border-color:%5;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(),
                 ui::colors::BG_HOVER(), ui::colors::AMBER()));
        return btn;
    };

    auto* refresh_btn = make_btn("F5", "REFRESH");
    connect(refresh_btn, &QPushButton::clicked, this, &MarketsScreen::refresh_all);
    h->addWidget(refresh_btn);

    auto* auto_btn = make_btn("F9", auto_update_ ? "AUTO ON" : "AUTO OFF");
    auto update_auto_style = [this, auto_btn]() {
        auto_btn->setText(QString("[F9] %1").arg(auto_update_ ? "AUTO ON" : "AUTO OFF"));
        auto_btn->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 8px;font-weight:bold;}"
                    "QPushButton:hover{background:%4;border-color:%5;color:%5;}")
            .arg(ui::colors::BG_RAISED(),
                 auto_update_ ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY(),
                 auto_update_ ? ui::colors::POSITIVE() : ui::colors::BORDER_DIM(),
                 ui::colors::BG_HOVER(), ui::colors::AMBER()));
    };
    update_auto_style();
    connect(auto_btn, &QPushButton::clicked, this, [this, update_auto_style]() {
        auto_update_ = !auto_update_;
        update_auto_style();
        if (auto_update_) auto_refresh_timer_->start();
        else              auto_refresh_timer_->stop();
    });
    h->addWidget(auto_btn);

    auto* iv = new QComboBox;
    iv->setFixedHeight(22);
    iv->addItem("10m", 600000);
    iv->addItem("15m", 900000);
    iv->addItem("30m", 1800000);
    iv->addItem("1h",  3600000);
    iv->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:0 6px;}"
                "QComboBox::drop-down{border:none;width:14px;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;border:1px solid %3;"
                "selection-background-color:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
             ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(iv, &QComboBox::currentIndexChanged, this, [this, iv](int i) {
        update_interval_ms_ = iv->itemData(i).toInt();
        auto_refresh_timer_->setInterval(update_interval_ms_);
    });
    h->addWidget(iv);
    h->addStretch();

    last_update_label_ = new QLabel("LAST UPDATE  --:--:--");
    last_update_label_->setStyleSheet(lbl_ss(ui::colors::TEXT_DIM()));
    h->addWidget(last_update_label_);

    h->addWidget(new QLabel("   "));

    status_label_ = new QLabel("● READY");
    status_label_->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
    h->addWidget(status_label_);

    return w;
}

void MarketsScreen::refresh_all() {
    if (refresh_in_progress_) return;
    refresh_in_progress_ = true;

    if (status_label_) {
        status_label_->setText("● LOADING");
        status_label_->setStyleSheet(lbl_ss(ui::colors::AMBER(), true));
    }

    for (auto* p : global_panels_)   p->refresh();
    for (auto* p : regional_panels_) p->refresh();

    const int total = global_panels_.size() + regional_panels_.size();
    if (total == 0) { refresh_in_progress_ = false; return; }

    auto* counter   = new QObject(this);
    auto* remaining = new int(total);
    auto finish = [this, counter, remaining]() {
        if (--(*remaining) > 0) return;
        refresh_in_progress_ = false;
        last_refresh_time_ = QDateTime::currentDateTime();
        delete remaining;
        counter->deleteLater();
        if (status_label_) {
            status_label_->setText("● READY");
            status_label_->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
        }
        if (last_update_label_) {
            last_update_label_->setText(
                QString("LAST UPDATE  %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            last_update_label_->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));
        }
    };
    for (auto* p : global_panels_)
        connect(p, &MarketPanel::refresh_finished, counter, finish, Qt::SingleShotConnection);
    for (auto* p : regional_panels_)
        connect(p, &MarketPanel::refresh_finished, counter, finish, Qt::SingleShotConnection);
}

} // namespace fincept::screens
