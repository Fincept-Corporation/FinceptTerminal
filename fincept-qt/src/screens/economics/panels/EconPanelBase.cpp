// src/screens/economics/panels/EconPanelBase.cpp
#include "screens/economics/panels/EconPanelBase.h"

#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTextStream>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Stylesheet ────────────────────────────────────────────────────────────────

QString EconPanelBase::panel_style() const {
    int r = QColor(color_).red();
    int g = QColor(color_).green();
    int b = QColor(color_).blue();
    return QString(
        "#econToolbar { background:#111111; border-bottom:1px solid #1a1a1a; }"
        "#econFetchBtn { background:%1; color:#080808; border:none;"
        "  font-size:10px; font-weight:700; padding:4px 14px; }"
        "#econFetchBtn:hover { background:rgba(%2,%3,%4,0.75); }"
        "#econFetchBtn:disabled { background:#1a1a1a; color:#404040; }"
        "#econCsvBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
        "  font-size:10px; font-weight:700; padding:4px 10px; }"
        "#econCsvBtn:hover { color:#e5e5e5; background:#161616; }"
        "QComboBox, QSpinBox, QDateEdit, QLineEdit {"
        "  background:#080808; color:#e5e5e5; border:1px solid #1a1a1a;"
        "  font-size:11px; padding:2px 6px; }"
        "QComboBox::drop-down, QDateEdit::drop-down { border:none; }"
        "QComboBox QAbstractItemView { background:#111111; color:#e5e5e5;"
        "  border:1px solid #1a1a1a; }"
        "QTableWidget { background:#080808; color:#e5e5e5; border:none;"
        "  gridline-color:#1a1a1a; font-size:11px; alternate-background-color:#0a0a0a; }"
        "QTableWidget::item { padding:5px 8px; border-bottom:1px solid #1a1a1a; }"
        "QTableWidget::item:selected { background:rgba(%2,%3,%4,0.1); color:%1; }"
        "QHeaderView::section { background:#111111; color:#808080; border:none;"
        "  border-bottom:2px solid #1a1a1a; border-right:1px solid #1a1a1a;"
        "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"
        "#econStatCard { background:#0a0a0a; border:1px solid #1a1a1a; }"
        "#econStatCard:hover { border-color:#333333; }"
        "#econStatLabel { color:#808080; font-size:8px; font-weight:700;"
        "  letter-spacing:1px; background:transparent; }"
        "#econStatVal { color:%1; font-size:15px; font-weight:700; background:transparent; }"
        "#econStatSub { color:#525252; font-size:9px; background:transparent; }"
        "#econEmptyPage { background:#080808; }"
        "#econEmptyMsg { color:#808080; font-size:13px; background:transparent; }"
        "#econErrMsg   { color:#dc2626; font-size:12px; background:transparent; }"
        "#econTitleLbl { color:#e5e5e5; font-size:11px; font-weight:700;"
        "  background:transparent; }"
        "#econRowCount { color:#525252; font-size:9px; background:transparent; }"
        "QScrollBar:vertical { background:#080808; width:5px; }"
        "QScrollBar::handle:vertical { background:#1a1a1a; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QScrollBar:horizontal { background:#080808; height:5px; }"
        "QScrollBar::handle:horizontal { background:#1a1a1a; min-width:20px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }"
    ).arg(color_).arg(r).arg(g).arg(b);
}

// ── Constructor ───────────────────────────────────────────────────────────────

EconPanelBase::EconPanelBase(const QString& source_id,
                             const QString& color,
                             QWidget* parent)
    : QWidget(parent), source_id_(source_id), color_(color) {}

// ── Build ─────────────────────────────────────────────────────────────────────

void EconPanelBase::build_base_ui(QWidget* container) {
    container->setStyleSheet(panel_style());

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget;
    toolbar->setObjectName("econToolbar");
    toolbar->setFixedHeight(40);
    auto* thl = new QHBoxLayout(toolbar);
    thl->setContentsMargins(10, 0, 10, 0);
    thl->setSpacing(6);
    build_controls(thl);
    thl->addStretch(1);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("econFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &EconPanelBase::on_fetch);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("econCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &EconPanelBase::export_csv);

    thl->addWidget(fetch_btn_);
    thl->addWidget(export_btn_);
    root->addWidget(toolbar);

    // Stat cards row
    auto* cards_row = new QWidget;
    cards_row->setStyleSheet("background:#080808; border-bottom:1px solid #1a1a1a;");
    auto* crhl = new QHBoxLayout(cards_row);
    crhl->setContentsMargins(10, 6, 10, 6);
    crhl->setSpacing(6);

    auto make_card = [&](const QString& label, QLabel*& out) {
        auto* card = new QWidget;
        card->setObjectName("econStatCard");
        card->setMinimumWidth(90);
        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(10, 6, 10, 6);
        vl->setSpacing(1);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("econStatLabel");
        out = new QLabel("—");
        out->setObjectName("econStatVal");
        vl->addWidget(lbl);
        vl->addWidget(out);
        crhl->addWidget(card);
    };
    make_card("LATEST",  stat_latest_);
    make_card("CHANGE",  stat_change_);
    make_card("MIN",     stat_min_);
    make_card("MAX",     stat_max_);
    make_card("AVG",     stat_avg_);
    make_card("POINTS",  stat_count_);
    crhl->addStretch(1);
    root->addWidget(cards_row);

    // Title bar
    auto* title_bar = new QWidget;
    title_bar->setStyleSheet("background:#0a0a0a; border-bottom:1px solid #1a1a1a;");
    auto* tbhl = new QHBoxLayout(title_bar);
    tbhl->setContentsMargins(12, 4, 12, 4);
    title_lbl_ = new QLabel;
    title_lbl_->setObjectName("econTitleLbl");
    row_count_ = new QLabel;
    row_count_->setObjectName("econRowCount");
    tbhl->addWidget(title_lbl_);
    tbhl->addStretch(1);
    tbhl->addWidget(row_count_);
    root->addWidget(title_bar);

    // Content stack
    stack_ = new QStackedWidget;

    auto* empty_pg = new QWidget;
    empty_pg->setObjectName("econEmptyPage");
    auto* evl = new QVBoxLayout(empty_pg);
    evl->setAlignment(Qt::AlignCenter);
    empty_lbl_ = new QLabel("Select parameters and click FETCH");
    empty_lbl_->setObjectName("econEmptyMsg");
    empty_lbl_->setAlignment(Qt::AlignCenter);
    empty_lbl_->setWordWrap(true);
    evl->addWidget(empty_lbl_);
    stack_->addWidget(empty_pg);   // index 0

    table_ = new QTableWidget;
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(true);
    stack_->addWidget(table_);     // index 1

    root->addWidget(stack_, 1);
}

// ── State ─────────────────────────────────────────────────────────────────────

void EconPanelBase::show_loading(const QString& msg) {
    if (!empty_lbl_) return;
    empty_lbl_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(color_));
    empty_lbl_->setText(msg);
    stack_->setCurrentIndex(0);
    if (fetch_btn_) fetch_btn_->setEnabled(false);
}

void EconPanelBase::show_error(const QString& msg) {
    if (!empty_lbl_) return;
    empty_lbl_->setStyleSheet("color:#dc2626; font-size:12px; background:transparent;");
    empty_lbl_->setText("Error: " + msg);
    stack_->setCurrentIndex(0);
    if (fetch_btn_) fetch_btn_->setEnabled(true);
}

void EconPanelBase::show_empty(const QString& msg) {
    if (!empty_lbl_) return;
    empty_lbl_->setStyleSheet("color:#808080; font-size:13px; background:transparent;");
    empty_lbl_->setText(msg);
    stack_->setCurrentIndex(0);
    if (fetch_btn_) fetch_btn_->setEnabled(true);
}

void EconPanelBase::show_table() {
    stack_->setCurrentIndex(1);
    if (fetch_btn_) fetch_btn_->setEnabled(true);
}

// ── Display ───────────────────────────────────────────────────────────────────

void EconPanelBase::display(const QJsonArray& rows, const QString& title) {
    if (rows.isEmpty()) { show_empty("No data returned for this selection"); return; }

    // Collect all keys, prioritise date then value
    QStringList cols;
    for (const auto& v : rows) {
        for (const auto& k : v.toObject().keys())
            if (!cols.contains(k)) cols << k;
    }
    static const QStringList kDateKeys = {
        "date","period","year","time","Date","Period","TIME_PERIOD","record_date","ref_date"};
    for (const auto& dk : kDateKeys) {
        int idx = cols.indexOf(dk);
        if (idx > 0) { cols.move(idx, 0); break; }
    }
    static const QStringList kValKeys = {
        "value","Value","val","OBS_VALUE","obs_value","amount","rate"};
    for (const auto& vk : kValKeys) {
        int idx = cols.indexOf(vk);
        if (idx > 1) { cols.move(idx, 1); break; }
    }

    table_->setColumnCount(cols.size());
    table_->setHorizontalHeaderLabels(cols);
    table_->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        const auto obj = rows[r].toObject();
        for (int c = 0; c < cols.size(); ++c) {
            const auto jv = obj[cols[c]];
            QString text;
            if (jv.isDouble())
                text = QString::number(jv.toDouble(), 'g', 8);
            else
                text = jv.toString();
            auto* item = new QTableWidgetItem(text);
            if (c == 0) item->setForeground(QColor(color_));
            table_->setItem(r, c, item);
        }
    }

    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    if (cols.size() > 1)
        table_->horizontalHeader()->setSectionResizeMode(
            cols.size() - 1, QHeaderView::Stretch);

    if (title_lbl_) title_lbl_->setText(title);
    if (row_count_) row_count_->setText(QString::number(rows.size()) + " records");

    update_stats(rows);
    show_table();
}

void EconPanelBase::update_stats(const QJsonArray& rows) {
    QVector<double> vals;
    for (const auto& v : rows) {
        const auto obj = v.toObject();
        QJsonValue jv  = obj["value"];
        if (jv.isUndefined()) jv = obj["Value"];
        if (jv.isUndefined()) jv = obj["OBS_VALUE"];
        if (jv.isUndefined()) jv = obj["obs_value"];
        if (jv.isUndefined()) jv = obj["amount"];
        if (jv.isUndefined()) jv = obj["rate"];
        if (jv.isUndefined()) {
            auto keys = obj.keys();
            if (keys.size() > 1) jv = obj[keys[1]];
        }
        bool ok = false;
        double d = jv.toString().toDouble(&ok);
        if (!ok && jv.isDouble()) { d = jv.toDouble(); ok = true; }
        if (ok) vals << d;
    }

    if (vals.isEmpty()) {
        for (auto* l : {stat_latest_, stat_change_, stat_min_,
                        stat_max_,    stat_avg_,    stat_count_})
            if (l) l->setText("—");
        if (stat_count_) stat_count_->setText(QString::number(rows.size()));
        return;
    }

    double latest = vals.last();
    double prev   = vals.size() > 1 ? vals[vals.size() - 2] : latest;
    double change = (prev != 0.0) ? ((latest - prev) / qAbs(prev)) * 100.0 : 0.0;
    double mn     = *std::min_element(vals.begin(), vals.end());
    double mx     = *std::max_element(vals.begin(), vals.end());
    double sum    = 0.0;
    for (double x : vals) sum += x;
    double avg    = sum / vals.size();

    auto fmt = [](double v) { return QString::number(v, 'g', 6); };

    if (stat_latest_) stat_latest_->setText(fmt(latest));
    if (stat_change_) {
        stat_change_->setText((change >= 0 ? "+" : "") +
                              QString::number(change, 'f', 2) + "%");
        stat_change_->setStyleSheet(
            QString("color:%1; font-size:15px; font-weight:700; background:transparent;")
                .arg(change >= 0 ? "#16a34a" : "#dc2626"));
    }
    if (stat_min_)   stat_min_->setText(fmt(mn));
    if (stat_max_)   stat_max_->setText(fmt(mx));
    if (stat_avg_)   stat_avg_->setText(fmt(avg));
    if (stat_count_) stat_count_->setText(QString::number(rows.size()));
}

// ── CSV ───────────────────────────────────────────────────────────────────────

void EconPanelBase::export_csv() {
    if (!table_ || table_->rowCount() == 0) return;

    QString path = QFileDialog::getSaveFileName(
        this, "Export CSV", source_id_ + "_data.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    QStringList hdrs;
    for (int c = 0; c < table_->columnCount(); ++c) {
        auto* h = table_->horizontalHeaderItem(c);
        hdrs << (h ? h->text() : "");
    }
    out << hdrs.join(",") << "\n";

    for (int r = 0; r < table_->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < table_->columnCount(); ++c) {
            auto* item = table_->item(r, c);
            QString val = item ? item->text() : "";
            if (val.contains(',') || val.contains('"'))
                val = "\"" + val.replace("\"", "\"\"") + "\"";
            row << val;
        }
        out << row.join(",") << "\n";
    }
}

} // namespace fincept::screens
