#include "SurfaceDataInspector.h"

#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>

namespace fincept::surface {

using namespace fincept::ui;

namespace {

QString section_header_qss() {
    return QString("background:%1; color:%2; font-size:9px; font-weight:bold; "
                   "padding:4px 8px; border-bottom:1px solid %3;")
        .arg(colors::BG_SURFACE())
        .arg(colors::TEXT_SECONDARY())
        .arg(colors::BORDER_DIM());
}

QString table_qss() {
    return QString("QTableView { background:%1; color:%2; gridline-color:%3; "
                   "border:none; font-size:10px; font-family:Consolas; "
                   "selection-background-color:%4; selection-color:%5; }"
                   "QHeaderView::section { background:%6; color:%7; padding:3px 6px; "
                   "border:none; border-right:1px solid %3; border-bottom:1px solid %3; "
                   "font-size:9px; font-weight:bold; }")
        .arg(colors::BG_BASE())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_DIM())
        .arg(colors::BG_RAISED())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BG_SURFACE())
        .arg(colors::TEXT_SECONDARY());
}

QString tab_qss() {
    return QString("QTabBar::tab { background:%1; color:%2; padding:4px 12px; "
                   "border:1px solid %3; border-bottom:none; font-size:9px; }"
                   "QTabBar::tab:selected { background:%4; color:%5; }")
        .arg(colors::BG_BASE())
        .arg(colors::TEXT_DIM())
        .arg(colors::BORDER_DIM())
        .arg(colors::BG_SURFACE())
        .arg(colors::TEXT_PRIMARY());
}

QString small_btn_qss() {
    return QString("QPushButton { background:%1; color:%2; border:1px solid %3; "
                   "padding:3px 10px; font-size:9px; border-radius:2px; }"
                   "QPushButton:hover { background:%3; color:%4; }")
        .arg(colors::BG_RAISED())
        .arg(colors::TEXT_SECONDARY())
        .arg(colors::BORDER_MED())
        .arg(colors::TEXT_PRIMARY());
}

} // namespace

SurfaceDataInspector::SurfaceDataInspector(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void SurfaceDataInspector::setup_ui() {
    setStyleSheet(QString("background:%1; color:%2;")
                      .arg(colors::BG_BASE())
                      .arg(colors::TEXT_PRIMARY()));
    setMinimumHeight(190);

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── COLUMN 1: Raw data tabs + table ────────────────────────────────────
    auto* col1 = new QWidget(this);
    col1->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));
    auto* col1_layout = new QVBoxLayout(col1);
    col1_layout->setContentsMargins(0, 0, 0, 0);
    col1_layout->setSpacing(0);

    auto* col1_header_row = new QHBoxLayout();
    col1_header_row->setContentsMargins(0, 0, 0, 0);
    col1_header_row->setSpacing(0);
    auto* col1_header = new QLabel("RAW DATA", col1);
    col1_header->setStyleSheet(section_header_qss());
    col1_header->setMinimumHeight(22);
    col1_header_row->addWidget(col1_header, 1);
    export_btn_ = new QPushButton("EXPORT CSV", col1);
    export_btn_->setStyleSheet(small_btn_qss());
    connect(export_btn_, &QPushButton::clicked, this, &SurfaceDataInspector::on_export_csv_clicked);
    col1_header_row->addWidget(export_btn_);
    col1_layout->addLayout(col1_header_row);

    tab_bar_ = new QTabBar(col1);
    tab_bar_->setStyleSheet(tab_qss());
    tab_bar_->setExpanding(false);
    connect(tab_bar_, &QTabBar::currentChanged, this, &SurfaceDataInspector::on_tab_changed);
    col1_layout->addWidget(tab_bar_);

    table_view_ = new QTableView(col1);
    table_model_ = new QStandardItemModel(this);
    table_view_->setModel(table_model_);
    table_view_->setStyleSheet(table_qss());
    table_view_->setAlternatingRowColors(false);
    table_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_view_->setSortingEnabled(true);
    table_view_->verticalHeader()->setVisible(false);
    table_view_->verticalHeader()->setDefaultSectionSize(18);
    col1_layout->addWidget(table_view_, 1);

    outer->addWidget(col1, 6);

    // ── COLUMN 2: Lineage ──────────────────────────────────────────────────
    auto* col2 = new QWidget(this);
    col2->setStyleSheet(QString("background:%1; border-left:1px solid %2;")
                            .arg(colors::BG_BASE())
                            .arg(colors::BORDER_DIM()));
    auto* col2_layout = new QVBoxLayout(col2);
    col2_layout->setContentsMargins(0, 0, 0, 0);
    col2_layout->setSpacing(0);

    auto* col2_header = new QLabel("LINEAGE", col2);
    col2_header->setStyleSheet(section_header_qss());
    col2_header->setMinimumHeight(22);
    col2_layout->addWidget(col2_header);

    auto* lin_body = new QWidget(col2);
    auto* lin_l = new QVBoxLayout(lin_body);
    lin_l->setContentsMargins(8, 8, 8, 8);
    lin_l->setSpacing(4);

    auto add_row = [&](const QString& key, QLabel*& target) {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto* k = new QLabel(key, lin_body);
        k->setStyleSheet(QString("color:%1; font-size:9px; font-family:Consolas;")
                             .arg(colors::TEXT_DIM()));
        k->setMinimumWidth(72);
        row->addWidget(k);
        target = new QLabel("—", lin_body);
        target->setStyleSheet(QString("color:%1; font-size:10px; font-family:Consolas;")
                                  .arg(colors::TEXT_PRIMARY()));
        target->setWordWrap(true);
        row->addWidget(target, 1);
        lin_l->addLayout(row);
    };
    add_row("dataset", lin_dataset_);
    add_row("schema", lin_schema_);
    add_row("symbology", lin_symbology_);
    add_row("symbols", lin_symbols_);
    add_row("range", lin_dates_);
    add_row("rows", lin_count_);
    add_row("cost", lin_cost_);
    lin_l->addStretch();
    col2_layout->addWidget(lin_body, 1);

    outer->addWidget(col2, 3);

    // ── COLUMN 3: Status / errors ──────────────────────────────────────────
    auto* col3 = new QWidget(this);
    col3->setStyleSheet(QString("background:%1; border-left:1px solid %2;")
                            .arg(colors::BG_BASE())
                            .arg(colors::BORDER_DIM()));
    auto* col3_layout = new QVBoxLayout(col3);
    col3_layout->setContentsMargins(0, 0, 0, 0);
    col3_layout->setSpacing(0);

    auto* col3_header = new QLabel("STATUS / ERRORS", col3);
    col3_header->setStyleSheet(section_header_qss());
    col3_header->setMinimumHeight(22);
    col3_layout->addWidget(col3_header);

    auto* status_body = new QWidget(col3);
    auto* status_l = new QVBoxLayout(status_body);
    status_l->setContentsMargins(8, 8, 8, 8);
    status_l->setSpacing(6);

    auto* dot_row = new QHBoxLayout();
    dot_row->setSpacing(6);
    status_dot_ = new QLabel("●", status_body);
    status_dot_->setStyleSheet(QString("color:%1; font-size:14px;").arg(colors::TEXT_DIM()));
    dot_row->addWidget(status_dot_);
    status_label_ = new QLabel("Idle", status_body);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(colors::TEXT_PRIMARY()));
    dot_row->addWidget(status_label_, 1);
    status_l->addLayout(dot_row);

    error_label_ = new QLabel("", status_body);
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet(QString("color:%1; font-size:9px; font-family:Consolas;")
                                    .arg(colors::NEGATIVE()));
    status_l->addWidget(error_label_);

    view_raw_btn_ = new QPushButton("VIEW RAW RESPONSE", status_body);
    view_raw_btn_->setStyleSheet(small_btn_qss());
    connect(view_raw_btn_, &QPushButton::clicked, this, &SurfaceDataInspector::on_view_raw_clicked);
    status_l->addWidget(view_raw_btn_);
    status_l->addStretch();
    col3_layout->addWidget(status_body, 1);

    outer->addWidget(col3, 2);
}

void SurfaceDataInspector::clear() {
    snapshots_.clear();
    while (tab_bar_->count() > 0)
        tab_bar_->removeTab(0);
    table_model_->clear();
    set_lineage("", "", "", "", "", 0, 0.0);
    error_label_->setText("");
    set_status("Idle", true);
    raw_output_.clear();
}

void SurfaceDataInspector::show_table(const QString& tab_name, const QStringList& headers,
                                      const QVector<QStringList>& rows) {
    // Append (or replace if same name already exists) and select.
    int existing = -1;
    for (int i = 0; i < snapshots_.size(); ++i) {
        if (snapshots_[i].name == tab_name) {
            existing = i;
            break;
        }
    }
    TableSnapshot snap{tab_name, headers, rows};
    if (existing >= 0) {
        snapshots_[existing] = snap;
        tab_bar_->blockSignals(true);
        tab_bar_->setCurrentIndex(existing);
        tab_bar_->blockSignals(false);
        on_tab_changed(existing);
    } else {
        snapshots_.push_back(snap);
        tab_bar_->blockSignals(true);
        int idx = tab_bar_->addTab(tab_name);
        tab_bar_->setCurrentIndex(idx);
        tab_bar_->blockSignals(false);
        on_tab_changed(idx);
    }
}

void SurfaceDataInspector::on_tab_changed(int index) {
    if (index < 0 || index >= snapshots_.size())
        return;
    const auto& s = snapshots_[index];
    table_model_->clear();
    table_model_->setHorizontalHeaderLabels(s.headers);
    for (const auto& r : s.rows) {
        QList<QStandardItem*> items;
        for (const QString& cell : r)
            items.push_back(new QStandardItem(cell));
        table_model_->appendRow(items);
    }
    table_view_->resizeColumnsToContents();
}

void SurfaceDataInspector::set_lineage(const QString& dataset, const QString& schema,
                                       const QString& symbology, const QString& symbols,
                                       const QString& date_range, qint64 row_count,
                                       double cost_usd) {
    lin_dataset_->setText(dataset.isEmpty() ? "—" : dataset);
    lin_schema_->setText(schema.isEmpty() ? "—" : schema);
    lin_symbology_->setText(symbology.isEmpty() ? "—" : symbology);
    lin_symbols_->setText(symbols.isEmpty() ? "—" : symbols);
    lin_dates_->setText(date_range.isEmpty() ? "—" : date_range);
    lin_count_->setText(row_count > 0 ? QString::number(row_count) : "—");
    lin_cost_->setText(cost_usd > 0.0 ? QString("$%1").arg(cost_usd, 0, 'f', 4) : "—");
}

void SurfaceDataInspector::set_status(const QString& message, bool ok) {
    QString stamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    status_label_->setText(QString("[%1] %2").arg(stamp).arg(message));
    status_dot_->setStyleSheet(QString("color:%1; font-size:14px;")
                                   .arg(ok ? colors::POSITIVE() : colors::NEGATIVE()));
    if (ok)
        error_label_->setText("");
}

void SurfaceDataInspector::set_error(const QString& error) {
    error_label_->setText(error);
    if (!error.isEmpty()) {
        status_dot_->setStyleSheet(QString("color:%1; font-size:14px;").arg(colors::NEGATIVE()));
    }
}

void SurfaceDataInspector::set_raw_output(const QString& raw_stdout) {
    raw_output_ = raw_stdout;
}

void SurfaceDataInspector::on_view_raw_clicked() {
    QDialog dlg(this);
    dlg.setWindowTitle("Raw Databento response");
    dlg.resize(720, 480);
    auto* lay = new QVBoxLayout(&dlg);
    auto* edit = new QPlainTextEdit(&dlg);
    edit->setReadOnly(true);
    edit->setStyleSheet(QString("background:%1; color:%2; font-family:Consolas; font-size:10px;")
                            .arg(colors::BG_BASE())
                            .arg(colors::TEXT_PRIMARY()));
    edit->setPlainText(raw_output_.isEmpty() ? "(no response captured yet)" : raw_output_);
    lay->addWidget(edit);
    auto* close_btn = new QPushButton("CLOSE", &dlg);
    close_btn->setStyleSheet(small_btn_qss());
    connect(close_btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(close_btn, 0, Qt::AlignRight);
    dlg.exec();
}

void SurfaceDataInspector::on_export_csv_clicked() {
    if (snapshots_.isEmpty())
        return;
    int idx = tab_bar_->currentIndex();
    if (idx < 0 || idx >= snapshots_.size())
        return;
    const auto& snap = snapshots_[idx];

    QString suggested = QString("%1.csv").arg(snap.name);
    QString path = QFileDialog::getSaveFileName(this, "Export CSV", suggested,
                                                "CSV files (*.csv)");
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&f);
    out << snap.headers.join(",") << "\n";
    for (const auto& r : snap.rows)
        out << r.join(",") << "\n";
}

} // namespace fincept::surface
