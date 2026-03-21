// Theme 10: TITANIUM — Ultra-clean, white-on-dark, minimal chrome
// Inspired by Stripe/Linear dashboards — precise spacing, no noise
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QScrollArea>

static QLabel* lbl(const QString& t, const QString& c, int sz=11, bool b=false) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
        "font-family:'SF Mono','Menlo','Consolas',monospace;").arg(c).arg(sz).arg(b?"font-weight:500;":"font-weight:400;"));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(44);
    w->setStyleSheet("background:#09090b;border-bottom:1px solid #18181b;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(20,0,20,0); h->setSpacing(16);
    auto* brand = lbl("fincept","#fafafa",15,true);
    h->addWidget(brand);
    auto* dot = lbl("\xe2\x80\xa2","#27272a",8); h->addWidget(dot);
    h->addWidget(lbl("terminal","#52525b",13));
    h->addStretch();
    h->addWidget(lbl("14:32","#3f3f46",11));
    h->addWidget(lbl("\xe2\x80\xa2","#27272a",8));
    h->addWidget(lbl("v4.0","#3f3f46",11));
    h->addWidget(lbl("\xe2\x80\xa2","#27272a",8));
    auto* s = lbl("connected","#22c55e",11,true); h->addWidget(s);
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(36);
    w->setStyleSheet("background:#09090b;border-bottom:1px solid #18181b;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(16,0,16,0); h->setSpacing(4);
    QStringList tabs={"Dashboard","Markets","Portfolio","News","Reports"};
    for(int i=0;i<tabs.size();i++){
        auto* b = new QPushButton(tabs[i]); b->setFixedHeight(28);
        b->setStyleSheet(i==1?
            "QPushButton{background:#18181b;color:#fafafa;border:none;border-radius:6px;padding:0 14px;font-size:12px;font-weight:500;font-family:'Inter',sans-serif;}"
            :"QPushButton{background:transparent;color:#52525b;border:none;border-radius:6px;padding:0 14px;font-size:12px;font-family:'Inter',sans-serif;}"
            "QPushButton:hover{color:#a1a1aa;background:#111113;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#09090b;border:1px solid #18181b;border-radius:8px;");
    w->setMinimumWidth(280); w->setFixedHeight(280);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(36);
    hdr->setStyleSheet("background:transparent;border-bottom:1px solid #18181b;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(16,0,16,0);
    hl->addWidget(lbl(title,"#fafafa",12,true)); hl->addStretch();
    auto* badge = new QLabel("Live");
    badge->setStyleSheet("color:#22c55e;font-size:9px;font-weight:500;background:#052e16;"
        "border:1px solid #14532d;border-radius:10px;padding:2px 8px;font-family:'Inter',sans-serif;");
    hl->addWidget(badge);
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"Symbol","Price","Change","%"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(24);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(28);
    t->setStyleSheet(
        "QTableWidget{background:#09090b;alternate-background-color:#0c0c0e;border:none;"
        "font-family:'SF Mono','Consolas',monospace;font-size:12px;}"
        "QTableWidget::item{padding:0 10px;border-bottom:1px solid #111113;}"
        "QHeaderView::section{background:#09090b;color:#3f3f46;border:none;"
        "border-bottom:1px solid #18181b;font-size:10px;font-weight:500;padding:0 10px;"
        "font-family:'Inter',sans-serif;letter-spacing:0.3px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#22c55e":"#ef4444";
        t->setItem(r,0,mk(syms[r],"#fafafa",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#e4e4e7"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 10: Titanium"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#050505;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#050505;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(12,12,12,12); grid->setSpacing(8);
    grid->addWidget(market_panel("Indices")); grid->addWidget(market_panel("Forex"));
    grid->addWidget(market_panel("Commodities")); grid->addWidget(market_panel("Crypto"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(24);
    sb->setStyleSheet("background:#09090b;border-top:1px solid #18181b;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(20,0,20,0);
    sl->addWidget(lbl("fincept","#27272a",9)); sl->addStretch();
    sl->addWidget(lbl("ready","#22c55e",9,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
