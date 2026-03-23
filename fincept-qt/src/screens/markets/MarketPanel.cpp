#include "screens/markets/MarketPanel.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPointer>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

MarketPanel::MarketPanel(const QString& title, const QStringList& symbols, bool show_name, QWidget* parent)
    : QWidget(parent), title_(title), symbols_(symbols), show_name_(show_name) {
    setMinimumWidth(280);
    setMaximumWidth(520);
    setFixedHeight(show_name ? 340 : 340);
    setStyleSheet("background:#0a0a0a;border:1px solid #1a1a1a;");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget;
    header->setFixedHeight(32);
    header->setStyleSheet("background:#111111;border-bottom:1px solid #1a1a1a;");
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(8, 0, 8, 0);
    title_label_ = new QLabel(title.toUpper());
    title_label_->setStyleSheet("color:#d97706;font-size:14px;font-weight:700;background:transparent;"
                                "letter-spacing:0.8px;font-family:'Consolas',monospace;");
    hhl->addWidget(title_label_);
    hhl->addStretch();
    status_label_ = new QLabel;
    status_label_->setStyleSheet(
        "color:#333333;font-size:12px;background:transparent;font-family:'Consolas',monospace;");
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
    table_->setStyleSheet("QTableWidget{background:#080808;alternate-background-color:#0c0c0c;border:none;"
                          "font-family:'Consolas',monospace;font-size:14px;}"
                          "QTableWidget::item{padding:0 6px;border-bottom:1px solid #111111;}"
                          "QHeaderView::section{background:#0a0a0a;color:#404040;border:none;"
                          "border-bottom:1px solid #1a1a1a;font-size:8px;font-weight:600;padding:0 6px;"
                          "font-family:'Consolas',monospace;}");

    if (show_name_) {
        table_->setColumnCount(5);
        table_->setHorizontalHeaderLabels({"SYM", "NAME", "PRICE", "CHG", "CHG%"});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int i = 2; i < 5; i++)
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    } else {
        table_->setColumnCount(6);
        table_->setHorizontalHeaderLabels({"SYMBOL", "PRICE", "CHG", "CHG%", "HIGH", "LOW"});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 1; i < 6; i++)
            table_->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    vl->addWidget(table_, 1);

    // Skeleton pulse timer — shimmer every 350ms
    skeleton_timer_ = new QTimer(this);
    skeleton_timer_->setInterval(350);
    connect(skeleton_timer_, &QTimer::timeout, this, &MarketPanel::pulse_skeleton);

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

    show_skeleton();

    QPointer<MarketPanel> self = this;
    services::MarketDataService::instance().fetch_quotes(symbols_, [self](bool ok, QVector<services::QuoteData> q) {
        if (!self) return;
        self->skeleton_timer_->stop();
        if (!ok) {
            self->status_label_->setText("FAIL");
            self->status_label_->setStyleSheet(
                "color:#dc2626;font-size:12px;background:transparent;font-family:'Consolas',monospace;");
            // Clear skeleton rows on failure
            self->table_->setRowCount(0);
            return;
        }
        self->status_label_->setText(QString::number(q.size()));
        self->status_label_->setStyleSheet(
            "color:#333333;font-size:12px;background:transparent;font-family:'Consolas',monospace;");
        self->populate(q);
    });
}

void MarketPanel::show_skeleton() {
    skeleton_offset_ = 0;
    int cols = show_name_ ? 5 : 6;
    int rows = std::max(6, static_cast<int>(symbols_.size()));

    table_->setUpdatesEnabled(false);
    table_->setRowCount(0);
    table_->setRowCount(rows);

    for (int r = 0; r < rows; r++) {
        // Vary placeholder width per row for a natural stagger
        int w = 30 + (r % 4) * 15; // 30%, 45%, 60%, 75%, repeat
        QString bg = QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                             "stop:0 #111111,stop:%1 #1a1a1a,stop:1 #111111);")
                         .arg(w / 100.0, 0, 'f', 2);
        for (int c = 0; c < cols; c++) {
            auto* item = new QTableWidgetItem;
            item->setBackground(QBrush(QColor(c == 0 ? "#111111" : "#0d0d0d")));
            item->setFlags(Qt::NoItemFlags);
            table_->setItem(r, c, item);
        }
        table_->setRowHeight(r, 26);
        (void)bg; // used in pulse_skeleton
    }
    table_->setUpdatesEnabled(true);
    skeleton_timer_->start();
}

void MarketPanel::pulse_skeleton() {
    skeleton_offset_ = (skeleton_offset_ + 20) % 100;
    double s = skeleton_offset_ / 100.0;
    double e = std::min(1.0, s + 0.3);

    int cols = table_->columnCount();
    int rows = table_->rowCount();

    table_->setUpdatesEnabled(false);
    for (int r = 0; r < rows; r++) {
        // Shift phase per row so shimmer appears diagonal
        double rs = std::fmod(s + r * 0.05, 1.0);
        double re = std::min(1.0, rs + 0.3);
        QColor dark(r % 2 == 0 ? 0x11 : 0x0d, r % 2 == 0 ? 0x11 : 0x0d, r % 2 == 0 ? 0x11 : 0x0d);
        QColor light(0x1e, 0x1e, 0x1e);
        // Interpolate: cells before shimmer=dark, at shimmer=light, after=dark
        for (int c = 0; c < cols; c++) {
            double pos = c / static_cast<double>(std::max(1, cols - 1));
            double t = 0.0;
            if (pos >= rs && pos <= re)
                t = 1.0 - std::abs((pos - (rs + re) / 2.0) / (re - rs + 0.001));
            int rv = static_cast<int>(dark.red() + t * (light.red() - dark.red()));
            int gv = static_cast<int>(dark.green() + t * (light.green() - dark.green()));
            int bv = static_cast<int>(dark.blue() + t * (light.blue() - dark.blue()));
            if (auto* item = table_->item(r, c))
                item->setBackground(QBrush(QColor(rv, gv, bv)));
        }
    }
    table_->setUpdatesEnabled(true);
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
        QString cc = pos ? "#16a34a" : "#dc2626";
        int prec = q.price > 100 ? 2 : (q.price > 1 ? 2 : 4);
        int col = 0;
        table_->setItem(row, col++, mk(q.symbol, "#e5e5e5", Qt::AlignLeft | Qt::AlignVCenter));
        if (show_name_)
            table_->setItem(row, col++, mk(q.name, "#525252", Qt::AlignLeft | Qt::AlignVCenter));
        table_->setItem(row, col++, mk(QString::number(q.price, 'f', prec), "#e5e5e5"));
        table_->setItem(row, col++, mk(QString("%1%2").arg(pos ? "+" : "").arg(q.change, 0, 'f', 2), cc));
        table_->setItem(row, col++, mk(QString("%1%2%").arg(pos ? "+" : "").arg(q.change_pct, 0, 'f', 2), cc));
        if (!show_name_) {
            table_->setItem(row, col++, mk(QString::number(q.high, 'f', 2), "#333333"));
            table_->setItem(row, col++, mk(QString::number(q.low, 'f', 2), "#333333"));
        }
        table_->setRowHeight(row, 26);
    }
}

} // namespace fincept::screens
