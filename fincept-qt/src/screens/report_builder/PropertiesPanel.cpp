#include "screens/report_builder/PropertiesPanel.h"

#include "screens/report_builder/ReportBuilderScreen.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextEdit>

namespace fincept::screens {

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    // Use min/max width pair (not setFixedWidth) so the parent splitter can
    // drive resize and the collapse animation can drive maximumWidth → 0.
    setMinimumWidth(0);
    setMaximumWidth(280);
    setStyleSheet(QString("background: %1; border-left: 1px solid %2;").arg(ui::colors::PANEL(), ui::colors::BORDER()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    stack_ = new QStackedWidget;
    build_empty_page();
    build_editor_page();
    stack_->setCurrentWidget(empty_widget_);

    vl->addWidget(stack_);
}

void PropertiesPanel::build_empty_page() {
    empty_widget_ = new QWidget(this);
    auto* vl = new QVBoxLayout(empty_widget_);
    vl->setAlignment(Qt::AlignCenter);

    auto* lbl = new QLabel("Select a component\nto edit properties");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(ui::colors::MUTED()));
    vl->addWidget(lbl);

    stack_->addWidget(empty_widget_);
}

void PropertiesPanel::build_editor_page() {
    // Wrap in a scroll area so tall editors don't get clipped
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("background: transparent;");

    editor_widget_ = new QWidget(this);
    editor_widget_->setStyleSheet("background: transparent;");
    editor_layout_ = new QVBoxLayout(editor_widget_);
    editor_layout_->setContentsMargins(8, 8, 8, 8);
    editor_layout_->setSpacing(6);
    editor_layout_->addStretch();

    scroll->setWidget(editor_widget_);
    stack_->addWidget(scroll);
}

void PropertiesPanel::show_empty() {
    current_index_ = -1;
    stack_->setCurrentIndex(0);
}

// Helper to add a styled label
static QLabel* make_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::GRAY()));
    return lbl;
}

void PropertiesPanel::show_properties(const ReportComponent* component, int index) {
    if (!component) {
        show_empty();
        return;
    }

    current_index_ = index;

    // Clear previous editor content (keep the stretch at end)
    while (editor_layout_->count() > 0) {
        auto* item = editor_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    // Title
    auto* title = new QLabel(QString("— %1 —").arg(component->type.toUpper()));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: %2; "
                                 "padding: 6px; border-bottom: 1px solid %3;")
                             .arg(ui::colors::AMBER(), ui::colors::BG_RAISED(), ui::colors::BORDER()));
    editor_layout_->addWidget(title);

    // ── Text-based types ──────────────────────────────────────────────────────
    if (component->type == "heading" || component->type == "text" || component->type == "code" ||
        component->type == "quote") {
        editor_layout_->addWidget(make_label("Content:"));
        auto* edit = new QTextEdit;
        edit->setPlainText(component->content);
        edit->setMinimumHeight(80);
        edit->setMaximumHeight(220);
        connect(edit, &QTextEdit::textChanged, this,
                [this, edit]() { emit content_changed(current_index_, edit->toPlainText()); });
        editor_layout_->addWidget(edit);
    }

    // ── Table: full cell editor ───────────────────────────────────────────────
    if (component->type == "table") {
        int rows = component->config.value("rows", "3").toInt();
        int cols = component->config.value("cols", "3").toInt();

        // Row/col spinboxes
        auto* dim_row = new QWidget(this);
        dim_row->setStyleSheet("background: transparent;");
        auto* drl = new QHBoxLayout(dim_row);
        drl->setContentsMargins(0, 0, 0, 0);
        drl->setSpacing(8);

        drl->addWidget(make_label("Rows:"));
        auto* rows_spin = new QSpinBox;
        rows_spin->setRange(1, 50);
        rows_spin->setValue(rows);
        drl->addWidget(rows_spin);

        drl->addWidget(make_label("Cols:"));
        auto* cols_spin = new QSpinBox;
        cols_spin->setRange(1, 20);
        cols_spin->setValue(cols);
        drl->addWidget(cols_spin);
        editor_layout_->addWidget(dim_row);

        // Cell editor table
        editor_layout_->addWidget(make_label("Cell data:"));
        auto* cell_table = new QTableWidget(rows, cols);
        cell_table->setMinimumHeight(150);
        cell_table->setMaximumHeight(250);
        cell_table->horizontalHeader()->setVisible(false);
        cell_table->verticalHeader()->setVisible(false);
        cell_table->setStyleSheet(
            QString("QTableWidget { background: %1; color: %2; border: 1px solid %3; font-size: 11px; }"
                    "QTableWidget::item { padding: 3px; } "
                    "QTableWidget::item:selected { background: %4; }")
                .arg(ui::colors::DARK(), ui::colors::GRAY(), ui::colors::BORDER(), ui::colors::BG_RAISED()));

        // Populate from saved JSON data
        QJsonObject cell_data;
        if (component->config.contains("data")) {
            QJsonDocument doc = QJsonDocument::fromJson(component->config.value("data").toUtf8());
            if (doc.isObject())
                cell_data = doc.object();
        }

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                QString key = QString("%1_%2").arg(r).arg(c);
                QString val = cell_data.contains(key) ? cell_data.value(key).toString()
                                                      : (r == 0 ? QString("Header %1").arg(c + 1) : "");
                cell_table->setItem(r, c, new QTableWidgetItem(val));
            }
        }
        editor_layout_->addWidget(cell_table);

        // Save cell data when any cell changes
        auto save_cells = [this, cell_table]() {
            QJsonObject obj;
            for (int r = 0; r < cell_table->rowCount(); ++r) {
                for (int c = 0; c < cell_table->columnCount(); ++c) {
                    auto* it = cell_table->item(r, c);
                    if (it && !it->text().isEmpty()) {
                        obj[QString("%1_%2").arg(r).arg(c)] = it->text();
                    }
                }
            }
            QString json = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
            emit config_changed(current_index_, "data", json);
        };
        connect(cell_table, &QTableWidget::itemChanged, this, save_cells);

        // Resize table when dims change
        connect(rows_spin, &QSpinBox::valueChanged, this, [this, cell_table, save_cells](int val) {
            emit config_changed(current_index_, "rows", QString::number(val));
            cell_table->setRowCount(val);
            save_cells();
        });
        connect(cols_spin, &QSpinBox::valueChanged, this, [this, cell_table, save_cells](int val) {
            emit config_changed(current_index_, "cols", QString::number(val));
            cell_table->setColumnCount(val);
            save_cells();
        });
    }

    // ── Chart: type + title + data + labels + width ───────────────────────────
    if (component->type == "chart") {
        editor_layout_->addWidget(make_label("Chart Type:"));
        auto* type_combo = new QComboBox;
        type_combo->addItems({"line", "bar", "pie"});
        int idx = type_combo->findText(component->config.value("chart_type", "line"));
        if (idx >= 0)
            type_combo->setCurrentIndex(idx);
        connect(type_combo, &QComboBox::currentTextChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "chart_type", val); });
        editor_layout_->addWidget(type_combo);

        editor_layout_->addWidget(make_label("Title:"));
        auto* title_edit = new QLineEdit(component->config.value("title", "Chart"));
        connect(title_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "title", val); });
        editor_layout_->addWidget(title_edit);

        editor_layout_->addWidget(make_label("Data (CSV values, e.g. 10,20,15,30):"));
        auto* data_edit = new QTextEdit;
        data_edit->setPlainText(component->config.value("data", ""));
        data_edit->setMaximumHeight(80);
        data_edit->setPlaceholderText("10,25,18,40,32,55");
        connect(data_edit, &QTextEdit::textChanged, this,
                [this, data_edit]() { emit config_changed(current_index_, "data", data_edit->toPlainText()); });
        editor_layout_->addWidget(data_edit);

        editor_layout_->addWidget(make_label("X Labels (comma-separated):"));
        auto* labels_edit = new QLineEdit(component->config.value("labels", ""));
        labels_edit->setPlaceholderText("Jan,Feb,Mar,Apr,May,Jun");
        connect(labels_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "labels", val); });
        editor_layout_->addWidget(labels_edit);

        editor_layout_->addWidget(make_label("Width (px):"));
        auto* w_spin = new QSpinBox;
        w_spin->setRange(200, 760);
        w_spin->setSingleStep(20);
        w_spin->setValue(component->config.value("width", "640").toInt());
        connect(w_spin, &QSpinBox::valueChanged, this,
                [this](int v) { emit config_changed(current_index_, "width", QString::number(v)); });
        editor_layout_->addWidget(w_spin);

        // Auto-fetch price history from ticker
        auto* sep_hist = new QFrame;
        sep_hist->setFixedHeight(1);
        sep_hist->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
        editor_layout_->addWidget(sep_hist);

        editor_layout_->addWidget(make_label("Fetch Price History:"));
        auto* hist_row = new QWidget(this);
        hist_row->setStyleSheet("background: transparent;");
        auto* hr_hl = new QHBoxLayout(hist_row);
        hr_hl->setContentsMargins(0, 0, 0, 0);
        hr_hl->setSpacing(4);

        auto* hist_ticker = new QLineEdit;
        hist_ticker->setPlaceholderText("e.g. AAPL");
        hr_hl->addWidget(hist_ticker, 1);

        auto* hist_btn = new QPushButton("Fetch");
        hist_btn->setFixedWidth(52);
        hist_btn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2; font-weight: bold; border: 1px solid %3; }"
                    "QPushButton:hover { background: %4; }")
                .arg(ui::colors::AMBER_DIM(), ui::colors::WHITE(), ui::colors::AMBER(), ui::colors::AMBER()));
        hr_hl->addWidget(hist_btn);
        editor_layout_->addWidget(hist_row);

        connect(hist_btn, &QPushButton::clicked, this, [this, hist_ticker]() {
            QString sym = hist_ticker->text().trimmed().toUpper();
            if (!sym.isEmpty())
                emit config_changed(current_index_, "fetch_history", sym);
        });

        auto* hint = new QLabel("Tip: re-select component after\nediting data to re-render.");
        hint->setWordWrap(true);
        hint->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::MUTED()));
        editor_layout_->addWidget(hint);
    }

    // ── Stats Block: title + key:value pairs + auto-fetch ────────────────────
    if (component->type == "stats_block") {
        editor_layout_->addWidget(make_label("Block Title:"));
        auto* title_edit = new QLineEdit(component->config.value("title", "Key Statistics"));
        connect(title_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "title", val); });
        editor_layout_->addWidget(title_edit);

        // Auto-fetch row
        editor_layout_->addWidget(make_label("Auto-fill from Ticker:"));
        auto* ticker_row = new QWidget(this);
        ticker_row->setStyleSheet("background: transparent;");
        auto* tr_hl = new QHBoxLayout(ticker_row);
        tr_hl->setContentsMargins(0, 0, 0, 0);
        tr_hl->setSpacing(4);

        auto* ticker_edit = new QLineEdit;
        ticker_edit->setPlaceholderText("e.g. AAPL");
        tr_hl->addWidget(ticker_edit, 1);

        auto* fetch_stats_btn = new QPushButton("Fetch");
        fetch_stats_btn->setFixedWidth(52);
        fetch_stats_btn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2; font-weight: bold; border: 1px solid %3; }"
                    "QPushButton:hover { background: %4; }")
                .arg(ui::colors::AMBER_DIM(), ui::colors::WHITE(), ui::colors::AMBER(), ui::colors::AMBER()));
        tr_hl->addWidget(fetch_stats_btn);
        editor_layout_->addWidget(ticker_row);

        // When fetch is clicked, signal the screen to fetch and populate
        connect(fetch_stats_btn, &QPushButton::clicked, this, [this, ticker_edit, title_edit]() {
            QString sym = ticker_edit->text().trimmed().toUpper();
            if (!sym.isEmpty()) {
                emit config_changed(current_index_, "fetch_ticker", sym);
                // Update title to match ticker
                if (title_edit->text().trimmed().isEmpty() || title_edit->text() == "Key Statistics")
                    title_edit->setText(sym + " Key Statistics");
            }
        });

        editor_layout_->addWidget(make_label("Stats (one per line, Label:Value):"));
        auto* data_edit = new QTextEdit;
        data_edit->setPlainText(component->config.value("data", ""));
        data_edit->setMinimumHeight(120);
        data_edit->setMaximumHeight(200);
        data_edit->setPlaceholderText("P/E Ratio: 28.4\n"
                                      "Market Cap: $2.9T\n"
                                      "52W High: $199.62\n"
                                      "52W Low: $124.17\n"
                                      "Dividend Yield: 0.51%\n"
                                      "EPS: $6.43");
        connect(data_edit, &QTextEdit::textChanged, this,
                [this, data_edit]() { emit config_changed(current_index_, "data", data_edit->toPlainText()); });
        editor_layout_->addWidget(data_edit);
    }

    // ── Callout box: style + heading + content ────────────────────────────────
    if (component->type == "callout") {
        editor_layout_->addWidget(make_label("Style:"));
        auto* style_combo = new QComboBox;
        style_combo->addItems({"info", "success", "warning", "danger"});
        int si = style_combo->findText(component->config.value("style", "info"));
        if (si >= 0)
            style_combo->setCurrentIndex(si);
        connect(style_combo, &QComboBox::currentTextChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "style", val); });
        editor_layout_->addWidget(style_combo);

        editor_layout_->addWidget(make_label("Heading (optional):"));
        auto* heading_edit = new QLineEdit(component->config.value("heading", ""));
        heading_edit->setPlaceholderText("e.g. Key Risk, Note, Important");
        connect(heading_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "heading", val); });
        editor_layout_->addWidget(heading_edit);

        editor_layout_->addWidget(make_label("Content:"));
        auto* body_edit = new QTextEdit;
        body_edit->setPlainText(component->content);
        body_edit->setMaximumHeight(120);
        connect(body_edit, &QTextEdit::textChanged, this,
                [this, body_edit]() { emit content_changed(current_index_, body_edit->toPlainText()); });
        editor_layout_->addWidget(body_edit);
    }

    // ── Sparkline: title + data + current value + change% ────────────────────
    if (component->type == "sparkline") {
        editor_layout_->addWidget(make_label("Label:"));
        auto* title_edit = new QLineEdit(component->config.value("title", ""));
        title_edit->setPlaceholderText("e.g. AAPL");
        connect(title_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "title", val); });
        editor_layout_->addWidget(title_edit);

        editor_layout_->addWidget(make_label("Current Value:"));
        auto* val_edit = new QLineEdit(component->config.value("current", ""));
        val_edit->setPlaceholderText("e.g. $189.30");
        connect(val_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "current", val); });
        editor_layout_->addWidget(val_edit);

        editor_layout_->addWidget(make_label("Change %:"));
        auto* chg_edit = new QLineEdit(component->config.value("change_pct", ""));
        chg_edit->setPlaceholderText("e.g. +2.34 or -1.10");
        connect(chg_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "change_pct", val); });
        editor_layout_->addWidget(chg_edit);

        editor_layout_->addWidget(make_label("Price Series (CSV values):"));
        auto* data_edit = new QTextEdit;
        data_edit->setPlainText(component->config.value("data", ""));
        data_edit->setMaximumHeight(80);
        data_edit->setPlaceholderText("175,178,182,179,185,188,186,190,189");
        connect(data_edit, &QTextEdit::textChanged, this,
                [this, data_edit]() { emit config_changed(current_index_, "data", data_edit->toPlainText()); });
        editor_layout_->addWidget(data_edit);

        auto* hint = new QLabel("Tip: re-select after editing\ndata to refresh sparkline.");
        hint->setWordWrap(true);
        hint->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::MUTED()));
        editor_layout_->addWidget(hint);
    }

    // ── Image: file + clipboard + width + alignment + caption ────────────────
    if (component->type == "image") {
        editor_layout_->addWidget(make_label("Image File:"));
        auto* path_lbl = new QLabel(component->config.value("path", "(none)"));
        path_lbl->setWordWrap(true);
        path_lbl->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(ui::colors::MUTED()));
        editor_layout_->addWidget(path_lbl);

        // Browse button
        auto* browse = new QPushButton("Browse File...");
        connect(browse, &QPushButton::clicked, this, [this, path_lbl]() {
            QString path =
                QFileDialog::getOpenFileName(this, "Select Image", "", "Images (*.png *.jpg *.jpeg *.bmp *.svg)");
            if (!path.isEmpty()) {
                path_lbl->setText(QFileInfo(path).fileName());
                emit config_changed(current_index_, "path", path);
                emit config_changed(current_index_, "clipboard", ""); // clear clipboard flag
            }
        });
        editor_layout_->addWidget(browse);

        // Paste from clipboard button
        auto* paste_btn = new QPushButton("Paste from Clipboard");
        paste_btn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2; border: 1px solid %3; }"
                    "QPushButton:hover { background: %4; }")
                .arg(ui::colors::BG_RAISED(), ui::colors::AMBER(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
        connect(paste_btn, &QPushButton::clicked, this, [this, path_lbl]() {
            const QClipboard* cb = QApplication::clipboard();
            const QMimeData* mime = cb->mimeData();
            if (mime->hasImage()) {
                // Save clipboard image to a temp file and use that path
                QImage img = qvariant_cast<QImage>(mime->imageData());
                QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                              QString("/fincept_paste_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
                if (img.save(tmp)) {
                    path_lbl->setText("(clipboard image)");
                    emit config_changed(current_index_, "path", tmp);
                    emit config_changed(current_index_, "clipboard", "1");
                }
            } else {
                path_lbl->setText("(no image in clipboard)");
            }
        });
        editor_layout_->addWidget(paste_btn);

        // Width
        editor_layout_->addWidget(make_label("Width (px):"));
        auto* width_spin = new QSpinBox;
        width_spin->setRange(50, 794);
        width_spin->setValue(component->config.value("width", "400").toInt());
        connect(width_spin, &QSpinBox::valueChanged, this,
                [this](int val) { emit config_changed(current_index_, "width", QString::number(val)); });
        editor_layout_->addWidget(width_spin);

        // Alignment
        editor_layout_->addWidget(make_label("Alignment:"));
        auto* align_combo = new QComboBox;
        align_combo->addItems({"left", "center", "right"});
        int ai = align_combo->findText(component->config.value("align", "left"));
        if (ai >= 0)
            align_combo->setCurrentIndex(ai);
        connect(align_combo, &QComboBox::currentTextChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "align", val); });
        editor_layout_->addWidget(align_combo);

        // Caption
        editor_layout_->addWidget(make_label("Caption (optional):"));
        auto* caption_edit = new QLineEdit(component->config.value("caption", ""));
        caption_edit->setPlaceholderText("e.g. Figure 1: Revenue growth");
        connect(caption_edit, &QLineEdit::textChanged, this,
                [this](const QString& val) { emit config_changed(current_index_, "caption", val); });
        editor_layout_->addWidget(caption_edit);
    }

    // ── List editor ───────────────────────────────────────────────────────────
    if (component->type == "list") {
        editor_layout_->addWidget(make_label("Items (one per line):"));
        auto* edit = new QTextEdit;
        edit->setPlainText(component->content);
        edit->setMaximumHeight(160);
        connect(edit, &QTextEdit::textChanged, this,
                [this, edit]() { emit content_changed(current_index_, edit->toPlainText()); });
        editor_layout_->addWidget(edit);
    }

    // ── Market Data: ticker input ─────────────────────────────────────────────
    if (component->type == "market_data") {
        editor_layout_->addWidget(make_label("Ticker Symbol:"));
        auto* sym_edit = new QLineEdit;
        sym_edit->setPlaceholderText("e.g. AAPL, BTC-USD, ^GSPC");
        sym_edit->setText(component->config.value("symbol", ""));
        editor_layout_->addWidget(sym_edit);

        auto* fetch_btn = new QPushButton("Fetch Price");
        fetch_btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; font-weight: bold; }"
                                         "QPushButton:hover { background: %3; }")
                                     .arg(ui::colors::AMBER_DIM(), ui::colors::WHITE(), ui::colors::AMBER()));
        editor_layout_->addWidget(fetch_btn);

        // When fetch is clicked, update config symbol — ReportBuilderScreen will
        // handle the actual MarketDataService call and update price fields
        connect(fetch_btn, &QPushButton::clicked, this, [this, sym_edit]() {
            QString sym = sym_edit->text().trimmed().toUpper();
            if (!sym.isEmpty()) {
                emit config_changed(current_index_, "symbol", sym);
                emit config_changed(current_index_, "status", "loading");
            }
        });

        // Also update symbol on text change (without fetching)
        connect(sym_edit, &QLineEdit::editingFinished, this, [this, sym_edit]() {
            emit config_changed(current_index_, "symbol", sym_edit->text().trimmed().toUpper());
        });

        // Show cached price info if available
        if (!component->config.value("price").isEmpty()) {
            auto* sep2 = new QFrame;
            sep2->setFixedHeight(1);
            sep2->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
            editor_layout_->addWidget(sep2);

            auto* price_row =
                new QLabel(QString("Price: %1   Chg: %2%")
                               .arg(component->config.value("price"), component->config.value("change_pct")));
            price_row->setStyleSheet(
                QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::AMBER()));
            editor_layout_->addWidget(price_row);
        }
    }

    // ── Page Break: no extra settings ─────────────────────────────────────────
    if (component->type == "page_break") {
        auto* info = new QLabel("Inserts a page break\nin PDF/print output.");
        info->setAlignment(Qt::AlignCenter);
        info->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(ui::colors::MUTED()));
        editor_layout_->addWidget(info);
    }

    // ── TOC: no extra settings ────────────────────────────────────────────────
    if (component->type == "toc") {
        auto* info = new QLabel("Auto-generated from\nHeading components.");
        info->setAlignment(Qt::AlignCenter);
        info->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(ui::colors::MUTED()));
        editor_layout_->addWidget(info);
    }

    // ── Action buttons ────────────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER()));
    editor_layout_->addWidget(sep);

    auto* dup_btn = new QPushButton("Duplicate");
    connect(dup_btn, &QPushButton::clicked, this, [this]() { emit duplicate_requested(current_index_); });
    editor_layout_->addWidget(dup_btn);

    auto* del_btn = new QPushButton("Delete");
    del_btn->setStyleSheet(QString("QPushButton { color: %1; border: 1px solid %1; }"
                                   "QPushButton:hover { background: %1; color: white; }")
                               .arg(ui::colors::RED()));
    connect(del_btn, &QPushButton::clicked, this, [this]() { emit delete_requested(current_index_); });
    editor_layout_->addWidget(del_btn);

    editor_layout_->addStretch();
    stack_->setCurrentIndex(1);
}

} // namespace fincept::screens
