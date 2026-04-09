// src/screens/file_manager/FileManagerScreen.cpp
#include "screens/file_manager/FileManagerScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMessageBox>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QTextStream>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
using fincept::services::FileManagerService;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";
static QString panel_ss() {
    return QString("background:%1;border:1px solid %2;border-radius:2px;")
        .arg(colors::BG_SURFACE(), colors::BORDER_DIM());
}

// ── Constructor ───────────────────────────────────────────────────────────────

FileManagerScreen::FileManagerScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect(&FileManagerService::instance(), &FileManagerService::files_changed, this, [this]() {
        render_files();
        update_stats();
    });
}

// ── UI ────────────────────────────────────────────────────────────────────────

void FileManagerScreen::build_ui() {
    setStyleSheet(QString("QWidget#FMRoot{background:%1;}").arg(colors::BG_BASE()));
    setObjectName("FMRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(14, 10, 14, 10);
    hhl->setSpacing(8);

    auto* title = new QLabel("FILE MANAGER");
    title->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:700;letter-spacing:0.5px;background:transparent;%2")
            .arg(colors::AMBER(), MF));
    hhl->addWidget(title);

    auto* sub = new QLabel("Manage files across the terminal");
    sub->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;%2").arg(colors::TEXT_TERTIARY(), MF));
    hhl->addWidget(sub);
    hhl->addStretch();

    stats_label_ = new QLabel("0 files | 0 B");
    stats_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2").arg(colors::TEXT_DIM(), MF));
    hhl->addWidget(stats_label_);

    refresh_btn_ = new QPushButton("REFRESH");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "padding:4px 12px;font-size:11px;%3}"
                "QPushButton:hover{color:%4;background:%5;}")
            .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(), MF, colors::TEXT_PRIMARY(), colors::BG_HOVER()));
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        render_files();
        update_stats();
    });
    hhl->addWidget(refresh_btn_);

    upload_btn_ = new QPushButton("UPLOAD FILES");
    upload_btn_->setCursor(Qt::PointingHandCursor);
    upload_btn_->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %3;"
                                       "padding:4px 12px;font-size:11px;font-weight:700;%2}"
                                       "QPushButton:hover{background:%1;color:%4;}")
                                   .arg(colors::AMBER(), MF, colors::AMBER_DIM(), colors::BG_BASE()));
    connect(upload_btn_, &QPushButton::clicked, this, &FileManagerScreen::upload_files);
    hhl->addWidget(upload_btn_);
    root->addWidget(header);

    // ── Quota bar ─────────────────────────────────────────────────────────────
    build_quota_bar(root);

    // ── Filter / sort bar ─────────────────────────────────────────────────────
    build_filter_bar(root);

    // ── Bulk action bar (hidden until items checked) ──────────────────────────
    bulk_bar_ = new QWidget(this);
    bulk_bar_->setStyleSheet(QString("background:#1a0a00;border-bottom:1px solid %1;").arg(colors::AMBER_DIM()));
    bulk_bar_->setVisible(false);
    auto* bbl = new QHBoxLayout(bulk_bar_);
    bbl->setContentsMargins(14, 6, 14, 6);
    bbl->setSpacing(10);

    auto* sel_lbl = new QLabel("Selected files:");
    sel_lbl->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2").arg(colors::TEXT_SECONDARY(), MF));
    bbl->addWidget(sel_lbl);

    bulk_delete_btn_ = new QPushButton("DELETE SELECTED");
    bulk_delete_btn_->setCursor(Qt::PointingHandCursor);
    bulk_delete_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid #7f1d1d;"
                                            "padding:3px 12px;font-size:11px;font-weight:700;%2}"
                                            "QPushButton:hover{background:%1;color:%3;}")
                                        .arg(colors::NEGATIVE(), MF, colors::TEXT_PRIMARY()));
    connect(bulk_delete_btn_, &QPushButton::clicked, this, &FileManagerScreen::delete_selected);
    bbl->addWidget(bulk_delete_btn_);
    bbl->addStretch();

    auto* desel_btn = new QPushButton("CLEAR SELECTION");
    desel_btn->setCursor(Qt::PointingHandCursor);
    desel_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                     "padding:3px 12px;font-size:11px;%3}"
                                     "QPushButton:hover{color:%4;}")
                                 .arg(colors::TEXT_DIM(), colors::BORDER_DIM(), MF, colors::TEXT_PRIMARY()));
    connect(desel_btn, &QPushButton::clicked, this, [this]() {
        for (auto* btn : check_btns_)
            btn->setChecked(false);
        update_bulk_bar();
    });
    bbl->addWidget(desel_btn);
    root->addWidget(bulk_bar_);

    // ── Splitter: file list | preview ─────────────────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setStyleSheet(QString("QSplitter::handle{background:%1;width:1px;}").arg(colors::BORDER_DIM()));
    splitter_->setChildrenCollapsible(false);

    // Left: scroll area with file cards
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}"
                          "QScrollBar:vertical{background:transparent;width:6px;}" +
                          QString("QScrollBar::handle:vertical{background:%1;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(colors::BORDER_MED()));

    file_container_ = new QWidget(this);
    file_container_->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));
    file_layout_ = new QVBoxLayout(file_container_);
    file_layout_->setContentsMargins(14, 8, 14, 8);
    file_layout_->setSpacing(6);
    file_layout_->addStretch();
    scroll->setWidget(file_container_);
    splitter_->addWidget(scroll);

    // Right: preview panel (initially hidden)
    splitter_->addWidget(build_preview_panel());
    preview_panel_->setVisible(false);

    root->addWidget(splitter_, 1);
}

void FileManagerScreen::build_quota_bar(QVBoxLayout* root) {
    auto* qw = new QWidget(this);
    qw->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(colors::BG_BASE(), colors::BORDER_DIM()));
    auto* ql = new QHBoxLayout(qw);
    ql->setContentsMargins(14, 6, 14, 6);
    ql->setSpacing(10);

    quota_label_ = new QLabel("Storage: 0 B / 500 MB");
    quota_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2").arg(colors::TEXT_DIM(), MF));
    ql->addWidget(quota_label_);

    quota_bar_ = new QProgressBar;
    quota_bar_->setRange(0, 1000);
    quota_bar_->setValue(0);
    quota_bar_->setTextVisible(false);
    quota_bar_->setFixedHeight(6);
    quota_bar_->setStyleSheet(QString("QProgressBar{background:%1;border:none;border-radius:3px;}"
                                      "QProgressBar::chunk{background:%2;border-radius:3px;}")
                                  .arg(colors::BORDER_DIM(), colors::AMBER()));
    ql->addWidget(quota_bar_, 1);

    root->addWidget(qw);
}

void FileManagerScreen::build_filter_bar(QVBoxLayout* root) {
    auto* bar = new QWidget(this);
    bar->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(colors::BG_BASE(), colors::BORDER_DIM()));
    auto* bl = new QVBoxLayout(bar);
    bl->setContentsMargins(14, 8, 14, 8);
    bl->setSpacing(6);

    // Row 1: search + sort
    auto* row1 = new QWidget(this);
    row1->setStyleSheet("background:transparent;");
    auto* r1l = new QHBoxLayout(row1);
    r1l->setContentsMargins(0, 0, 0, 0);
    r1l->setSpacing(8);

    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search files by name, type, or source...");
    search_input_->setStyleSheet(
        QString("QLineEdit{background:%1;color:%2;border:1px solid %3;"
                "padding:5px 10px;font-size:12px;%4}"
                "QLineEdit:focus{border-color:%5;}")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), MF, colors::AMBER()));
    connect(search_input_, &QLineEdit::textChanged, this, [this]() { render_files(); });
    r1l->addWidget(search_input_, 1);

    auto* sort_lbl = new QLabel("Sort:");
    sort_lbl->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;%2").arg(colors::TEXT_DIM(), MF));
    r1l->addWidget(sort_lbl);

    sort_combo_ = new QComboBox;
    sort_combo_->addItems(
        {"Date (newest)", "Date (oldest)", "Name (A-Z)", "Name (Z-A)", "Size (largest)", "Size (smallest)", "Type"});
    sort_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;"
                "padding:4px 8px;font-size:11px;%4}"
                "QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;border:1px solid %3;"
                "selection-background-color:%5;}")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), MF, colors::BG_HOVER()));
    connect(sort_combo_, &QComboBox::currentIndexChanged, this, [this]() { render_files(); });
    r1l->addWidget(sort_combo_);
    bl->addWidget(row1);

    // Row 2: screen filter chips
    chips_bar_ = new QWidget(this);
    chips_bar_->setStyleSheet("background:transparent;");
    auto* cl = new QHBoxLayout(chips_bar_);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(4);

    chip_group_ = new QButtonGroup(this);
    chip_group_->setExclusive(true);

    QString chip_ss = QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                              "border-radius:10px;padding:2px 10px;font-size:10px;%3}"
                              "QPushButton:checked{background:%4;color:#000;border-color:%4;}"
                              "QPushButton:hover:!checked{border-color:%4;}")
                          .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(), MF, colors::AMBER());

    static const QStringList screens = {"All",          "portfolio",    "backtesting", "news",        "equity_research",
                                        "algo_trading", "ai_quant_lab", "notes",       "code_editor", "report_builder",
                                        "data_sources", "excel"};
    for (const QString& s : screens) {
        QString label = (s == "All") ? "All" : QString(s).replace('_', ' ').toUpper();
        auto* chip = new QPushButton(label);
        chip->setCheckable(true);
        chip->setStyleSheet(chip_ss);
        if (s == "All")
            chip->setChecked(true);
        chip_group_->addButton(chip);
        cl->addWidget(chip);
        QString filter_val = (s == "All") ? "" : s;
        connect(chip, &QPushButton::toggled, this, [this, filter_val](bool checked) {
            if (checked) {
                active_screen_filter_ = filter_val;
                render_files();
                ScreenStateManager::instance().notify_changed(this);
            }
        });
    }
    cl->addStretch();
    bl->addWidget(chips_bar_);

    root->addWidget(bar);
}

QWidget* FileManagerScreen::build_preview_panel() {
    preview_panel_ = new QWidget(this);
    preview_panel_->setStyleSheet(
        QString("background:%1;border-left:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(preview_panel_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Preview header
    auto* hdr = new QWidget(this);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 8, 12, 8);

    auto* plbl = new QLabel("PREVIEW");
    plbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;background:transparent;%2")
                            .arg(colors::AMBER(), MF));
    hhl->addWidget(plbl);
    hhl->addStretch();

    auto* close_btn = new QPushButton("✕");
    close_btn->setFixedSize(20, 20);
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:none;font-size:12px;}"
                                     "QPushButton:hover{color:%2;}")
                                 .arg(colors::TEXT_DIM(), colors::NEGATIVE()));
    connect(close_btn, &QPushButton::clicked, this, &FileManagerScreen::clear_preview);
    hhl->addWidget(close_btn);
    vl->addWidget(hdr);

    // File meta
    preview_title_ = new QLabel;
    preview_title_->setWordWrap(true);
    preview_title_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;background:transparent;"
                                          "padding:10px 12px 2px 12px;%2")
                                      .arg(colors::TEXT_PRIMARY(), MF));
    vl->addWidget(preview_title_);

    preview_meta_ = new QLabel;
    preview_meta_->setWordWrap(true);
    preview_meta_->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;padding:0 12px 8px 12px;%2")
                                     .arg(colors::TEXT_DIM(), MF));
    vl->addWidget(preview_meta_);

    auto* div = new QFrame;
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet(QString("background:%1;max-height:1px;border:none;").arg(colors::BORDER_DIM()));
    vl->addWidget(div);

    // Text preview
    preview_text_ = new QTextEdit;
    preview_text_->setReadOnly(true);
    preview_text_->setStyleSheet(QString("QTextEdit{background:%1;color:%2;border:none;"
                                         "padding:10px;font-size:11px;%3}")
                                     .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), MF));
    preview_text_->setVisible(false);
    vl->addWidget(preview_text_, 1);

    // Table preview (for CSV)
    preview_table_ = new QTableWidget;
    preview_table_->setStyleSheet(
        QString("QTableWidget{background:%1;color:%2;border:none;gridline-color:%3;font-size:11px;%4}"
                "QHeaderView::section{background:%5;color:%6;border:none;"
                "border-bottom:1px solid %3;padding:4px;font-size:10px;font-weight:700;%4}")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), MF, colors::BG_SURFACE(),
                 colors::TEXT_SECONDARY));
    preview_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    preview_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    preview_table_->horizontalHeader()->setStretchLastSection(true);
    preview_table_->verticalHeader()->setVisible(false);
    preview_table_->setVisible(false);
    vl->addWidget(preview_table_, 1);

    // Empty/binary state
    preview_empty_ = new QLabel("Select a file to preview");
    preview_empty_->setAlignment(Qt::AlignCenter);
    preview_empty_->setWordWrap(true);
    preview_empty_->setStyleSheet(
        QString("color:%1;font-size:12px;background:transparent;padding:30px;%2").arg(colors::TEXT_DIM(), MF));
    vl->addWidget(preview_empty_, 1);

    return preview_panel_;
}

// ── Visibility ────────────────────────────────────────────────────────────────

void FileManagerScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!loaded_) {
        loaded_ = true;
        render_files();
        update_stats();
    }
}

// ── File operations ───────────────────────────────────────────────────────────

void FileManagerScreen::upload_files() {
    QStringList paths = QFileDialog::getOpenFileNames(this, "Select Files to Upload");
    if (paths.isEmpty())
        return;
    for (const QString& path : paths)
        FileManagerService::instance().import_file(path);
}

void FileManagerScreen::download_file(const QString& file_id) {
    auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;
    QString dest = QFileDialog::getSaveFileName(this, "Save File As", f.original_name);
    if (dest.isEmpty())
        return;
    QString src = FileManagerService::instance().full_path(f.name);
    if (QFile::exists(dest))
        QFile::remove(dest);
    if (!QFile::copy(src, dest))
        QMessageBox::warning(this, "Download Failed", "Could not save file to selected location.");
    else
        LOG_INFO("FileManager", "Downloaded: " + f.original_name);
}

void FileManagerScreen::delete_file(const QString& file_id) {
    auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;
    auto reply = QMessageBox::question(this, "Delete File",
                                       QString("Delete \"%1\"? This cannot be undone.").arg(f.original_name),
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        FileManagerService::instance().remove_file(file_id);
}

void FileManagerScreen::delete_selected() {
    QStringList to_delete;
    for (int i = 0; i < check_btns_.size(); ++i) {
        if (check_btns_[i]->isChecked())
            to_delete << check_ids_[i];
    }
    if (to_delete.isEmpty())
        return;
    auto reply = QMessageBox::question(
        this, "Delete Files", QString("Delete %1 selected file(s)? This cannot be undone.").arg(to_delete.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;
    for (const QString& id : to_delete)
        FileManagerService::instance().remove_file(id);
}

void FileManagerScreen::open_with(const QString& file_id) {
    auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;
    QString route = route_for_mime(f.mime_type);
    if (route.isEmpty())
        return;
    emit open_file_in_screen(route, FileManagerService::instance().full_path(f.name));
}

void FileManagerScreen::show_preview(const QString& file_id) {
    auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;

    // Expand preview panel if hidden
    preview_panel_->setVisible(true);
    splitter_->setSizes({550, 350});

    preview_title_->setText(f.original_name);
    QString date_str = QDateTime::fromString(f.uploaded_at, Qt::ISODate).toLocalTime().toString("yyyy-MM-dd HH:mm");
    preview_meta_->setText(format_size(f.size) + "  |  " + f.mime_type + "  |  " + f.source_screen + "  |  " +
                           date_str);

    preview_text_->setVisible(false);
    preview_table_->setVisible(false);
    preview_empty_->setVisible(false);

    // Binary types — show info only
    if (f.mime_type.contains("image") || f.mime_type.contains("video") || f.mime_type.contains("audio") ||
        f.mime_type.contains("zip") || f.mime_type.contains("spreadsheet") || f.mime_type.contains("excel") ||
        f.mime_type.contains("vnd.openxmlformats")) {
        preview_empty_->setText("Binary file — preview not available.\nUse SAVE to download.");
        preview_empty_->setVisible(true);
        return;
    }

    QString path = FileManagerService::instance().full_path(f.name);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        preview_empty_->setText("Cannot open file for preview.");
        preview_empty_->setVisible(true);
        return;
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.read(64000);
    file.close();

    // CSV → table
    if (f.mime_type.contains("csv") || f.original_name.endsWith(".csv")) {
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        if (lines.isEmpty()) {
            preview_empty_->setVisible(true);
            return;
        }

        auto parse_csv_row = [](const QString& line) -> QStringList {
            QStringList fields;
            QString cur;
            bool in_quotes = false;
            for (QChar c : line) {
                if (c == '"') {
                    in_quotes = !in_quotes;
                } else if (c == ',' && !in_quotes) {
                    fields << cur.trimmed();
                    cur.clear();
                } else
                    cur += c;
            }
            fields << cur.trimmed();
            return fields;
        };

        QStringList headers = parse_csv_row(lines[0]);
        preview_table_->setColumnCount(headers.size());
        preview_table_->setHorizontalHeaderLabels(headers);
        preview_table_->setRowCount(qMin(lines.size() - 1, 200));
        for (int r = 1; r < qMin(lines.size(), 201); ++r) {
            QStringList row = parse_csv_row(lines[r]);
            for (int c = 0; c < headers.size(); ++c) {
                preview_table_->setItem(r - 1, c, new QTableWidgetItem(c < row.size() ? row[c] : QString()));
            }
        }
        preview_table_->resizeColumnsToContents();
        preview_table_->setVisible(true);
        return;
    }

    // JSON → formatted
    if (f.mime_type.contains("json") || f.original_name.endsWith(".json")) {
        auto doc = QJsonDocument::fromJson(content.toUtf8());
        if (!doc.isNull())
            content = doc.toJson(QJsonDocument::Indented);
    }

    preview_text_->setPlainText(content);
    if (content.length() == 64000)
        preview_text_->append("\n\n[... truncated at 64K characters ...]");
    preview_text_->setVisible(true);
}

void FileManagerScreen::clear_preview() {
    preview_text_->clear();
    preview_table_->clearContents();
    preview_table_->setRowCount(0);
    preview_text_->setVisible(false);
    preview_table_->setVisible(false);
    preview_title_->clear();
    preview_meta_->clear();
    preview_empty_->setText("Select a file to preview");
    preview_empty_->setVisible(true);
    preview_panel_->setVisible(false);
}

void FileManagerScreen::update_bulk_bar() {
    int selected = 0;
    for (auto* btn : check_btns_)
        if (btn->isChecked())
            selected++;
    bulk_bar_->setVisible(selected > 0);
    if (selected > 0)
        bulk_delete_btn_->setText(QString("DELETE %1 SELECTED").arg(selected));
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void FileManagerScreen::render_files() {
    check_btns_.clear();
    check_ids_.clear();

    while (QLayoutItem* item = file_layout_->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    QString filter = search_input_ ? search_input_->text().toLower() : QString();
    int sort_idx = sort_combo_ ? sort_combo_->currentIndex() : 0;
    QJsonArray all = FileManagerService::instance().all_files();

    // Filter
    QVector<QJsonObject> files;
    for (const auto& v : all) {
        if (!v.isObject())
            continue;
        QJsonObject obj = v.toObject();
        if (!active_screen_filter_.isEmpty() && obj["sourceScreen"].toString() != active_screen_filter_)
            continue;
        if (!filter.isEmpty()) {
            bool match = obj["originalName"].toString().toLower().contains(filter) ||
                         obj["type"].toString().toLower().contains(filter) ||
                         obj["sourceScreen"].toString().toLower().contains(filter);
            if (!match)
                continue;
        }
        files.append(obj);
    }

    // Sort
    auto cmp = [&](const QJsonObject& a, const QJsonObject& b) -> bool {
        switch (sort_idx) {
            case 0: // Date newest
                return a["uploadedAt"].toString() > b["uploadedAt"].toString();
            case 1: // Date oldest
                return a["uploadedAt"].toString() < b["uploadedAt"].toString();
            case 2: // Name A-Z
                return a["originalName"].toString().toLower() < b["originalName"].toString().toLower();
            case 3: // Name Z-A
                return a["originalName"].toString().toLower() > b["originalName"].toString().toLower();
            case 4: // Size largest
                return a["size"].toInteger() > b["size"].toInteger();
            case 5: // Size smallest
                return a["size"].toInteger() < b["size"].toInteger();
            case 6: // Type
                return a["type"].toString() < b["type"].toString();
            default:
                return false;
        }
    };
    std::sort(files.begin(), files.end(), cmp);

    if (files.isEmpty()) {
        if (all.isEmpty()) {
            // ── Full empty skeleton ──────────────────────────────────────
            auto* skeleton = new QWidget(this);
            skeleton->setStyleSheet("background:transparent;");
            auto* skl = new QVBoxLayout(skeleton);
            skl->setContentsMargins(24, 24, 24, 24);
            skl->setSpacing(20);

            auto* hero_lbl = new QLabel("[ FM ]");
            hero_lbl->setAlignment(Qt::AlignCenter);
            hero_lbl->setStyleSheet(
                QString("color:%1;font-size:36px;font-weight:700;background:transparent;%2").arg(colors::AMBER(), MF));
            skl->addWidget(hero_lbl);

            auto* hero_sub = new QLabel("Your terminal file index is empty.");
            hero_sub->setAlignment(Qt::AlignCenter);
            hero_sub->setStyleSheet(
                QString("color:%1;font-size:14px;background:transparent;%2").arg(colors::TEXT_SECONDARY(), MF));
            skl->addWidget(hero_sub);

            auto* hero_hint =
                new QLabel("Files are registered automatically when you export, save, or generate output\n"
                           "from any screen. You can also upload files manually using the button above.");
            hero_hint->setAlignment(Qt::AlignCenter);
            hero_hint->setWordWrap(true);
            hero_hint->setStyleSheet(
                QString("color:%1;font-size:12px;background:transparent;%2").arg(colors::TEXT_DIM(), MF));
            skl->addWidget(hero_hint);

            auto* div = new QFrame;
            div->setFrameShape(QFrame::HLine);
            div->setStyleSheet(QString("background:%1;max-height:1px;border:none;").arg(colors::BORDER_DIM()));
            skl->addWidget(div);

            auto* sources_lbl = new QLabel("FILES ARE COLLECTED FROM");
            sources_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;"
                                               "background:transparent;%2")
                                           .arg(colors::TEXT_DIM(), MF));
            skl->addWidget(sources_lbl);

            struct SI {
                QString icon, name, desc;
            };
            static const SI sources[] = {
                {"RPT", "Report Builder", "Saved reports and exported PDFs"},
                {"NTB", "Code Editor", "Saved Jupyter notebooks (.ipynb)"},
                {"XLS", "Excel", "Imported/exported spreadsheets & CSV"},
                {"NOTE", "Notes", "Exported markdown note files"},
                {"PFL", "Portfolio", "Portfolio CSV & JSON exports"},
                {"DS", "Data Sources", "Connector configuration JSON"},
                {"ALG", "Algo Trading", "Saved strategy definitions"},
                {"BT", "Backtesting", "Backtest result JSON exports"},
                {"AQL", "AI Quant Lab", "Module result exports"},
                {"NEWS", "News", "Saved article text files"},
                {"EQR", "Equity Research", "Financial statement CSV exports"},
            };

            auto* grid_w = new QWidget(this);
            grid_w->setStyleSheet("background:transparent;");
            auto* grid = new QGridLayout(grid_w);
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setSpacing(8);

            for (int i = 0; i < 11; ++i) {
                const auto& s = sources[i];
                auto* card = new QWidget(this);
                card->setStyleSheet(panel_ss());
                auto* cl2 = new QHBoxLayout(card);
                cl2->setContentsMargins(10, 8, 10, 8);
                cl2->setSpacing(8);
                auto* badge = new QLabel(s.icon);
                badge->setFixedWidth(36);
                badge->setAlignment(Qt::AlignCenter);
                badge->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;"
                                             "background:rgba(217,119,6,0.08);border:1px solid %2;"
                                             "border-radius:2px;padding:2px 0;%3")
                                         .arg(colors::AMBER(), colors::AMBER_DIM(), MF));
                cl2->addWidget(badge);
                auto* txt = new QWidget(this);
                txt->setStyleSheet("background:transparent;");
                auto* tl = new QVBoxLayout(txt);
                tl->setContentsMargins(0, 0, 0, 0);
                tl->setSpacing(1);
                auto* nl = new QLabel(s.name);
                nl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;%2")
                                      .arg(colors::TEXT_PRIMARY(), MF));
                tl->addWidget(nl);
                auto* dl = new QLabel(s.desc);
                dl->setStyleSheet(
                    QString("color:%1;font-size:10px;background:transparent;%2").arg(colors::TEXT_DIM(), MF));
                tl->addWidget(dl);
                cl2->addWidget(txt, 1);
                grid->addWidget(card, i / 3, i % 3);
            }
            skl->addWidget(grid_w);

            auto* div2 = new QFrame;
            div2->setFrameShape(QFrame::HLine);
            div2->setStyleSheet(QString("background:%1;max-height:1px;border:none;").arg(colors::BORDER_DIM()));
            skl->addWidget(div2);

            auto* tips_lbl = new QLabel("TIPS");
            tips_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;"
                                            "background:transparent;%2")
                                        .arg(colors::TEXT_DIM(), MF));
            skl->addWidget(tips_lbl);

            for (const char* tip : {
                     "Use the filter chips above to narrow files by source screen.",
                     "Click any file card to preview its contents in the right panel.",
                     "Use checkboxes on file cards to bulk-delete multiple files at once.",
                     "MCP tools can list and read your files directly in AI Chat.",
                 }) {
                auto* tl2 = new QLabel(QString("•  ") + tip);
                tl2->setWordWrap(true);
                tl2->setStyleSheet(
                    QString("color:%1;font-size:11px;background:transparent;%2").arg(colors::TEXT_TERTIARY(), MF));
                skl->addWidget(tl2);
            }
            skl->addStretch();
            file_layout_->addWidget(skeleton);
        } else {
            auto* empty = new QLabel("No files match your search or filter.");
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet(QString("color:%1;font-size:13px;padding:40px;%2").arg(colors::TEXT_DIM(), MF));
            file_layout_->addWidget(empty);
        }
        file_layout_->addStretch();
        update_bulk_bar();
        return;
    }

    // Group by source screen
    QString current_group;
    for (const QJsonObject& file : files) {
        QString source = file["sourceScreen"].toString();
        if (active_screen_filter_.isEmpty() && source != current_group) {
            current_group = source;
            auto* grp_lbl = new QLabel(source.toUpper().replace('_', ' '));
            grp_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;"
                                           "background:transparent;padding:8px 0 2px 0;%2")
                                       .arg(colors::TEXT_DIM(), MF));
            file_layout_->addWidget(grp_lbl);
        }

        // ── Card ─────────────────────────────────────────────────────────────
        auto* card = new QWidget(this);
        card->setStyleSheet(panel_ss());
        card->setCursor(Qt::PointingHandCursor);
        auto* hl = new QHBoxLayout(card);
        hl->setContentsMargins(8, 8, 12, 8);
        hl->setSpacing(8);

        // Checkbox
        auto* chk = new QPushButton("☐");
        chk->setCheckable(true);
        chk->setFixedSize(22, 22);
        chk->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:none;font-size:14px;}"
                                   "QPushButton:checked{color:%2;}")
                               .arg(colors::TEXT_DIM(), colors::AMBER()));
        connect(chk, &QPushButton::toggled, this, [this, chk](bool checked) {
            chk->setText(checked ? "☑" : "☐");
            update_bulk_bar();
        });
        check_btns_.append(chk);
        check_ids_.append(file["id"].toString());
        hl->addWidget(chk);

        // Type badge
        QString type = file["type"].toString();
        auto* type_dot = new QLabel("[" + type.section('/', 1, 1).left(4).toUpper() + "]");
        type_dot->setFixedWidth(46);
        type_dot->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;background:transparent;%2")
                                    .arg(file_type_color(type), MF));
        hl->addWidget(type_dot);

        // Name + meta (clickable for preview)
        auto* info = new QWidget(this);
        info->setStyleSheet("background:transparent;");
        info->setCursor(Qt::PointingHandCursor);
        auto* info_vl = new QVBoxLayout(info);
        info_vl->setContentsMargins(0, 0, 0, 0);
        info_vl->setSpacing(1);

        auto* name_lbl = new QLabel(file["originalName"].toString());
        name_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;background:transparent;%2")
                                    .arg(colors::TEXT_PRIMARY(), MF));
        info_vl->addWidget(name_lbl);

        QString date_str = QDateTime::fromString(file["uploadedAt"].toString(), Qt::ISODate)
                               .toLocalTime()
                               .toString("yyyy-MM-dd HH:mm");
        auto* meta_lbl = new QLabel(format_size(file["size"].toInteger()) + "  |  " + date_str);
        meta_lbl->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2").arg(colors::TEXT_DIM(), MF));
        info_vl->addWidget(meta_lbl);

        QString fid = file["id"].toString();
        // Click card body → preview
        info->installEventFilter(this);
        connect(name_lbl, &QLabel::linkActivated, this, [this, fid]() { show_preview(fid); });
        // Use a transparent button overlay trick — simpler: connect via QPushButton
        auto* preview_btn = new QPushButton("PREVIEW");
        preview_btn->setCursor(Qt::PointingHandCursor);
        preview_btn->setFixedHeight(26);
        preview_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                           "font-size:10px;font-weight:700;padding:0 8px;%3}"
                                           "QPushButton:hover{color:%4;border-color:%4;}")
                                       .arg(colors::TEXT_DIM(), colors::BORDER_DIM(), MF, colors::AMBER()));
        connect(preview_btn, &QPushButton::clicked, this, [this, fid]() { show_preview(fid); });
        hl->addWidget(info, 1);
        hl->addWidget(preview_btn);

        // "Open With" button
        QString route = route_for_mime(type);
        if (!route.isEmpty()) {
            auto* open_btn = new QPushButton(open_with_label(type));
            open_btn->setCursor(Qt::PointingHandCursor);
            open_btn->setFixedHeight(26);
            open_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                            "font-size:10px;font-weight:700;padding:0 8px;%3}"
                                            "QPushButton:hover{color:%1;background:rgba(217,119,6,0.15);}")
                                        .arg(colors::AMBER(), colors::BORDER_DIM(), MF));
            connect(open_btn, &QPushButton::clicked, this, [this, fid]() { open_with(fid); });
            hl->addWidget(open_btn);
        }

        // Save button
        auto* dl_btn = new QPushButton("SAVE");
        dl_btn->setCursor(Qt::PointingHandCursor);
        dl_btn->setFixedSize(54, 26);
        dl_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                      "font-size:10px;font-weight:700;%3}"
                                      "QPushButton:hover{color:#38bdf8;background:%4;}")
                                  .arg(colors::CYAN(), colors::BORDER_DIM(), MF, colors::BG_RAISED()));
        connect(dl_btn, &QPushButton::clicked, this, [this, fid]() { download_file(fid); });
        hl->addWidget(dl_btn);

        // Delete button
        auto* del_btn = new QPushButton("DEL");
        del_btn->setCursor(Qt::PointingHandCursor);
        del_btn->setFixedSize(44, 26);
        del_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                       "font-size:10px;font-weight:700;%3}"
                                       "QPushButton:hover{color:%4;background:rgba(239,68,68,0.1);}")
                                   .arg(colors::TEXT_DIM(), colors::BORDER_DIM(), MF, colors::NEGATIVE()));
        connect(del_btn, &QPushButton::clicked, this, [this, fid]() { delete_file(fid); });
        hl->addWidget(del_btn);

        file_layout_->addWidget(card);
    }

    file_layout_->addStretch();
    update_bulk_bar();
}

void FileManagerScreen::update_stats() {
    QJsonArray files = FileManagerService::instance().all_files();
    qint64 total_size = 0;
    for (const auto& v : files)
        if (v.isObject())
            total_size += v.toObject()["size"].toInteger();

    stats_label_->setText(QString("%1 files | %2").arg(files.size()).arg(format_size(total_size)));

    // Quota bar
    if (quota_bar_ && quota_label_) {
        int pct = (int)(total_size * 1000 / kQuotaBytes);
        quota_bar_->setValue(qMin(pct, 1000));
        quota_label_->setText(QString("Storage: %1 / 500 MB").arg(format_size(total_size)));
        // Turn red if > 80%
        QString chunk_color = (pct > 800) ? colors::NEGATIVE() : colors::AMBER();
        quota_bar_->setStyleSheet(QString("QProgressBar{background:%1;border:none;border-radius:3px;}"
                                          "QProgressBar::chunk{background:%2;border-radius:3px;}")
                                      .arg(colors::BORDER_DIM(), chunk_color));
    }
}

// ── Static helpers ────────────────────────────────────────────────────────────

QString FileManagerScreen::format_size(qint64 bytes) {
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString FileManagerScreen::file_type_color(const QString& mime) {
    if (mime.contains("spreadsheet") || mime.contains("csv") || mime.contains("excel"))
        return colors::POSITIVE();
    if (mime.contains("image"))
        return colors::INFO();
    if (mime.contains("video"))
        return "#7c3aed";
    if (mime.contains("audio"))
        return "#db2777";
    if (mime.contains("zip") || mime.contains("archive"))
        return colors::WARNING();
    if (mime.contains("pdf"))
        return colors::NEGATIVE();
    if (mime.contains("json") || mime.contains("python") || mime.contains("ipynb"))
        return colors::CYAN();
    return colors::TEXT_SECONDARY();
}

QString FileManagerScreen::open_with_label(const QString& mime) {
    if (mime.contains("ipynb") || mime.contains("python"))
        return "NOTEBOOK";
    if (mime.contains("spreadsheet") || mime.contains("csv") || mime.contains("excel"))
        return "EXCEL";
    if (mime.contains("pdf"))
        return "REPORT";
    return "OPEN";
}

QString FileManagerScreen::route_for_mime(const QString& mime) {
    if (mime.contains("ipynb") || mime.contains("x-ipynb"))
        return "code_editor";
    if (mime.contains("spreadsheet") || mime.contains("excel") || mime.contains("csv"))
        return "excel";
    if (mime.contains("pdf"))
        return "report_builder";
    return {};
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap FileManagerScreen::save_state() const {
    return {{"screen_filter", active_screen_filter_}};
}

void FileManagerScreen::restore_state(const QVariantMap& state) {
    const QString filter = state.value("screen_filter").toString();
    if (filter != active_screen_filter_) {
        active_screen_filter_ = filter;
        // Find and check the matching chip
        if (chip_group_) {
            for (auto* btn : chip_group_->buttons()) {
                auto* chip = qobject_cast<QPushButton*>(btn);
                if (!chip)
                    continue;
                QString chip_filter = chip->text() == "All" ? "" : chip->text();
                if (chip_filter == filter) {
                    chip->setChecked(true);
                    break;
                }
            }
        }
        render_files();
    }
}

} // namespace fincept::screens
