// Theme 06: NEON — Cyberpunk dark with vivid cyan/magenta accents
// High contrast, glowing borders, futuristic HUD aesthetic
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
        "font-family:'Fira Code','Consolas',monospace;").arg(c).arg(sz).arg(b?"font-weight:700;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(30);
    w->setStyleSheet("background:#09090b;border-bottom:1px solid #06b6d4;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(12,0,12,0);
    h->addWidget(lbl("FIN","#06b6d4",13,true));
    h->addWidget(lbl("CEPT","#d946ef",13,true));
    h->addWidget(lbl("  //  MARKET FEED","#3f3f46",10));
    h->addStretch();
    h->addWidget(lbl("SYS.ONLINE","#06b6d4",9,true));
    h->addWidget(lbl("  |  ","#27272a",9));
    h->addWidget(lbl("14:32:01","#52525b",9));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(26);
    w->setStyleSheet("background:#09090b;border-bottom:1px solid #18181b;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(4,0,4,0); h->setSpacing(2);
    QStringList tabs={">>DASH",">>MKT",">>PORT",">>NEWS",">>RPT"};
    for(int i=0;i<tabs.size();i++){
        auto* b = new QPushButton(tabs[i]); b->setFixedHeight(26);
        b->setStyleSheet(i==1?
            "QPushButton{background:rgba(6,182,212,0.15);color:#06b6d4;border:1px solid #06b6d4;padding:0 12px;font-size:10px;font-weight:600;font-family:'Fira Code',monospace;}"
            :"QPushButton{background:transparent;color:#3f3f46;border:none;padding:0 12px;font-size:10px;font-family:'Fira Code',monospace;}"
            "QPushButton:hover{color:#06b6d4;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title, const QString& glow) {
    auto* w = new QWidget;
    w->setStyleSheet(QString("background:#09090b;border:1px solid %1;").arg(glow));
    w->setMinimumWidth(280); w->setFixedHeight(260);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(26);
    hdr->setStyleSheet(QString("background:rgba(%1,0.08);border-bottom:1px solid %2;")
        .arg(glow=="rgba(6,182,212,0.4)"?"6,182,212":"217,70,239").arg(glow));
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(8,0,8,0);
    hl->addWidget(lbl("[ "+title+" ]",glow=="rgba(6,182,212,0.4)"?"#06b6d4":"#d946ef",9,true));
    hl->addStretch();
    hl->addWidget(lbl("STREAM","#22c55e",8,true));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"SYM","PX","CHG","%"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(20);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(20);
    t->setStyleSheet(
        "QTableWidget{background:#050505;alternate-background-color:#09090b;border:none;"
        "font-family:'Fira Code','Consolas',monospace;font-size:10px;}"
        "QTableWidget::item{padding:0 6px;border-bottom:1px solid #111111;color:#a1a1aa;}"
        "QHeaderView::section{background:#09090b;color:#3f3f46;border:none;"
        "border-bottom:1px solid #18181b;font-size:8px;font-weight:600;padding:0 6px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#06b6d4":"#d946ef";
        t->setItem(r,0,mk(syms[r],"#e4e4e7",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#e4e4e7"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 06: Neon"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#050505;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#050505;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(8,8,8,8); grid->setSpacing(6);
    grid->addWidget(market_panel("INDICES","rgba(6,182,212,0.4)"));
    grid->addWidget(market_panel("FOREX","rgba(217,70,239,0.4)"));
    grid->addWidget(market_panel("METALS","rgba(6,182,212,0.4)"));
    grid->addWidget(market_panel("CRYPTO","rgba(217,70,239,0.4)"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(18);
    sb->setStyleSheet("background:#09090b;border-top:1px solid #18181b;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(12,0,12,0);
    sl->addWidget(lbl("NODE:PRIMARY","#27272a",8)); sl->addStretch();
    sl->addWidget(lbl("\xe2\x96\xb2 ONLINE","#06b6d4",8,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
