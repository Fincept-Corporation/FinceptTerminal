#include "screens/markets/MarketsScreen.h"

#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"

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

MarketsScreen::MarketsScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background:#080808;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(build_header());
    root->addWidget(build_controls());

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#080808;}");
    auto* content = new QWidget;
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(6, 6, 6, 6);
    cvl->setSpacing(2);

    // Global Markets section header
    auto* gt_row = new QWidget;
    gt_row->setFixedHeight(28);
    gt_row->setStyleSheet("background:transparent;");
    auto* gt_hl = new QHBoxLayout(gt_row);
    gt_hl->setContentsMargins(0, 4, 0, 0);
    gt_hl->setSpacing(10);
    auto* gt = new QLabel("GLOBAL MARKETS");
    gt->setStyleSheet("color:#d97706;font-size:11px;font-weight:700;background:transparent;"
                      "letter-spacing:1.5px;font-family:'Consolas',monospace;");
    gt_hl->addWidget(gt);
    int g_count = services::MarketDataService::default_global_markets().size();
    auto* gt_count = new QLabel(QString("·  %1 instruments").arg(g_count));
    gt_count->setStyleSheet("color:#303030;font-size:11px;background:transparent;"
                            "font-family:'Consolas',monospace;");
    gt_hl->addWidget(gt_count);
    gt_hl->addStretch();
    cvl->addWidget(gt_row);
    auto* gs = new QFrame;
    gs->setFixedHeight(1);
    gs->setStyleSheet("background:#252525;border:none;");
    cvl->addWidget(gs);

    auto* gg = new QGridLayout;
    gg->setSpacing(6);
    gg->setContentsMargins(0, 6, 0, 4);
    auto globals = services::MarketDataService::default_global_markets();
    for (int i = 0; i < globals.size(); i++) {
        auto* p = new MarketPanel(globals[i].category, globals[i].tickers, false);
        global_panels_.append(p);
        connect(p, &MarketPanel::refresh_finished, this, &MarketsScreen::on_panel_refresh_finished);
        gg->addWidget(p, i / 3, i % 3);
    }
    cvl->addLayout(gg);

    // Regional Markets section header
    auto* rt_row = new QWidget;
    rt_row->setFixedHeight(28);
    rt_row->setStyleSheet("background:transparent;");
    auto* rt_hl = new QHBoxLayout(rt_row);
    rt_hl->setContentsMargins(0, 12, 0, 0);
    rt_hl->setSpacing(10);
    auto* rt = new QLabel("REGIONAL MARKETS");
    rt->setStyleSheet("color:#d97706;font-size:11px;font-weight:700;background:transparent;"
                      "letter-spacing:1.5px;font-family:'Consolas',monospace;");
    rt_hl->addWidget(rt);
    int r_count = services::MarketDataService::default_regional_markets().size();
    auto* rt_count = new QLabel(QString("·  %1 regions").arg(r_count));
    rt_count->setStyleSheet("color:#303030;font-size:11px;background:transparent;"
                            "font-family:'Consolas',monospace;");
    rt_hl->addWidget(rt_count);
    rt_hl->addStretch();
    cvl->addWidget(rt_row);
    auto* rs = new QFrame;
    rs->setFixedHeight(1);
    rs->setStyleSheet("background:#252525;border:none;");
    cvl->addWidget(rs);

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
        connect(p, &MarketPanel::refresh_finished, this, &MarketsScreen::on_panel_refresh_finished);
        rg->addWidget(p, 0, i);
    }
    cvl->addLayout(rg);
    cvl->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto_refresh_timer_ = new QTimer(this);
    auto_refresh_timer_->setInterval(update_interval_ms_);
    connect(auto_refresh_timer_, &QTimer::timeout, this, &MarketsScreen::refresh_all);
    // Timer starts via showEvent, not eagerly

    loading_animation_timer_ = new QTimer(this);
    loading_animation_timer_->setInterval(150);
    connect(loading_animation_timer_, &QTimer::timeout, this, [this]() {
        if (status_label_)
            status_label_->setText(loading_text());
    });
}

void MarketsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (auto_update_ && auto_refresh_timer_)
        auto_refresh_timer_->start();
    refresh_all(); // fresh data on show
}

void MarketsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (auto_refresh_timer_)
        auto_refresh_timer_->stop();
}

QWidget* MarketsScreen::build_header() {
    auto* w = new QWidget;
    w->setFixedHeight(40);
    w->setStyleSheet("background:#09090b;border-bottom:1px solid #1e1e1e;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(14, 0, 14, 0);
    h->setSpacing(0);

    auto mk = [](const QString& t, const QString& c, int sz = 12, bool b = false) {
        auto* l = new QLabel(t);
        l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
                                 "font-family:'Consolas',monospace;")
                             .arg(c).arg(sz).arg(b ? "font-weight:700;" : ""));
        return l;
    };

    // Brand
    h->addWidget(mk("FINCEPT", "#d97706", 14, true));
    h->addWidget(mk("  MARKETS", "#404040", 12));

    // Separator
    auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedSize(1, 20);
    sep1->setStyleSheet("background:#252525;margin:0 16px;");
    h->addWidget(sep1);

    // Market session badge — computed from current NY time
    auto* session_dot = mk("●", "#22c55e", 9);
    auto* session_lbl = mk("NYSE  OPEN", "#22c55e", 11, true);
    h->addWidget(session_dot);
    h->addWidget(mk(" ", "#000"));
    h->addWidget(session_lbl);

    // Update session badge every minute
    auto update_session = [session_dot, session_lbl]() {
        // NYSE: Mon-Fri 09:30-16:00 ET (UTC-5 or UTC-4 DST)
        QDateTime utc = QDateTime::currentDateTimeUtc();
        int day = utc.date().dayOfWeek(); // 1=Mon..7=Sun
        // Approximate ET as UTC-4 (EDT). Good enough for a badge.
        QDateTime et = utc.addSecs(-4 * 3600);
        int hhmm = et.time().hour() * 100 + et.time().minute();
        bool weekday = (day >= 1 && day <= 5);
        QString label, color;
        if (!weekday || hhmm < 400 || hhmm >= 2000) {
            label = "NYSE  CLOSED"; color = "#404040";
        } else if (hhmm < 930) {
            label = "NYSE  PRE-MKT"; color = "#d97706";
        } else if (hhmm < 1600) {
            label = "NYSE  OPEN";   color = "#22c55e";
        } else {
            label = "NYSE  AFTER-HRS"; color = "#6366f1";
        }
        session_dot->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;"
                                           "font-family:'Consolas',monospace;").arg(color));
        session_lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;"
                                           "font-family:'Consolas',monospace;").arg(color));
        session_lbl->setText(label);
    };
    update_session();
    auto* session_timer = new QTimer(this);
    session_timer->setInterval(60000);
    connect(session_timer, &QTimer::timeout, this, update_session);
    session_timer->start();

    h->addStretch();

    // Three market clocks
    auto* ny_lbl  = mk("", "#606060", 11);
    auto* lon_lbl = mk("", "#606060", 11);
    auto* tok_lbl = mk("", "#606060", 11);

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
    auto* clock_timer = new QTimer(this);
    clock_timer->setInterval(1000);
    connect(clock_timer, &QTimer::timeout, this, update_clocks);
    clock_timer->start();

    h->addWidget(ny_lbl);
    h->addWidget(mk("   ·   ", "#252525", 11));
    h->addWidget(lon_lbl);
    h->addWidget(mk("   ·   ", "#252525", 11));
    h->addWidget(tok_lbl);

    return w;
}

QWidget* MarketsScreen::build_controls() {
    auto* w = new QWidget;
    w->setFixedHeight(32);
    w->setStyleSheet("background:#0c0c0c;border-bottom:1px solid #1e1e1e;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(14, 0, 14, 0);
    h->setSpacing(4);

    // F-key chip helper
    auto fkey_btn = [](const QString& key, const QString& label) {
        auto* btn = new QPushButton;
        btn->setFixedHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        // Build text with key chip prefix
        btn->setText(QString("  %1  ").arg(label));
        // Custom paint via stylesheet: dim key badge + bright label
        btn->setStyleSheet(
            QString("QPushButton { background:#141414; color:#909090; border:1px solid #2a2a2a;"
                    "font-size:11px; font-family:'Consolas',monospace; padding:0 2px; }"
                    "QPushButton:hover { background:#1e1e1e; color:#d97706; border-color:#d97706; }"
                    "QPushButton:pressed { background:#d97706; color:#080808; }"));
        // Prepend key chip as part of text — use unicode thin space trick
        btn->setText(QString("[%1] %2").arg(key, label));
        Q_UNUSED(key)
        return btn;
    };

    auto* refresh_btn = fkey_btn("F5", "REFRESH");
    connect(refresh_btn, &QPushButton::clicked, this, &MarketsScreen::refresh_all);
    h->addWidget(refresh_btn);

    auto* auto_btn = fkey_btn("F9", auto_update_ ? "AUTO·ON" : "AUTO·OFF");
    auto_btn->setStyleSheet(
        QString("QPushButton{background:#141414;color:%1;border:1px solid %2;"
                "font-size:11px;font-family:'Consolas',monospace;padding:0 6px;}"
                "QPushButton:hover{background:#1e1e1e;border-color:#d97706;color:#d97706;}")
            .arg(auto_update_ ? "#22c55e" : "#505050",
                 auto_update_ ? "#1a3a1a"  : "#252525"));
    connect(auto_btn, &QPushButton::clicked, this, [this, auto_btn]() {
        auto_update_ = !auto_update_;
        auto_btn->setText(QString("[F9] %1").arg(auto_update_ ? "AUTO·ON" : "AUTO·OFF"));
        auto_btn->setStyleSheet(
            QString("QPushButton{background:#141414;color:%1;border:1px solid %2;"
                    "font-size:11px;font-family:'Consolas',monospace;padding:0 6px;}"
                    "QPushButton:hover{background:#1e1e1e;border-color:#d97706;color:#d97706;}")
                .arg(auto_update_ ? "#22c55e" : "#505050",
                     auto_update_ ? "#1a3a1a"  : "#252525"));
        if (auto_update_) auto_refresh_timer_->start();
        else              auto_refresh_timer_->stop();
    });
    h->addWidget(auto_btn);

    // Interval combo styled as a terminal chip
    auto* iv = new QComboBox;
    iv->setFixedHeight(22);
    iv->addItem("10m", 600000);
    iv->addItem("15m", 900000);
    iv->addItem("30m", 1800000);
    iv->addItem("1h",  3600000);
    iv->setStyleSheet("QComboBox{background:#141414;color:#606060;border:1px solid #252525;"
                      "padding:0 6px;font-size:11px;font-family:'Consolas',monospace;}"
                      "QComboBox::drop-down{border:none;width:14px;}"
                      "QComboBox QAbstractItemView{background:#141414;color:#909090;"
                      "border:1px solid #252525;selection-background-color:#1e1e0a;}");
    connect(iv, &QComboBox::currentIndexChanged, this, [this, iv](int i) {
        update_interval_ms_ = iv->itemData(i).toInt();
        auto_refresh_timer_->setInterval(update_interval_ms_);
    });
    h->addWidget(iv);

    h->addStretch();

    // Last updated timestamp
    auto* updated_lbl = new QLabel("LAST UPDATE  --:--:--");
    updated_lbl->setStyleSheet("color:#303030;font-size:11px;background:transparent;"
                               "font-family:'Consolas',monospace;");
    h->addWidget(updated_lbl);
    last_update_label_ = updated_lbl;

    h->addWidget(new QLabel("   "));  // spacer

    // Status
    status_label_ = new QLabel("● READY");
    status_label_->setStyleSheet("color:#22c55e;font-size:11px;font-weight:700;background:transparent;"
                                 "font-family:'Consolas',monospace;");
    h->addWidget(status_label_);
    return w;
}

void MarketsScreen::refresh_all() {
    if (loading_in_progress_) {
        refresh_queued_ = true;
        return;
    }

    begin_loading();

    for (auto* p : global_panels_)
        p->refresh();
    for (auto* p : regional_panels_)
        p->refresh();
}

void MarketsScreen::begin_loading() {
    loading_in_progress_ = true;
    refresh_queued_ = false;
    loading_frame_ = 0;
    pending_panels_ = global_panels_.size() + regional_panels_.size();
    if (pending_panels_ == 0) {
        finish_loading();
        return;
    }
    if (status_label_) {
        status_label_->setText(loading_text());
        status_label_->setStyleSheet("color:#d97706;font-size:11px;font-weight:700;background:transparent;"
                                     "font-family:'Consolas',monospace;");
    }
    if (loading_animation_timer_)
        loading_animation_timer_->start();
}

void MarketsScreen::finish_loading() {
    loading_in_progress_ = false;
    pending_panels_ = 0;
    if (loading_animation_timer_)
        loading_animation_timer_->stop();
    if (status_label_) {
        status_label_->setText("● READY");
        status_label_->setStyleSheet("color:#22c55e;font-size:11px;font-weight:700;background:transparent;"
                                     "font-family:'Consolas',monospace;");
    }
    if (last_update_label_) {
        last_update_label_->setText(
            QString("LAST UPDATE  %1")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        last_update_label_->setStyleSheet("color:#404040;font-size:11px;background:transparent;"
                                          "font-family:'Consolas',monospace;");
    }
    if (refresh_queued_)
        refresh_all();
}

void MarketsScreen::on_panel_refresh_finished() {
    if (!loading_in_progress_)
        return;

    if (pending_panels_ > 0)
        --pending_panels_;
    if (status_label_)
        status_label_->setText(loading_text());

    if (pending_panels_ == 0)
        finish_loading();
}

QString MarketsScreen::loading_text() {
    static const QStringList frames = {"LOADING   ", "LOADING.  ", "LOADING.. ", "LOADING..."};
    const int idx = loading_frame_ % frames.size();
    const int completed = (global_panels_.size() + regional_panels_.size()) - pending_panels_;
    const int total = global_panels_.size() + regional_panels_.size();
    loading_frame_ = loading_frame_ + 1;
    return QString("%1 %2/%3").arg(frames[idx]).arg(completed).arg(total);
}

} // namespace fincept::screens
