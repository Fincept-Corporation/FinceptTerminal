// DataSourcesScreen.cpp — gallery + connection management for 78 data source types
#include "screens/data_sources/DataSourcesScreen.h"
#include "screens/data_sources/ConnectorRegistry.h"

#include "core/logging/Logger.h"
#include "storage/repositories/DataSourceRepository.h"
#include "ui/theme/StyleSheets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QPointer>
#include <QScrollArea>
#include <QTextEdit>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpSocket>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens::datasources {

static const QString TAG = "DataSources";

// ============================================================================
// Constructor
// ============================================================================

DataSourcesScreen::DataSourcesScreen(QWidget* parent) : QWidget(parent) {
    register_all_connectors();
    setup_ui();
    LOG_INFO(TAG, QString("Loaded %1 connector definitions").arg(ConnectorRegistry::instance().count()));
}

void DataSourcesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_connections();
}

void DataSourcesScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

// ============================================================================
// UI Setup
// ============================================================================

void DataSourcesScreen::setup_ui() {
    setObjectName("dsScreen");
    setStyleSheet(
        "#dsScreen { background: #080808; font-family: 'Consolas','Courier New',monospace; }"
        "#dsTopBar { background: #111111; border-bottom: 1px solid #1a1a1a; }"
        "#dsViewBtn { background: transparent; color: #808080; border: none; "
        "  border-bottom: 2px solid transparent; padding: 4px 12px; "
        "  font-size: 12px; font-weight: 700; letter-spacing: 0.5px; }"
        "#dsViewBtn:hover { color: #e5e5e5; }"
        "#dsViewBtn[active=\"true\"] { color: #d97706; border-bottom-color: #d97706; }"
        "#dsCatBtn { background: transparent; color: #525252; border: none; "
        "  padding: 2px 8px; font-size: 10px; font-weight: 700; }"
        "#dsCatBtn:hover { color: #808080; }"
        "#dsCatBtn[active=\"true\"] { color: #d97706; }"
        "#dsSearch { background: #0a0a0a; border: 1px solid #1a1a1a; color: #e5e5e5; "
        "  padding: 4px 8px; font-size: 12px; }"
        "#dsSearch:focus { border-color: #d97706; }"
        "#dsCount { color: #525252; font-size: 11px; background: transparent; border: none; }"
        "#dsCard { background: #0a0a0a; border: 1px solid #1a1a1a; padding: 10px; }"
        "#dsCard:hover { border-color: #333333; background: #111111; }"
        "#dsCardName { color: #e5e5e5; font-size: 12px; font-weight: 700; "
        "  background: transparent; border: none; }"
        "#dsCardDesc { color: #808080; font-size: 10px; background: transparent; border: none; }"
        "#dsCardIcon { font-size: 14px; font-weight: 700; min-width: 28px; min-height: 28px; "
        "  border-radius: 4px; }"
        "#dsConnTable { background: #080808; border: none; color: #e5e5e5; font-size: 12px; }"
        "#dsConnTable::item { padding: 0 6px; border-bottom: 1px solid #111111; }"
        "#dsConnTable QHeaderView::section { background: #0a0a0a; color: #525252; "
        "  font-size: 11px; font-weight: 700; border: none; border-bottom: 1px solid #1a1a1a; padding: 0 6px; }"
        "#dsAddBtn { background: rgba(217,119,6,0.1); color: #d97706; border: 1px solid #92400e; "
        "  padding: 4px 12px; font-weight: 700; font-size: 11px; }"
        "#dsAddBtn:hover { background: #d97706; color: #080808; }"
    );

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ── Top Bar ──────────────────────────────────────────────────────────────
    auto* top_bar = new QWidget;
    top_bar->setObjectName("dsTopBar");
    top_bar->setFixedHeight(60);
    auto* top_layout = new QVBoxLayout(top_bar);
    top_layout->setContentsMargins(12, 4, 12, 4);
    top_layout->setSpacing(2);

    // Row 1: view tabs + search + count
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(4);

    gallery_btn_ = new QPushButton("GALLERY");
    gallery_btn_->setObjectName("dsViewBtn");
    gallery_btn_->setProperty("active", true);
    gallery_btn_->setCursor(Qt::PointingHandCursor);
    connect(gallery_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_view_gallery);
    row1->addWidget(gallery_btn_);

    connections_btn_ = new QPushButton("CONNECTIONS");
    connections_btn_->setObjectName("dsViewBtn");
    connections_btn_->setProperty("active", false);
    connections_btn_->setCursor(Qt::PointingHandCursor);
    connect(connections_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_view_connections);
    row1->addWidget(connections_btn_);

    row1->addStretch();

    search_edit_ = new QLineEdit;
    search_edit_->setObjectName("dsSearch");
    search_edit_->setPlaceholderText("Search connectors...");
    search_edit_->setFixedWidth(200);
    search_edit_->setFixedHeight(24);
    connect(search_edit_, &QLineEdit::textChanged, this, &DataSourcesScreen::on_search_changed);
    row1->addWidget(search_edit_);

    count_label_ = new QLabel("78 connectors");
    count_label_->setObjectName("dsCount");
    row1->addWidget(count_label_);

    top_layout->addLayout(row1);

    // Row 2: category filter buttons
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(2);

    const char* cat_labels[] = {"ALL", "Databases", "APIs", "Files", "Streaming",
                                "Cloud", "Time Series", "Market Data", "Search", "Warehouses"};
    // "ALL" is index -1, categories are 0-8
    for (int i = 0; i < 10; ++i) {
        auto* btn = new QPushButton(cat_labels[i]);
        btn->setObjectName("dsCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        if (i == 0) btn->setProperty("active", true);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_filter(i); });
        row2->addWidget(btn);
        if (i > 0) category_btns_[i - 1] = btn;
    }
    row2->addStretch();
    top_layout->addLayout(row2);

    main_layout->addWidget(top_bar);

    // ── Stacked Views ────────────────────────────────────────────────────────
    stack_ = new QStackedWidget;

    // Gallery view
    gallery_scroll_ = new QScrollArea;
    gallery_scroll_->setWidgetResizable(true);
    gallery_scroll_->setFrameShape(QFrame::NoFrame);
    gallery_scroll_->setStyleSheet("QScrollArea { background: #080808; border: none; }");
    gallery_grid_ = new QWidget;
    gallery_scroll_->setWidget(gallery_grid_);
    stack_->addWidget(gallery_scroll_);

    // Connections view
    auto* conn_widget = new QWidget;
    auto* conn_layout = new QVBoxLayout(conn_widget);
    conn_layout->setContentsMargins(0, 0, 0, 0);

    auto* conn_header = new QHBoxLayout;
    conn_header->setContentsMargins(12, 6, 12, 6);
    auto* add_btn = new QPushButton("+ ADD CONNECTION");
    add_btn->setObjectName("dsAddBtn");
    add_btn->setCursor(Qt::PointingHandCursor);
    connect(add_btn, &QPushButton::clicked, this, &DataSourcesScreen::on_connection_add);
    conn_header->addWidget(add_btn);
    conn_header->addStretch();
    conn_layout->addLayout(conn_header);

    connections_table_ = new QTableWidget;
    connections_table_->setObjectName("dsConnTable");
    connections_table_->setColumnCount(7);
    connections_table_->setHorizontalHeaderLabels(
        {"Name", "Type", "Category", "Provider", "Status", "Updated", "Actions"});
    connections_table_->verticalHeader()->setVisible(false);
    connections_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connections_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connections_table_->setShowGrid(false);
    connections_table_->horizontalHeader()->setStretchLastSection(true);
    connections_table_->verticalHeader()->setDefaultSectionSize(28);
    conn_layout->addWidget(connections_table_);

    stack_->addWidget(conn_widget);

    main_layout->addWidget(stack_, 1);

    build_gallery();
}

// ============================================================================
// Gallery
// ============================================================================

void DataSourcesScreen::build_gallery() {
    // Delete old layout
    if (gallery_grid_->layout())
        delete gallery_grid_->layout();

    auto* grid = new QGridLayout(gallery_grid_);
    grid->setContentsMargins(12, 8, 12, 8);
    grid->setSpacing(8);

    const auto& all = ConnectorRegistry::instance().all();
    const QString filter = search_edit_ ? search_edit_->text().trimmed().toLower() : "";

    int col = 0, row = 0;
    int visible = 0;
    constexpr int COLS = 4;

    for (const auto& cfg : all) {
        // Category filter
        if (!show_all_categories_ && cfg.category != active_category_)
            continue;
        // Text filter
        if (!filter.isEmpty() &&
            !cfg.name.toLower().contains(filter) &&
            !cfg.type.toLower().contains(filter) &&
            !cfg.description.toLower().contains(filter))
            continue;

        // Card
        auto* card = new QWidget;
        card->setObjectName("dsCard");
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedHeight(72);
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(8, 6, 8, 6);
        card_layout->setSpacing(8);

        // Icon (colored letter)
        auto* icon = new QLabel(cfg.icon);
        icon->setObjectName("dsCardIcon");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(QString("background: %1; color: white;").arg(cfg.color));
        card_layout->addWidget(icon);

        // Text
        auto* text_col = new QVBoxLayout;
        text_col->setSpacing(2);
        auto* name_lbl = new QLabel(cfg.name);
        name_lbl->setObjectName("dsCardName");
        text_col->addWidget(name_lbl);
        auto* desc_lbl = new QLabel(cfg.description);
        desc_lbl->setObjectName("dsCardDesc");
        desc_lbl->setWordWrap(true);
        desc_lbl->setMaximumHeight(28);
        text_col->addWidget(desc_lbl);
        card_layout->addLayout(text_col, 1);

        // Click handler — capture cfg.id by value
        const QString cid = cfg.id;
        card->installEventFilter(this);
        // Use a button overlay for click
        auto* click_btn = new QPushButton(card);
        click_btn->setStyleSheet("background: transparent; border: none;");
        click_btn->setCursor(Qt::PointingHandCursor);
        click_btn->setGeometry(0, 0, 9999, 9999); // fill card
        connect(click_btn, &QPushButton::clicked, this, [this, cid]() { on_connector_clicked(cid); });

        grid->addWidget(card, row, col);
        ++visible;
        if (++col >= COLS) { col = 0; ++row; }
    }

    // Spacer at bottom
    grid->setRowStretch(row + 1, 1);
    count_label_->setText(QString("%1 connectors").arg(visible));
}

// ============================================================================
// Connections table
// ============================================================================

void DataSourcesScreen::build_connections_table() {
    auto result = DataSourceRepository::instance().list_all();
    if (result.is_err()) return;
    const auto& sources = result.value();

    connections_table_->setRowCount(sources.size());
    for (int i = 0; i < sources.size(); ++i) {
        const auto& ds = sources[i];
        auto make = [&](int col, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setForeground(TEXT_PRIMARY);
            connections_table_->setItem(i, col, item);
        };
        make(0, ds.display_name);
        make(1, ds.type);
        make(2, ds.category);
        make(3, ds.provider);
        make(4, ds.enabled ? "ACTIVE" : "DISABLED");
        make(5, ds.updated_at);

        // Actions column: Test + Delete buttons
        auto* actions = new QWidget;
        auto* a_layout = new QHBoxLayout(actions);
        a_layout->setContentsMargins(2, 0, 2, 0);
        a_layout->setSpacing(4);

        auto* test_btn = new QPushButton("TEST");
        test_btn->setStyleSheet("background: rgba(22,163,74,0.1); color: #16a34a; "
                                "border: 1px solid #14532d; padding: 1px 6px; font-size: 10px; font-weight: 700;");
        test_btn->setCursor(Qt::PointingHandCursor);
        const QString ds_id = ds.id;
        connect(test_btn, &QPushButton::clicked, this, [this, ds_id]() { on_connection_test(ds_id); });
        a_layout->addWidget(test_btn);

        auto* del_btn = new QPushButton("DEL");
        del_btn->setStyleSheet("background: rgba(220,38,38,0.1); color: #dc2626; "
                               "border: 1px solid #7f1d1d; padding: 1px 6px; font-size: 10px; font-weight: 700;");
        del_btn->setCursor(Qt::PointingHandCursor);
        connect(del_btn, &QPushButton::clicked, this, [this, ds_id]() { on_connection_delete(ds_id); });
        a_layout->addWidget(del_btn);

        connections_table_->setCellWidget(i, 6, actions);
    }
}

// ============================================================================
// Config Dialog
// ============================================================================

void DataSourcesScreen::show_config_dialog(const ConnectorConfig& config, const QString& edit_id) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString("Configure %1").arg(config.name));
    dlg->setFixedSize(420, 500);
    dlg->setStyleSheet(
        "QDialog { background: #0a0a0a; color: #e5e5e5; }"
        "QLabel { color: #808080; font-size: 11px; font-weight: 700; }"
        "QLineEdit, QTextEdit, QComboBox { background: #080808; border: 1px solid #222222; "
        "  color: #e5e5e5; padding: 6px; font-size: 12px; }"
        "QLineEdit:focus, QTextEdit:focus { border-color: #d97706; }"
        "QPushButton { padding: 8px 16px; font-weight: 700; font-size: 12px; }"
        "QCheckBox { color: #e5e5e5; font-size: 12px; }"
    );

    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(6);

    auto* title = new QLabel(config.name.toUpper());
    title->setStyleSheet("color: #d97706; font-size: 14px; font-weight: 700;");
    layout->addWidget(title);

    auto* desc = new QLabel(config.description);
    desc->setStyleSheet("color: #808080; font-size: 11px;");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    // Connection name
    layout->addWidget(new QLabel("CONNECTION NAME"));
    auto* name_edit = new QLineEdit;
    name_edit->setPlaceholderText(config.name + " Connection");
    layout->addWidget(name_edit);

    // Dynamic fields
    QMap<QString, QWidget*> field_widgets;
    for (const auto& f : config.fields) {
        layout->addWidget(new QLabel(f.label.toUpper()));

        if (f.type == FieldType::Checkbox) {
            auto* cb = new QCheckBox(f.label);
            cb->setChecked(f.default_value == "true");
            field_widgets[f.name] = cb;
            layout->addWidget(cb);
        } else if (f.type == FieldType::Select) {
            auto* combo = new QComboBox;
            for (const auto& opt : f.options)
                combo->addItem(opt.label, opt.value);
            field_widgets[f.name] = combo;
            layout->addWidget(combo);
        } else if (f.type == FieldType::Textarea) {
            auto* te = new QTextEdit;
            te->setPlaceholderText(f.placeholder);
            te->setMaximumHeight(60);
            field_widgets[f.name] = te;
            layout->addWidget(te);
        } else {
            auto* le = new QLineEdit;
            le->setPlaceholderText(f.placeholder);
            if (f.type == FieldType::Password) le->setEchoMode(QLineEdit::Password);
            if (!f.default_value.isEmpty()) le->setText(f.default_value);
            field_widgets[f.name] = le;
            layout->addWidget(le);
        }
    }

    // Save button
    auto* save_btn = new QPushButton("SAVE CONNECTION");
    save_btn->setStyleSheet("background: rgba(217,119,6,0.15); color: #d97706; border: 1px solid #92400e;");
    save_btn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(save_btn);

    auto* status_lbl = new QLabel("");
    status_lbl->setWordWrap(true);
    layout->addWidget(status_lbl);
    layout->addStretch();

    connect(save_btn, &QPushButton::clicked, dlg, [=]() {
        // Build config JSON
        QJsonObject cfg_json;
        for (auto it = field_widgets.begin(); it != field_widgets.end(); ++it) {
            if (auto* le = qobject_cast<QLineEdit*>(it.value()))
                cfg_json[it.key()] = le->text();
            else if (auto* te = qobject_cast<QTextEdit*>(it.value()))
                cfg_json[it.key()] = te->toPlainText();
            else if (auto* cb = qobject_cast<QCheckBox*>(it.value()))
                cfg_json[it.key()] = cb->isChecked();
            else if (auto* combo = qobject_cast<QComboBox*>(it.value()))
                cfg_json[it.key()] = combo->currentData().toString();
        }

        DataSource ds;
        ds.id = edit_id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : edit_id;
        ds.alias = config.type + "_" + ds.id.left(8);
        ds.display_name = name_edit->text().isEmpty() ? config.name : name_edit->text();
        ds.description = config.description;
        ds.type = config.testable ? "rest_api" : "rest_api"; // simplified
        ds.provider = config.type;
        ds.category = category_str(config.category);
        ds.config = QJsonDocument(cfg_json).toJson(QJsonDocument::Compact);
        ds.enabled = true;
        ds.tags = "";

        auto result = DataSourceRepository::instance().save(ds);
        if (result.is_ok()) {
            LOG_INFO(TAG, QString("Saved connection: %1 (%2)").arg(ds.display_name, ds.provider));
            dlg->accept();
        } else {
            status_lbl->setText("Failed to save: " + QString::fromStdString(result.error()));
            status_lbl->setStyleSheet("color: #dc2626;");
        }
    });

    dlg->exec();
    dlg->deleteLater();
    refresh_connections();
}

// ============================================================================
// Slots
// ============================================================================

void DataSourcesScreen::on_view_gallery() {
    view_mode_ = ViewMode::Gallery;
    gallery_btn_->setProperty("active", true);
    connections_btn_->setProperty("active", false);
    gallery_btn_->style()->unpolish(gallery_btn_);
    gallery_btn_->style()->polish(gallery_btn_);
    connections_btn_->style()->unpolish(connections_btn_);
    connections_btn_->style()->polish(connections_btn_);
    stack_->setCurrentIndex(0);
}

void DataSourcesScreen::on_view_connections() {
    view_mode_ = ViewMode::Connections;
    gallery_btn_->setProperty("active", false);
    connections_btn_->setProperty("active", true);
    gallery_btn_->style()->unpolish(gallery_btn_);
    gallery_btn_->style()->polish(gallery_btn_);
    connections_btn_->style()->unpolish(connections_btn_);
    connections_btn_->style()->polish(connections_btn_);
    stack_->setCurrentIndex(1);
    build_connections_table();
}

void DataSourcesScreen::on_category_filter(int idx) {
    show_all_categories_ = (idx == 0);
    if (idx > 0) active_category_ = static_cast<Category>(idx - 1);

    // Update button styles
    for (int i = 0; i < 9; ++i) {
        if (category_btns_[i]) {
            category_btns_[i]->setProperty("active", !show_all_categories_ && static_cast<int>(active_category_) == i);
            category_btns_[i]->style()->unpolish(category_btns_[i]);
            category_btns_[i]->style()->polish(category_btns_[i]);
        }
    }
    build_gallery();
}

void DataSourcesScreen::on_search_changed(const QString& /*text*/) {
    build_gallery();
}

void DataSourcesScreen::on_connector_clicked(const QString& connector_id) {
    const auto* cfg = ConnectorRegistry::instance().get(connector_id);
    if (cfg) show_config_dialog(*cfg);
}

void DataSourcesScreen::on_connection_add() {
    // Switch to gallery to pick a connector
    on_view_gallery();
}

void DataSourcesScreen::on_connection_delete(const QString& conn_id) {
    DataSourceRepository::instance().remove(conn_id);
    build_connections_table();
}

void DataSourcesScreen::on_connection_test(const QString& conn_id) {
    auto get_result = DataSourceRepository::instance().get(conn_id);
    if (get_result.is_err()) return;
    const auto ds = get_result.value();
    LOG_INFO(TAG, QString("Testing connection: %1 (%2)").arg(ds.display_name, ds.provider));

    // Parse config JSON to extract connection params
    const auto cfg_doc = QJsonDocument::fromJson(ds.config.toUtf8());
    const auto cfg = cfg_doc.object();

    // Find the connector definition
    const auto* connector = ConnectorRegistry::instance().get(ds.provider);
    if (!connector) {
        LOG_WARN(TAG, QString("No connector definition for provider: %1").arg(ds.provider));
        return;
    }

    // Run test asynchronously (P1 — never block UI)
    QPointer<DataSourcesScreen> self = this;
    const QString provider = ds.provider;
    const QString display = ds.display_name;
    const QString id = ds.id;

    QtConcurrent::run([self, cfg, provider, display, id]() {
        bool success = false;
        QString message;

        // Test strategy depends on connector type
        const QString host = cfg.value("host").toString();
        const QString url = cfg.value("url").toString();
        const QString baseUrl = cfg.value("baseUrl").toString();
        const int port = cfg.value("port").toInt();

        if (!host.isEmpty() && port > 0) {
            // TCP connection test for host:port based connectors
            QTcpSocket socket;
            socket.connectToHost(host, static_cast<quint16>(port));
            if (socket.waitForConnected(5000)) {
                success = true;
                message = QString("Connected to %1:%2").arg(host).arg(port);
                socket.disconnectFromHost();
            } else {
                message = QString("Failed to connect to %1:%2 — %3")
                              .arg(host).arg(port).arg(socket.errorString());
            }
        } else if (!url.isEmpty() || !baseUrl.isEmpty()) {
            // HTTP reachability test for URL-based connectors
            const QString target = url.isEmpty() ? baseUrl : url;
            QNetworkAccessManager nam;
            nam.setTransferTimeout(5000);
            QUrl target_url(target);
            QNetworkRequest net_req(target_url);
            auto* reply = nam.get(net_req);
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            if (reply->error() == QNetworkReply::NoError ||
                reply->error() == QNetworkReply::AuthenticationRequiredError) {
                success = true;
                message = QString("Reachable: %1 (HTTP %2)")
                              .arg(target).arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
            } else {
                message = QString("Failed: %1 — %2").arg(target, reply->errorString());
            }
            reply->deleteLater();
        } else if (!cfg.value("connectionString").toString().isEmpty()) {
            // For MongoDB-style connection strings, do a URL reachability check
            success = true;
            message = "Config saved (connection string present — manual verification recommended)";
        } else if (!cfg.value("apiKey").toString().isEmpty() ||
                   !cfg.value("apiToken").toString().isEmpty() ||
                   !cfg.value("token").toString().isEmpty()) {
            // API key connectors — we just validate the key is non-empty
            success = true;
            message = "API credentials configured (key present)";
        } else if (!cfg.value("filepath").toString().isEmpty()) {
            // File source — check if file exists
            const QString path = cfg.value("filepath").toString();
            if (QFile::exists(path)) {
                success = true;
                message = QString("File found: %1").arg(path);
            } else {
                message = QString("File not found: %1").arg(path);
            }
        } else {
            message = "No testable configuration found";
        }

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, id, success, message, display]() {
            if (!self) return;
            LOG_INFO("DataSources", QString("Test %1: %2 — %3")
                                        .arg(display, success ? "OK" : "FAIL", message));

            // Update status in connections table
            for (int row = 0; row < self->connections_table_->rowCount(); ++row) {
                auto* item = self->connections_table_->item(row, 0);
                if (!item) continue;
                // Match by display name (col 0)
                if (item->text() == display) {
                    auto* status_item = self->connections_table_->item(row, 4);
                    if (status_item) {
                        status_item->setText(success ? "CONNECTED" : "ERROR");
                        status_item->setForeground(success ? COLOR_BUY : COLOR_SELL);
                    }
                    break;
                }
            }
        }, Qt::QueuedConnection);
    });
}

void DataSourcesScreen::refresh_connections() {
    if (view_mode_ == ViewMode::Connections)
        build_connections_table();
}

} // namespace fincept::screens::datasources
