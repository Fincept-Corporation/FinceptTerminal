// Theme 03: ARCTIC — Cool blue-gray palette, frosted glass feel
// Steel blue accents, soft borders, modern institutional look
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
#include <QFrame>

static QLabel* lbl(const QString& t, const QString& c, int sz=10, bool b=false) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
        "font-family:'Segoe UI','Inter',sans-serif;").arg(c).arg(sz).arg(b?"font-weight:600;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(32);
    w->setStyleSheet("background:#0f1923;border-bottom:1px solid #1e3a5f;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(16,0,16,0);
    h->addWidget(lbl("FINCEPT","#60a5fa",13,true));
    h->addWidget(lbl("  Terminal","#475569",11));
    h->addStretch();
    h->addWidget(lbl("20 Mar 2026  14:32","#334155",10));
    h->addWidget(lbl("   |   ","#1e3a5f",10));
    h->addWidget(lbl("\xe2\x97\x8f Connected","#34d399",10,true));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(28);
    w->setStyleSheet("background:#0c1520;border-bottom:1px solid #1e3a5f;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(8,0,8,0); h->setSpacing(2);
    QStringList tabs={"Dashboard","Markets","Portfolio","News","Reports"};
    for(int i=0;i<tabs.size();i++){
        auto* b = new QPushButton(tabs[i]); b->setFixedHeight(28);
        b->setStyleSheet(i==1?
            "QPushButton{background:#1e3a5f;color:#e2e8f0;border:none;border-bottom:2px solid #60a5fa;padding:0 16px;font-size:11px;font-weight:500;font-family:'Segoe UI',sans-serif;}"
            :"QPushButton{background:transparent;color:#64748b;border:none;padding:0 16px;font-size:11px;font-family:'Segoe UI',sans-serif;}"
            "QPushButton:hover{color:#94a3b8;background:#111d2e;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#0f1923;border:1px solid #1e3a5f;border-radius:4px;");
    w->setMinimumWidth(280); w->setFixedHeight(260);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(28);
    hdr->setStyleSheet("background:#111d2e;border-bottom:1px solid #1e3a5f;border-radius:4px 4px 0 0;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(12,0,12,0);
    hl->addWidget(lbl(title,"#60a5fa",10,true)); hl->addStretch();
    hl->addWidget(lbl("LIVE","#34d399",8,true));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"Symbol","Price","Change","%"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(24);
    t->setStyleSheet(
        "QTableWidget{background:#0a1219;alternate-background-color:#0e1825;border:none;"
        "font-family:'Segoe UI',sans-serif;font-size:11px;}"
        "QTableWidget::item{padding:0 8px;border-bottom:1px solid #152238;}"
        "QHeaderView::section{background:#0f1923;color:#475569;border:none;"
        "border-bottom:1px solid #1e3a5f;font-size:10px;font-weight:600;padding:0 8px;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#34d399":"#f87171";
        t->setItem(r,0,mk(syms[r],"#e2e8f0",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#e2e8f0"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 03: Arctic"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#0a1219;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#0a1219;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(8,8,8,8); grid->setSpacing(6);
    grid->addWidget(market_panel("INDICES")); grid->addWidget(market_panel("FOREX"));
    grid->addWidget(market_panel("COMMODITIES")); grid->addWidget(market_panel("CRYPTO"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(20);
    sb->setStyleSheet("background:#0c1520;border-top:1px solid #1e3a5f;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(16,0,16,0);
    sl->addWidget(lbl("Fincept v4.0.0","#334155",9)); sl->addStretch();
    sl->addWidget(lbl("\xe2\x97\x8f Ready","#34d399",9,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
