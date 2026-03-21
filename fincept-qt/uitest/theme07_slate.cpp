// Theme 07: SLATE — Neutral gray workspace, clean sans-serif, VS Code inspired
// Subtle depth via shadows, rounded corners, warm neutrals
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
        "font-family:'Inter','Segoe UI',sans-serif;").arg(c).arg(sz).arg(b?"font-weight:600;":""));
    return l;
}

static QWidget* header_bar() {
    auto* w = new QWidget; w->setFixedHeight(34);
    w->setStyleSheet("background:#1e1e1e;border-bottom:1px solid #333333;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(14,0,14,0); h->setSpacing(10);
    h->addWidget(lbl("Fincept","#569cd6",13,true));
    h->addWidget(lbl("Terminal","#808080",11));
    h->addStretch();
    h->addWidget(lbl("Mar 20, 2026 14:32","#666666",10));
    h->addWidget(lbl("|","#333333"));
    h->addWidget(lbl("v4.0.0","#608b4e",10,true));
    return w;
}

static QWidget* tab_bar() {
    auto* w = new QWidget; w->setFixedHeight(30);
    w->setStyleSheet("background:#252526;border-bottom:1px solid #3c3c3c;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(0,0,0,0); h->setSpacing(0);
    QStringList tabs={"Dashboard","Markets","Portfolio","News","Reports"};
    for(int i=0;i<tabs.size();i++){
        auto* b = new QPushButton(tabs[i]); b->setFixedHeight(30);
        b->setStyleSheet(i==1?
            "QPushButton{background:#1e1e1e;color:#ffffff;border:none;border-top:2px solid #569cd6;padding:0 16px;font-size:12px;font-family:'Inter',sans-serif;}"
            :"QPushButton{background:#2d2d2d;color:#808080;border:none;border-right:1px solid #252526;padding:0 16px;font-size:12px;font-family:'Inter',sans-serif;}"
            "QPushButton:hover{color:#cccccc;background:#2a2d2e;}");
        h->addWidget(b);
    }
    h->addStretch();
    return w;
}

static QWidget* market_panel(const QString& title) {
    auto* w = new QWidget;
    w->setStyleSheet("background:#1e1e1e;border:1px solid #3c3c3c;border-radius:6px;");
    w->setMinimumWidth(280); w->setFixedHeight(270);
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0,0,0,0); v->setSpacing(0);

    auto* hdr = new QWidget; hdr->setFixedHeight(32);
    hdr->setStyleSheet("background:#252526;border-bottom:1px solid #3c3c3c;border-radius:6px 6px 0 0;");
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(12,0,12,0);
    hl->addWidget(lbl(title,"#cccccc",11,true)); hl->addStretch();
    hl->addWidget(lbl("\xe2\x97\x8f","#608b4e",8)); hl->addWidget(lbl("Live","#608b4e",9));
    v->addWidget(hdr);

    auto* t = new QTableWidget(8,4);
    t->setHorizontalHeaderLabels({"Symbol","Price","Change","%"});
    t->verticalHeader()->setVisible(false); t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus); t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setFixedHeight(24);
    t->setStyleSheet(
        "QTableWidget{background:#1e1e1e;alternate-background-color:#222222;border:none;"
        "font-family:'Cascadia Code','Consolas',monospace;font-size:11px;}"
        "QTableWidget::item{padding:0 8px;border-bottom:1px solid #2d2d2d;}"
        "QHeaderView::section{background:#252526;color:#808080;border:none;"
        "border-bottom:1px solid #3c3c3c;font-size:10px;font-weight:500;padding:0 8px;"
        "font-family:'Inter',sans-serif;}");
    t->setAlternatingRowColors(true);

    QString syms[]={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM"};
    double pr[]={189.84,378.91,141.80,178.25,875.28,485.39,175.21,183.12};
    double ch[]={1.23,-0.45,0.67,-1.12,2.34,0.89,-2.45,0.56};
    for(int r=0;r<8;r++){
        auto mk=[](const QString&s,const QString&c,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto*i=new QTableWidgetItem(s);i->setForeground(QColor(c));i->setTextAlignment(a);return i;};
        QString cc=ch[r]>=0?"#608b4e":"#ce9178";
        t->setItem(r,0,mk(syms[r],"#d4d4d4",Qt::AlignLeft|Qt::AlignVCenter));
        t->setItem(r,1,mk(QString::number(pr[r],'f',2),"#d4d4d4"));
        t->setItem(r,2,mk(QString("%1%2").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
        t->setItem(r,3,mk(QString("%1%2%").arg(ch[r]>=0?"+":"").arg(ch[r],0,'f',2),cc));
    }
    v->addWidget(t,1);
    return w;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow win; win.setWindowTitle("Theme 07: Slate"); win.resize(1200,700);
    auto* c = new QWidget; c->setStyleSheet("background:#181818;");
    auto* root = new QVBoxLayout(c);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    root->addWidget(header_bar()); root->addWidget(tab_bar());
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:#181818;}");
    auto* body = new QWidget; auto* grid = new QHBoxLayout(body);
    grid->setContentsMargins(10,10,10,10); grid->setSpacing(8);
    grid->addWidget(market_panel("Indices")); grid->addWidget(market_panel("Forex"));
    grid->addWidget(market_panel("Commodities")); grid->addWidget(market_panel("Crypto"));
    scroll->setWidget(body); root->addWidget(scroll,1);
    auto* sb = new QWidget; sb->setFixedHeight(22);
    sb->setStyleSheet("background:#1e1e1e;border-top:1px solid #3c3c3c;");
    auto* sl = new QHBoxLayout(sb); sl->setContentsMargins(14,0,14,0);
    sl->addWidget(lbl("Fincept v4.0","#555555",9)); sl->addStretch();
    sl->addWidget(lbl("Ready","#608b4e",9,true));
    root->addWidget(sb);
    win.setCentralWidget(c); win.show(); return app.exec();
}
