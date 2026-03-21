// Theme 01: OBSIDIAN — Deep blacks, amber data, ultra-minimal borders
// Dense monospace grid, no rounded corners, hairline separators
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>

static QLabel* lbl(const QString& t, const QString& c, int sz=10, bool bold=false) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
        "font-family:'JetBrains Mono','Consolas',monospace;").arg(c).arg(sz).arg(bold?"font-weight:700;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(28);
    w->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #1a1a1a;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(12,0,12,0);
    h->addWidget(lbl("FINCEPT","#d97706",12,true));
    h->addWidget(lbl("TERMINAL","#525252",10));
    h->addStretch();
    h->addWidget(lbl("2026-03-20  14:32:01","#404040",9));
    h->addStretch();
    auto* dot = lbl("\xe2\x97\x8f","#16a34a",7); h->addWidget(dot);
    h->addWidget(lbl("LIVE","#16a34a",9,true));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(22);
    w->setStyleSheet("background:#0e0e0e;border-bottom:1px solid #1a1a1a;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(4,0,4,0); h->setSpacing(0);
    QStringList tabs={"DASHBOARD","MARKETS","PORTFOLIO","NEWS","REPORTS"};
    for(int i=0;i<tabs.size();i++){
        auto* b=new QPushButton(tabs[i]); b->setFixedHeight(22);
        b->setStyleSheet(i==1?
            "QPushButton{background:#b45309;color:#fafafa;border:none;padding:0 12px;font-size:9px;font-weight:600;letter-spacing:1px;font-family:'Consolas',monospace;}"
            :"QPushButton{background:transparent;color:#525252;border:none;padding:0 12px;font-size:9px;letter-spacing:0.5px;font-family:'Consolas',monospace;}"
            "QPushButton:hover{color:#a3a3a3;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#0a0a0a;border:1px solid #1a1a1a;");
    w->setMinimumWidth(280); w->setFixedHeight(240);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(24);
    hdr->setStyleSheet("background:#111111;border-bottom:1px solid #1a1a1a;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(8,0,8,0);
    hl->addWidget(lbl(title,"#d97706",9,true)); hl->addStretch();
    hl->addWidget(lbl("12","#333333",8));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"SYMBOL","PRICE","CHG","CHG%"});
    t->verticalHeader()->setVisible(false);
    t->setShowGrid(false); t->setFocusPolicy(Qt::NoFocus);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(20);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(20);
    t->setStyleSheet(
        "QTableWidget{background:#080808;alternate-background-color:#0c0c0c;border:none;"
        "font-family:'Consolas',monospace;font-size:10px;}"
        "QTableWidget::item{padding:0 6px;border-bottom:1px solid #111111;}"
        "QHeaderView::section{background:#0a0a0a;color:#404040;border:none;"
        "border-bottom:1px solid #1a1a1a;font-size:8px;font-weight:600;padding:0 6px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"^GSPC","^IXIC","^DJI","^RUT","^FTSE","^N225","^HSI","^GDAXI"};
    double prices[]={5118.37,16085.11,38996.39,2089.47,7930.92,39098.68,16589.55,17940.20};
    double chgs[]={0.87,1.12,0.34,-0.23,0.45,1.23,-0.89,0.67};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=chgs[r]>=0?"#16a34a":"#dc2626";
        t->setItem(r,0,mk(syms[r],"#e5e5e5",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(prices[r],'f',2),"#e5e5e5"));
        t->setItem(r,2,mk(QString("%1%2").arg(chgs[r]>=0?"+":"").arg(chgs[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(chgs[r]>=0?"+":"").arg(chgs[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 01: Obsidian"); win.resize(1200,700);
    auto* c = new QWidget; auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());

    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#080808;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(6,6,6,6); grid->setSpacing(4);
    grid->addWidget(market_panel("INDICES")); grid->addWidget(market_panel("FOREX"));
    grid->addWidget(market_panel("COMMODITIES")); grid->addWidget(market_panel("CRYPTO"));
    scroll->setWidget(body); root->addWidget(scroll,1);

    auto* sb = new QWidget; sb->setFixedHeight(18);
    sb->setStyleSheet("background:#0a0a0a;border-top:1px solid #1a1a1a;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(12,0,12,0);
    sl->addWidget(lbl("v4.0.0","#333333",8)); sl->addStretch();
    sl->addWidget(lbl("READY","#16a34a",8,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
