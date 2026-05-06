// StorageSection.cpp — disk usage / data categories / file management / SQL console.

#include "screens/settings/StorageSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/StorageManager.h"
#include "storage/cache/CacheManager.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <functional>

namespace fincept::screens {

namespace {

QString format_bytes(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1048576) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1073741824) return QString::number(bytes / 1048576.0, 'f', 1) + " MB";
    return QString::number(bytes / 1073741824.0, 'f', 2) + " GB";
}

// Obsidian-standard panel with 34px header bar
QFrame* make_panel(const QString& title, QWidget* status_widget = nullptr) {
    auto* panel = new QFrame;
    panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(0, 0, 0, 0);
    pvl->setSpacing(0);

    auto* hdr = new QWidget(nullptr);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                           .arg(ui::colors::AMBER()));
    hhl->addWidget(lbl);
    hhl->addStretch();
    if (status_widget) {
        status_widget->setStyleSheet(status_widget->styleSheet() + "background:transparent;");
        hhl->addWidget(status_widget);
    }
    pvl->addWidget(hdr);

    return panel;
}

// Obsidian-standard data row: LABEL ······ VALUE, 26px, bottom border
QWidget* make_data_row(const QString& label_text, QLabel* value_lbl, bool alt_bg = false) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(alt_bg ? ui::colors::ROW_ALT() : "transparent", ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto* lbl = new QLabel(label_text);
    lbl->setStyleSheet(QString("color:%1;font-weight:600;letter-spacing:0.5px;background:transparent;")
                           .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(lbl);
    hl->addStretch();

    value_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_lbl->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;")
                                 .arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(value_lbl);

    return row;
}

QWidget* make_category_row(const QString& name, QLabel* count_lbl, QPushButton* clear_btn, bool alt) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(alt ? ui::colors::ROW_ALT() : ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* lbl = new QLabel(name);
    lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(lbl, 1);

    count_lbl->setObjectName("cat_count");
    count_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    count_lbl->setFixedWidth(70);
    count_lbl->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::CYAN()));
    hl->addWidget(count_lbl);

    clear_btn->setFixedSize(56, 20);
    clear_btn->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;}"
                "QPushButton:hover{background:%1;color:%2;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
    hl->addWidget(clear_btn);

    return row;
}

QWidget* make_group_header(const QString& title) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                           .arg(ui::colors::AMBER_DIM()));
    hl->addWidget(lbl);
    return row;
}

QWidget* make_file_action_row(const QString& name, QLabel* size_lbl, QPushButton* btn, bool alt) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(alt ? ui::colors::ROW_ALT() : ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* lbl = new QLabel(name);
    lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(lbl, 1);

    size_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    size_lbl->setFixedWidth(80);
    size_lbl->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(size_lbl);

    btn->setFixedSize(56, 20);
    btn->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;}"
                "QPushButton:hover{background:%1;color:%2;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
    hl->addWidget(btn);

    return row;
}

} // namespace

StorageSection::StorageSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void StorageSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_storage_stats();
}

void StorageSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    auto* t = new QLabel("STORAGE & DATA MANAGEMENT");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addSpacing(4);

    auto* info = new QLabel("Manage all persistent data, databases, and files. "
                            "Execute SQL queries directly against terminal databases.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info);
    vl->addSpacing(10);

    // ── SECTION 1: DISK USAGE ──────────────────────────────────────────────────
    {
        auto* refresh_btn = new QPushButton("Refresh");
        refresh_btn->setFixedSize(70, 22);
        refresh_btn->setStyleSheet(
            QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;font-weight:700;}"
                    "QPushButton:hover{background:%1;color:%3;}")
                .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));
        connect(refresh_btn, &QPushButton::clicked, this, [this]() { refresh_storage_stats(); });

        auto* panel = make_panel("DISK USAGE", refresh_btn);
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        auto* stat_row = new QWidget(this);
        stat_row->setStyleSheet("background:transparent;");
        auto* shl = new QHBoxLayout(stat_row);
        shl->setContentsMargins(10, 10, 10, 10);
        shl->setSpacing(10);

        auto make_stat_box = [&](const QString& label, QLabel*& val_out) {
            auto* box = new QFrame;
            box->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* bx = new QVBoxLayout(box);
            bx->setContentsMargins(12, 12, 12, 14);
            bx->setSpacing(4);
            bx->setAlignment(Qt::AlignCenter);

            val_out = new QLabel("—");
            val_out->setAlignment(Qt::AlignCenter);
            val_out->setStyleSheet(
                QString("color:%1;font-size:18px;font-weight:700;background:transparent;").arg(ui::colors::CYAN()));
            bx->addWidget(val_out);

            auto* ll = new QLabel(label);
            ll->setAlignment(Qt::AlignCenter);
            ll->setStyleSheet(
                QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
            bx->addWidget(ll);

            shl->addWidget(box, 1);
        };

        make_stat_box("MAIN DB",    storage_main_db_);
        make_stat_box("CACHE DB",   storage_cache_db_);
        make_stat_box("LOG FILES",  storage_log_size_);
        make_stat_box("WORKSPACES", storage_ws_size_);
        make_stat_box("TOTAL",      storage_total_size_);

        bvl->addWidget(stat_row);

        storage_count_ = new QLabel("—");
        bvl->addWidget(make_data_row("Cache Entries", storage_count_, false));

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── SECTION 2: DATA CATEGORIES ─────────────────────────────────────────────
    {
        auto* panel = make_panel("DATA CATEGORIES");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");

        storage_categories_ = new QVBoxLayout(body);
        storage_categories_->setContentsMargins(0, 0, 0, 0);
        storage_categories_->setSpacing(0);

        auto* th = new QWidget(this);
        th->setFixedHeight(26);
        th->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* thl = new QHBoxLayout(th);
        thl->setContentsMargins(12, 0, 12, 0);
        thl->setSpacing(8);
        auto* th1 = new QLabel("CATEGORY");
        th1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th1, 1);
        auto* th2 = new QLabel("ENTRIES");
        th2->setFixedWidth(70);
        th2->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        th2->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th2);
        auto* th3 = new QLabel("ACTION");
        th3->setFixedWidth(56);
        th3->setAlignment(Qt::AlignCenter);
        th3->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th3);
        storage_categories_->addWidget(th);

        auto& sm = StorageManager::instance();
        auto stats = sm.all_stats();

        QString current_group;
        int row_idx = 0;
        for (const auto& cat : stats) {
            if (cat.group != current_group) {
                current_group = cat.group;
                storage_categories_->addWidget(make_group_header(cat.group.toUpper()));
                row_idx = 0;
            }

            auto* count_lbl = new QLabel(QString::number(cat.count));
            auto* clear_btn = new QPushButton("CLR");

            QString cat_id = cat.id;
            QString cat_label = cat.label;
            connect(clear_btn, &QPushButton::clicked, this, [this, cat_id, cat_label, count_lbl]() {
                auto answer = QMessageBox::warning(this, "Clear " + cat_label,
                                                   "Permanently delete all " + cat_label.toLower() +
                                                       "?\n\nThis cannot be undone.",
                                                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes) return;

                auto r = StorageManager::instance().clear_category(cat_id);
                if (r.is_ok()) {
                    count_lbl->setText("0");
                    LOG_INFO("Settings", "Cleared: " + cat_label);
                } else {
                    QMessageBox::critical(this, "Error",
                                          "Failed to clear " + cat_label + ":\n" + QString::fromStdString(r.error()));
                }
                refresh_storage_stats();
            });

            storage_categories_->addWidget(make_category_row(cat.label, count_lbl, clear_btn, (row_idx % 2) == 1));
            ++row_idx;
        }

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── SECTION 3: FILE & STATE MANAGEMENT ─────────────────────────────────────
    {
        auto* panel = make_panel("FILE & STATE MANAGEMENT");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        auto* th = new QWidget(this);
        th->setFixedHeight(26);
        th->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* thl = new QHBoxLayout(th);
        thl->setContentsMargins(12, 0, 12, 0);
        thl->setSpacing(8);
        auto* th1 = new QLabel("STORE");
        th1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th1, 1);
        auto* th2 = new QLabel("SIZE");
        th2->setFixedWidth(80);
        th2->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        th2->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th2);
        auto* th3 = new QLabel("ACTION");
        th3->setFixedWidth(56);
        th3->setAlignment(Qt::AlignCenter);
        th3->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th3);
        bvl->addWidget(th);

        auto add_file_row = [&](const QString& name, QLabel* size_lbl, const QString& confirm_title,
                                const QString& confirm_msg, std::function<void()> action, bool alt) {
            auto* btn = new QPushButton("CLR");
            connect(btn, &QPushButton::clicked, this, [this, confirm_title, confirm_msg, action]() {
                auto answer = QMessageBox::warning(this, confirm_title, confirm_msg,
                                                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes) return;
                action();
                refresh_storage_stats();
            });
            bvl->addWidget(make_file_action_row(name, size_lbl, btn, alt));
        };

        auto* log_sz = new QLabel("—");
        auto* ws_sz  = new QLabel("—");
        auto* qs_lbl = new QLabel("Registry");

        add_file_row("Log Files", log_sz, "Clear Logs",
                     "Clear all application log files?\nCurrent log data will be lost.",
                     []() { StorageManager::instance().clear_log_files(); LOG_INFO("Settings", "Logs cleared"); },
                     false);

        add_file_row("Workspace Files (.fwsp)", ws_sz, "Delete Workspaces",
                     "Delete all saved workspace files?\nThis cannot be undone.",
                     []() { StorageManager::instance().clear_workspace_files(); LOG_INFO("Settings", "Workspaces deleted"); },
                     true);

        add_file_row("Window & UI State", qs_lbl, "Reset UI State",
                     "Reset all window positions, dock layouts, and perspectives?\nTakes effect on next restart.",
                     []() { StorageManager::instance().clear_qsettings(); LOG_INFO("Settings", "QSettings cleared"); },
                     false);

        // Cache subcategories
        auto* cache_row = new QWidget(this);
        cache_row->setFixedHeight(30);
        cache_row->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::ROW_ALT(), ui::colors::BORDER_DIM()));
        auto* chl = new QHBoxLayout(cache_row);
        chl->setContentsMargins(12, 0, 12, 0);
        chl->setSpacing(4);
        auto* cache_label = new QLabel("Cache:");
        cache_label->setStyleSheet(
            QString("color:%1;font-weight:600;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        chl->addWidget(cache_label);

        static const QStringList CACHE_CATS = {"market_data", "news", "quotes", "charts", "general"};
        for (const QString& cat : CACHE_CATS) {
            auto* btn = new QPushButton(cat);
            btn->setFixedHeight(18);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 6px;}"
                                       "QPushButton:hover{color:%4;border-color:%4;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
                                        ui::colors::BORDER_DIM(), ui::colors::AMBER()));
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

    // ── SECTION 4: SQL CONSOLE ─────────────────────────────────────────────────
    {
        auto* panel = make_panel("SQL CONSOLE");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(10, 8, 10, 8);
        bvl->setSpacing(6);

        auto* hint = new QLabel("Execute SQL queries directly against terminal databases. "
                                "Use SELECT to inspect data, or INSERT/UPDATE/DELETE to modify.");
        hint->setWordWrap(true);
        hint->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        bvl->addWidget(hint);

        auto* input_row = new QWidget(this);
        input_row->setStyleSheet("background:transparent;");
        auto* irl = new QHBoxLayout(input_row);
        irl->setContentsMargins(0, 0, 0, 0);
        irl->setSpacing(6);

        sql_db_selector_ = new QComboBox;
        sql_db_selector_->addItem("fincept.db", "main");
        sql_db_selector_->addItem("cache.db",   "cache");
        sql_db_selector_->setFixedWidth(110);
        sql_db_selector_->setStyleSheet(combo_ss());
        irl->addWidget(sql_db_selector_);

        sql_input_ = new QLineEdit;
        sql_input_->setPlaceholderText("SELECT * FROM chat_sessions LIMIT 10");
        sql_input_->setStyleSheet(input_ss());
        irl->addWidget(sql_input_, 1);

        auto* exec_btn = new QPushButton("EXEC");
        exec_btn->setFixedSize(56, 28);
        exec_btn->setStyleSheet(
            QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;font-weight:700;}"
                    "QPushButton:hover{background:%1;color:%3;}")
                .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));
        irl->addWidget(exec_btn);

        bvl->addWidget(input_row);

        sql_status_ = new QLabel("Ready");
        sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        bvl->addWidget(sql_status_);

        // Results area
        auto* results_scroll = new QScrollArea;
        results_scroll->setWidgetResizable(true);
        results_scroll->setFixedHeight(200);
        results_scroll->setStyleSheet(
            QString("QScrollArea{border:1px solid %1;background:%2;}"
                    "QScrollBar:vertical{width:5px;background:transparent;}"
                    "QScrollBar::handle:vertical{background:%3;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
                    "QScrollBar:horizontal{height:5px;background:transparent;}"
                    "QScrollBar::handle:horizontal{background:%3;}"
                    "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}")
                .arg(ui::colors::BORDER_DIM(), ui::colors::BG_BASE(), ui::colors::BORDER_MED()));

        auto* results_widget = new QWidget(this);
        results_widget->setStyleSheet("background:transparent;");
        sql_results_layout_ = new QVBoxLayout(results_widget);
        sql_results_layout_->setContentsMargins(0, 0, 0, 0);
        sql_results_layout_->setSpacing(0);
        sql_results_layout_->addStretch();
        results_scroll->setWidget(results_widget);
        bvl->addWidget(results_scroll);

        // Quick-access table list
        auto* tables_row = new QWidget(this);
        tables_row->setStyleSheet("background:transparent;");
        auto* trhl = new QHBoxLayout(tables_row);
        trhl->setContentsMargins(0, 0, 0, 0);
        trhl->setSpacing(4);
        auto* trl = new QLabel("Quick:");
        trl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        trhl->addWidget(trl);

        static const QStringList QUICK_TABLES = {"chat_sessions", "news_articles", "financial_notes", "portfolios",
                                                 "watchlists",    "pt_portfolios", "workflows",       "settings"};
        for (const QString& tbl : QUICK_TABLES) {
            auto* btn = new QPushButton(tbl);
            btn->setFixedHeight(18);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 4px;}"
                                       "QPushButton:hover{color:%4;border-color:%4;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
                                        ui::colors::BORDER_DIM(), ui::colors::AMBER()));
            connect(btn, &QPushButton::clicked, this, [this, tbl]() {
                sql_input_->setText("SELECT * FROM " + tbl + " LIMIT 20");
                sql_db_selector_->setCurrentIndex(0);
            });
            trhl->addWidget(btn);
        }
        trhl->addStretch();
        bvl->addWidget(tables_row);

        // Execute handler
        auto execute_sql = [this]() {
            QString sql = sql_input_->text().trimmed();
            if (sql.isEmpty()) return;

            while (sql_results_layout_->count() > 0) {
                auto* item = sql_results_layout_->takeAt(0);
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }

            bool use_cache = (sql_db_selector_->currentData().toString() == "cache");

            QString upper = sql.toUpper().trimmed();
            bool is_write = upper.startsWith("INSERT") || upper.startsWith("UPDATE") || upper.startsWith("DELETE") ||
                            upper.startsWith("DROP")   || upper.startsWith("ALTER")  || upper.startsWith("CREATE");

            if (is_write) {
                auto answer = QMessageBox::warning(this, "Execute Write Query",
                                                   "This will modify the database:\n\n" + sql + "\n\nContinue?",
                                                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes) {
                    sql_status_->setText("Cancelled");
                    sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::WARNING()));
                    return;
                }
            }

            QSqlQuery query(use_cache ? CacheDatabase::instance().raw_db() : Database::instance().raw_db());
            if (!query.exec(sql)) {
                sql_status_->setText("Error: " + query.lastError().text());
                sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
                LOG_ERROR("SQL Console", "Query failed: " + query.lastError().text());
                return;
            }

            if (is_write) {
                int affected = query.numRowsAffected();
                sql_status_->setText(QString("OK — %1 row(s) affected").arg(affected));
                sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                refresh_storage_stats();
                LOG_INFO("SQL Console", QString("Write query: %1 rows affected").arg(affected));
                return;
            }

            auto rec = query.record();
            int cols = rec.count();
            if (cols == 0) {
                sql_status_->setText("OK — no columns returned");
                sql_status_->setStyleSheet(
                    QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
                return;
            }

            // Column header
            auto* hdr_row = new QWidget(this);
            hdr_row->setFixedHeight(22);
            hdr_row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* hhl = new QHBoxLayout(hdr_row);
            hhl->setContentsMargins(6, 0, 6, 0);
            hhl->setSpacing(4);
            for (int c = 0; c < cols; ++c) {
                auto* cl = new QLabel(rec.fieldName(c));
                cl->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
                hhl->addWidget(cl, 1);
            }
            sql_results_layout_->addWidget(hdr_row);

            int row_count = 0;
            while (query.next() && row_count < 100) {
                auto* dr = new QWidget(this);
                dr->setFixedHeight(20);
                dr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                      .arg((row_count % 2) ? ui::colors::ROW_ALT() : "transparent",
                                           ui::colors::BORDER_DIM()));
                auto* dhl = new QHBoxLayout(dr);
                dhl->setContentsMargins(6, 0, 6, 0);
                dhl->setSpacing(4);
                for (int c = 0; c < cols; ++c) {
                    QString val = query.value(c).toString();
                    if (val.length() > 60) val = val.left(57) + "...";
                    auto* vl2 = new QLabel(val);
                    vl2->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
                    dhl->addWidget(vl2, 1);
                }
                sql_results_layout_->addWidget(dr);
                ++row_count;
            }

            sql_status_->setText(QString("OK — %1 row(s) returned%2")
                                     .arg(row_count)
                                     .arg(row_count >= 100 ? " (limited to 100)" : ""));
            sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
        };

        connect(exec_btn,    &QPushButton::clicked,    this, execute_sql);
        connect(sql_input_,  &QLineEdit::returnPressed, this, execute_sql);

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── SECTION 5: DANGER ZONE ─────────────────────────────────────────────────
    {
        auto* panel = new QFrame;
        panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                                 .arg(ui::colors::BG_SURFACE(), ui::colors::NEGATIVE_DIM()));
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        auto* hdr = new QWidget(this);
        hdr->setFixedHeight(34);
        hdr->setStyleSheet(QString("background:rgba(220,38,38,0.08);border-bottom:1px solid %1;")
                               .arg(ui::colors::NEGATIVE_DIM()));
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        auto* hlbl = new QLabel("DANGER ZONE");
        hlbl->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                                .arg(ui::colors::NEGATIVE()));
        hhl->addWidget(hlbl);
        pvl->addWidget(hdr);

        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(12, 10, 12, 10);
        bvl->setSpacing(8);

        // Clear all cache
        auto* cache_row = new QWidget(this);
        cache_row->setStyleSheet("background:transparent;");
        auto* cr_hl = new QHBoxLayout(cache_row);
        cr_hl->setContentsMargins(0, 0, 0, 0);
        cr_hl->setSpacing(8);
        auto* cache_desc = new QWidget(this);
        auto* cd_vl = new QVBoxLayout(cache_desc);
        cd_vl->setContentsMargins(0, 0, 0, 0);
        cd_vl->setSpacing(2);
        auto* cd1 = new QLabel("Clear All Cache");
        cd1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
        cd_vl->addWidget(cd1);
        auto* cd2 = new QLabel("Delete all temporary cached data. Will be re-fetched on next access.");
        cd2->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        cd_vl->addWidget(cd2);
        cr_hl->addWidget(cache_desc, 1);
        auto* cache_btn = new QPushButton("CLEAR CACHE");
        cache_btn->setFixedSize(110, 26);
        cache_btn->setStyleSheet(
            QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;}"
                    "QPushButton:hover{background:%1;color:%2;}")
                .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
        connect(cache_btn, &QPushButton::clicked, this, [this]() {
            auto answer = QMessageBox::warning(
                this, "Clear All Cache",
                "Delete all temporary cached data?\nData will be re-fetched on next access.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) return;
            CacheManager::instance().clear();
            refresh_storage_stats();
            LOG_INFO("Settings", "All cache cleared");
        });
        cr_hl->addWidget(cache_btn);
        bvl->addWidget(cache_row);

        bvl->addWidget(make_sep());

        // Nuclear: clear ALL data
        auto* nuke_row = new QWidget(this);
        nuke_row->setStyleSheet("background:transparent;");
        auto* nr_hl = new QHBoxLayout(nuke_row);
        nr_hl->setContentsMargins(0, 0, 0, 0);
        nr_hl->setSpacing(8);
        auto* nuke_desc = new QWidget(this);
        auto* nd_vl = new QVBoxLayout(nuke_desc);
        nd_vl->setContentsMargins(0, 0, 0, 0);
        nd_vl->setSpacing(2);
        auto* nd1 = new QLabel("Clear ALL User Data");
        nd1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::NEGATIVE()));
        nd_vl->addWidget(nd1);
        auto* nd2 = new QLabel("Permanently delete all databases, files, cache, and UI state. OS keychain is preserved.");
        nd2->setWordWrap(true);
        nd2->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        nd_vl->addWidget(nd2);
        nr_hl->addWidget(nuke_desc, 1);
        auto* nuke_btn = new QPushButton("DELETE ALL");
        nuke_btn->setFixedSize(110, 26);
        nuke_btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:2px solid %1;font-weight:700;}"
                                        "QPushButton:hover{background:%2;color:%3;}")
                                    .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE()));
        connect(nuke_btn, &QPushButton::clicked, this, [this]() {
            auto a1 = QMessageBox::critical(this, "Clear ALL User Data",
                                            "WARNING: This will permanently delete ALL data:\n\n"
                                            "  Chat history, notes, reports, watchlists\n"
                                            "  Portfolios, transactions, paper trades\n"
                                            "  Workflows, dashboard layouts\n"
                                            "  News articles, RSS feeds, monitors\n"
                                            "  Data sources, MCP servers\n"
                                            "  Agent configs, LLM configs & profiles\n"
                                            "  App settings, credentials, key-value storage\n"
                                            "  All cache, log files, workspaces, UI state\n\n"
                                            "OS keychain credentials are NOT affected.\n"
                                            "This action CANNOT be undone.",
                                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (a1 != QMessageBox::Yes) return;

            auto a2 = QMessageBox::critical(this, "Final Confirmation",
                                            "ALL data will be permanently deleted.\nAre you absolutely sure?",
                                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (a2 != QMessageBox::Yes) return;

            auto& sm = StorageManager::instance();
            for (const auto& info : sm.all_stats()) sm.clear_category(info.id);
            sm.clear_log_files();
            sm.clear_workspace_files();
            sm.clear_qsettings();
            refresh_storage_stats();
            LOG_INFO("Settings", "ALL user data cleared");
        });
        nr_hl->addWidget(nuke_btn);
        bvl->addWidget(nuke_row);

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

void StorageSection::refresh_storage_stats() {
    auto& sm = StorageManager::instance();

    if (storage_main_db_)    storage_main_db_->setText(format_bytes(sm.main_db_size()));
    if (storage_cache_db_)   storage_cache_db_->setText(format_bytes(sm.cache_db_size()));
    if (storage_log_size_)   storage_log_size_->setText(format_bytes(sm.log_files_size()));
    if (storage_ws_size_)    storage_ws_size_->setText(format_bytes(sm.workspace_files_size()));
    if (storage_total_size_)
        storage_total_size_->setText(format_bytes(sm.main_db_size() + sm.cache_db_size() +
                                                  sm.log_files_size() + sm.workspace_files_size()));

    if (storage_count_)
        storage_count_->setText(QString::number(CacheManager::instance().entry_count()));

    if (storage_categories_) {
        auto stats = sm.all_stats();
        int stat_idx = 0;
        for (int i = 0; i < storage_categories_->count() && stat_idx < stats.size(); ++i) {
            auto* item = storage_categories_->itemAt(i);
            if (!item || !item->widget()) continue;
            auto* count_lbl = item->widget()->findChild<QLabel*>("cat_count");
            if (!count_lbl) continue;
            count_lbl->setText(QString::number(stats[stat_idx].count));
            ++stat_idx;
        }
    }
}

} // namespace fincept::screens
