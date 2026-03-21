#include "screens/markets/MarketsScreen.h"
#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"
#include <QShowEvent>
#include <QHideEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFrame>
#include <QDateTime>
#include <QGridLayout>

namespace fincept::screens {

MarketsScreen::MarketsScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background:#080808;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(build_header());
    root->addWidget(build_controls());

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#080808;}");
    auto* content = new QWidget;
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(6,6,6,6); cvl->setSpacing(2);

    // Global Markets
    auto* gt = new QLabel("GLOBAL MARKETS");
    gt->setStyleSheet("color:#d97706;font-size:14px;font-weight:700;background:transparent;"
        "letter-spacing:1px;padding:6px 0;font-family:'Consolas',monospace;");
    cvl->addWidget(gt);
    auto* gs = new QFrame; gs->setFixedHeight(1);
    gs->setStyleSheet("background:#1a1a1a;border:none;");
    cvl->addWidget(gs);

    auto* gg = new QGridLayout; gg->setSpacing(4); gg->setContentsMargins(0,4,0,4);
    auto globals = services::MarketDataService::default_global_markets();
    for (int i=0; i<globals.size(); i++) {
        auto* p = new MarketPanel(globals[i].category, globals[i].tickers, false);
        global_panels_.append(p);
        gg->addWidget(p, i/3, i%3);
    }
    cvl->addLayout(gg);

    // Regional Markets
    auto* rt = new QLabel("REGIONAL MARKETS");
    rt->setStyleSheet("color:#d97706;font-size:14px;font-weight:700;background:transparent;"
        "letter-spacing:1px;padding:12px 0 6px 0;font-family:'Consolas',monospace;");
    cvl->addWidget(rt);
    auto* rs = new QFrame; rs->setFixedHeight(1);
    rs->setStyleSheet("background:#1a1a1a;border:none;");
    cvl->addWidget(rs);

    auto* rg = new QGridLayout; rg->setSpacing(4); rg->setContentsMargins(0,4,0,4);
    auto regionals = services::MarketDataService::default_regional_markets();
    for (int i=0; i<regionals.size(); i++) {
        QStringList syms;
        for (const auto& t : regionals[i].tickers) syms << t.symbol;
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
    // Timer starts via showEvent, not eagerly
}

void MarketsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (auto_update_ && auto_refresh_timer_) auto_refresh_timer_->start();
    refresh_all();  // fresh data on show
}

void MarketsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (auto_refresh_timer_) auto_refresh_timer_->stop();
}

QWidget* MarketsScreen::build_header() {
    auto* w = new QWidget; w->setFixedHeight(34);
    w->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #1a1a1a;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(12,0,12,0); h->setSpacing(0);
    auto mk = [](const QString& t, const QString& c, int sz=13, bool b=false) {
        auto* l = new QLabel(t);
        l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
            "font-family:'Consolas',monospace;").arg(c).arg(sz).arg(b?"font-weight:700;":""));
        return l;
    };
    h->addWidget(mk("FINCEPT","#d97706",16,true));
    h->addWidget(mk("  MARKET TERMINAL","#525252",13));
    h->addWidget(mk("   ","#000"));
    h->addWidget(mk("\xe2\x97\x8f","#16a34a",10));
    h->addWidget(mk(" LIVE","#16a34a",13,true));
    h->addStretch();
    auto* clock = mk("","#404040",9);
    auto* ct = new QTimer(this);
    connect(ct, &QTimer::timeout, this, [clock]() {
        clock->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
    });
    ct->start(1000);
    clock->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
    h->addWidget(clock);
    return w;
}

QWidget* MarketsScreen::build_controls() {
    auto* w = new QWidget; w->setFixedHeight(30);
    w->setStyleSheet("background:#0e0e0e;border-bottom:1px solid #1a1a1a;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(12,0,12,0); h->setSpacing(6);

    auto* rb = new QPushButton("REFRESH"); rb->setFixedHeight(22);
    rb->setCursor(Qt::PointingHandCursor);
    rb->setStyleSheet("QPushButton{background:#d97706;color:#080808;border:none;padding:0 8px;"
        "font-size:12px;font-weight:700;letter-spacing:0.5px;font-family:'Consolas',monospace;}"
        "QPushButton:hover{background:#b45309;}");
    connect(rb, &QPushButton::clicked, this, &MarketsScreen::refresh_all);
    h->addWidget(rb);

    auto* ab = new QPushButton(auto_update_?"AUTO ON":"AUTO OFF");
    ab->setFixedHeight(22); ab->setCursor(Qt::PointingHandCursor);
    ab->setStyleSheet(QString("QPushButton{background:%1;color:#080808;border:none;padding:0 8px;"
        "font-size:12px;font-weight:700;font-family:'Consolas',monospace;}")
        .arg(auto_update_?"#16a34a":"#404040"));
    connect(ab, &QPushButton::clicked, this, [this, ab]() {
        auto_update_ = !auto_update_;
        ab->setText(auto_update_?"AUTO ON":"AUTO OFF");
        ab->setStyleSheet(QString("QPushButton{background:%1;color:#080808;border:none;padding:0 8px;"
            "font-size:12px;font-weight:700;font-family:'Consolas',monospace;}")
            .arg(auto_update_?"#16a34a":"#404040"));
        if (auto_update_) auto_refresh_timer_->start(); else auto_refresh_timer_->stop();
    });
    h->addWidget(ab);

    auto* iv = new QComboBox; iv->setFixedHeight(22);
    iv->addItem("10m",600000); iv->addItem("15m",900000);
    iv->addItem("30m",1800000); iv->addItem("1h",3600000);
    iv->setStyleSheet("QComboBox{background:#0a0a0a;color:#808080;border:1px solid #1a1a1a;"
        "padding:0 4px;font-size:12px;font-family:'Consolas',monospace;}");
    connect(iv, &QComboBox::currentIndexChanged, this, [this,iv](int i) {
        update_interval_ms_ = iv->itemData(i).toInt();
        auto_refresh_timer_->setInterval(update_interval_ms_);
    });
    h->addWidget(iv);
    h->addStretch();

    auto* st = new QLabel("READY");
    st->setStyleSheet("color:#16a34a;font-size:12px;font-weight:700;background:transparent;"
        "font-family:'Consolas',monospace;");
    h->addWidget(st);
    return w;
}

void MarketsScreen::refresh_all() {
    for (auto* p : global_panels_) p->refresh();
    for (auto* p : regional_panels_) p->refresh();
}

} // namespace fincept::screens
