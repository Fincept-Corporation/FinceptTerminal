// src/screens/excel/ExcelScreen.cpp
#include "screens/excel/ExcelScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/excel/SpreadsheetWidget.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabBar>
#include <QVBoxLayout>

#include <xlsxdocument.h>

namespace fincept::screens {

using namespace fincept::ui;

static QString kAccent() {
    return QString("#ea580c");
} // Orange accent matching Tauri version

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ExcelScreen::ExcelScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
}

// ─────────────────────────────────────────────────────────────────────────────
// Show / Hide
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    LOG_INFO("ExcelScreen", "Screen shown");
}

void ExcelScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    LOG_INFO("ExcelScreen", "Screen hidden");
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::build_ui() {
    setStyleSheet(QString("QWidget { background:%1; color:%2; }").arg(colors::BG_BASE, colors::TEXT_PRIMARY));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar
    root->addWidget(build_toolbar());

    // Tab widget for sheets
    sheet_tabs_ = new QTabWidget(this);
    sheet_tabs_->setTabPosition(QTabWidget::South);
    sheet_tabs_->setMovable(true);
    sheet_tabs_->setTabsClosable(false); // We handle close via button
    sheet_tabs_->setStyleSheet(QString("QTabWidget::pane { border:none; background:%3; }"
                                       "QTabBar { background:%4; }"
                                       "QTabBar::tab { background:%5; color:%6; border:1px solid %7;"
                                       "  padding:4px 14px; font-family:%1; font-size:10px; margin-right:2px; }"
                                       "QTabBar::tab:selected { background:%2; color:%8; border-color:%2; }"
                                       "QTabBar::tab:hover { background:%7; }")
                                   .arg(fonts::DATA_FAMILY, kAccent(), colors::BG_HOVER, colors::BORDER_MED,
                                        colors::TEXT_DIM, colors::TEXT_SECONDARY, colors::TEXT_TERTIARY,
                                        colors::TEXT_PRIMARY));

    // Add initial sheet
    auto* sheet1 = new SpreadsheetWidget("Sheet1", 100, 26, sheet_tabs_);
    sheet_tabs_->addTab(sheet1, "Sheet1");

    connect(sheet_tabs_, &QTabWidget::currentChanged, this, &ExcelScreen::on_tab_changed);

    root->addWidget(sheet_tabs_, 1);

    // Status bar
    auto* status_bar = new QWidget(this);
    status_bar->setFixedHeight(24);
    status_bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(colors::BG_HOVER, colors::BORDER_MED));

    auto* status_hl = new QHBoxLayout(status_bar);
    status_hl->setContentsMargins(12, 0, 12, 0);
    status_hl->setSpacing(12);

    status_label_ = new QLabel(this);
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    status_hl->addWidget(status_label_);
    status_hl->addStretch();

    root->addWidget(status_bar);

    update_status();
}

QWidget* ExcelScreen::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(40);
    bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(colors::BORDER_MED, colors::TEXT_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(4);

    // Title
    auto* title = new QLabel("EXCEL SPREADSHEET", bar);
    title->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:700; margin-right:12px;")
                             .arg(kAccent(), fonts::DATA_FAMILY));
    hl->addWidget(title);

    // Button factory
    auto make_btn = [&](const QString& text, const QString& tooltip = {}) -> QPushButton* {
        auto* btn = new QPushButton(text, bar);
        btn->setToolTip(tooltip);
        btn->setStyleSheet(
            QString("QPushButton { background:%3; color:%4; border:none;"
                    " font-family:%1; font-size:10px; font-weight:600; padding:6px 12px; }"
                    "QPushButton:hover { background:%5; }"
                    "QPushButton:pressed { background:%2; }")
                .arg(fonts::DATA_FAMILY, kAccent(), colors::TEXT_DIM, colors::TEXT_PRIMARY, colors::TEXT_TERTIARY));
        return btn;
    };

    auto* import_btn = make_btn("IMPORT", "Import XLSX/CSV file");
    connect(import_btn, &QPushButton::clicked, this, &ExcelScreen::on_import);
    hl->addWidget(import_btn);

    auto* export_btn = make_btn("EXPORT", "Export as XLSX");
    connect(export_btn, &QPushButton::clicked, this, &ExcelScreen::on_export);
    hl->addWidget(export_btn);

    auto* export_csv_btn = make_btn("CSV", "Export active sheet as CSV");
    connect(export_csv_btn, &QPushButton::clicked, this, &ExcelScreen::on_export_csv);
    hl->addWidget(export_csv_btn);

    // Separator
    auto* sep1 = new QWidget(bar);
    sep1->setFixedSize(1, 20);
    sep1->setStyleSheet(QString("background:%1;").arg(colors::TEXT_TERTIARY));
    hl->addWidget(sep1);

    auto* add_btn = make_btn("+ SHEET", "Add new sheet");
    connect(add_btn, &QPushButton::clicked, this, &ExcelScreen::on_add_sheet);
    hl->addWidget(add_btn);

    auto* rename_btn = make_btn("RENAME", "Rename current sheet");
    connect(rename_btn, &QPushButton::clicked, this, &ExcelScreen::on_rename_sheet);
    hl->addWidget(rename_btn);

    auto* del_btn = make_btn("DELETE", "Delete current sheet");
    del_btn->setStyleSheet(QString("QPushButton { background:%2; color:%3; border:none;"
                                   " font-family:%1; font-size:10px; font-weight:600; padding:6px 12px; }"
                                   "QPushButton:hover { background:%4; }")
                               .arg(fonts::DATA_FAMILY, colors::TEXT_DIM, colors::TEXT_PRIMARY, colors::NEGATIVE));
    connect(del_btn, &QPushButton::clicked, this, &ExcelScreen::on_delete_sheet);
    hl->addWidget(del_btn);

    hl->addStretch();

    // File name label
    auto* fname_label = new QLabel(file_name_, bar);
    fname_label->setObjectName("excelFileName");
    fname_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px;").arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    hl->addWidget(fname_label);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Import (XLSX via QXlsx)
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::on_import() {
    QString path = QFileDialog::getOpenFileName(this, "Import Spreadsheet", {},
                                                "Spreadsheet Files (*.xlsx *.xls *.csv);;All Files (*)");
    if (path.isEmpty())
        return;

    QXlsx::Document xlsx(path);
    QStringList sheet_names = xlsx.sheetNames();
    if (sheet_names.isEmpty()) {
        LOG_ERROR("ExcelScreen", "No sheets found in file");
        return;
    }

    // Remove existing tabs
    while (sheet_tabs_->count() > 0) {
        auto* w = sheet_tabs_->widget(0);
        sheet_tabs_->removeTab(0);
        w->deleteLater();
    }

    for (const auto& name : sheet_names) {
        xlsx.selectSheet(name);

        // Determine dimensions
        auto dim = xlsx.dimension();
        int max_row = std::max(dim.lastRow(), 100);
        int max_col = std::max(dim.lastColumn(), 26);

        auto* sheet = new SpreadsheetWidget(name, max_row, max_col, sheet_tabs_);

        // Load data
        QVector<QVector<QString>> data(max_row);
        for (int r = 0; r < max_row; ++r) {
            data[r].resize(max_col);
            for (int c = 0; c < max_col; ++c) {
                auto cell = xlsx.read(r + 1, c + 1); // QXlsx is 1-based
                data[r][c] = cell.isValid() ? cell.toString() : "";
            }
        }
        sheet->set_data(data);
        sheet_tabs_->addTab(sheet, name);
    }

    // Update file info
    file_name_ = QFileInfo(path).fileName();
    file_path_ = path;
    auto* fname = findChild<QLabel*>("excelFileName");
    if (fname)
        fname->setText(file_name_);

    sheet_counter_ = sheet_names.size() + 1;
    update_status();
    LOG_INFO("ExcelScreen", QString("Imported %1 sheets from %2").arg(sheet_names.size()).arg(file_name_));

    // Register with File Manager so it appears in the Files tab
    services::FileManagerService::instance().import_file(path, "excel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Export (XLSX via QXlsx)
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::on_export() {
    QString path = QFileDialog::getSaveFileName(this, "Export as XLSX", file_name_, "Excel Files (*.xlsx)");
    if (path.isEmpty())
        return;

    QXlsx::Document xlsx;

    for (int i = 0; i < sheet_tabs_->count(); ++i) {
        auto* sheet = qobject_cast<SpreadsheetWidget*>(sheet_tabs_->widget(i));
        if (!sheet)
            continue;

        if (i > 0)
            xlsx.addSheet(sheet->sheet_name());
        else
            xlsx.selectSheet(xlsx.sheetNames().first());

        // Rename sheet
        xlsx.renameSheet(xlsx.sheetNames().last(), sheet->sheet_name());

        auto data = sheet->get_data();
        for (int r = 0; r < data.size(); ++r) {
            for (int c = 0; c < data[r].size(); ++c) {
                const QString& val = data[r][c];
                if (val.isEmpty())
                    continue;

                // Write formulas as formulas, numbers as numbers
                bool ok = false;
                double d = val.toDouble(&ok);
                if (ok) {
                    xlsx.write(r + 1, c + 1, d);
                } else if (val.startsWith('=')) {
                    xlsx.write(r + 1, c + 1, val); // QXlsx handles formulas
                } else {
                    xlsx.write(r + 1, c + 1, val);
                }
            }
        }
    }

    if (xlsx.saveAs(path)) {
        file_name_ = QFileInfo(path).fileName();
        file_path_ = path;
        auto* fname = findChild<QLabel*>("excelFileName");
        if (fname)
            fname->setText(file_name_);
        LOG_INFO("ExcelScreen", QString("Exported to %1").arg(path));

        // Register with File Manager so it appears in the Files tab
        services::FileManagerService::instance().import_file(path, "excel");
    } else {
        LOG_ERROR("ExcelScreen", "Failed to save XLSX file");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Export CSV (active sheet)
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::on_export_csv() {
    auto* sheet = current_sheet();
    if (!sheet)
        return;

    QString path = QFileDialog::getSaveFileName(this, "Export CSV", sheet->sheet_name() + ".csv", "CSV Files (*.csv)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    auto data = sheet->get_data();

    // Find the last row/col with data to avoid huge trailing empty rows
    int last_row = 0;
    int last_col = 0;
    for (int r = 0; r < data.size(); ++r) {
        for (int c = 0; c < data[r].size(); ++c) {
            if (!data[r][c].isEmpty()) {
                last_row = std::max(last_row, r);
                last_col = std::max(last_col, c);
            }
        }
    }

    for (int r = 0; r <= last_row; ++r) {
        QStringList row;
        for (int c = 0; c <= last_col; ++c) {
            QString val = (c < data[r].size()) ? data[r][c] : "";
            if (val.contains(',') || val.contains('"') || val.contains('\n'))
                val = "\"" + val.replace("\"", "\"\"") + "\"";
            row << val;
        }
        out << row.join(",") << "\n";
    }

    LOG_INFO("ExcelScreen", QString("Exported CSV to %1").arg(path));

    // Register with File Manager so it appears in the Files tab
    services::FileManagerService::instance().import_file(path, "excel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Sheet management
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::on_add_sheet() {
    QString name = generate_sheet_name();
    auto* sheet = new SpreadsheetWidget(name, 100, 26, sheet_tabs_);
    sheet_tabs_->addTab(sheet, name);
    sheet_tabs_->setCurrentIndex(sheet_tabs_->count() - 1);
    update_status();
}

void ExcelScreen::on_delete_sheet() {
    if (sheet_tabs_->count() <= 1) {
        QMessageBox::warning(this, "Cannot Delete", "Cannot delete the last sheet.");
        return;
    }

    int idx = sheet_tabs_->currentIndex();
    QString name = sheet_tabs_->tabText(idx);

    auto reply = QMessageBox::question(this, "Delete Sheet", QString("Delete \"%1\"? This cannot be undone.").arg(name),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        auto* w = sheet_tabs_->widget(idx);
        sheet_tabs_->removeTab(idx);
        w->deleteLater();
        update_status();
    }
}

void ExcelScreen::on_rename_sheet() {
    int idx = sheet_tabs_->currentIndex();
    if (idx < 0)
        return;

    bool ok = false;
    QString current = sheet_tabs_->tabText(idx);
    QString name = QInputDialog::getText(this, "Rename Sheet", "New name:", QLineEdit::Normal, current, &ok);

    if (ok && !name.trimmed().isEmpty()) {
        sheet_tabs_->setTabText(idx, name.trimmed());
        auto* sheet = qobject_cast<SpreadsheetWidget*>(sheet_tabs_->widget(idx));
        if (sheet)
            sheet->set_sheet_name(name.trimmed());
        update_status();
    }
}

void ExcelScreen::on_tab_changed(int index) {
    Q_UNUSED(index);
    update_status();
    ScreenStateManager::instance().notify_changed(this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

SpreadsheetWidget* ExcelScreen::current_sheet() const {
    return qobject_cast<SpreadsheetWidget*>(sheet_tabs_->currentWidget());
}

QString ExcelScreen::generate_sheet_name() const {
    int n = sheet_tabs_->count() + 1;
    QString name;
    do {
        name = QString("Sheet%1").arg(n++);
    } while (sheet_tabs_->indexOf(sheet_tabs_->findChild<QWidget*>(name)) >= 0 || n > 999);
    return name;
}

void ExcelScreen::update_status() {
    auto* sheet = current_sheet();
    QString info = QString("File: %1  |  Sheets: %2").arg(file_name_).arg(sheet_tabs_->count());
    if (sheet) {
        info += QString("  |  Active: %1  |  %2 rows x %3 cols")
                    .arg(sheet->sheet_name())
                    .arg(sheet->row_count())
                    .arg(sheet->col_count());
    }
    if (status_label_)
        status_label_->setText(info);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap ExcelScreen::save_state() const {
    return {{"tab_index", sheet_tabs_ ? sheet_tabs_->currentIndex() : 0}};
}

void ExcelScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("tab_index", 0).toInt();
    if (sheet_tabs_ && idx < sheet_tabs_->count())
        sheet_tabs_->setCurrentIndex(idx);
}

} // namespace fincept::screens
