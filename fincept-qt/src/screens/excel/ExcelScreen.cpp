// src/screens/excel/ExcelScreen.cpp
#include "screens/excel/ExcelScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/excel/SpreadsheetWidget.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QTabBar>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

#ifdef FINCEPT_HAS_QXLSX
#    include <xlsxdocument.h>
#endif

namespace fincept::screens {

using namespace fincept::ui;

static QString kAccent() {
    return QString("#ea580c");
} // Orange accent

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
    setStyleSheet(QString("QWidget { background:%1; color:%2; }").arg(colors::BG_BASE(), colors::TEXT_PRIMARY()));

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
                                   .arg(fonts::DATA_FAMILY, kAccent(), colors::BG_HOVER(), colors::BORDER_MED(),
                                        colors::TEXT_DIM(), colors::TEXT_SECONDARY(), colors::TEXT_TERTIARY(),
                                        colors::TEXT_PRIMARY()));

    // Add initial sheet
    auto* sheet1 = new SpreadsheetWidget("Sheet1", 100, 26, sheet_tabs_);
    sheet_tabs_->addTab(sheet1, "Sheet1");
    watch_sheet(sheet1);

    connect(sheet_tabs_, &QTabWidget::currentChanged, this, &ExcelScreen::on_tab_changed);

    // Ctrl+S saves back to the current file path (or prompts on first save) —
    // previously the only way to persist work was the EXPORT button, which
    // always re-asked for a filename.
    auto* save_sc = new QShortcut(QKeySequence::Save, this);
    save_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(save_sc, &QShortcut::activated, this, &ExcelScreen::on_export);

    root->addWidget(sheet_tabs_, 1);

    // Status bar
    auto* status_bar = new QWidget(this);
    status_bar->setFixedHeight(24);
    status_bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(colors::BG_HOVER(), colors::BORDER_MED()));

    auto* status_hl = new QHBoxLayout(status_bar);
    status_hl->setContentsMargins(12, 0, 12, 0);
    status_hl->setSpacing(12);

    status_label_ = new QLabel(this);
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY));
    status_hl->addWidget(status_label_);
    status_hl->addStretch();

    root->addWidget(status_bar);

    update_status();
}

QWidget* ExcelScreen::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(40);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(colors::BORDER_MED(), colors::TEXT_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(4);

    // Title
    toolbar_title_ = new QLabel(tr("EXCEL SPREADSHEET"), bar);
    toolbar_title_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:11px; font-weight:700; margin-right:12px;")
            .arg(kAccent(), fonts::DATA_FAMILY));
    hl->addWidget(toolbar_title_);

    // Button factory
    auto make_btn = [&](const QString& text, const QString& tooltip = {}) -> QPushButton* {
        auto* btn = new QPushButton(text, bar);
        btn->setToolTip(tooltip);
        btn->setStyleSheet(QString("QPushButton { background:%3; color:%4; border:none;"
                                   " font-family:%1; font-size:10px; font-weight:600; padding:6px 12px; }"
                                   "QPushButton:hover { background:%5; }"
                                   "QPushButton:pressed { background:%2; }")
                               .arg(fonts::DATA_FAMILY, kAccent(), colors::TEXT_DIM(), colors::TEXT_PRIMARY(),
                                    colors::TEXT_TERTIARY()));
        return btn;
    };

    import_btn_ = make_btn(tr("IMPORT"), tr("Import XLSX/CSV file"));
    connect(import_btn_, &QPushButton::clicked, this, &ExcelScreen::on_import);
    hl->addWidget(import_btn_);

    export_btn_ = make_btn(tr("EXPORT"), tr("Export as XLSX"));
    connect(export_btn_, &QPushButton::clicked, this, &ExcelScreen::on_export);
    hl->addWidget(export_btn_);

    export_csv_btn_ = make_btn(tr("CSV"), tr("Export active sheet as CSV"));
    connect(export_csv_btn_, &QPushButton::clicked, this, &ExcelScreen::on_export_csv);
    hl->addWidget(export_csv_btn_);

    // Separator
    auto* sep1 = new QWidget(bar);
    sep1->setFixedSize(1, 20);
    sep1->setStyleSheet(QString("background:%1;").arg(colors::TEXT_TERTIARY()));
    hl->addWidget(sep1);

    add_sheet_btn_ = make_btn(tr("+ SHEET"), tr("Add new sheet"));
    connect(add_sheet_btn_, &QPushButton::clicked, this, &ExcelScreen::on_add_sheet);
    hl->addWidget(add_sheet_btn_);

    rename_btn_ = make_btn(tr("RENAME"), tr("Rename current sheet"));
    connect(rename_btn_, &QPushButton::clicked, this, &ExcelScreen::on_rename_sheet);
    hl->addWidget(rename_btn_);

    delete_btn_ = make_btn(tr("DELETE"), tr("Delete current sheet"));
    delete_btn_->setStyleSheet(
        QString("QPushButton { background:%2; color:%3; border:none;"
                " font-family:%1; font-size:10px; font-weight:600; padding:6px 12px; }"
                "QPushButton:hover { background:%4; }")
            .arg(fonts::DATA_FAMILY, colors::TEXT_DIM(), colors::TEXT_PRIMARY(), colors::NEGATIVE()));
    connect(delete_btn_, &QPushButton::clicked, this, &ExcelScreen::on_delete_sheet);
    hl->addWidget(delete_btn_);

    hl->addStretch();

    // File name label
    auto* fname_label = new QLabel(file_name_, bar);
    fname_label->setObjectName("excelFileName");
    fname_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px;").arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY));
    hl->addWidget(fname_label);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Import (XLSX via QXlsx)
// ─────────────────────────────────────────────────────────────────────────────

// Split one CSV line honouring RFC-4180 quoting (""-escaped quotes inside a
// quoted field). Kept local — the watchlist screen has its own copy but lives
// in a different slice.
static QStringList split_csv_line(const QString& line) {
    QStringList fields;
    QString cur;
    bool in_quotes = false;
    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            if (in_quotes && i + 1 < line.length() && line[i + 1] == '"') {
                cur += '"';
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == ',' && !in_quotes) {
            fields.append(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.append(cur);
    return fields;
}

bool ExcelScreen::import_csv(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import failed"), tr("Could not open file for reading:\n%1").arg(path));
        return false;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    QVector<QVector<QString>> cells;
    // Hard cap so a pathological file can't allocate millions of table items
    // and freeze the UI thread.
    constexpr int kMaxRows = 20000;
    while (!in.atEnd() && cells.size() < kMaxRows) {
        const QStringList fields = split_csv_line(in.readLine());
        QVector<QString> row;
        row.reserve(fields.size());
        for (const auto& v : fields)
            row.append(v);
        cells.append(row);
    }
    const bool truncated = !in.atEnd();

    const QString name = QFileInfo(path).completeBaseName();
    auto* sheet = new SpreadsheetWidget(name, std::max<int>(100, static_cast<int>(cells.size())), 26, sheet_tabs_);
    sheet->set_data(cells);

    // Replace the current sheet set with the imported file (matches the XLSX path).
    while (sheet_tabs_->count() > 0) {
        auto* w = sheet_tabs_->widget(0);
        sheet_tabs_->removeTab(0);
        w->deleteLater();
    }
    sheet_tabs_->addTab(sheet, name);
    connect(sheet, &SpreadsheetWidget::data_changed, this, &ExcelScreen::mark_dirty);

    file_name_ = QFileInfo(path).fileName();
    file_path_ = path;
    if (auto* fname = findChild<QLabel*>("excelFileName"))
        fname->setText(file_name_);
    dirty_ = false;
    update_status();
    LOG_INFO("ExcelScreen", QString("Imported %1 CSV rows from %2").arg(cells.size()).arg(file_name_));
    services::FileManagerService::instance().import_file(path, "excel");

    if (truncated)
        QMessageBox::information(this, tr("Import truncated"),
                                 tr("Only the first %1 rows were imported.").arg(kMaxRows));
    return true;
}

void ExcelScreen::on_import() {
    if (!confirm_discard(tr("Import a different file")))
        return;

    QString path = QFileDialog::getOpenFileName(this, tr("Import Spreadsheet"), {},
                                                tr("Spreadsheet Files (*.xlsx *.xls *.csv);;All Files (*)"));
    if (path.isEmpty())
        return;

    // CSV is advertised in the file filter (and in the no-QXlsx message) but was
    // never actually handled — QXlsx::Document on a .csv just reported "no
    // sheets" to the log and the screen silently did nothing.
    if (QFileInfo(path).suffix().compare(QLatin1String("csv"), Qt::CaseInsensitive) == 0) {
        import_csv(path);
        return;
    }

#ifdef FINCEPT_HAS_QXLSX
    QXlsx::Document xlsx(path);
    QStringList sheet_names = xlsx.sheetNames();
    if (sheet_names.isEmpty()) {
        LOG_ERROR("ExcelScreen", "No sheets found in file");
        QMessageBox::warning(this, tr("Import failed"),
                             tr("No sheets could be read from:\n%1\n\nThe file may be corrupt or "
                                "in an unsupported format.")
                                 .arg(path));
        return;
    }

    // Remove existing tabs
    while (sheet_tabs_->count() > 0) {
        auto* w = sheet_tabs_->widget(0);
        sheet_tabs_->removeTab(0);
        w->deleteLater();
    }

    // Every cell becomes a heap-allocated SpreadsheetItem, so an unbounded
    // dimension (a stray value in row 1,000,000) would allocate tens of millions
    // of items and hang the UI. Cap the import window.
    constexpr int kMaxImportRows = 20000;
    constexpr int kMaxImportCols = 512;
    bool truncated = false;

    for (const auto& name : sheet_names) {
        xlsx.selectSheet(name);

        // Determine dimensions
        auto dim = xlsx.dimension();
        int max_row = std::max(dim.lastRow(), 100);
        int max_col = std::max(dim.lastColumn(), 26);
        if (max_row > kMaxImportRows) {
            max_row = kMaxImportRows;
            truncated = true;
        }
        if (max_col > kMaxImportCols) {
            max_col = kMaxImportCols;
            truncated = true;
        }

        auto* sheet = new SpreadsheetWidget(name, max_row, max_col, sheet_tabs_);

        // Load data
        QVector<QVector<QString>> cells(max_row);
        for (int r = 0; r < max_row; ++r) {
            cells[r].resize(max_col);
            for (int c = 0; c < max_col; ++c) {
                auto cell = xlsx.read(r + 1, c + 1); // QXlsx is 1-based
                cells[r][c] = cell.isValid() ? cell.toString() : "";
            }
        }
        sheet->set_data(cells);
        sheet_tabs_->addTab(sheet, name);
        connect(sheet, &SpreadsheetWidget::data_changed, this, &ExcelScreen::mark_dirty);
    }

    // Update file info
    file_name_ = QFileInfo(path).fileName();
    file_path_ = path;
    auto* fname = findChild<QLabel*>("excelFileName");
    if (fname)
        fname->setText(file_name_);

    dirty_ = false;
    update_status();
    LOG_INFO("ExcelScreen", QString("Imported %1 sheets from %2").arg(sheet_names.size()).arg(file_name_));

    // Register with File Manager so it appears in the Files tab
    services::FileManagerService::instance().import_file(path, "excel");

    if (truncated)
        QMessageBox::information(this, tr("Import truncated"),
                                 tr("This workbook is larger than the %1 × %2 import window; "
                                    "only that region was loaded.")
                                     .arg(kMaxImportRows)
                                     .arg(kMaxImportCols));
#else
    QMessageBox::information(this, tr("Excel Import"),
                             tr("Excel (.xlsx) import requires Qt6 private headers.\n"
                                "This build was compiled without QXlsx support.\n\n"
                                "CSV files can still be imported via the toolbar."));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Export (XLSX via QXlsx)
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::on_export() {
#ifdef FINCEPT_HAS_QXLSX
    QString path = QFileDialog::getSaveFileName(this, tr("Export as XLSX"), file_name_, tr("Excel Files (*.xlsx)"));
    if (path.isEmpty())
        return;
    // QFileDialog on Linux/macOS does not always append the filter suffix.
    if (!path.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive))
        path += QLatin1String(".xlsx");

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

        auto cells = sheet->get_data();
        for (int r = 0; r < cells.size(); ++r) {
            for (int c = 0; c < cells[r].size(); ++c) {
                const QString& val = cells[r][c];
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
        dirty_ = false;
        update_status();
        LOG_INFO("ExcelScreen", QString("Exported to %1").arg(path));

        // Register with File Manager so it appears in the Files tab
        services::FileManagerService::instance().import_file(path, "excel");
    } else {
        LOG_ERROR("ExcelScreen", "Failed to save XLSX file");
        // Silently logging a failed save is the worst possible outcome — the
        // user believes their workbook is on disk when it isn't.
        QMessageBox::warning(this, tr("Export failed"),
                             tr("Could not write the workbook to:\n%1\n\nCheck that the file is not "
                                "open in another application and that you have write permission.")
                                 .arg(path));
    }
#else
    QMessageBox::information(this, tr("Excel Export"),
                             tr("Excel (.xlsx) export requires Qt6 private headers.\n"
                                "This build was compiled without QXlsx support.\n\n"
                                "CSV export is still available via the toolbar."));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Export CSV (active sheet)
// ─────────────────────────────────────────────────────────────────────────────

void ExcelScreen::on_export_csv() {
    auto* sheet = current_sheet();
    if (!sheet)
        return;

    QString path =
        QFileDialog::getSaveFileName(this, tr("Export CSV"), sheet->sheet_name() + ".csv", tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export failed"), tr("Could not open file for writing:\n%1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    auto cells = sheet->get_data();

    // Find the last row/col with data to avoid huge trailing empty rows
    int last_row = 0;
    int last_col = 0;
    for (int r = 0; r < cells.size(); ++r) {
        for (int c = 0; c < cells[r].size(); ++c) {
            if (!cells[r][c].isEmpty()) {
                last_row = std::max(last_row, r);
                last_col = std::max(last_col, c);
            }
        }
    }

    for (int r = 0; r <= last_row; ++r) {
        QStringList row;
        for (int c = 0; c <= last_col; ++c) {
            QString val = (c < cells[r].size()) ? cells[r][c] : "";
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
    watch_sheet(sheet);
    sheet_tabs_->setCurrentIndex(sheet_tabs_->count() - 1);
    mark_dirty();
    update_status();
}

// ── Dirty tracking ───────────────────────────────────────────────────────────

void ExcelScreen::watch_sheet(SpreadsheetWidget* sheet) {
    if (sheet)
        connect(sheet, &SpreadsheetWidget::data_changed, this, &ExcelScreen::mark_dirty);
}

void ExcelScreen::mark_dirty() {
    if (dirty_)
        return;
    dirty_ = true;
    update_status();
}

bool ExcelScreen::confirm_discard(const QString& action) {
    if (!dirty_)
        return true;
    const auto reply =
        QMessageBox::warning(this, tr("Unsaved changes"),
                             tr("\"%1\" has unsaved changes.\n\n%2 will discard them.").arg(file_name_, action),
                             QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return reply == QMessageBox::Discard;
}

void ExcelScreen::on_delete_sheet() {
    if (sheet_tabs_->count() <= 1) {
        QMessageBox::warning(this, tr("Cannot Delete"), tr("Cannot delete the last sheet."));
        return;
    }

    int idx = sheet_tabs_->currentIndex();
    QString name = sheet_tabs_->tabText(idx);

    auto reply = QMessageBox::question(this, tr("Delete Sheet"), tr("Delete \"%1\"? This cannot be undone.").arg(name),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        auto* w = sheet_tabs_->widget(idx);
        sheet_tabs_->removeTab(idx);
        w->deleteLater();
        mark_dirty();
        update_status();
    }
}

void ExcelScreen::on_rename_sheet() {
    int idx = sheet_tabs_->currentIndex();
    if (idx < 0)
        return;

    bool ok = false;
    QString current = sheet_tabs_->tabText(idx);
    QString name = QInputDialog::getText(this, tr("Rename Sheet"), tr("New name:"), QLineEdit::Normal, current, &ok);

    if (!ok || name.trimmed().isEmpty())
        return;
    name = name.trimmed();

    // Sheet names must be unique — QXlsx::addSheet() fails on a duplicate, which
    // used to silently drop a sheet on export.
    for (int i = 0; i < sheet_tabs_->count(); ++i) {
        if (i != idx && sheet_tabs_->tabText(i).compare(name, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, tr("Rename Sheet"), tr("A sheet named \"%1\" already exists.").arg(name));
            return;
        }
    }

    sheet_tabs_->setTabText(idx, name);
    if (auto* sheet = qobject_cast<SpreadsheetWidget*>(sheet_tabs_->widget(idx)))
        sheet->set_sheet_name(name);
    mark_dirty();
    update_status();
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
    // Compare against the TAB TEXT. The old loop asked findChild<QWidget*>(name)
    // — sheets never have an objectName, so it always returned nullptr and the
    // uniqueness check was a no-op: deleting "Sheet2" from Sheet1/2/3 and adding
    // a sheet produced a duplicate "Sheet3".
    auto taken = [this](const QString& n) {
        for (int i = 0; i < sheet_tabs_->count(); ++i)
            if (sheet_tabs_->tabText(i).compare(n, Qt::CaseInsensitive) == 0)
                return true;
        return false;
    };
    for (int n = sheet_tabs_->count() + 1; n <= 999; ++n) {
        const QString name = QString("Sheet%1").arg(n);
        if (!taken(name))
            return name;
    }
    return QString("Sheet%1").arg(QDateTime::currentMSecsSinceEpoch());
}

void ExcelScreen::update_status() {
    auto* sheet = current_sheet();
    QString info = tr("File: %1%2  |  Sheets: %3")
                       .arg(file_name_, dirty_ ? QStringLiteral(" *") : QString())
                       .arg(sheet_tabs_->count());
    if (sheet) {
        info += tr("  |  Active: %1  |  %2 rows x %3 cols")
                    .arg(sheet->sheet_name())
                    .arg(sheet->row_count())
                    .arg(sheet->col_count());
    }
    if (status_label_)
        status_label_->setText(info);
}

// ── Live language switch ─────────────────────────────────────────────────────

void ExcelScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ExcelScreen::retranslateUi() {
    if (toolbar_title_)
        toolbar_title_->setText(tr("EXCEL SPREADSHEET"));
    if (import_btn_) {
        import_btn_->setText(tr("IMPORT"));
        import_btn_->setToolTip(tr("Import XLSX/CSV file"));
    }
    if (export_btn_) {
        export_btn_->setText(tr("EXPORT"));
        export_btn_->setToolTip(tr("Export as XLSX"));
    }
    if (export_csv_btn_) {
        export_csv_btn_->setText(tr("CSV"));
        export_csv_btn_->setToolTip(tr("Export active sheet as CSV"));
    }
    if (add_sheet_btn_) {
        add_sheet_btn_->setText(tr("+ SHEET"));
        add_sheet_btn_->setToolTip(tr("Add new sheet"));
    }
    if (rename_btn_) {
        rename_btn_->setText(tr("RENAME"));
        rename_btn_->setToolTip(tr("Rename current sheet"));
    }
    if (delete_btn_) {
        delete_btn_->setText(tr("DELETE"));
        delete_btn_->setToolTip(tr("Delete current sheet"));
    }
    update_status(); // re-render the status bar summary in the new language
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
