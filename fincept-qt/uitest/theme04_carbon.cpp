// Theme 04: CARBON — Warm charcoal, orange highlights, IBM Carbon Design inspired
// Structured panels with thick left borders, grouped data
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
        "font-family:'IBM Plex Mono','Consolas',monospace;").arg(c).arg(sz).arg(b?"font-weight:600;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(36);
    w->setStyleSheet("background:#161616;border-bottom:1px solid #393939;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(16,0,16,0); h->setSpacing(16);
    auto* brand = lbl("Fincept","#f97316",14,true);
    h->addWidget(brand);
    auto* sep = lbl("|","#393939",14); h->addWidget(sep);
    h->addWidget(lbl("Market Terminal","#8d8d8d",12));
    h->addStretch();
    h->addWidget(lbl("14:32 UTC","#6f6f6f",10));
    h->addWidget(lbl("|","#393939",10));
    h->addWidget(lbl("v4.0.0","#4589ff",10,true));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(32);
    w->setStyleSheet("background:#1c1c1c;border-bottom:2px solid #393939;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(12,0,12,0); h->setSpacing(0);
    QStringList tabs={"Dashboard","Markets","Portfolio","News","Reports","Settings"};
    for(int i=0;i<tabs.size();i++){
        auto* b = new QPushButton(tabs[i]); b->setFixedHeight(32);
        b->setStyleSheet(i==1?
            "QPushButton{background:transparent;color:#f4f4f4;border:none;border-bottom:3px solid #f97316;padding:0 16px;font-size:12px;font-weight:500;font-family:'IBM Plex Sans',sans-serif;}"
            :"QPushButton{background:transparent;color:#6f6f6f;border:none;padding:0 16px;font-size:12px;font-family:'IBM Plex Sans',sans-serif;}"
            "QPushButton:hover{color:#c6c6c6;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title, const QString& accent) {
    auto* w = new QWidget;
    w->setStyleSheet(QString("background:#1c1c1c;border:1px solid #393939;border-left:3px solid %1;").arg(accent));
    w->setMinimumWidth(280); w->setFixedHeight(260);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(32);
    hdr->setStyleSheet("background:#262626;border-bottom:1px solid #393939;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(12,0,12,0);
    hl->addWidget(lbl(title,"#f4f4f4",11,true)); hl->addStretch();
    hl->addWidget(lbl("Live",accent,9,true));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"Symbol","Last","Chg","%Chg"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(24);
    t->setStyleSheet(
        "QTableWidget{background:#161616;alternate-background-color:#1c1c1c;border:none;"
        "font-family:'IBM Plex Mono',monospace;font-size:11px;}"
        "QTableWidget::item{padding:0 8px;border-bottom:1px solid #262626;}"
        "QHeaderView::section{background:#1c1c1c;color:#6f6f6f;border:none;"
        "border-bottom:1px solid #393939;font-size:10px;font-weight:600;padding:0 8px;"
        "font-family:'IBM Plex Mono',monospace;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#42be65":"#fa4d56";
        t->setItem(r,0,mk(syms[r],"#f4f4f4",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#f4f4f4"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 04: Carbon"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#161616;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#161616;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(8,8,8,8); grid->setSpacing(6);
    grid->addWidget(market_panel("INDICES","#f97316"));
    grid->addWidget(market_panel("FOREX","#4589ff"));
    grid->addWidget(market_panel("COMMODITIES","#42be65"));
    grid->addWidget(market_panel("CRYPTO","#be95ff"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(24);
    sb->setStyleSheet("background:#161616;border-top:1px solid #393939;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(16,0,16,0);
    sl->addWidget(lbl("Fincept Corporation","#525252",9));
    sl->addStretch(); sl->addWidget(lbl("Ready","#42be65",9,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
