// src/screens/excel/ExcelScreen.cpp
#include "screens/excel/ExcelScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/excel/SpreadsheetWidget.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QColor>
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
#    include <xlsxformat.h>
#endif

#include <QPointer>
#include <QSet>

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

    // The import window bounds BOTH the xlsx.read() calls and the grid we build
    // from them, so it has to stay something a grid can plausibly show. The old
    // 20 000 × 512 window was 10.2M cells — a cap that itself caused the hang it
    // was meant to prevent. 64 columns is already past "BL" in Excel lettering;
    // anything wider is a data dump, not a spreadsheet.
    constexpr int kMaxImportRows = 20000;
    constexpr int kMaxImportCols = 64;
    bool truncated = false;

    for (const auto& name : sheet_names) {
        xlsx.selectSheet(name);

        // Read the sheet's USED range only. Padding out to a minimum 100 × 26
        // grid here meant every read loop did at least 2 600 xlsx.read() calls
        // for cells the file doesn't have; SpreadsheetWidget pads the visible
        // grid to that minimum itself, without touching the file.
        const auto dim = xlsx.dimension();
        int data_rows = std::clamp(dim.lastRow(), 0, kMaxImportRows);
        int data_cols = std::clamp(dim.lastColumn(), 0, kMaxImportCols);
        if (dim.lastRow() > kMaxImportRows || dim.lastColumn() > kMaxImportCols)
            truncated = true;

        auto* sheet =
            new SpreadsheetWidget(name, std::max(data_rows, 100), std::max(data_cols, 26), sheet_tabs_);

        // Load data
        QVector<QVector<QString>> cells(data_rows);
        for (int r = 0; r < data_rows; ++r) {
            cells[r].resize(data_cols);
            for (int c = 0; c < data_cols; ++c) {
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

// ── XLSX writing ────────────────────────────────────────────────────────────
// Data is snapshotted out of the widgets before any dialog opens (see
// on_export), so this works on plain values and touches no UI state.

struct ExcelSheetSnapshot {
    QString name;
    QVector<QVector<QString>> cells;
};

#ifdef FINCEPT_HAS_QXLSX

// Excel sheet-name rules: 1-31 chars, none of []:*?/\, and unique per workbook.
// A name that violates these makes addSheet/renameSheet fail, after which the
// writes land on whatever sheet happened to be current — silently exporting one
// sheet's data under another's name.
static QString excel_safe_sheet_name(QString name, QSet<QString>& taken) {
    static const QString kIllegal = QStringLiteral("[]:*?/\\");
    for (const QChar& ch : kIllegal)
        name.replace(ch, QLatin1Char('_'));
    name = name.trimmed().left(31);
    if (name.isEmpty())
        name = QStringLiteral("Sheet");

    QString candidate = name;
    int n = 2;
    while (taken.contains(candidate.toLower())) {
        const QString suffix = QStringLiteral("_") + QString::number(n++);
        candidate = name.left(31 - suffix.size()) + suffix;
    }
    taken.insert(candidate.toLower());
    return candidate;
}

// Last row/col holding anything. get_data() pads every sheet to at least
// 100x26, so without trimming a 12-row model exports as a 100-row sheet with
// column widths and header styling stretched across empty space.
static void excel_used_extent(const QVector<QVector<QString>>& cells, int& last_row, int& last_col) {
    last_row = -1;
    last_col = -1;
    for (int r = 0; r < cells.size(); ++r) {
        for (int c = 0; c < cells[r].size(); ++c) {
            if (!cells[r][c].trimmed().isEmpty()) {
                last_row = std::max(last_row, r);
                last_col = std::max(last_col, c);
            }
        }
    }
}

static bool write_snapshots_to_xlsx(QXlsx::Document& xlsx, const QVector<ExcelSheetSnapshot>& snapshots) {
    using QXlsx::Format;

    // Header: white on the terminal's accent, bold, centred.
    Format header_fmt;
    header_fmt.setFontBold(true);
    header_fmt.setFontColor(QColor(Qt::white));
    header_fmt.setPatternBackgroundColor(QColor(kAccent()));
    header_fmt.setHorizontalAlignment(Format::AlignHCenter);
    header_fmt.setBorderStyle(Format::BorderThin);

    // Row label / section heading: bold, left.
    Format label_fmt;
    label_fmt.setFontBold(true);

    // Numbers: thousands separator, 2dp, negatives in parentheses — the
    // convention finance reads by default.
    Format num_fmt;
    num_fmt.setNumberFormat(QStringLiteral("#,##0.00;(#,##0.00)"));
    num_fmt.setHorizontalAlignment(Format::AlignRight);

    // Rates are stored as decimals (0.105), so a percent format is what makes
    // them readable — "10.5%" rather than "0.11".
    Format pct_fmt;
    pct_fmt.setNumberFormat(QStringLiteral("0.0%"));
    pct_fmt.setHorizontalAlignment(Format::AlignRight);

    Format price_fmt;
    price_fmt.setNumberFormat(QStringLiteral("$#,##0.00"));
    price_fmt.setHorizontalAlignment(Format::AlignRight);

    // Counts (shares, years, periods) — no decimals, no currency.
    Format count_fmt;
    count_fmt.setNumberFormat(QStringLiteral("#,##0.##"));
    count_fmt.setHorizontalAlignment(Format::AlignRight);

    QSet<QString> taken_names;
    bool wrote_any = false;

    for (int s = 0; s < snapshots.size(); ++s) {
        const ExcelSheetSnapshot& snap = snapshots[s];
        const QString name = excel_safe_sheet_name(snap.name, taken_names);

        // First sheet: rename the default one. Later sheets: add. Both are
        // checked — a silent failure here is what sends writes to the wrong
        // sheet. sheetNames() is never assumed non-empty.
        if (s == 0) {
            const QStringList names = xlsx.sheetNames();
            if (names.isEmpty()) {
                if (!xlsx.addSheet(name))
                    continue;
            } else {
                xlsx.selectSheet(names.first());
                if (names.first() != name)
                    xlsx.renameSheet(names.first(), name);
            }
        } else if (!xlsx.addSheet(name)) {
            LOG_WARN("ExcelScreen", "Export: could not add sheet " + name + " — skipped");
            continue;
        }

        int last_row = -1;
        int last_col = -1;
        excel_used_extent(snap.cells, last_row, last_col);
        if (last_row < 0) {
            wrote_any = true; // an empty sheet is still a valid sheet
            continue;
        }

        // Header row = first row with content, if it is text rather than
        // numbers (a numeric first row is data, not a header).
        int header_row = -1;
        for (int r = 0; r <= last_row && header_row < 0; ++r) {
            for (int c = 0; c <= last_col; ++c) {
                const QString v = (c < snap.cells[r].size()) ? snap.cells[r][c].trimmed() : QString();
                if (v.isEmpty())
                    continue;
                bool numeric = false;
                v.toDouble(&numeric);
                header_row = (numeric || v.startsWith(QLatin1Char('='))) ? -2 : r;
                break;
            }
        }

        QVector<int> col_width(last_col + 1, 10);

        for (int r = 0; r <= last_row; ++r) {
            // Row-level number format, chosen from the row's label in column A.
            // A single blanket "#,##0.00" is wrong for half a financial model:
            // a WACC of 0.105 rendered as "0.11" and a 3% terminal growth as
            // "0.03", losing both precision and meaning. The label is the only
            // signal available for what a row actually holds.
            const QString label = (!snap.cells[r].isEmpty()) ? snap.cells[r][0].trimmed().toLower() : QString();
            const Format* row_fmt = &num_fmt;
            if (label.contains(QLatin1String("rate")) || label.contains(QLatin1String("growth")) ||
                label.contains(QLatin1String("margin")) || label.contains(QLatin1String("wacc")) ||
                label.contains(QLatin1String("yield")) || label.contains(QLatin1String("upside")) ||
                label.contains(QLatin1String("downside")) || label.startsWith(QLatin1Char('%')) ||
                label.contains(QLatin1String("% of"))) {
                row_fmt = &pct_fmt;
            } else if (label.contains(QLatin1String("price")) || label.contains(QLatin1String("per share"))) {
                row_fmt = &price_fmt;
            } else if (label.contains(QLatin1String("shares")) || label.contains(QLatin1String("years")) ||
                       label.contains(QLatin1String("period"))) {
                row_fmt = &count_fmt;
            }

            for (int c = 0; c <= last_col; ++c) {
                const QString val = (c < snap.cells[r].size()) ? snap.cells[r][c] : QString();
                if (val.trimmed().isEmpty())
                    continue;

                // Width from what the cell DISPLAYS, not from its source. A
                // formula's text is far longer than its result, so sizing on it
                // pushed every column to the 52-char cap on a model whose widest
                // visible value was six digits.
                const int display_len =
                    val.startsWith(QLatin1Char('=')) ? 12 : static_cast<int>(val.size()) + 2;
                col_width[c] = std::max(col_width[c], display_len);

                if (r == header_row) {
                    xlsx.write(r + 1, c + 1, val, header_fmt);
                    continue;
                }

                // Formula first — "=1+2" also parses as text, and a formula
                // written as a string exports as a literal instead of a live cell.
                if (val.startsWith(QLatin1Char('='))) {
                    xlsx.write(r + 1, c + 1, val, *row_fmt);
                    continue;
                }

                // Percentages authored as text ("12.5%") become real percentages.
                if (val.endsWith(QLatin1Char('%'))) {
                    bool pct_ok = false;
                    const double pct = val.left(val.size() - 1).toDouble(&pct_ok);
                    if (pct_ok) {
                        xlsx.write(r + 1, c + 1, pct / 100.0, pct_fmt);
                        continue;
                    }
                }

                bool ok = false;
                const double d = val.toDouble(&ok);
                // qIsFinite: "nan"/"inf" parse as doubles and would write XML
                // Excel refuses to open.
                if (ok && qIsFinite(d)) {
                    xlsx.write(r + 1, c + 1, d, *row_fmt);
                } else if (c == 0) {
                    xlsx.write(r + 1, c + 1, val, label_fmt); // first column reads as labels
                } else {
                    xlsx.write(r + 1, c + 1, val);
                }
            }
        }

        for (int c = 0; c <= last_col; ++c)
            xlsx.setColumnWidth(c + 1, std::min(col_width[c], 52));
        wrote_any = true;
    }

    return wrote_any;
}

#endif // FINCEPT_HAS_QXLSX

void ExcelScreen::on_export() {
#ifdef FINCEPT_HAS_QXLSX
    // ── Snapshot BEFORE the file dialog ───────────────────────────────────
    // getSaveFileName runs a nested event loop. Anything queued on the UI
    // thread runs inside it — including MCP tool bodies, which can call
    // navigate() and tear down or rebuild the Excel dock. The old code read
    // sheet_tabs_ and every SpreadsheetWidget* AFTER the dialog returned, so a
    // dock rebuild mid-dialog left it walking freed widgets: the observed crash
    // was a read of address 0x8 (a null QObject's d_ptr) inside Qt6Core during
    // export. Copying the data out first means the dialog can't invalidate
    // anything we still need.
    QVector<ExcelSheetSnapshot> snapshots;
    if (sheet_tabs_) {
        for (int i = 0; i < sheet_tabs_->count(); ++i) {
            auto* sheet = qobject_cast<SpreadsheetWidget*>(sheet_tabs_->widget(i));
            if (!sheet)
                continue;
            snapshots.append({sheet->sheet_name(), sheet->get_data()});
        }
    }
    if (snapshots.isEmpty()) {
        QMessageBox::warning(this, tr("Export failed"), tr("There is nothing to export."));
        return;
    }

    // `this` can still be destroyed while the modal dialog is open; every
    // member touched after it returns is guarded on the QPointer.
    QPointer<ExcelScreen> self = this;
    QString path = QFileDialog::getSaveFileName(this, tr("Export as XLSX"), file_name_, tr("Excel Files (*.xlsx)"));
    if (!self || path.isEmpty())
        return;
    // QFileDialog on Linux/macOS does not always append the filter suffix.
    if (!path.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive))
        path += QLatin1String(".xlsx");

    QXlsx::Document xlsx;
    if (!write_snapshots_to_xlsx(xlsx, snapshots)) {
        QMessageBox::warning(this, tr("Export failed"), tr("Could not build the workbook contents."));
        return;
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

    // Snapshot before the dialog — same re-entrancy hazard as on_export: the
    // modal loop can run queued work that rebuilds the Excel dock and frees
    // `sheet`. Reading it afterwards is a use-after-free.
    const QString sheet_name = sheet->sheet_name();
    const QVector<QVector<QString>> cells = sheet->get_data();

    QPointer<ExcelScreen> self = this;
    QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), sheet_name + ".csv", tr("CSV Files (*.csv)"));
    if (!self || path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive))
        path += QLatin1String(".csv");

    // Find the last row/col with data to avoid huge trailing empty rows
    int last_row = -1;
    int last_col = -1;
    for (int r = 0; r < cells.size(); ++r) {
        for (int c = 0; c < cells[r].size(); ++c) {
            if (!cells[r][c].trimmed().isEmpty()) {
                last_row = std::max(last_row, r);
                last_col = std::max(last_col, c);
            }
        }
    }

    // Scoped so the stream flushes and the file closes BEFORE File Manager
    // copies it. Previously both were destroyed at end of function — i.e. after
    // import_file — so the Files tab could receive a truncated copy.
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Export failed"), tr("Could not open file for writing:\n%1").arg(path));
            return;
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        for (int r = 0; r <= last_row; ++r) {
            QStringList row;
            for (int c = 0; c <= last_col; ++c) {
                QString val = (r < cells.size() && c < cells[r].size()) ? cells[r][c] : QString();
                if (val.contains(',') || val.contains('"') || val.contains('\n'))
                    val = "\"" + val.replace("\"", "\"\"") + "\"";
                row << val;
            }
            out << row.join(",") << "\n";
        }
        out.flush();
        file.close();
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
