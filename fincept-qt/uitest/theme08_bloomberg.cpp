// Theme 08: BLOOMBERG — Authentic Bloomberg Terminal recreation
// Orange-on-black, dense grids, uppercase everything, stark monospace
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

static QLabel* lbl(const QString& t, const QString& c="#ff8c00", int sz=10, bool b=false) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
        "font-family:'Consolas','Courier New',monospace;").arg(c).arg(sz).arg(b?"font-weight:700;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(24);
    w->setStyleSheet("background:#1a1100;border-bottom:1px solid #332200;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(8,0,8,0); h->setSpacing(4);
    h->addWidget(lbl("BLOOMBERG","#ff8c00",11,true));
    h->addWidget(lbl("<FINCEPT>","#666633",9));
    h->addStretch();
    h->addWidget(lbl("THU 20MAR26","#666633",9));
    h->addWidget(lbl("14:32:01","#ff8c00",9,true));
    h->addWidget(lbl("NYSE OPEN","#00cc00",9,true));
    return w;
}

static QWidget* fkey_bar() {
    auto* w = new QWidget; w->setFixedHeight(20);
    w->setStyleSheet("background:#000000;border-bottom:1px solid #1a1100;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(2,0,2,0); h->setSpacing(1);
    QStringList keys={"1)GOVT","2)CORP","3)MTGE","4)M-MKT","5)MUNI","6)PFD","7)EQUITY","8)CMDTY","9)INDEX","10)CRNCY"};
    for(int i=0;i<keys.size();i++){
        auto* b = new QPushButton(keys[i]); b->setFixedHeight(20);
        b->setStyleSheet(i==6?
            "QPushButton{background:#332200;color:#ff8c00;border:1px solid #4d3300;padding:0 6px;font-size:8px;font-weight:600;font-family:'Consolas',monospace;}"
            :"QPushButton{background:#0a0a00;color:#666633;border:1px solid #1a1100;padding:0 6px;font-size:8px;font-family:'Consolas',monospace;}"
            "QPushButton:hover{color:#ff8c00;background:#1a1100;}");
        h->addWidget(b,1);
    }
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#0a0a00;border:1px solid #332200;");
    w->setMinimumWidth(280); w->setFixedHeight(240);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(20);
    hdr->setStyleSheet("background:#1a1100;border-bottom:1px solid #332200;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(6,0,6,0);
    hl->addWidget(lbl(title.toUpper(),"#ff8c00",9,true)); hl->addStretch();
    hl->addWidget(lbl("PG1","#666633",7));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,5);
    t->setHorizontalHeaderLabels({"TICKER","LAST","NET","PCT","VOL"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(18);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(18);
    t->setStyleSheet(
        "QTableWidget{background:#000000;alternate-background-color:#050500;border:none;"
        "font-family:'Consolas',monospace;font-size:10px;}"
        "QTableWidget::item{padding:0 4px;border-bottom:1px solid #0a0a00;}"
        "QHeaderView::section{background:#0a0a00;color:#666633;border:none;"
        "border-bottom:1px solid #332200;font-size:8px;font-weight:600;padding:0 4px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"SPX","NDX","INDU","RTY","UKX","NKY","HSI","DAX"};
    double pr[]={5118.37,18085.11,38996.39,2089.47,7930.92,39098.68,16589.55,17940.20};
    double ch[]={44.32,180.54,132.87,-4.81,35.69,479.41,-147.82,120.13};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#00cc00":"#ff3333";
        double pct=(ch[r]/pr[r])*100;
        t->setItem(r,0,mk(syms[r],"#ff8c00",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#ffffff"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(pct>=0?"+":"").arg(pct,0,'f',2),cc));
        t->setItem(r,4,mk("---","#333300"));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 08: Bloomberg"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#000000;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(fkey_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#000000;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(4,4,4,4); grid->setSpacing(2);
    grid->addWidget(market_panel("WORLD INDICES"));
    grid->addWidget(market_panel("G10 CRNCY"));
    grid->addWidget(market_panel("CMDTY FUTURES"));
    grid->addWidget(market_panel("CRYPTO SPOT"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(16);
    sb->setStyleSheet("background:#000000;border-top:1px solid #332200;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(8,0,8,0);
    sl->addWidget(lbl("FINCEPT TERMINAL","#333300",7));
    sl->addStretch(); sl->addWidget(lbl("CONNECTED","#00cc00",7,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
