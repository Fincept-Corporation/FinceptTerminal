// Theme 02: MATRIX — Green-on-black classic terminal, retro CRT feel
// Phosphor green text, scanline effect, DOS-style borders
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

static QLabel* lbl(const QString& t, const QString& c="#33ff33", int sz=10, bool b=false) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
        "font-family:'Courier New','Consolas',monospace;").arg(c).arg(sz).arg(b?"font-weight:700;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(30);
    w->setStyleSheet("background:#001100;border-bottom:2px solid #004400;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(12,0,12,0);
    h->addWidget(lbl(">>> FINCEPT TERMINAL v4.0","#00ff00",11,true));
    h->addStretch();
    h->addWidget(lbl("[CONNECTED]","#00cc00",10,true));
    h->addWidget(lbl("  2026-03-20 14:32:01","#006600",9));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(24);
    w->setStyleSheet("background:#000800;border-bottom:1px solid #003300;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(4,0,4,0); h->setSpacing(0);
    QStringList tabs={"[F1]DASH","[F2]MKT","[F3]PORT","[F4]NEWS","[F5]RPT"};
    for(int i=0;i<tabs.size();i++){
        auto* b=new QPushButton(tabs[i]); b->setFixedHeight(24);
        b->setStyleSheet(i==1?
            "QPushButton{background:#003300;color:#33ff33;border:1px solid #006600;padding:0 8px;font-size:10px;font-family:'Courier New',monospace;}"
            :"QPushButton{background:transparent;color:#006600;border:none;padding:0 8px;font-size:10px;font-family:'Courier New',monospace;}"
            "QPushButton:hover{color:#33ff33;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#000a00;border:1px solid #003300;");
    w->setMinimumWidth(280); w->setFixedHeight(240);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(22);
    hdr->setStyleSheet("background:#001a00;border-bottom:1px solid #003300;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(8,0,8,0);
    hl->addWidget(lbl("=== "+title+" ===","#00ff00",9,true));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"SYM","PRICE","CHG","CHG%"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(20);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(20);
    t->setStyleSheet(
        "QTableWidget{background:#000500;alternate-background-color:#000a00;border:none;"
        "font-family:'Courier New',monospace;font-size:10px;}"
        "QTableWidget::item{padding:0 4px;color:#33ff33;border-bottom:1px solid #002200;}"
        "QHeaderView::section{background:#001100;color:#009900;border:none;"
        "border-bottom:1px solid #004400;font-size:9px;font-weight:600;padding:0 4px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#00ff00":"#ff3333";
        t->setItem(r,0,mk(syms[r],"#33ff33",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#33ff33"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 02: Matrix"); win.resize(1200,700);
    auto* c = new QWidget; auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#000500;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(6,6,6,6); grid->setSpacing(4);
    grid->addWidget(market_panel("US EQUITIES")); grid->addWidget(market_panel("FOREX"));
    grid->addWidget(market_panel("COMMODITIES")); grid->addWidget(market_panel("CRYPTO"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(18);
    sb->setStyleSheet("background:#001100;border-top:1px solid #003300;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(12,0,12,0);
    sl->addWidget(lbl("SYS OK","#009900",8,true)); sl->addStretch();
    sl->addWidget(lbl("MEM: 847MB | CPU: 12%","#004400",8));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
