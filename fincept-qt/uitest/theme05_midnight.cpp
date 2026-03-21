// Theme 05: MIDNIGHT — Deep navy, gold accents, luxury finance aesthetic
// Inspired by private banking terminals, serif headers, refined spacing
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

static QLabel* lbl(const QString& t, const QString& c, int sz=10, bool b=false, const QString& fam="'Segoe UI',sans-serif") {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;font-family:%4;").arg(c).arg(sz).arg(b?"font-weight:600;":"").arg(fam));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(40);
    w->setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #0f172a,stop:1 #0c1322);border-bottom:1px solid #d4a843;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(20,0,20,0); h->setSpacing(12);
    h->addWidget(lbl("FINCEPT","#d4a843",16,true,"'Georgia','Times New Roman',serif"));
    h->addWidget(lbl("Private Terminal","#4a5568",11,false));
    h->addStretch();
    h->addWidget(lbl("March 20, 2026","#4a5568",10));
    h->addWidget(lbl("\xe2\x80\xa2","#d4a843",14));
    h->addWidget(lbl("14:32 UTC","#64748b",10,"'Consolas',monospace"));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(30);
    w->setStyleSheet("background:#0c1322;border-bottom:1px solid #1e293b;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(16,0,16,0); h->setSpacing(2);
    QStringList tabs={"Overview","Markets","Wealth","Research","Advisory"};
    for(int i=0;i<tabs.size();i++){
        auto* b = new QPushButton(tabs[i]); b->setFixedHeight(30);
        b->setStyleSheet(i==1?
            "QPushButton{background:transparent;color:#f8fafc;border:none;border-bottom:2px solid #d4a843;padding:0 18px;font-size:11px;font-weight:500;font-family:'Segoe UI',sans-serif;}"
            :"QPushButton{background:transparent;color:#475569;border:none;padding:0 18px;font-size:11px;font-family:'Segoe UI',sans-serif;}"
            "QPushButton:hover{color:#94a3b8;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#0f172a;border:1px solid #1e293b;border-top:2px solid #d4a843;");
    w->setMinimumWidth(280); w->setFixedHeight(270);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(30);
    hdr->setStyleSheet("background:#131d33;border-bottom:1px solid #1e293b;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(14,0,14,0);
    hl->addWidget(lbl(title,"#e2e8f0",11,true,"'Georgia',serif")); hl->addStretch();
    hl->addWidget(lbl("LIVE","#d4a843",8,true,"'Consolas',monospace"));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"Instrument","Price","Change","%"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(22);
    t->setStyleSheet(
        "QTableWidget{background:#0c1322;alternate-background-color:#0f172a;border:none;"
        "font-family:'Consolas',monospace;font-size:11px;}"
        "QTableWidget::item{padding:0 8px;border-bottom:1px solid #172035;}"
        "QHeaderView::section{background:#0f172a;color:#475569;border:none;"
        "border-bottom:1px solid #1e293b;font-size:9px;font-weight:600;padding:0 8px;"
        "font-family:'Segoe UI',sans-serif;letter-spacing:0.5px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#34d399":"#f87171";
        t->setItem(r,0,mk(syms[r],"#f1f5f9",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#e2e8f0"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 05: Midnight"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#0c1322;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#0c1322;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(10,10,10,10); grid->setSpacing(8);
    grid->addWidget(market_panel("Global Indices"));
    grid->addWidget(market_panel("Foreign Exchange"));
    grid->addWidget(market_panel("Precious Metals"));
    grid->addWidget(market_panel("Digital Assets"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(22);
    sb->setStyleSheet("background:#0c1322;border-top:1px solid #1e293b;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(20,0,20,0);
    sl->addWidget(lbl("Fincept Corporation \xc2\xa9 2026","#334155",9));
    sl->addStretch(); sl->addWidget(lbl("\xe2\x97\x8f Online","#d4a843",9,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
