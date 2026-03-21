// Theme 09: EMBER — Warm dark with ember/copper accents
// Rich warm blacks, copper highlights, soft amber data, cozy professional
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

static QLabel* lbl(const QString& t, const QString& c, int sz=10, bool b=false) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
        "font-family:'Cascadia Code','Consolas',monospace;").arg(c).arg(sz).arg(b?"font-weight:600;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(32);
    w->setStyleSheet("background:#1c1210;border-bottom:1px solid #3d2b22;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(14,0,14,0); h->setSpacing(10);
    h->addWidget(lbl("FINCEPT","#c2703e",14,true));
    h->addWidget(lbl("TERMINAL","#6b4530",10));
    h->addStretch();
    h->addWidget(lbl("20 MAR 2026","#5a3e2c",9));
    h->addWidget(lbl(" \xe2\x80\xa2 ","#3d2b22"));
    h->addWidget(lbl("14:32:01","#8b6040",10));
    h->addStretch();
    h->addWidget(lbl("\xe2\x97\x8f","#4ade80",7));
    h->addWidget(lbl("ONLINE","#4ade80",9,true));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(26);
    w->setStyleSheet("background:#16100e;border-bottom:1px solid #2d1f18;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(6,0,6,0); h->setSpacing(1);
    QStringList tabs={"DASH","MARKETS","PORTFOLIO","NEWS","REPORTS","CONFIG"};
    for(int i=0;i<tabs.size();i++){
        auto* b = new QPushButton(tabs[i]); b->setFixedHeight(26);
        b->setStyleSheet(i==1?
            "QPushButton{background:#3d2b22;color:#e8c4a0;border:none;padding:0 12px;font-size:9px;font-weight:600;letter-spacing:0.8px;font-family:'Cascadia Code',monospace;}"
            :"QPushButton{background:transparent;color:#5a3e2c;border:none;padding:0 12px;font-size:9px;letter-spacing:0.5px;font-family:'Cascadia Code',monospace;}"
            "QPushButton:hover{color:#c2703e;background:#1c1210;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#16100e;border:1px solid #2d1f18;");
    w->setMinimumWidth(280); w->setFixedHeight(260);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(26);
    hdr->setStyleSheet("background:#1c1210;border-bottom:1px solid #2d1f18;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(10,0,10,0);
    hl->addWidget(lbl(title,"#c2703e",10,true)); hl->addStretch();
    hl->addWidget(lbl("LIVE","#8b6040",8));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"SYM","PRICE","CHG","CHG%"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(22);
    t->setStyleSheet(
        "QTableWidget{background:#110d0b;alternate-background-color:#16100e;border:none;"
        "font-family:'Cascadia Code','Consolas',monospace;font-size:11px;}"
        "QTableWidget::item{padding:0 6px;border-bottom:1px solid #1c1210;}"
        "QHeaderView::section{background:#16100e;color:#5a3e2c;border:none;"
        "border-bottom:1px solid #2d1f18;font-size:9px;font-weight:600;padding:0 6px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#4ade80":"#f87171";
        t->setItem(r,0,mk(syms[r],"#e8c4a0",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#e8c4a0"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 09: Ember"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#110d0b;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#110d0b;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(8,8,8,8); grid->setSpacing(6);
    grid->addWidget(market_panel("INDICES")); grid->addWidget(market_panel("FOREX"));
    grid->addWidget(market_panel("COMMODITIES")); grid->addWidget(market_panel("CRYPTO"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(20);
    sb->setStyleSheet("background:#16100e;border-top:1px solid #2d1f18;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(14,0,14,0);
    sl->addWidget(lbl("FINCEPT v4.0","#3d2b22",8)); sl->addStretch();
    sl->addWidget(lbl("READY","#4ade80",8,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
