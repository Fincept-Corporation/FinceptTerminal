#include "screens/markets/MarketPanel.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPointer>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace fincept::screens {

MarketPanel::MarketPanel(const QString& title, const QStringList& symbols, bool show_name, QWidget* parent)
    : QWidget(parent), title_(title), symbols_(symbols), show_name_(show_name) {
    setMinimumWidth(280);
    setMaximumWidth(520);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(show_name ? 322 : 322);
    setStyleSheet("background:#0a0a0a;border:1px solid #1e1e1e;");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget;
    header->setFixedHeight(32);
    header->setStyleSheet("background:#111418;border-bottom:1px solid #1e1e1e;"
                          "border-left:2px solid #d97706;");
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(10, 0, 8, 0);
    title_label_ = new QLabel(title.toUpper());
    title_label_->setStyleSheet("color:#d97706;font-size:12px;font-weight:700;background:transparent;"
                                "letter-spacing:1.2px;font-family:'Consolas',monospace;");
    hhl->addWidget(title_label_);
    hhl->addStretch();
    status_label_ = new QLabel;
    status_label_->setStyleSheet(
        "color:#333333;font-size:11px;background:transparent;font-family:'Consolas',monospace;");
    hhl->addWidget(status_label_);
    vl->addWidget(header);

    table_ = new QTableWidget;
    table_->setAlternatingRowColors(true);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(26);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setFixedHeight(18);
    table_->setStyleSheet("QTableWidget{background:#080808;alternate-background-color:#0d0d0d;border:none;"
                          "font-family:'Consolas',monospace;font-size:13px;}"
                          "QTableWidget::item{padding:0 6px;border-bottom:1px solid #161616;}"
                          "QTableWidget::item:selected{background:#1a1a0a;}"
                          "QHeaderView::section{background:#0e0e0e;color:#606060;border:none;"
                          "border-bottom:1px solid #1e1e1e;font-size:10px;font-weight:600;"
                          "letter-spacing:0.5px;padding:0 6px;font-family:'Consolas',monospace;}");

    if (show_name_) {
        table_->setColumnCount(6);
        table_->setHorizontalHeaderLabels({"SYM", "NAME", "LAST", "CHG", "CHG%", "VOL"});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int i = 2; i < 6; i++)
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    } else {
        table_->setColumnCount(7);
        table_->setHorizontalHeaderLabels({"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW", "VOL"});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 1; i < 7; i++)
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    vl->addWidget(table_, 1);

    loading_overlay_ = new ui::LoadingOverlay(this);

    refresh();
}

void MarketPanel::set_symbols(const QStringList& s) {
    symbols_ = s;
    refresh();
}

void MarketPanel::refresh() {
    status_label_->setText("LOADING");
    status_label_->setStyleSheet(
        "color:#d97706;font-size:12px;background:transparent;font-family:'Consolas',monospace;");

    loading_overlay_->show_loading("LOADING\xe2\x80\xa6");

    QPointer<MarketPanel> self = this;
    services::MarketDataService::instance().fetch_quotes(symbols_, [self](bool ok, QVector<services::QuoteData> q) {
        if (!self)
            return;
        self->loading_overlay_->hide_loading();
        if (!ok) {
            self->status_label_->setText("FAIL");
            self->status_label_->setStyleSheet(
                "color:#dc2626;font-size:12px;background:transparent;font-family:'Consolas',monospace;");
            emit self->refresh_finished();
            return;
        }
        self->status_label_->setText(QString::number(q.size()));
        self->status_label_->setStyleSheet(
            "color:#333333;font-size:12px;background:transparent;font-family:'Consolas',monospace;");
        self->populate(q);
        emit self->refresh_finished();
    });
}

void MarketPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (loading_overlay_)
        loading_overlay_->setGeometry(rect());
}

void MarketPanel::populate(const QVector<services::QuoteData>& quotes) {
    table_->setRowCount(0);
    for (const auto& q : quotes) {
        int row = table_->rowCount();
        table_->insertRow(row);
        auto mk = [](const QString& s, const QString& c, Qt::Alignment a = Qt::AlignRight | Qt::AlignVCenter) {
            auto* i = new QTableWidgetItem(s);
            i->setForeground(QColor(c));
            i->setTextAlignment(a);
            return i;
        };
        bool pos = q.change >= 0;
        const QString cc  = pos ? "#22c55e" : "#ef4444";   // brighter green/red
        const QString arr = pos ? "▲" : "▼";
        int prec = q.price > 100 ? 2 : (q.price > 1 ? 2 : 4);
        int col = 0;
        // Symbol — white identifier
        table_->setItem(row, col++, mk(q.symbol, "#e5e5e5", Qt::AlignLeft | Qt::AlignVCenter));
        // Name (regional panels)
        if (show_name_)
            table_->setItem(row, col++, mk(q.name, "#505050", Qt::AlignLeft | Qt::AlignVCenter));
        // LAST price — amber (the primary data point)
        table_->setItem(row, col++, mk(QString::number(q.price, 'f', prec), "#e5a020"));
        // CHG with directional arrow
        table_->setItem(row, col++,
            mk(QString("%1 %2").arg(arr).arg(std::abs(q.change), 0, 'f', 2), cc));
        // CHG%
        table_->setItem(row, col++,
            mk(QString("%1%2%").arg(arr).arg(std::abs(q.change_pct), 0, 'f', 2), cc));
        // HIGH / LOW — visible but secondary
        if (!show_name_) {
            table_->setItem(row, col++, mk(QString::number(q.high, 'f', 2), "#4a4a4a"));
            table_->setItem(row, col++, mk(QString::number(q.low,  'f', 2), "#4a4a4a"));
        }
        // VOL — dim placeholder (service doesn't provide volume yet)
        table_->setItem(row, col++, mk("--", "#2a2a2a"));
        table_->setRowHeight(row, 26);
    }
}

} // namespace fincept::screens
