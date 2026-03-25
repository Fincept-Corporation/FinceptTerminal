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
    status_label_->setText(has_loaded_data_ ? "UPDATING" : "LOADING");
    status_label_->setStyleSheet(
        "color:#d97706;font-size:12px;background:transparent;font-family:'Consolas',monospace;");

    // Keep existing table visible during background refreshes.
    // Show skeleton only for first load or empty table.
    if (!has_loaded_data_ || table_->rowCount() == 0)
        show_skeleton();

    QPointer<MarketPanel> self = this;
    services::MarketDataService::instance().fetch_quotes(symbols_, [self](bool ok, QVector<services::QuoteData> q) {
        if (!self)
            return;
        self->skeleton_timer_->stop();
        if (!ok) {
            self->status_label_->setText(self->has_loaded_data_ ? "STALE" : "FAIL");
            self->status_label_->setStyleSheet(
                "color:#dc2626;font-size:12px;background:transparent;font-family:'Consolas',monospace;");
            if (!self->has_loaded_data_) {
                // First load failed: clear any skeleton rows.
                self->table_->setRowCount(0);
            }
            emit self->refresh_finished();
            return;
        }
        self->status_label_->setText(QString::number(q.size()));
        self->status_label_->setStyleSheet(
            "color:#333333;font-size:12px;background:transparent;font-family:'Consolas',monospace;");
        self->has_loaded_data_ = !q.isEmpty();
        self->populate(q);
        emit self->refresh_finished();
    });
}

void MarketPanel::show_skeleton() {
    skeleton_offset_ = 0;
    int cols = show_name_ ? 6 : 7;
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
