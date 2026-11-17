// src/screens/file_manager/FileManagerScreen.cpp
#include "screens/file_manager/FileManagerScreen.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/common/CommandRows.h"
#include "screens/excel/ExcelScreen.h"
#include "services/report_builder/ReportBuilderService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QTableWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
using fincept::services::FileManagerService;

namespace {

enum FmColumn { kFmName, kFmType, kFmSource, kFmSize, kFmAdded, kFmColumnCount };
constexpr int kFmIdRole = Qt::UserRole;       // column 0: file id
constexpr int kFmSortRole = Qt::UserRole + 1; // numeric / ISO sort key
constexpr int kFmMimeRole = Qt::UserRole + 2; // column 1: full mime type

// Every sourceScreen tag a producer registers has a chip.
const char* const kFmScreens[] = {"portfolio",    "backtesting", "news",          "equity_research", "algo_trading",
                                  "ai_quant_lab", "notes",       "code_editor",   "report_builder",  "data_sources",
                                  "excel",        "workflow_node", "file_manager"};

QString fm_screen_label(const QString& key) {
    return key.isEmpty() ? QCoreApplication::translate("FileManagerScreen", "UPLOADED")
                         : QString(key).replace(QLatin1Char('_'), QLatin1Char(' ')).toUpper();
}

// RFC-4180 CSV (quoted fields, "" escapes, line breaks inside quotes).
QVector<QStringList> fm_parse_csv(const QString& text, int max_rows) {
    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool in_quotes = false;
    for (int i = 0; i < text.size() && rows.size() < max_rows; ++i) {
        const QChar c = text[i];
        if (in_quotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) {
                    field += c;
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += c;
            }
        } else if (c == QLatin1Char('"')) {
            in_quotes = true;
        } else if (c == QLatin1Char(',')) {
            row << field;
            field.clear();
        } else if (c == QLatin1Char('\n')) {
            row << field;
            rows << row;
            row.clear();
            field.clear();
        } else if (c != QLatin1Char('\r')) {
            field += c;
        }
    }
    if ((!field.isEmpty() || !row.isEmpty()) && rows.size() < max_rows) {
        row << field;
        rows << row;
    }
    return rows;
}

} // namespace

/// Search text + source filter over the file table.
class FileManagerFilterProxy : public QSortFilterProxyModel {
  public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void set_filters(const QString& needle, const QString& screen) {
        needle_ = needle.trimmed();
        screen_ = screen;
        invalidateFilter();
    }

  protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        const auto* m = sourceModel();
        if (!screen_.isEmpty() && m->index(row, kFmSource, parent).data(kFmSortRole).toString() != screen_)
            return false;
        if (needle_.isEmpty())
            return true;
        for (int c : {int(kFmName), int(kFmType), int(kFmSource)})
            if (m->index(row, c, parent).data().toString().contains(needle_, Qt::CaseInsensitive))
                return true;
        return m->index(row, kFmType, parent).data(kFmMimeRole).toString().contains(needle_, Qt::CaseInsensitive);
    }

  private:
    QString needle_;
    QString screen_;
};

// ── Constructor ───────────────────────────────────────────────────────────────

FileManagerScreen::FileManagerScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect(&FileManagerService::instance(), &FileManagerService::files_changed, this, [this]() {
        if (loaded_)
            reload_model();
    });
}

// ── UI ────────────────────────────────────────────────────────────────────────

void FileManagerScreen::build_ui() {
    setObjectName("FMRoot");
    // ONE stylesheet for the whole screen, keyed on object names.
    setStyleSheet(
        QString("QWidget#FMRoot { background:%1; }"
                "#fmHeader { background:%2; border-bottom:1px solid %3; }"
                "#fmTitle { color:%4; font-size:" + fincept::ui::fonts::px(0) + "px; font-weight:700; letter-spacing:0.5px; background:transparent; }"
                "#fmSubtitle, #fmMuted { color:%7; font-size:" + fincept::ui::fonts::px(-2) + "px; background:transparent; }"
                "#fmStats { color:%7; font-size:" + fincept::ui::fonts::px(-3) + "px; background:transparent; }"
                "#fmQuotaLabel { color:%6; font-size:" + fincept::ui::fonts::px(-3) + "px; background:transparent; }"
                "#fmBtn { background:transparent; color:%7; border:1px solid %3; padding:4px 12px; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "#fmBtn:hover { color:%8; background:%9; }"
                "#fmPrimaryBtn { background:%13; color:%4; border:1px solid %10; padding:4px 12px;"
                "  font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; }"
                "#fmPrimaryBtn:hover { background:%4; color:%14; }"
                "#fmDangerBtn { background:transparent; color:%11; border:1px solid %15; padding:3px 12px;"
                "  font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; }"
                "#fmDangerBtn:hover { background:%11; color:%14; }"
                "#fmBar { background:%1; border-bottom:1px solid %3; }"
                "#fmBulkBar { background:%13; border-bottom:1px solid %10; }"
                "#fmSearch { background:%12; color:%8; border:1px solid %3; padding:5px 10px; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                "#fmSearch:focus { border-color:%4; }"
                "#fmChip { background:transparent; color:%7; border:1px solid %3; border-radius:10px; padding:2px 10px;"
                "  font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                "#fmChip:checked { background:%4; color:%14; border-color:%4; }"
                "#fmChip:hover:!checked { border-color:%4; }"
                "QProgressBar#fmQuota { background:%3; border:none; border-radius:3px; }"
                "QProgressBar#fmQuota::chunk { background:%4; border-radius:3px; }"
                "QProgressBar#fmQuota[over=\"true\"]::chunk { background:%11; }"
                "QTableView#fmTable { background:%1; color:%8; border:none; gridline-color:%3; font-size:" + fincept::ui::fonts::px(-2) + "px;"
                "  selection-background-color:%16; selection-color:%8; }"
                "QTableView#fmTable QHeaderView::section { background:%2; color:%7; border:none;"
                "  border-bottom:1px solid %3; border-right:1px solid %3; padding:4px 6px; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                "  font-weight:700; }"
                "#fmEmpty { color:%7; font-size:" + fincept::ui::fonts::px(-1) + "px; background:transparent; padding:40px; }"
                "#fmPreviewPanel { background:%2; border-left:1px solid %5; }"
                "#fmPreviewHeader { background:%12; border-bottom:1px solid %3; }"
                "#fmPreviewTitle { color:%8; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:600; background:transparent;"
                "  padding:10px 12px 2px 12px; }"
                "#fmPreviewMeta { color:%7; font-size:" + fincept::ui::fonts::px(-4) + "px; background:transparent; padding:0 12px 8px 12px; }"
                "#fmPreviewHeading { color:%4; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; letter-spacing:1px;"
                "  background:transparent; }"
                "#fmClose { background:transparent; color:%7; border:none; font-size:" + fincept::ui::fonts::px(-2) + "px; }"
                "#fmClose:hover { color:%11; }"
                "QTextEdit#fmPreviewText { background:%1; color:%8; border:none; padding:10px; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "QTableWidget#fmPreviewTable { background:%1; color:%8; border:none; gridline-color:%3;"
                "  font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "QTableWidget#fmPreviewTable QHeaderView::section { background:%12; color:%7; border:none;"
                "  border-bottom:1px solid %3; padding:4px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                "QSplitter::handle { background:%5; width:1px; }")
            .arg(colors::BG_BASE(), colors::BG_RAISED(), colors::BORDER_DIM(), colors::AMBER(),
                 colors::BORDER_MED(), colors::TEXT_TERTIARY(), colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(),
                 colors::BG_HOVER())
            .arg(colors::AMBER_DIM(), colors::DANGER(), colors::BG_SURFACE(), colors::ACCENT_BG(),
                 colors::TEXT_ON_ACCENT(), colors::alpha(colors::DANGER, 0.45), colors::alpha(colors::AMBER, 0.18)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("fmHeader");
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(14, 10, 14, 10);
    hhl->setSpacing(8);
    title_label_ = new QLabel(header);
    title_label_->setObjectName("fmTitle");
    hhl->addWidget(title_label_);
    subtitle_label_ = new QLabel(header);
    subtitle_label_->setObjectName("fmSubtitle");
    hhl->addWidget(subtitle_label_);
    hhl->addStretch();
    stats_label_ = new QLabel(header);
    stats_label_->setObjectName("fmStats");
    hhl->addWidget(stats_label_);
    refresh_btn_ = new QPushButton(header);
    refresh_btn_->setObjectName("fmBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    // No F5 shortcut here — KeyConfigManager owns F5 as the global Refresh
    // action (Qt::ApplicationShortcut); a second binding makes both ambiguous.
    connect(refresh_btn_, &QPushButton::clicked, this, &FileManagerScreen::reload_model);
    hhl->addWidget(refresh_btn_);
    upload_btn_ = new QPushButton(header);
    upload_btn_->setObjectName("fmPrimaryBtn");
    upload_btn_->setCursor(Qt::PointingHandCursor);
    connect(upload_btn_, &QPushButton::clicked, this, &FileManagerScreen::upload_files);
    hhl->addWidget(upload_btn_);
    root->addWidget(header);

    // ── Storage bar ──────────────────────────────────────────────────────────
    auto* qw = new QWidget(this);
    qw->setObjectName("fmBar");
    qw->setAttribute(Qt::WA_StyledBackground, true);
    auto* ql = new QHBoxLayout(qw);
    ql->setContentsMargins(14, 6, 14, 6);
    ql->setSpacing(10);
    quota_label_ = new QLabel(qw);
    quota_label_->setObjectName("fmQuotaLabel");
    ql->addWidget(quota_label_);
    quota_bar_ = new QProgressBar(qw);
    quota_bar_->setObjectName("fmQuota");
    quota_bar_->setRange(0, 1000);
    quota_bar_->setTextVisible(false);
    quota_bar_->setFixedHeight(6);
    ql->addWidget(quota_bar_, 1);
    root->addWidget(qw);

    build_filter_bar(root);

    // ── Bulk action bar (shown while several files are selected) ─────────────
    bulk_bar_ = new QWidget(this);
    bulk_bar_->setObjectName("fmBulkBar");
    bulk_bar_->setAttribute(Qt::WA_StyledBackground, true);
    bulk_bar_->setVisible(false);
    auto* bbl = new QHBoxLayout(bulk_bar_);
    bbl->setContentsMargins(14, 6, 14, 6);
    bbl->setSpacing(10);
    bulk_sel_label_ = new QLabel(bulk_bar_);
    bulk_sel_label_->setObjectName("fmStats");
    bbl->addWidget(bulk_sel_label_);
    bulk_delete_btn_ = new QPushButton(bulk_bar_);
    bulk_delete_btn_->setObjectName("fmDangerBtn");
    bulk_delete_btn_->setProperty("fincept_agent_risk", "delete");
    bulk_delete_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_delete_btn_, &QPushButton::clicked, this, [this]() { delete_files(selected_ids()); });
    bbl->addWidget(bulk_delete_btn_);
    bbl->addStretch();
    bulk_clear_btn_ = new QPushButton(bulk_bar_);
    bulk_clear_btn_->setObjectName("fmBtn");
    bulk_clear_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_clear_btn_, &QPushButton::clicked, this, [this]() { table_->clearSelection(); });
    bbl->addWidget(bulk_clear_btn_);
    root->addWidget(bulk_bar_);

    // ── Table | preview ──────────────────────────────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);

    auto* list_host = new QWidget(splitter_);
    auto* lvl = new QVBoxLayout(list_host);
    lvl->setContentsMargins(0, 0, 0, 0);
    lvl->setSpacing(0);

    model_ = new QStandardItemModel(0, kFmColumnCount, this);
    proxy_ = new FileManagerFilterProxy(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(kFmSortRole);
    proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);

    table_ = new QTableView(list_host);
    table_->setObjectName("fmTable");
    table_->setModel(proxy_);
    table_->setSortingEnabled(true);
    table_->sortByColumn(kFmAdded, Qt::DescendingOrder);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->horizontalHeader()->setSectionResizeMode(kFmName, QHeaderView::Stretch);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->setColumnWidth(kFmType, 90);
    table_->setColumnWidth(kFmSource, 130);
    table_->setColumnWidth(kFmSize, 90);
    table_->setColumnWidth(kFmAdded, 140);
    table_->setAccessibleName(tr("Managed files"));
    connect(table_, &QTableView::doubleClicked, this, &FileManagerScreen::activate);
    connect(table_, &QTableView::customContextMenuRequested, this, &FileManagerScreen::show_context_menu);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        update_bulk_bar();
        const QStringList ids = selected_ids();
        if (ids.size() == 1)
            show_preview(ids.first());
    });
    lvl->addWidget(table_, 1);

    // No files / nothing matching: a designed state with the action that fills the list.
    list_state_ = new ui::EmptyState(list_host);
    list_state_->setVisible(false);
    connect(list_state_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (model_->rowCount() == 0) {
            upload_files();
            return;
        }
        // Filtered out: clear the search and go back to every source.
        search_input_->clear();
        if (chip_group_)
            for (auto* b : chip_group_->buttons())
                if (b->property("screenFilter").toString().isEmpty())
                    b->setChecked(true); // toggled() re-applies the filter
        apply_filter();
    });
    lvl->addWidget(list_state_, 1);

    // Keyboard: Enter opens, Delete deletes the selection, F2 renames.
    auto add_key = [this](const QKeySequence& seq, std::function<void()> fn) {
        auto* sc = new QShortcut(seq, table_);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, std::move(fn));
        return sc;
    };
    add_key(QKeySequence(Qt::Key_Return), [this]() { activate(table_->currentIndex()); });
    add_key(QKeySequence(Qt::Key_Enter), [this]() { activate(table_->currentIndex()); });
    // Agents: Delete deletes the selected files — on the shortcut itself (judged when it
    // fires) and as the table's key map (judged for a key press on the focused table).
    QShortcut* delete_key = add_key(QKeySequence(QKeySequence::Delete), [this]() { delete_files(selected_ids()); });
    delete_key->setProperty(fincept::core::agent::kAgentRiskProperty, "delete");
    table_->setProperty(fincept::core::agent::kAgentRiskKeysProperty,
                        QKeySequence(QKeySequence::Delete).toString(QKeySequence::PortableText) +
                            QStringLiteral("=delete"));
    add_key(QKeySequence(Qt::Key_F2), [this]() {
        if (!current_id().isEmpty())
            rename_file(current_id());
    });

    splitter_->addWidget(list_host);
    splitter_->addWidget(build_preview_panel());
    preview_panel_->setVisible(false);
    root->addWidget(splitter_, 1);

    // Row height (24 px) and the preview close button (20 px) were sized for the
    // default font; follow the user's font size instead of clipping at large ones.
    auto apply_metrics = [this]() {
        const QFontMetrics row_fm(fonts::font(-2));
        table_->verticalHeader()->setDefaultSectionSize(std::max(24, row_fm.height() + 8));
        if (auto* close_btn = preview_panel_->findChild<QPushButton*>(QStringLiteral("fmClose"))) {
            const int side = std::max(20, row_fm.height() + 4);
            close_btn->setFixedSize(side, side);
        }
    };
    apply_metrics();
    // Sizes are re-fitted once the new font has settled (never inside theme_changed).
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, apply_metrics);

    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    search_debounce_->setInterval(150);
    connect(search_debounce_, &QTimer::timeout, this, &FileManagerScreen::apply_filter);

    retranslateUi();
}

void FileManagerScreen::build_filter_bar(QVBoxLayout* root) {
    auto* bar = new QWidget(this);
    bar->setObjectName("fmBar");
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* bl = new QVBoxLayout(bar);
    bl->setContentsMargins(14, 8, 14, 8);
    bl->setSpacing(6);

    search_input_ = new QLineEdit(bar);
    search_input_->setObjectName("fmSearch");
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::textChanged, this, [this]() { search_debounce_->start(); });
    bl->addWidget(search_input_);

    auto* chips = new QWidget(bar);
    auto* cl = new QHBoxLayout(chips);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(4);
    chip_group_ = new QButtonGroup(this);
    chip_group_->setExclusive(true);
    auto add_chip = [&](const QString& key) {
        auto* chip = new QPushButton(key.isEmpty() ? tr("All") : fm_screen_label(key), chips);
        chip->setObjectName("fmChip");
        chip->setCheckable(true);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setProperty("screenFilter", key);
        chip->setChecked(key.isEmpty());
        chip_group_->addButton(chip);
        cl->addWidget(chip);
        connect(chip, &QPushButton::toggled, this, [this, key](bool checked) {
            if (!checked)
                return;
            active_screen_filter_ = key;
            apply_filter();
            ScreenStateManager::instance().notify_changed(this);
        });
    };
    add_chip(QString());
    for (const char* s : kFmScreens)
        add_chip(QString::fromLatin1(s));
    cl->addStretch();
    bl->addWidget(chips);
    root->addWidget(bar);
}

QWidget* FileManagerScreen::build_preview_panel() {
    preview_panel_ = new QWidget(this);
    preview_panel_->setObjectName("fmPreviewPanel");
    preview_panel_->setAttribute(Qt::WA_StyledBackground, true);
    auto* vl = new QVBoxLayout(preview_panel_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(preview_panel_);
    hdr->setObjectName("fmPreviewHeader");
    hdr->setAttribute(Qt::WA_StyledBackground, true);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 8, 12, 8);
    preview_header_label_ = new QLabel(hdr);
    preview_header_label_->setObjectName("fmPreviewHeading");
    hhl->addWidget(preview_header_label_);
    hhl->addStretch();
    auto* close_btn = new QPushButton(QStringLiteral("✕"), hdr);
    close_btn->setObjectName("fmClose"); // sized for the font in build_ui()
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setAccessibleName(tr("Close preview"));
    connect(close_btn, &QPushButton::clicked, this, &FileManagerScreen::clear_preview);
    hhl->addWidget(close_btn);
    vl->addWidget(hdr);

    preview_title_ = new QLabel(preview_panel_);
    preview_title_->setObjectName("fmPreviewTitle");
    preview_title_->setWordWrap(true);
    vl->addWidget(preview_title_);
    preview_meta_ = new QLabel(preview_panel_);
    preview_meta_->setObjectName("fmPreviewMeta");
    preview_meta_->setWordWrap(true);
    vl->addWidget(preview_meta_);

    preview_text_ = new QTextEdit(preview_panel_);
    preview_text_->setObjectName("fmPreviewText");
    preview_text_->setReadOnly(true);
    preview_text_->setVisible(false);
    vl->addWidget(preview_text_, 1);

    preview_table_ = new QTableWidget(preview_panel_);
    preview_table_->setObjectName("fmPreviewTable");
    preview_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    preview_table_->horizontalHeader()->setStretchLastSection(true);
    preview_table_->verticalHeader()->setVisible(false);
    preview_table_->setVisible(false);
    vl->addWidget(preview_table_, 1);

    // Binary / unreadable / empty file: say so and offer what does open it.
    preview_state_ = new ui::EmptyState(preview_panel_);
    preview_state_->set_compact(true);
    preview_state_->setVisible(false);
    connect(preview_state_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (preview_id_.isEmpty())
            return;
        if (preview_note_ == PreviewNote::Unreadable)
            show_preview(preview_id_); // try again
        else if (!preview_route_.isEmpty())
            open_with(preview_id_);
        else
            open_external(preview_id_);
    });
    connect(preview_state_, &ui::EmptyState::secondary_clicked, this, [this]() {
        if (preview_note_ == PreviewNote::Unreadable)
            QDesktopServices::openUrl(QUrl::fromLocalFile(FileManagerService::instance().storage_dir()));
        else if (!preview_id_.isEmpty())
            open_external(preview_id_);
    });
    vl->addWidget(preview_state_, 1);
    return preview_panel_;
}

// ── Visibility ────────────────────────────────────────────────────────────────

void FileManagerScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!loaded_) {
        loaded_ = true;
        reload_model();
    }
}

// ── Model ─────────────────────────────────────────────────────────────────────

void FileManagerScreen::reload_model() {
    // Keep the user's selection across a rebuild (files_changed fires on every
    // import/delete anywhere in the terminal).
    const QStringList keep = selected_ids();
    const QJsonArray all = FileManagerService::instance().all_files();

    model_->removeRows(0, model_->rowCount());
    model_->setRowCount(static_cast<int>(all.size()));
    int row = 0;
    for (const auto& v : all) {
        if (!v.isObject())
            continue;
        const QJsonObject f = v.toObject();
        const QString id = f["id"].toString();
        const QString mime = f["type"].toString();
        const QString source = f["sourceScreen"].toString();
        const qint64 size = f["size"].toInteger();
        const QDateTime added = QDateTime::fromString(f["uploadedAt"].toString(), Qt::ISODate).toLocalTime();
        const QString name = f["originalName"].toString();

        auto* name_item = new QStandardItem(name);
        name_item->setData(id, kFmIdRole);
        name_item->setData(name.toLower(), kFmSortRole);
        name_item->setToolTip(name);
        auto* type_item = new QStandardItem(QFileInfo(name).suffix().toUpper());
        type_item->setData(type_item->text(), kFmSortRole);
        type_item->setData(mime, kFmMimeRole);
        type_item->setToolTip(mime);
        auto* source_item = new QStandardItem(fm_screen_label(source));
        source_item->setData(source, kFmSortRole);
        auto* size_item = new QStandardItem(format_size(size));
        size_item->setData(size, kFmSortRole);
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* added_item = new QStandardItem(added.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        added_item->setData(added.toMSecsSinceEpoch(), kFmSortRole);
        model_->setItem(row, kFmName, name_item);
        model_->setItem(row, kFmType, type_item);
        model_->setItem(row, kFmSource, source_item);
        model_->setItem(row, kFmSize, size_item);
        model_->setItem(row, kFmAdded, added_item);
        ++row;
    }
    model_->setRowCount(row);

    if (!keep.isEmpty()) {
        for (int r = 0; r < proxy_->rowCount(); ++r) {
            const QModelIndex idx = proxy_->index(r, 0);
            if (keep.contains(idx.data(kFmIdRole).toString()))
                table_->selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
    // The previewed file may have been deleted or renamed elsewhere.
    if (!preview_id_.isEmpty()) {
        if (FileManagerService::instance().find_by_id(preview_id_).id.isEmpty())
            clear_preview();
        else
            show_preview(preview_id_);
    }
    apply_filter();
    update_stats();

    // First fill: preview the newest file (the table is sorted by Added, newest first),
    // so the right side shows a file rather than nothing until a row is clicked.
    if (!first_fill_done_) {
        first_fill_done_ = true;
        if (proxy_->rowCount() > 0 && selected_ids().isEmpty() && preview_id_.isEmpty())
            table_->selectRow(0); // selectionChanged -> show_preview
    }
}

void FileManagerScreen::apply_filter() {
    proxy_->set_filters(search_input_ ? search_input_->text() : QString(), active_screen_filter_);
    const bool none = model_->rowCount() == 0;
    const bool filtered_out = !none && proxy_->rowCount() == 0;
    update_list_state(none, filtered_out);
    table_->setVisible(!(none || filtered_out));
    update_bulk_bar();
}

void FileManagerScreen::update_list_state(bool none, bool filtered_out) {
    if (!none && !filtered_out) {
        list_state_->setVisible(false);
        return;
    }
    ui::EmptyState::Spec spec;
    if (none) {
        spec = ui::EmptyState::no_data_spec(
            tr("No files yet"),
            tr("Files are registered here when you export, save or generate output in a screen. UPLOAD FILES "
               "copies your own files in."),
            tr("UPLOAD FILES"));
        spec.glyph = QStringLiteral("▤");
        spec.primary_tooltip = tr("Copy files from your computer into terminal storage");
        spec.details = ui::EmptyState::bullet_rows({tr("Code: notebooks you save"),
                                                    tr("Excel: workbooks you save or export"),
                                                    tr("Report Builder: saved reports"),
                                                    tr("Backtesting and Data Sources: exported results and configs")});
        spec.details_heading = tr("WHERE FILES COME FROM");
    } else {
        const QString needle = search_input_ ? search_input_->text().trimmed() : QString();
        const QString source =
            active_screen_filter_.isEmpty() ? tr("all sources") : fm_screen_label(active_screen_filter_);
        spec = ui::EmptyState::no_data_spec(
            tr("No files match"),
            needle.isEmpty() ? tr("No file from %1 is stored yet.").arg(source)
                             : tr("Nothing named or typed like \"%1\" in %2.").arg(needle, source),
            tr("SHOW ALL FILES"));
        spec.glyph = QStringLiteral("⌕");
    }
    list_state_->apply(spec);
    list_state_->setVisible(true);
}

QStringList FileManagerScreen::selected_ids() const {
    QStringList ids;
    if (!table_ || !table_->selectionModel())
        return ids;
    const auto rows = table_->selectionModel()->selectedRows(kFmName);
    for (const QModelIndex& idx : rows)
        ids << idx.data(kFmIdRole).toString();
    return ids;
}

QString FileManagerScreen::current_id() const {
    const QModelIndex idx = table_->currentIndex();
    return idx.isValid() ? idx.sibling(idx.row(), kFmName).data(kFmIdRole).toString() : QString();
}

void FileManagerScreen::activate(const QModelIndex& index) {
    if (!index.isValid())
        return;
    const QString id = index.sibling(index.row(), kFmName).data(kFmIdRole).toString();
    const auto f = FileManagerService::instance().find_by_id(id);
    if (f.id.isEmpty())
        return;
    if (!route_for_mime(f.mime_type, f.original_name).isEmpty())
        open_with(id);
    else
        show_preview(id);
}

void FileManagerScreen::show_context_menu(const QPoint& pos) {
    const QModelIndex idx = table_->indexAt(pos);
    if (!idx.isValid())
        return;
    if (!table_->selectionModel()->isRowSelected(idx.row(), idx.parent()))
        table_->selectRow(idx.row());
    const QStringList ids = selected_ids();
    const QString id = idx.sibling(idx.row(), kFmName).data(kFmIdRole).toString();
    const auto f = FileManagerService::instance().find_by_id(id);

    QMenu menu(this);
    if (ids.size() <= 1) {
        const QString route = route_for_mime(f.mime_type, f.original_name);
        if (!route.isEmpty())
            menu.addAction(tr("Open in %1").arg(open_with_label(f.mime_type, f.original_name)), this, [this, id]() { open_with(id); });
        menu.addAction(tr("Preview"), this, [this, id]() { show_preview(id); });
        menu.addAction(tr("Open with Default App"), this, [this, id]() { open_external(id); });
        menu.actions().constLast()->setProperty("fincept_agent_risk", "code"); // launches an outside program
        menu.addAction(tr("Show in Folder"), this, []() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(FileManagerService::instance().storage_dir()));
        });
        menu.addSeparator();
        menu.addAction(tr("Rename…  (F2)"), this, [this, id]() { rename_file(id); });
        menu.addAction(tr("Save a Copy…"), this, [this, id]() { download_file(id); });
        menu.addSeparator();
    }
    menu.addAction(ids.size() > 1 ? tr("Delete %1 Files  (Del)").arg(ids.size()) : tr("Delete  (Del)"), this,
                   [this, ids]() { delete_files(ids); });
    menu.actions().constLast()->setProperty("fincept_agent_risk", "delete");
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

// ── File operations ───────────────────────────────────────────────────────────

void FileManagerScreen::upload_files() {
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Select Files to Upload"));
    if (paths.isEmpty())
        return;
    QStringList failed;
    for (const QString& path : paths) {
        // Tagged so manual uploads are filterable.
        if (FileManagerService::instance().import_file(path, QStringLiteral("file_manager")).isEmpty())
            failed << QFileInfo(path).fileName();
    }
    if (!failed.isEmpty()) {
        LOG_WARN("FileManager", QString("Upload failed for %1 file(s)").arg(failed.size()));
        QMessageBox::warning(this, tr("Upload Files"),
                             tr("%n file(s) could not be copied into storage:\n%1", "", static_cast<int>(failed.size()))
                                 .arg(failed.join('\n')));
    }
}

void FileManagerScreen::download_file(const QString& file_id) {
    const auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;
    const QString dest = QFileDialog::getSaveFileName(this, tr("Save File As"), f.original_name);
    if (dest.isEmpty())
        return;
    const QString src = FileManagerService::instance().full_path(f.name);
    if (!QFile::exists(src)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("The stored file is missing from disk:\n%1\n\n"
                                "Its index entry is stale — delete it and re-import.")
                                 .arg(src));
        return;
    }
    // Write to a sibling temp file first so a failure can't destroy the target.
    const QString staging = dest + QStringLiteral(".fincept-partial");
    QFile::remove(staging);
    if (!QFile::copy(src, staging)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not write to:\n%1\n\nCheck that the folder exists and is writable.").arg(dest));
        return;
    }
    if (QFile::exists(dest) && !QFile::remove(dest)) {
        QFile::remove(staging);
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not replace the existing file:\n%1\n\nIt may be open in another program.")
                                 .arg(dest));
        return;
    }
    if (!QFile::rename(staging, dest)) {
        QFile::remove(staging);
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not finish writing:\n%1").arg(dest));
        return;
    }
    LOG_INFO("FileManager", "Saved a copy of: " + f.original_name);
}

void FileManagerScreen::delete_files(const QStringList& ids) {
    if (ids.isEmpty())
        return;
    QStringList names;
    for (const QString& id : ids) {
        const auto f = FileManagerService::instance().find_by_id(id);
        if (!f.original_name.isEmpty() && names.size() < 12)
            names << f.original_name;
    }
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Delete Files"));
    box.setText(ids.size() == 1 ? tr("Delete \"%1\"?").arg(names.value(0))
                                : tr("Delete %n selected file(s)?", "", static_cast<int>(ids.size())));
    box.setInformativeText(tr("This cannot be undone."));
    if (ids.size() > 1)
        box.setDetailedText(names.join('\n') +
                            (ids.size() > names.size()
                                 ? tr("\n… and %n more", "", static_cast<int>(ids.size() - names.size()))
                                 : QString()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    int failed = 0;
    for (const QString& id : ids)
        if (!FileManagerService::instance().remove_file(id))
            ++failed;
    if (failed > 0)
        QMessageBox::warning(this, tr("Delete Files"),
                             tr("%n file(s) could not be deleted — they may be open in another program.", "", failed));
}

void FileManagerScreen::rename_file(const QString& file_id) {
    const auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename File"), tr("New name:"), QLineEdit::Normal,
                                               f.original_name, &ok)
                             .trimmed();
    if (!ok || name.isEmpty() || name == f.original_name)
        return;
    if (!FileManagerService::instance().rename_file(file_id, name))
        QMessageBox::warning(this, tr("Rename File"), tr("\"%1\" is not a valid file name.").arg(name));
}

void FileManagerScreen::open_with(const QString& file_id) {
    const auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;
    const QString route = route_for_mime(f.mime_type, f.original_name);
    if (route.isEmpty())
        return;
    const QString path = FileManagerService::instance().full_path(f.name);
    // Park the path BEFORE the shell switches tabs: the target screen takes it when it
    // shows (it may not exist yet). The shell's open_file_in_screen handler only forwards
    // the path for the notebook editor.
    if (route == QLatin1String("excel") && !path.isEmpty())
        ExcelScreen::open_when_ready(path);
    if (route == QLatin1String("report_builder") && !path.isEmpty()) {
        services::ReportBuilderService::OpenRequest req;
        req.file_path = path;
        services::ReportBuilderService::instance().request_open(req);
    }
    emit open_file_in_screen(route, path);
}

void FileManagerScreen::open_external(const QString& file_id) {
    const auto f = FileManagerService::instance().find_by_id(file_id);
    const QString path = f.id.isEmpty() ? QString() : FileManagerService::instance().full_path(f.name);
    if (path.isEmpty() || !QFile::exists(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        QMessageBox::warning(this, tr("Open File"), tr("No application could open this file."));
}

// ── Preview ───────────────────────────────────────────────────────────────────

void FileManagerScreen::show_preview(const QString& file_id) {
    const auto f = FileManagerService::instance().find_by_id(file_id);
    if (f.id.isEmpty())
        return;
    preview_id_ = file_id;
    if (!preview_panel_->isVisible()) {
        preview_panel_->setVisible(true);
        splitter_->setSizes({550, 350});
    }

    preview_title_->setText(f.original_name);
    const QString date_str =
        QDateTime::fromString(f.uploaded_at, Qt::ISODate).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    preview_meta_->setText(format_size(f.size) + "  |  " + f.mime_type + "  |  " + fm_screen_label(f.source_screen) +
                           "  |  " + date_str);
    preview_text_->setVisible(false);
    preview_table_->setVisible(false);
    preview_state_->setVisible(false);

    if (f.mime_type.contains("image") || f.mime_type.contains("video") || f.mime_type.contains("audio") ||
        f.mime_type.contains("zip") || f.mime_type.contains("spreadsheet") || f.mime_type.contains("excel") ||
        f.mime_type.contains("vnd.openxmlformats") || f.mime_type.contains("pdf")) {
        show_preview_note(PreviewNote::Binary, route_for_mime(f.mime_type, f.original_name));
        return;
    }

    QFile file(FileManagerService::instance().full_path(f.name));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        show_preview_note(PreviewNote::Unreadable, QString());
        return;
    }
    constexpr int kMaxChars = 64000;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.read(kMaxChars);
    const bool truncated = !in.atEnd();
    file.close();

    if (f.mime_type.contains("csv") || f.original_name.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        constexpr int kMaxRows = 201;
        const QVector<QStringList> rows = fm_parse_csv(content, kMaxRows);
        if (rows.isEmpty()) {
            show_preview_note(PreviewNote::EmptyFile, route_for_mime(f.mime_type, f.original_name));
            return;
        }
        const QStringList& headers = rows.first();
        preview_table_->clear();
        preview_table_->setColumnCount(static_cast<int>(headers.size()));
        preview_table_->setHorizontalHeaderLabels(headers);
        preview_table_->setRowCount(static_cast<int>(rows.size()) - 1);
        for (int r = 1; r < rows.size(); ++r)
            for (int c = 0; c < headers.size(); ++c)
                preview_table_->setItem(r - 1, c, new QTableWidgetItem(rows[r].value(c)));
        preview_table_->resizeColumnsToContents();
        preview_table_->setVisible(true);
        return;
    }
    if (f.mime_type.contains("json") || f.original_name.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
        const auto doc = QJsonDocument::fromJson(content.toUtf8());
        if (!doc.isNull())
            content = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    }
    preview_text_->setPlainText(content);
    if (truncated)
        preview_text_->append(QStringLiteral("\n\n") + tr("[... preview truncated at 64K characters ...]"));
    preview_text_->setVisible(true);
}

void FileManagerScreen::show_preview_note(PreviewNote note, const QString& route) {
    preview_note_ = note;
    preview_route_ = route;
    const auto f = FileManagerService::instance().find_by_id(preview_id_);
    const QString app = open_with_label(f.mime_type, f.original_name);
    ui::EmptyState::Spec spec;
    switch (note) {
    case PreviewNote::Binary:
        spec = ui::EmptyState::no_data_spec(
            tr("No text preview"),
            route.isEmpty() ? tr("This is a binary file (image, PDF, archive or office document). Your default "
                                 "app for it can open it.")
                            : tr("This file has no text preview here — it opens in %1.").arg(app),
            route.isEmpty() ? tr("OPEN WITH DEFAULT APP") : tr("OPEN IN %1").arg(app));
        // Agents: launching an outside program is "code" (as the menu's Open with Default App).
        if (route.isEmpty())
            spec.primary_agent_risk = QStringLiteral("code");
        if (!route.isEmpty()) {
            spec.secondary = tr("OPEN WITH DEFAULT APP");
            spec.secondary_agent_risk = QStringLiteral("code");
        }
        spec.glyph = QStringLiteral("◫");
        break;
    case PreviewNote::Unreadable:
        spec = ui::EmptyState::error_spec(tr("The stored file could not be read — it may have been moved or deleted."),
                                          tr("Cannot preview this file"), tr("TRY AGAIN"));
        spec.secondary = tr("SHOW IN FOLDER");
        break;
    case PreviewNote::EmptyFile:
        spec = ui::EmptyState::no_data_spec(tr("Empty file"), tr("This file has no rows to show."),
                                            route.isEmpty() ? QString() : tr("OPEN IN %1").arg(app));
        spec.glyph = QStringLiteral("○");
        break;
    }
    preview_state_->apply(spec);
    preview_state_->setVisible(true);
}

void FileManagerScreen::clear_preview() {
    preview_id_.clear();
    preview_text_->clear();
    preview_table_->clear();
    preview_table_->setRowCount(0);
    preview_title_->clear();
    preview_meta_->clear();
    preview_panel_->setVisible(false);
}

// ── Stats / bulk bar ──────────────────────────────────────────────────────────

void FileManagerScreen::update_stats() {
    const QJsonArray files = FileManagerService::instance().all_files();
    qint64 total = 0;
    for (const auto& v : files)
        if (v.isObject())
            total += v.toObject()["size"].toInteger();
    stats_label_->setText(tr("%1 files | %2").arg(files.size()).arg(format_size(total)));

    const int permille = static_cast<int>(std::min<qint64>(total * 1000 / kQuotaBytes, 1000));
    quota_bar_->setValue(permille);
    // The bar is a soft budget, not an enforced quota — say so.
    quota_label_->setText(tr("Storage used: %1 of a %2 budget").arg(format_size(total), format_size(kQuotaBytes)));
    quota_bar_->setToolTip(tr("A guideline only — the terminal never deletes your files. Remove what you no longer "
                              "need to keep backups small."));
    const bool over = permille > 800;
    if (quota_bar_->property("over").toBool() != over) {
        quota_bar_->setProperty("over", over);
        quota_bar_->style()->unpolish(quota_bar_);
        quota_bar_->style()->polish(quota_bar_);
    }
}

void FileManagerScreen::update_bulk_bar() {
    const int selected = static_cast<int>(selected_ids().size());
    bulk_bar_->setVisible(selected > 1);
    if (selected > 1) {
        bulk_sel_label_->setText(tr("%1 files selected").arg(selected));
        bulk_delete_btn_->setText(tr("DELETE %1 SELECTED").arg(selected));
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

QString FileManagerScreen::open_with_label(const QString& mime, const QString& name) {
    if (name.endsWith(QLatin1String(".fincept"), Qt::CaseInsensitive))
        return QCoreApplication::translate("FileManagerScreen", "REPORT BUILDER");
    if (mime.contains("ipynb") || mime.contains("python"))
        return QCoreApplication::translate("FileManagerScreen", "NOTEBOOK");
    if (mime.contains("spreadsheet") || mime.contains("csv") || mime.contains("excel"))
        return QCoreApplication::translate("FileManagerScreen", "EXCEL");
    return QCoreApplication::translate("FileManagerScreen", "OPEN");
}

QString FileManagerScreen::route_for_mime(const QString& mime, const QString& name) {
    // Report Builder documents are JSON (mime application/json or text/plain), so they
    // are recognised by extension. PDFs are NOT routed there: the Report Builder edits
    // reports, it cannot open a PDF (they open in Preview / the default app instead).
    if (name.endsWith(QLatin1String(".fincept"), Qt::CaseInsensitive))
        return "report_builder";
    if (mime.contains("ipynb") || mime.contains("x-ipynb"))
        return "code_editor";
    if (mime.contains("spreadsheet") || mime.contains("excel") || mime.contains("csv"))
        return "excel";
    return {};
}

// ── Live language switch ─────────────────────────────────────────────────────

void FileManagerScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void FileManagerScreen::retranslateUi() {
    title_label_->setText(tr("FILE MANAGER"));
    subtitle_label_->setText(tr("Manage files across the terminal"));
    refresh_btn_->setText(tr("REFRESH"));
    refresh_btn_->setAccessibleName(tr("Refresh the file list"));
    upload_btn_->setText(tr("UPLOAD FILES"));
    upload_btn_->setAccessibleName(tr("Copy files into terminal storage"));
    search_input_->setPlaceholderText(tr("Search files by name, type, or source..."));
    search_input_->setAccessibleName(tr("Search files"));
    bulk_clear_btn_->setText(tr("CLEAR SELECTION"));
    preview_header_label_->setText(tr("PREVIEW"));
    model_->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Source"), tr("Size"), tr("Added")});
    if (chip_group_)
        for (auto* b : chip_group_->buttons())
            if (b->property("screenFilter").toString().isEmpty())
                b->setText(tr("All"));
    table_->setToolTip(tr("Double-click or Enter opens a file · F2 renames · Del deletes · right-click for more"));
    if (loaded_) {
        update_stats();
        apply_filter();
    }
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap FileManagerScreen::save_state() const {
    return {{"screen_filter", active_screen_filter_}};
}

void FileManagerScreen::restore_state(const QVariantMap& state) {
    const QString filter = state.value("screen_filter").toString();
    if (filter == active_screen_filter_ || !chip_group_)
        return;
    // Match on the "screenFilter" property, not the localized button text.
    for (auto* btn : chip_group_->buttons()) {
        if (btn->property("screenFilter").toString() == filter) {
            btn->setChecked(true); // toggled() applies the filter
            return;
        }
    }
}

// ── ISectionLinked ────────────────────────────────────────────────────────────

bool FileManagerScreen::open_section(const QString& section, const QString& focus) {
    const QString key = section.trimmed();
    const QString lower = key.toLower();
    const QString text = focus.trimmed();
    // The source chip whose "screenFilter" is `tag` (exact, then any case); null when there is none.
    auto chip_for = [this](const QString& tag) -> QAbstractButton* {
        if (!chip_group_)
            return nullptr;
        for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive})
            for (QAbstractButton* b : chip_group_->buttons())
                if (b->property("screenFilter").toString().compare(tag, cs) == 0)
                    return b;
        return nullptr;
    };
    QAbstractButton* chip = nullptr;
    QString rest;
    if (key.isEmpty()) {
        // the current chip
    } else if (lower == QLatin1String("all")) {
        chip = chip_for(QString());
    } else if (command_rows::split_kind(key, "source", &rest)) {
        chip = rest.isEmpty() ? nullptr : chip_for(rest);
        if (!chip)
            return false;
    } else if (lower == QLatin1String("upload")) {
        // The UPLOAD button's file picker (modal): after the link has landed.
        QMetaObject::invokeMethod(this, [this]() { upload_files(); }, Qt::QueuedConnection);
        return true;
    } else {
        return false;
    }
    if (chip && !chip->isChecked())
        chip->click(); // the chip's click (toggled() filters and saves the state)
    if (!text.isEmpty()) {
        search_input_->setText(text); // filters after the search box's own debounce
        search_input_->setFocus(Qt::OtherFocusReason);
    }
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> fm_command_rows() {
    namespace rows = command_rows;
    const QString where = FileManagerScreen::tr("File Manager");
    command::CommandItem upload =
        rows::section(QStringLiteral("file_manager"), QStringLiteral("upload"), FileManagerScreen::tr("Upload files"),
                      {QStringLiteral("upload"), QStringLiteral("import file"), QStringLiteral("add file"),
                       QStringLiteral("attach")},
                      where, FileManagerScreen::tr("Opens the File Manager's file picker"));
    upload.agent_risk = QString::fromLatin1(command::risk::kFile);
    return {upload};
}

void fm_install_commands() {
    command_rows::install(QStringLiteral("file_manager"), &fm_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(fm_install_commands)

} // namespace fincept::screens
