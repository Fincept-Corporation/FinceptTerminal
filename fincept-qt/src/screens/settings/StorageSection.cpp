// StorageSection.cpp — disk usage / data categories / file management /
// read-only SQL console / danger zone.

#include "screens/settings/StorageSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/StorageSqlConsoleGuard.h"
#include "screens/settings/StorageSqlConsoleRunner.h"
#include "storage/StorageManager.h"
#include "storage/cache/CacheManager.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>

namespace fincept::screens {

namespace {

/// Rows the SQL console shows per statement; the rest are reported, not fetched.
constexpr int kStorageConsoleMaxRows = 500;

QString storage_format_bytes(qint64 bytes) {
    if (bytes < 1024)
        return QString::number(bytes) + " B";
    if (bytes < 1048576)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1073741824)
        return QString::number(bytes / 1048576.0, 'f', 1) + " MB";
    return QString::number(bytes / 1073741824.0, 'f', 2) + " GB";
}

/// Re-style a widget whose colour follows its "state" property (the rules live
/// in StorageSection::apply_theme()).
void storage_set_state(QWidget* w, const char* state) {
    if (!w)
        return;
    w->setProperty("state", QString::fromLatin1(state));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

// Every builder below only assigns object names; StorageSection::apply_theme()
// styles them all from one sheet.

// Obsidian panel with a 34px header bar. title_out (when non-null) receives the
// header QLabel so its text can be re-applied on a live language switch.
QFrame* storage_make_panel(const QString& title, QWidget* status_widget = nullptr, QLabel** title_out = nullptr) {
    auto* panel = new QFrame;
    panel->setObjectName(QStringLiteral("storPanel"));
    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(0, 0, 0, 0);
    pvl->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setObjectName(QStringLiteral("storPanelHdr"));
    hdr->setFixedHeight(34);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);

    auto* lbl = new QLabel(title);
    lbl->setObjectName(QStringLiteral("storPanelTitle"));
    hhl->addWidget(lbl);
    hhl->addStretch();
    if (status_widget)
        hhl->addWidget(status_widget);
    pvl->addWidget(hdr);

    if (title_out)
        *title_out = lbl;
    return panel;
}

QWidget* storage_make_body() {
    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("storBody"));
    return body;
}

// LABEL ······ VALUE, 26px, bottom hairline. label_out receives the text label.
QWidget* storage_make_data_row(const QString& label_text, QLabel* value_lbl, QLabel** label_out = nullptr) {
    auto* row = new QWidget;
    row->setObjectName(QStringLiteral("storRow"));
    row->setFixedHeight(26);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto* lbl = new QLabel(label_text);
    lbl->setObjectName(QStringLiteral("storRowLabel"));
    hl->addWidget(lbl);
    hl->addStretch();
    if (label_out)
        *label_out = lbl;

    value_lbl->setObjectName(QStringLiteral("storRowValue"));
    value_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hl->addWidget(value_lbl);
    return row;
}

QWidget* storage_make_category_row(const QString& name, QLabel* count_lbl, QPushButton* clear_btn, bool alt) {
    auto* row = new QWidget;
    row->setObjectName(alt ? QStringLiteral("storRowAlt") : QStringLiteral("storRowBase"));
    row->setFixedHeight(26);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* lbl = new QLabel(name);
    lbl->setObjectName(QStringLiteral("storRowName"));
    hl->addWidget(lbl, 1);

    // refresh_storage_stats() finds the count by this object name.
    count_lbl->setObjectName(QStringLiteral("cat_count"));
    count_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    count_lbl->setFixedWidth(70);
    hl->addWidget(count_lbl);

    clear_btn->setObjectName(QStringLiteral("storClearBtn"));
    clear_btn->setFixedSize(56, 20);
    hl->addWidget(clear_btn);
    return row;
}

QWidget* storage_make_group_header(const QString& title) {
    auto* row = new QWidget;
    row->setObjectName(QStringLiteral("storGroupHdr"));
    row->setFixedHeight(26);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* lbl = new QLabel(title);
    lbl->setObjectName(QStringLiteral("storGroupTitle"));
    hl->addWidget(lbl);
    return row;
}

QWidget* storage_make_file_action_row(const QString& name, QLabel* size_lbl, QPushButton* btn, bool alt,
                                      QLabel** label_out = nullptr) {
    auto* row = new QWidget;
    row->setObjectName(alt ? QStringLiteral("storRowAlt") : QStringLiteral("storRowBase"));
    row->setFixedHeight(26);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* lbl = new QLabel(name);
    lbl->setObjectName(QStringLiteral("storRowName"));
    hl->addWidget(lbl, 1);
    if (label_out)
        *label_out = lbl;

    size_lbl->setObjectName(QStringLiteral("storSizeLabel"));
    size_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    size_lbl->setFixedWidth(80);
    hl->addWidget(size_lbl);

    btn->setObjectName(QStringLiteral("storClearBtn"));
    btn->setFixedSize(56, 20);
    hl->addWidget(btn);
    return row;
}

/// Header strip of a CATEGORY / ENTRIES / ACTION style table.
QWidget* storage_make_table_header(QLabel*& first, QLabel*& second, int second_width, QLabel*& third) {
    auto* th = new QWidget;
    th->setObjectName(QStringLiteral("storTableHdr"));
    th->setFixedHeight(26);
    auto* thl = new QHBoxLayout(th);
    thl->setContentsMargins(12, 0, 12, 0);
    thl->setSpacing(8);
    for (QLabel* l : {first, second, third})
        l->setObjectName(QStringLiteral("storTableHdrCell"));
    thl->addWidget(first, 1);
    second->setFixedWidth(second_width);
    second->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    thl->addWidget(second);
    third->setFixedWidth(56);
    third->setAlignment(Qt::AlignCenter);
    thl->addWidget(third);
    return th;
}

} // namespace

StorageSection::StorageSection(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_theme();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });
}

void StorageSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_storage_stats();
}

void StorageSection::apply_theme() {
    using namespace ui::colors;
    const QString sheet =
        QString(
            // Containers and text
            "QScrollArea#storScroll{border:none;background:transparent;}"
            "QScrollBar:vertical{width:6px;background:transparent;}"
            "QScrollBar::handle:vertical{background:%9;border-radius:3px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
            "QScrollBar:horizontal{height:6px;background:transparent;}"
            "QScrollBar::handle:horizontal{background:%9;}"
            "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
            "QWidget#storBody{background:transparent;}"
            "QLabel{background:transparent;}"
            "QLabel#storPageTitle{color:%1;font-weight:bold;letter-spacing:0.5px;}"
            "QLabel#storPageInfo{color:%4;}"
            "QLabel#storHint{color:%5;}"
            // Panels
            "QFrame#storPanel{background:%2;border:1px solid %3;}"
            "QWidget#storPanelHdr{background:%6;border-bottom:1px solid %3;}"
            "QLabel#storPanelTitle{color:%1;font-weight:700;letter-spacing:0.5px;}"
            // Rows
            "QWidget#storRow{background:transparent;border-bottom:1px solid %3;}"
            "QWidget#storRowBase{background:%7;border-bottom:1px solid %3;}"
            "QWidget#storRowAlt{background:%8;border-bottom:1px solid %3;}"
            "QWidget#storTableHdr,QWidget#storGroupHdr{background:%6;border-bottom:1px solid %3;}"
            "QWidget#storCacheRow{background:%8;border-bottom:1px solid %3;}"
            "QLabel#storTableHdrCell{color:%5;font-weight:700;}"
            "QLabel#storGroupTitle{color:%10;font-weight:700;letter-spacing:0.5px;}"
            "QLabel#storRowLabel{color:%4;font-weight:600;letter-spacing:0.5px;}"
            "QLabel#storRowValue{color:%11;font-weight:700;}"
            "QLabel#storRowName{color:%11;}"
            "QLabel#storSizeLabel{color:%4;font-weight:700;}"
            "QLabel#cat_count{color:%12;font-weight:700;}"
            // Stat boxes
            "QFrame#storStatBox{background:%6;border:1px solid %3;}"
            "QLabel#storStatValue{color:%12;font-size:18px;font-weight:700;}"
            "QLabel#storStatLabel{color:%4;font-weight:700;}")
            .arg(AMBER(), BG_SURFACE(), BORDER_DIM(), TEXT_SECONDARY(), TEXT_DIM(), BG_RAISED(), BG_BASE(),
                 ROW_ALT(), BORDER_MED())
            .arg(AMBER_DIM(), TEXT_PRIMARY(), CYAN()) +
        QString(
            // Buttons
            "QPushButton#storAmberBtn{background:%1;color:%2;border:1px solid %3;font-weight:700;padding:0 8px;}"
            "QPushButton#storAmberBtn:hover{background:%2;color:%4;}"
            "QPushButton#storAmberBtn:disabled{background:transparent;color:%5;border-color:%6;}"
            "QPushButton#storClearBtn,QPushButton#storDangerBtn{background:%7;color:%8;border:1px solid %9;"
            "font-weight:700;}"
            "QPushButton#storClearBtn:hover,QPushButton#storDangerBtn:hover{background:%8;color:%10;}"
            "QPushButton#storChipBtn{background:%11;color:%12;border:1px solid %6;padding:0 6px;}"
            "QPushButton#storChipBtn:hover{color:%2;border-color:%2;}"
            "QPushButton#storChipBtn:disabled{color:%5;}"
            "QPushButton#storNukeBtn{background:%8;color:%10;border:2px solid %8;font-weight:700;}"
            "QPushButton#storNukeBtn:hover{background:%10;color:%4;}")
            .arg(ACCENT_BG(), AMBER(), AMBER_DIM(), BG_BASE(), TEXT_DIM(), BORDER_DIM(), NEGATIVE_BG(), NEGATIVE(),
                 NEGATIVE_DIM())
            .arg(TEXT_PRIMARY(), BG_RAISED(), TEXT_SECONDARY()) +
        QString(
            // SQL console
            "QLineEdit#storSqlInput{background:%1;color:%2;border:1px solid %3;padding:6px;}"
            "QLineEdit#storSqlInput:focus{border:1px solid %4;}"
            "QComboBox#storSqlDb{background:%1;color:%2;border:1px solid %3;padding:5px 8px;}"
            "QComboBox#storSqlDb:focus{border:1px solid %4;}"
            "QComboBox#storSqlDb::drop-down{border:none;width:20px;}"
            "QComboBox#storSqlDb QAbstractItemView{background:%1;color:%2;selection-background-color:%5;"
            "border:1px solid %3;}"
            "QLabel#storSqlStatus{color:%6;}"
            "QLabel#storSqlStatus[state=\"ok\"]{color:%7;}"
            "QLabel#storSqlStatus[state=\"error\"]{color:%8;}"
            "QLabel#storSqlStatus[state=\"warn\"]{color:%9;}"
            "QTableWidget#storSqlTable{background:%10;alternate-background-color:%11;color:%2;"
            "border:1px solid %12;gridline-color:%12;selection-background-color:%5;selection-color:%2;}")
            .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED(), AMBER(), BG_HOVER(), TEXT_DIM(), POSITIVE(), NEGATIVE(),
                 WARNING())
            .arg(BG_BASE(), ROW_ALT(), BORDER_DIM()) +
        QString("QTableWidget#storSqlTable QHeaderView::section{background:%1;color:%2;border:none;"
                "border-right:1px solid %3;border-bottom:1px solid %3;padding:2px 6px;font-weight:700;}"
                "QTableWidget#storSqlTable QTableCornerButton::section{background:%1;border:none;}"
                // Danger zone
                "QFrame#storDangerPanel{background:%4;border:1px solid %5;}"
                "QWidget#storDangerHdr{background:%6;border-bottom:1px solid %5;}"
                "QLabel#storDangerTitle{color:%7;font-weight:700;letter-spacing:0.5px;}"
                "QLabel#storDangerItemTitle{color:%8;font-weight:700;}"
                "QLabel#storNukeTitle{color:%7;font-weight:700;}")
            .arg(BG_RAISED(), TEXT_DIM(), BORDER_DIM(), BG_SURFACE(), NEGATIVE_DIM(), NEGATIVE_BG(), NEGATIVE(),
                 TEXT_PRIMARY());
    setStyleSheet(sheet);
}

void StorageSection::build_ui() {
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("storScroll"));
    scroll->setWidgetResizable(true);

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    page_title_ = new QLabel(tr("STORAGE & DATA MANAGEMENT"));
    page_title_->setObjectName(QStringLiteral("storPageTitle"));
    vl->addWidget(page_title_);
    vl->addSpacing(4);

    page_info_ = new QLabel(tr("Manage all persistent data, databases, and files. "
                               "Inspect terminal databases with the read-only SQL console."));
    page_info_->setObjectName(QStringLiteral("storPageInfo"));
    page_info_->setWordWrap(true);
    vl->addWidget(page_info_);
    vl->addSpacing(10);

    // ── SECTION 1: DISK USAGE ──────────────────────────────────────────────────
    {
        disk_refresh_btn_ = new QPushButton(tr("Refresh"));
        disk_refresh_btn_->setObjectName(QStringLiteral("storAmberBtn"));
        disk_refresh_btn_->setFixedSize(70, 22);
        connect(disk_refresh_btn_, &QPushButton::clicked, this, [this]() { refresh_storage_stats(); });

        auto* panel = storage_make_panel(tr("DISK USAGE"), disk_refresh_btn_, &disk_panel_title_);
        auto* body = storage_make_body();
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        auto* stat_row = storage_make_body();
        auto* shl = new QHBoxLayout(stat_row);
        shl->setContentsMargins(10, 10, 10, 10);
        shl->setSpacing(10);

        auto make_stat_box = [&](const QString& label, QLabel*& val_out, QLabel** label_out) {
            auto* box = new QFrame;
            box->setObjectName(QStringLiteral("storStatBox"));
            auto* bx = new QVBoxLayout(box);
            bx->setContentsMargins(12, 12, 12, 14);
            bx->setSpacing(4);
            bx->setAlignment(Qt::AlignCenter);

            val_out = new QLabel(QStringLiteral("—"));
            val_out->setObjectName(QStringLiteral("storStatValue"));
            val_out->setAlignment(Qt::AlignCenter);
            bx->addWidget(val_out);

            auto* ll = new QLabel(label);
            ll->setObjectName(QStringLiteral("storStatLabel"));
            ll->setAlignment(Qt::AlignCenter);
            bx->addWidget(ll);
            if (label_out)
                *label_out = ll;

            shl->addWidget(box, 1);
        };

        make_stat_box(tr("MAIN DB"), storage_main_db_, &main_db_box_lbl_);
        make_stat_box(tr("CACHE DB"), storage_cache_db_, &cache_db_box_lbl_);
        make_stat_box(tr("LOG FILES"), storage_log_size_, &log_box_lbl_);
        make_stat_box(tr("WORKSPACES"), storage_ws_size_, &ws_box_lbl_);
        make_stat_box(tr("TOTAL"), storage_total_size_, &total_box_lbl_);

        bvl->addWidget(stat_row);

        storage_count_ = new QLabel(QStringLiteral("—"));
        bvl->addWidget(storage_make_data_row(tr("Cache Entries"), storage_count_, &cache_entries_row_lbl_));

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── SECTION 2: DATA CATEGORIES ─────────────────────────────────────────────
    {
        auto* panel = storage_make_panel(tr("DATA CATEGORIES"), nullptr, &categories_panel_title_);
        auto* body = storage_make_body();

        storage_categories_ = new QVBoxLayout(body);
        storage_categories_->setContentsMargins(0, 0, 0, 0);
        storage_categories_->setSpacing(0);

        cat_hdr_category_ = new QLabel(tr("CATEGORY"));
        cat_hdr_entries_ = new QLabel(tr("ENTRIES"));
        cat_hdr_action_ = new QLabel(tr("ACTION"));
        storage_categories_->addWidget(
            storage_make_table_header(cat_hdr_category_, cat_hdr_entries_, 70, cat_hdr_action_));

        auto& sm = StorageManager::instance();
        auto stats = sm.all_stats();

        QString current_group;
        int row_idx = 0;
        for (const auto& cat : stats) {
            if (cat.group != current_group) {
                current_group = cat.group;
                storage_categories_->addWidget(storage_make_group_header(cat.group.toUpper()));
                row_idx = 0;
            }

            auto* count_lbl = new QLabel(QString::number(cat.count));
            auto* clear_btn = new QPushButton(tr("CLR"));

            QString cat_id = cat.id;
            QString cat_label = cat.label;
            connect(clear_btn, &QPushButton::clicked, this, [this, cat_id, cat_label, count_lbl]() {
                auto answer = QMessageBox::warning(
                    this, tr("Clear %1").arg(cat_label),
                    tr("Permanently delete all %1?\n\nThis cannot be undone.").arg(cat_label.toLower()),
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes)
                    return;

                auto r = StorageManager::instance().clear_category(cat_id);
                if (r.is_ok()) {
                    count_lbl->setText(QStringLiteral("0"));
                    LOG_INFO("Settings", "Cleared: " + cat_label);
                } else {
                    QMessageBox::critical(
                        this, tr("Error"),
                        tr("Failed to clear %1:\n%2").arg(cat_label, QString::fromStdString(r.error())));
                }
                refresh_storage_stats();
            });

            storage_categories_->addWidget(
                storage_make_category_row(cat.label, count_lbl, clear_btn, (row_idx % 2) == 1));
            ++row_idx;
        }

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── SECTION 3: FILE & STATE MANAGEMENT ─────────────────────────────────────
    {
        auto* panel = storage_make_panel(tr("FILE & STATE MANAGEMENT"), nullptr, &files_panel_title_);
        auto* body = storage_make_body();
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        file_hdr_store_ = new QLabel(tr("STORE"));
        file_hdr_size_ = new QLabel(tr("SIZE"));
        file_hdr_action_ = new QLabel(tr("ACTION"));
        bvl->addWidget(storage_make_table_header(file_hdr_store_, file_hdr_size_, 80, file_hdr_action_));

        // The action returns the StorageManager result so a failed clear is reported
        // instead of being logged as if it had worked.
        auto add_file_row = [&](const QString& name, QLabel* size_lbl, const QString& confirm_title,
                                const QString& confirm_msg, std::function<Result<void>()> action, bool alt,
                                QLabel** label_out) {
            auto* btn = new QPushButton(tr("CLR"));
            connect(btn, &QPushButton::clicked, this, [this, confirm_title, confirm_msg, action]() {
                auto answer = QMessageBox::warning(this, confirm_title, confirm_msg,
                                                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes)
                    return;
                const auto result = action();
                if (result.is_err()) {
                    LOG_ERROR("Settings", confirm_title + " failed: " + QString::fromStdString(result.error()));
                    QMessageBox::critical(this, tr("Error"),
                                          tr("%1 failed:\n%2").arg(confirm_title, QString::fromStdString(result.error())));
                }
                refresh_storage_stats();
            });
            bvl->addWidget(storage_make_file_action_row(name, size_lbl, btn, alt, label_out));
        };

        auto* log_sz = new QLabel(QStringLiteral("—"));
        auto* ws_sz = new QLabel(QStringLiteral("—"));
        auto* qs_lbl = new QLabel(tr("Registry"));

        add_file_row(
            tr("Log Files"), log_sz, tr("Clear Logs"),
            tr("Clear all application log files?\nCurrent log data will be lost."),
            []() {
                auto r = StorageManager::instance().clear_log_files();
                if (r.is_ok())
                    LOG_INFO("Settings", "Logs cleared");
                return r;
            },
            false, &file_row_logs_lbl_);

        add_file_row(
            tr("Workspace Files (.fwsp)"), ws_sz, tr("Delete Workspaces"),
            tr("Delete all saved workspace files?\nThis cannot be undone."),
            []() {
                auto r = StorageManager::instance().clear_workspace_files();
                if (r.is_ok())
                    LOG_INFO("Settings", "Workspaces deleted");
                return r;
            },
            true, &file_row_ws_lbl_);

        add_file_row(
            tr("Window & UI State"), qs_lbl, tr("Reset UI State"),
            tr("Reset all window positions, dock layouts, and perspectives?\nTakes effect on next restart."),
            []() {
                auto r = StorageManager::instance().clear_qsettings();
                if (r.is_ok())
                    LOG_INFO("Settings", "QSettings cleared");
                return r;
            },
            false, &file_row_ui_lbl_);

        // Cache subcategories
        auto* cache_row = new QWidget(this);
        cache_row->setObjectName(QStringLiteral("storCacheRow"));
        cache_row->setFixedHeight(30);
        auto* chl = new QHBoxLayout(cache_row);
        chl->setContentsMargins(12, 0, 12, 0);
        chl->setSpacing(4);
        cache_subcat_lbl_ = new QLabel(tr("Cache:"));
        cache_subcat_lbl_->setObjectName(QStringLiteral("storRowLabel"));
        chl->addWidget(cache_subcat_lbl_);

        static const QStringList kStorageCacheCats = {"market_data", "news", "quotes", "charts", "general"};
        for (const QString& cat : kStorageCacheCats) {
            auto* btn = new QPushButton(cat);
            btn->setObjectName(QStringLiteral("storChipBtn"));
            btn->setFixedHeight(18);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setToolTip(tr("Clear the \"%1\" cache category").arg(cat));
            connect(btn, &QPushButton::clicked, this, [this, cat]() {
                CacheManager::instance().clear_category(cat);
                refresh_storage_stats();
                LOG_INFO("Settings", "Cache cleared: " + cat);
            });
            chl->addWidget(btn);
        }
        chl->addStretch();
        bvl->addWidget(cache_row);

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── SECTION 4: SQL CONSOLE (read-only) ─────────────────────────────────────
    // Inspection only: StorageSqlConsoleGuard.h rejects anything but a single
    // read statement and keeps credential tables / secret columns out of reach;
    // StorageSqlConsoleRunner.h executes on a private read-only connection off
    // the UI thread and masks secret-looking values before they are shown.
    {
        auto* panel = storage_make_panel(tr("SQL CONSOLE (READ-ONLY)"), nullptr, &sql_panel_title_);
        auto* body = storage_make_body();
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(10, 8, 10, 8);
        bvl->setSpacing(6);

        sql_hint_lbl_ = new QLabel(tr("Read-only inspection: SELECT, WITH, VALUES, EXPLAIN and schema PRAGMAs. "
                                      "Credential tables are blocked and secret-looking values are masked. "
                                      "Shows the first %1 rows.")
                                       .arg(kStorageConsoleMaxRows));
        sql_hint_lbl_->setObjectName(QStringLiteral("storHint"));
        sql_hint_lbl_->setWordWrap(true);
        bvl->addWidget(sql_hint_lbl_);

        auto* input_row = storage_make_body();
        auto* irl = new QHBoxLayout(input_row);
        irl->setContentsMargins(0, 0, 0, 0);
        irl->setSpacing(6);

        sql_db_selector_ = new QComboBox;
        sql_db_selector_->setObjectName(QStringLiteral("storSqlDb"));
        sql_db_selector_->addItem(QStringLiteral("fincept.db"), QStringLiteral("main"));
        sql_db_selector_->addItem(QStringLiteral("cache.db"), QStringLiteral("cache"));
        sql_db_selector_->setFixedWidth(110);
        sql_db_selector_->setAccessibleName(tr("Database to query"));
        irl->addWidget(sql_db_selector_);

        sql_input_ = new QLineEdit;
        sql_input_->setObjectName(QStringLiteral("storSqlInput"));
        sql_input_->setPlaceholderText(QStringLiteral("SELECT * FROM chat_sessions LIMIT 10"));
        sql_input_->setAccessibleName(tr("SQL statement"));
        sql_input_->setClearButtonEnabled(true);
        irl->addWidget(sql_input_, 1);

        sql_exec_btn_ = new QPushButton(tr("RUN"));
        sql_exec_btn_->setObjectName(QStringLiteral("storAmberBtn"));
        sql_exec_btn_->setFixedSize(56, 28);
        sql_exec_btn_->setToolTip(tr("Run the statement (Enter)"));
        irl->addWidget(sql_exec_btn_);

        bvl->addWidget(input_row);

        auto* status_row = storage_make_body();
        auto* srl = new QHBoxLayout(status_row);
        srl->setContentsMargins(0, 0, 0, 0);
        srl->setSpacing(6);
        sql_status_ = new QLabel(tr("Ready"));
        sql_status_->setObjectName(QStringLiteral("storSqlStatus"));
        sql_status_->setWordWrap(true);
        srl->addWidget(sql_status_, 1);
        sql_copy_btn_ = new QPushButton(tr("COPY"));
        sql_copy_btn_->setObjectName(QStringLiteral("storChipBtn"));
        sql_copy_btn_->setFixedHeight(20);
        sql_copy_btn_->setEnabled(false);
        sql_copy_btn_->setToolTip(tr("Copy the result (masked values stay masked) as tab-separated text"));
        connect(sql_copy_btn_, &QPushButton::clicked, this, &StorageSection::copy_sql_result);
        srl->addWidget(sql_copy_btn_, 0, Qt::AlignTop);
        bvl->addWidget(status_row);

        sql_table_ = new QTableWidget(this);
        sql_table_->setObjectName(QStringLiteral("storSqlTable"));
        sql_table_->setFixedHeight(220);
        sql_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        sql_table_->setSelectionBehavior(QAbstractItemView::SelectItems);
        sql_table_->setAlternatingRowColors(true);
        sql_table_->setWordWrap(false);
        sql_table_->setTextElideMode(Qt::ElideRight);
        sql_table_->verticalHeader()->setVisible(false);
        sql_table_->verticalHeader()->setDefaultSectionSize(20);
        sql_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        sql_table_->horizontalHeader()->setStretchLastSection(true);
        sql_table_->setAccessibleName(tr("Query results"));
        bvl->addWidget(sql_table_);

        // Quick-access starting points. No settings dump: key/value tables are
        // masked, but a one-click "show me every stored setting" is not something
        // to put a button on.
        auto* tables_row = storage_make_body();
        auto* trhl = new QHBoxLayout(tables_row);
        trhl->setContentsMargins(0, 0, 0, 0);
        trhl->setSpacing(4);
        sql_quick_lbl_ = new QLabel(tr("Quick:"));
        sql_quick_lbl_->setObjectName(QStringLiteral("storHint"));
        trhl->addWidget(sql_quick_lbl_);

        struct QuickQuery {
            const char* label;
            const char* sql;
        };
        static const QuickQuery kStorageQuickQueries[] = {
            {"tables", "SELECT type, name, tbl_name FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' ORDER BY type, name"},
            {"chat_sessions", "SELECT * FROM chat_sessions LIMIT 20"},
            {"news_articles", "SELECT * FROM news_articles LIMIT 20"},
            {"financial_notes", "SELECT * FROM financial_notes LIMIT 20"},
            {"portfolios", "SELECT * FROM portfolios LIMIT 20"},
            {"watchlists", "SELECT * FROM watchlists LIMIT 20"},
            {"pt_portfolios", "SELECT * FROM pt_portfolios LIMIT 20"},
            {"workflows", "SELECT * FROM workflows LIMIT 20"},
        };
        for (const auto& q : kStorageQuickQueries) {
            auto* btn = new QPushButton(QString::fromLatin1(q.label));
            btn->setObjectName(QStringLiteral("storChipBtn"));
            btn->setFixedHeight(18);
            btn->setCursor(Qt::PointingHandCursor);
            const QString sql = QString::fromLatin1(q.sql);
            connect(btn, &QPushButton::clicked, this, [this, sql]() {
                sql_db_selector_->setCurrentIndex(0);
                sql_input_->setText(sql);
                sql_input_->setFocus();
            });
            trhl->addWidget(btn);
        }
        trhl->addStretch();
        bvl->addWidget(tables_row);

        connect(sql_exec_btn_, &QPushButton::clicked, this, &StorageSection::run_sql);
        connect(sql_input_, &QLineEdit::returnPressed, this, &StorageSection::run_sql);

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── SECTION 5: DANGER ZONE ─────────────────────────────────────────────────
    {
        auto* panel = new QFrame;
        panel->setObjectName(QStringLiteral("storDangerPanel"));
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        auto* hdr = new QWidget(this);
        hdr->setObjectName(QStringLiteral("storDangerHdr"));
        hdr->setFixedHeight(34);
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        danger_panel_title_ = new QLabel(tr("DANGER ZONE"));
        danger_panel_title_->setObjectName(QStringLiteral("storDangerTitle"));
        hhl->addWidget(danger_panel_title_);
        pvl->addWidget(hdr);

        auto* body = storage_make_body();
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(12, 10, 12, 10);
        bvl->setSpacing(8);

        // Clear all cache
        auto* cache_row = storage_make_body();
        auto* cr_hl = new QHBoxLayout(cache_row);
        cr_hl->setContentsMargins(0, 0, 0, 0);
        cr_hl->setSpacing(8);
        auto* cache_desc = storage_make_body();
        auto* cd_vl = new QVBoxLayout(cache_desc);
        cd_vl->setContentsMargins(0, 0, 0, 0);
        cd_vl->setSpacing(2);
        clear_cache_title_ = new QLabel(tr("Clear All Cache"));
        clear_cache_title_->setObjectName(QStringLiteral("storDangerItemTitle"));
        cd_vl->addWidget(clear_cache_title_);
        clear_cache_desc_ = new QLabel(tr("Delete all temporary cached data. Will be re-fetched on next access."));
        clear_cache_desc_->setObjectName(QStringLiteral("storHint"));
        cd_vl->addWidget(clear_cache_desc_);
        cr_hl->addWidget(cache_desc, 1);
        clear_cache_btn_ = new QPushButton(tr("CLEAR CACHE"));
        clear_cache_btn_->setObjectName(QStringLiteral("storDangerBtn"));
        clear_cache_btn_->setFixedSize(110, 26);
        connect(clear_cache_btn_, &QPushButton::clicked, this, [this]() {
            auto answer =
                QMessageBox::warning(this, tr("Clear All Cache"),
                                     tr("Delete all temporary cached data?\nData will be re-fetched on next access."),
                                     QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes)
                return;
            CacheManager::instance().clear();
            refresh_storage_stats();
            LOG_INFO("Settings", "All cache cleared");
        });
        cr_hl->addWidget(clear_cache_btn_);
        bvl->addWidget(cache_row);

        bvl->addWidget(make_sep());

        // Nuclear: clear ALL data
        auto* nuke_row = storage_make_body();
        auto* nr_hl = new QHBoxLayout(nuke_row);
        nr_hl->setContentsMargins(0, 0, 0, 0);
        nr_hl->setSpacing(8);
        auto* nuke_desc = storage_make_body();
        auto* nd_vl = new QVBoxLayout(nuke_desc);
        nd_vl->setContentsMargins(0, 0, 0, 0);
        nd_vl->setSpacing(2);
        nuke_title_ = new QLabel(tr("Clear ALL User Data"));
        nuke_title_->setObjectName(QStringLiteral("storNukeTitle"));
        nd_vl->addWidget(nuke_title_);
        nuke_desc_ = new QLabel(tr("Permanently delete all databases, files, cache, and UI state. Encrypted API "
                                   "keys (Settings → Credentials) are kept."));
        nuke_desc_->setObjectName(QStringLiteral("storHint"));
        nuke_desc_->setWordWrap(true);
        nd_vl->addWidget(nuke_desc_);
        nr_hl->addWidget(nuke_desc, 1);
        nuke_btn_ = new QPushButton(tr("DELETE ALL"));
        nuke_btn_->setObjectName(QStringLiteral("storNukeBtn"));
        nuke_btn_->setFixedSize(110, 26);
        connect(nuke_btn_, &QPushButton::clicked, this, [this]() {
            auto a1 = QMessageBox::critical(this, tr("Clear ALL User Data"),
                                            tr("WARNING: This will permanently delete ALL data:\n\n"
                                               "  Chat history, notes, reports, watchlists\n"
                                               "  Portfolios, transactions, paper trades\n"
                                               "  Workflows, dashboard layouts\n"
                                               "  News articles, RSS feeds, monitors\n"
                                               "  Data sources, MCP servers\n"
                                               "  Agent configs, LLM configs & profiles\n"
                                               "  App settings, credentials, key-value storage\n"
                                               "  All cache, log files, workspaces, UI state\n\n"
                                               "Encrypted API keys (Settings → Credentials) are NOT deleted.\n"
                                               "This action CANNOT be undone."),
                                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (a1 != QMessageBox::Yes)
                return;

            auto a2 = QMessageBox::critical(this, tr("Final Confirmation"),
                                            tr("ALL data will be permanently deleted.\nAre you absolutely sure?"),
                                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (a2 != QMessageBox::Yes)
                return;

            auto& sm = StorageManager::instance();
            QStringList failures;
            auto note = [&failures](const QString& what, const Result<void>& r) {
                if (r.is_err()) {
                    LOG_ERROR("Settings", "Clear all: " + what + " failed: " + QString::fromStdString(r.error()));
                    failures << what;
                }
            };
            for (const auto& info : sm.all_stats())
                note(info.label, sm.clear_category(info.id));
            note(tr("Log files"), sm.clear_log_files());
            note(tr("Workspace files"), sm.clear_workspace_files());
            note(tr("Window & UI state"), sm.clear_qsettings());
            refresh_storage_stats();
            LOG_INFO("Settings", QString("ALL user data cleared (%1 item(s) failed)").arg(failures.size()));

            // The screens that are open still hold what they loaded, and nothing
            // told the user the wipe had finished (or only partly worked).
            if (failures.isEmpty()) {
                QMessageBox::information(this, tr("Clear ALL User Data"),
                                         tr("All user data was deleted. Restart Fincept Terminal so every screen "
                                            "starts from a clean state."));
            } else {
                QMessageBox::warning(this, tr("Clear ALL User Data"),
                                     tr("Some items could not be deleted:\n\n%1\n\nRestart Fincept Terminal and "
                                        "try again.")
                                         .arg(failures.join(QStringLiteral("\n"))));
            }
        });
        nr_hl->addWidget(nuke_btn_);
        bvl->addWidget(nuke_row);

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

// ── SQL console ───────────────────────────────────────────────────────────────

void StorageSection::set_sql_status(const QString& text, const char* state) {
    if (!sql_status_)
        return;
    sql_status_->setText(text);
    storage_set_state(sql_status_, state);
}

void StorageSection::run_sql() {
    if (sql_running_ || !sql_input_)
        return;
    const QString sql = sql_input_->text().trimmed();
    if (sql.isEmpty())
        return;

    namespace guard = storage_sql_guard;
    const guard::Decision decision = guard::check(sql);
    switch (decision.verdict) {
        case guard::Verdict::Allowed:
            break;
        case guard::Verdict::Empty:
            return;
        case guard::Verdict::Unparseable:
            set_sql_status(tr("Not run — the statement has an unterminated quote."), "error");
            return;
        case guard::Verdict::MultipleStatements:
            set_sql_status(tr("Not run — one statement at a time."), "error");
            return;
        case guard::Verdict::NotReadOnly:
            set_sql_status(tr("Not run — the console is read-only (%1). Use SELECT, WITH, VALUES, EXPLAIN or a "
                              "schema PRAGMA.")
                               .arg(decision.subject),
                           "error");
            return;
        case guard::Verdict::PragmaNotAllowed:
            set_sql_status(tr("Not run — only read-only schema PRAGMAs are allowed (e.g. PRAGMA table_info(t))."),
                           "error");
            return;
        case guard::Verdict::BlockedFunction:
            set_sql_status(tr("Not run — %1 is not available in the console.").arg(decision.subject), "error");
            return;
        case guard::Verdict::BlockedTable:
            set_sql_status(tr("Not run — %1 stores credentials and cannot be queried here. Manage keys in "
                              "Settings → Credentials.")
                               .arg(decision.subject),
                           "error");
            return;
        case guard::Verdict::SecretColumn:
            set_sql_status(tr("Not run — %1 can hold a secret. List the columns you need instead of * and leave "
                              "that one out.")
                               .arg(decision.subject),
                           "error");
            return;
    }

    const bool use_cache = sql_db_selector_ && sql_db_selector_->currentData().toString() == QLatin1String("cache");
    const QString path = use_cache ? CacheDatabase::instance().path() : Database::instance().path();
    if (path.isEmpty()) {
        set_sql_status(tr("Not run — the database is not open."), "error");
        return;
    }

    sql_running_ = true;
    sql_exec_btn_->setEnabled(false);
    set_sql_status(tr("Running…"), "idle");
    LOG_DEBUG("SQL Console", QString("Running read-only statement on %1").arg(use_cache ? "cache.db" : "fincept.db"));

    QPointer<StorageSection> self = this;
    (void)QtConcurrent::run([self, path, sql]() {
        storage_sql_runner::Result result = storage_sql_runner::run(path, sql, kStorageConsoleMaxRows);
        // qApp as the context: the lambda then runs on the UI thread, where the
        // QPointer check cannot race the section's destruction.
        QMetaObject::invokeMethod(
            qApp,
            [self, result = std::move(result)]() {
                if (self)
                    self->show_sql_result(result);
            },
            Qt::QueuedConnection);
    });
}

void StorageSection::show_sql_result(const storage_sql_runner::Result& result) {
    sql_running_ = false;
    if (sql_exec_btn_)
        sql_exec_btn_->setEnabled(true);
    if (!sql_table_)
        return;

    if (!result.ok) {
        // Keep the previous result on screen; only the status line changes.
        set_sql_status(tr("Error: %1").arg(result.error), "error");
        LOG_WARN("SQL Console", "Query failed: " + result.error);
        return;
    }

    sql_table_->setUpdatesEnabled(false);
    sql_table_->clear();
    sql_table_->setColumnCount(result.columns.size());
    sql_table_->setRowCount(result.rows.size());
    sql_table_->setHorizontalHeaderLabels(result.columns);
    const QColor null_color(ui::colors::TEXT_DIM());
    for (int r = 0; r < result.rows.size(); ++r) {
        const QStringList& row = result.rows.at(r);
        const QVector<bool>& nulls = result.null_cells.at(r);
        for (int c = 0; c < row.size(); ++c) {
            auto* item = new QTableWidgetItem(row.at(c));
            if (c < nulls.size() && nulls.at(c))
                item->setForeground(null_color);
            else if (row.at(c).size() > 40)
                item->setToolTip(row.at(c).left(1000));
            sql_table_->setItem(r, c, item);
        }
    }
    if (result.columns.size() > 1) {
        sql_table_->resizeColumnsToContents();
        for (int c = 0; c < sql_table_->columnCount(); ++c)
            sql_table_->setColumnWidth(c, qMin(sql_table_->columnWidth(c), 320));
    }
    sql_table_->setUpdatesEnabled(true);
    if (sql_copy_btn_)
        sql_copy_btn_->setEnabled(!result.rows.isEmpty());

    QString status;
    if (result.columns.isEmpty())
        status = tr("OK — no columns returned");
    else if (result.truncated)
        status = tr("OK — first %1 rows shown (more exist; add WHERE / LIMIT to narrow)").arg(result.rows.size());
    else
        status = tr("OK — %1 row(s)").arg(result.rows.size());
    if (result.masked > 0)
        status += tr(" · %1 value(s) masked").arg(result.masked);
    status += tr(" · %1 ms").arg(result.elapsed_ms);
    set_sql_status(status, result.truncated ? "warn" : "ok");
}

void StorageSection::copy_sql_result() {
    if (!sql_table_ || sql_table_->rowCount() == 0)
        return;
    QStringList lines;
    QStringList header;
    for (int c = 0; c < sql_table_->columnCount(); ++c) {
        auto* h = sql_table_->horizontalHeaderItem(c);
        header << (h ? h->text() : QString());
    }
    lines << header.join(QLatin1Char('\t'));
    for (int r = 0; r < sql_table_->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < sql_table_->columnCount(); ++c) {
            auto* it = sql_table_->item(r, c);
            QString text = it ? it->text() : QString();
            text.replace(QLatin1Char('\t'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' '));
            cells << text;
        }
        lines << cells.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    set_sql_status(tr("Copied %1 row(s) to the clipboard").arg(sql_table_->rowCount()), "ok");
}

// ── Stats ─────────────────────────────────────────────────────────────────────

void StorageSection::refresh_storage_stats() {
    auto& sm = StorageManager::instance();

    if (storage_main_db_)
        storage_main_db_->setText(storage_format_bytes(sm.main_db_size()));
    if (storage_cache_db_)
        storage_cache_db_->setText(storage_format_bytes(sm.cache_db_size()));
    if (storage_log_size_)
        storage_log_size_->setText(storage_format_bytes(sm.log_files_size()));
    if (storage_ws_size_)
        storage_ws_size_->setText(storage_format_bytes(sm.workspace_files_size()));
    if (storage_total_size_)
        storage_total_size_->setText(storage_format_bytes(sm.main_db_size() + sm.cache_db_size() +
                                                          sm.log_files_size() + sm.workspace_files_size()));

    if (storage_count_)
        storage_count_->setText(QString::number(CacheManager::instance().entry_count()));

    if (storage_categories_) {
        auto stats = sm.all_stats();
        int stat_idx = 0;
        for (int i = 0; i < storage_categories_->count() && stat_idx < stats.size(); ++i) {
            auto* item = storage_categories_->itemAt(i);
            if (!item || !item->widget())
                continue;
            auto* count_lbl = item->widget()->findChild<QLabel*>("cat_count");
            if (!count_lbl)
                continue;
            count_lbl->setText(QString::number(stats[stat_idx].count));
            ++stat_idx;
        }
    }
}

void StorageSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void StorageSection::retranslateUi() {
    // Page header.
    if (page_title_)
        page_title_->setText(tr("STORAGE & DATA MANAGEMENT"));
    if (page_info_)
        page_info_->setText(tr("Manage all persistent data, databases, and files. "
                               "Inspect terminal databases with the read-only SQL console."));

    // Disk usage panel.
    if (disk_refresh_btn_)
        disk_refresh_btn_->setText(tr("Refresh"));
    if (disk_panel_title_)
        disk_panel_title_->setText(tr("DISK USAGE"));
    if (main_db_box_lbl_)
        main_db_box_lbl_->setText(tr("MAIN DB"));
    if (cache_db_box_lbl_)
        cache_db_box_lbl_->setText(tr("CACHE DB"));
    if (log_box_lbl_)
        log_box_lbl_->setText(tr("LOG FILES"));
    if (ws_box_lbl_)
        ws_box_lbl_->setText(tr("WORKSPACES"));
    if (total_box_lbl_)
        total_box_lbl_->setText(tr("TOTAL"));
    if (cache_entries_row_lbl_)
        cache_entries_row_lbl_->setText(tr("Cache Entries"));

    // Data categories panel.
    if (categories_panel_title_)
        categories_panel_title_->setText(tr("DATA CATEGORIES"));
    if (cat_hdr_category_)
        cat_hdr_category_->setText(tr("CATEGORY"));
    if (cat_hdr_entries_)
        cat_hdr_entries_->setText(tr("ENTRIES"));
    if (cat_hdr_action_)
        cat_hdr_action_->setText(tr("ACTION"));

    // File & state management panel.
    if (files_panel_title_)
        files_panel_title_->setText(tr("FILE & STATE MANAGEMENT"));
    if (file_hdr_store_)
        file_hdr_store_->setText(tr("STORE"));
    if (file_hdr_size_)
        file_hdr_size_->setText(tr("SIZE"));
    if (file_hdr_action_)
        file_hdr_action_->setText(tr("ACTION"));
    if (file_row_logs_lbl_)
        file_row_logs_lbl_->setText(tr("Log Files"));
    if (file_row_ws_lbl_)
        file_row_ws_lbl_->setText(tr("Workspace Files (.fwsp)"));
    if (file_row_ui_lbl_)
        file_row_ui_lbl_->setText(tr("Window & UI State"));
    if (cache_subcat_lbl_)
        cache_subcat_lbl_->setText(tr("Cache:"));

    // SQL console panel.
    if (sql_panel_title_)
        sql_panel_title_->setText(tr("SQL CONSOLE (READ-ONLY)"));
    if (sql_hint_lbl_)
        sql_hint_lbl_->setText(tr("Read-only inspection: SELECT, WITH, VALUES, EXPLAIN and schema PRAGMAs. "
                                  "Credential tables are blocked and secret-looking values are masked. "
                                  "Shows the first %1 rows.")
                                   .arg(kStorageConsoleMaxRows));
    if (sql_exec_btn_) {
        sql_exec_btn_->setText(tr("RUN"));
        sql_exec_btn_->setToolTip(tr("Run the statement (Enter)"));
    }
    if (sql_copy_btn_) {
        sql_copy_btn_->setText(tr("COPY"));
        sql_copy_btn_->setToolTip(tr("Copy the result (masked values stay masked) as tab-separated text"));
    }
    if (sql_quick_lbl_)
        sql_quick_lbl_->setText(tr("Quick:"));
    // sql_status_ reflects live query state — leave it as-is.

    // Danger zone panel.
    if (danger_panel_title_)
        danger_panel_title_->setText(tr("DANGER ZONE"));
    if (clear_cache_title_)
        clear_cache_title_->setText(tr("Clear All Cache"));
    if (clear_cache_desc_)
        clear_cache_desc_->setText(tr("Delete all temporary cached data. Will be re-fetched on next access."));
    if (clear_cache_btn_)
        clear_cache_btn_->setText(tr("CLEAR CACHE"));
    if (nuke_title_)
        nuke_title_->setText(tr("Clear ALL User Data"));
    if (nuke_desc_)
        nuke_desc_->setText(
            tr("Permanently delete all databases, files, cache, and UI state. Encrypted API "
               "keys (Settings → Credentials) are kept."));
    if (nuke_btn_)
        nuke_btn_->setText(tr("DELETE ALL"));
}

} // namespace fincept::screens
