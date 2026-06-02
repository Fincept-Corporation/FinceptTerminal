// src/screens/data_mapping/DataMappingScreen_Layout.cpp
//
// UI construction for the DataMappingScreen wizard — header, step bar, the
// list/create/template views, every per-step panel (api config, schema,
// field mapping, cache, test & save), nav footer, and status bar.
//
// Part of the partial-class split of DataMappingScreen.cpp.

#include "screens/data_mapping/DataMappingScreen.h"
#include "screens/data_mapping/DataMappingScreen_internal.h"

#include "core/logging/Logger.h"
#include "storage/repositories/DataMappingRepository.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::screens::data_mapping_internal;

QWidget* DataMappingScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("dmHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    header_title_ = new QLabel(tr("DATA MAPPING ENGINE"));
    header_title_->setObjectName("dmHeaderTitle");
    header_sub_ = new QLabel(tr("API CONFIGURATION & SCHEMA TRANSFORMATION"));
    header_sub_->setObjectName("dmHeaderSub");
    title_col->addWidget(header_title_);
    title_col->addWidget(header_sub_);
    hl->addLayout(title_col);
    hl->addSpacing(24);

    const QStringList views = {tr("MAPPINGS"), tr("TEMPLATES"), tr("CREATE")};
    for (int i = 0; i < views.size(); ++i) {
        auto* btn = new QPushButton(views[i]);
        btn->setObjectName("dmViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        hl->addWidget(btn);
        view_btns_.append(btn);
    }

    hl->addStretch(1);

    header_badge_ = new QLabel(tr("7 SCHEMAS"));
    header_badge_->setObjectName("dmHeaderBadge");
    hl->addWidget(header_badge_);

    return bar;
}

QWidget* DataMappingScreen::create_step_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("dmStepBar");
    bar->setFixedHeight(32);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(0);

    const QStringList steps = {tr("API CONFIG"), tr("SCHEMA"), tr("FIELD MAPPING"), tr("CACHE"), tr("TEST & SAVE")};
    for (int i = 0; i < steps.size(); ++i) {
        auto* btn = new QPushButton(QString("%1. %2").arg(i + 1).arg(steps[i]));
        btn->setObjectName("dmStepBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        btn->setProperty("complete", false);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_step_changed(i); });
        hl->addWidget(btn);
        step_btns_.append(btn);
    }

    hl->addStretch(1);
    return bar;
}

QWidget* DataMappingScreen::create_main_area() {
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* left = create_left_panel();
    left->setMinimumWidth(180);
    left->setMaximumWidth(220);

    auto* center = new QWidget(this);
    auto* cvl = new QVBoxLayout(center);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(0);

    view_stack_ = new QStackedWidget;
    // Stack order must match header buttons: MAPPINGS=0, TEMPLATES=1, CREATE=2.
    view_stack_->addWidget(create_list_view());

    view_stack_->addWidget(create_template_view());

    auto* create_scroll = new QScrollArea;
    create_scroll->setWidgetResizable(true);
    auto* create_content = new QWidget(this);
    auto* ccl = new QVBoxLayout(create_content);
    ccl->setContentsMargins(16, 16, 16, 16);
    ccl->setSpacing(16);

    step_stack_ = new QStackedWidget;
    step_stack_->addWidget(create_api_config_panel());
    step_stack_->addWidget(create_schema_panel());
    step_stack_->addWidget(create_field_mapping_panel());
    step_stack_->addWidget(create_cache_panel());
    step_stack_->addWidget(create_test_save_panel());
    ccl->addWidget(step_stack_);
    ccl->addStretch(1);
    create_scroll->setWidget(create_content);
    view_stack_->addWidget(create_scroll);
    cvl->addWidget(view_stack_, 1);

    auto* right = create_right_panel();
    right->setMinimumWidth(190);
    right->setMaximumWidth(220);

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({200, 700, 200});

    return splitter;
}

QWidget* DataMappingScreen::create_left_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dmLeftPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    left_panel_title_ = new QLabel(tr("WIZARD STEPS"));
    left_panel_title_->setObjectName("dmPanelTitle");
    vl->addWidget(left_panel_title_);

    const QStringList step_labels = {tr("API Configuration"), tr("Schema Selection"), tr("Field Mapping"),
                                     tr("Cache Settings"), tr("Test & Save")};
    const QStringList step_icons = {"1", "2", "3", "4", "5"};
    for (int i = 0; i < step_labels.size(); ++i) {
        auto* btn = new QPushButton(step_icons[i] + "  " + step_labels[i]);
        btn->setObjectName("dmSecondaryBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("text-align: left; padding: 6px 10px; border: none; border-bottom: 1px solid " +
                           QLatin1String(colors::BORDER_DIM()) + ";");
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            on_view_changed(2); // switch to create view
            on_step_changed(i);
        });
        vl->addWidget(btn);
        left_step_btns_.append(btn);
    }

    vl->addStretch(1);

    auto* stats = new QWidget(this);
    auto* sl = new QVBoxLayout(stats);
    sl->setContentsMargins(10, 8, 10, 8);
    sl->setSpacing(4);

    quick_stats_title_ = new QLabel(tr("QUICK STATS"));
    quick_stats_title_->setObjectName("dmLabel");
    sl->addWidget(quick_stats_title_);

    status_mappings_ = new QLabel(tr("Saved: %1").arg(0));
    status_mappings_->setObjectName("dmInfoValue");
    sl->addWidget(status_mappings_);

    schemas_count_lbl_ = new QLabel(tr("Schemas: %1").arg(7));
    schemas_count_lbl_->setObjectName("dmInfoLabel");
    sl->addWidget(schemas_count_lbl_);

    parsers_count_lbl_ = new QLabel(tr("Parsers: %1").arg(6));
    parsers_count_lbl_->setObjectName("dmInfoLabel");
    sl->addWidget(parsers_count_lbl_);

    vl->addWidget(stats);
    return panel;
}

QWidget* DataMappingScreen::create_right_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dmRightPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    right_panel_title_ = new QLabel(tr("SYSTEM"));
    right_panel_title_->setObjectName("dmPanelTitle");
    vl->addWidget(right_panel_title_);

    auto* info = new QWidget(this);
    auto* il = new QVBoxLayout(info);
    il->setContentsMargins(10, 8, 10, 8);
    il->setSpacing(6);

    mapping_engine_title_ = new QLabel(tr("MAPPING ENGINE"));
    mapping_engine_title_->setObjectName("dmLabel");
    il->addWidget(mapping_engine_title_);

    right_engine_status_ = new QLabel(tr("ONLINE"));
    right_engine_status_->setObjectName("dmSuccessBadge");
    il->addWidget(right_engine_status_);

    il->addSpacing(8);

    parser_engines_title_ = new QLabel(tr("PARSER ENGINES"));
    parser_engines_title_->setObjectName("dmLabel");
    il->addWidget(parser_engines_title_);

    // Parser names are proper nouns (data); only the " — READY" suffix translates.
    const QStringList parsers = {"JSONPath", "JSONata", "JMESPath", "Direct", "JavaScript", "Regex"};
    for (const auto& p : parsers) {
        auto* lbl = new QLabel(tr("%1 — READY").arg(p));
        lbl->setObjectName("dmInfoLabel");
        il->addWidget(lbl);
    }

    il->addSpacing(8);

    // Security
    security_title_ = new QLabel(tr("SECURITY"));
    security_title_->setObjectName("dmLabel");
    il->addWidget(security_title_);
    auto* sec_val = new QLabel("AES-256-GCM"); // crypto standard name — not translated
    sec_val->setObjectName("dmInfoValue");
    il->addWidget(sec_val);

    il->addSpacing(8);

    current_mapping_title_ = new QLabel(tr("CURRENT MAPPING"));
    current_mapping_title_->setObjectName("dmLabel");
    il->addWidget(current_mapping_title_);

    right_schema_info_ = new QLabel(tr("Schema: --"));
    right_schema_info_->setObjectName("dmInfoLabel");
    il->addWidget(right_schema_info_);

    right_fields_info_ = new QLabel(tr("Fields: --"));
    right_fields_info_->setObjectName("dmInfoLabel");
    il->addWidget(right_fields_info_);

    right_test_info_ = new QLabel(tr("Test: --"));
    right_test_info_->setObjectName("dmInfoLabel");
    il->addWidget(right_test_info_);

    il->addStretch(1);
    vl->addWidget(info, 1);
    return panel;
}


QWidget* DataMappingScreen::create_api_config_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("1");
    icon->setObjectName("dmPanelHeaderIcon");
    api_panel_title_ = new QLabel(tr("API CONFIGURATION"));
    api_panel_title_->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(api_panel_title_);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    api_name_ = new QLineEdit;
    api_name_->setPlaceholderText(tr("e.g. Upstox OHLCV"));
    api_name_row_ = create_form_row(tr("MAPPING NAME"), api_name_);
    bl->addWidget(api_name_row_);

    api_base_url_ = new QLineEdit;
    api_base_url_->setPlaceholderText("https://api.example.com");
    api_endpoint_ = new QLineEdit;
    api_endpoint_->setPlaceholderText("/v2/historical-candle/{symbol}/{interval}");
    api_base_url_row_ = create_form_row(tr("BASE URL"), api_base_url_);
    api_endpoint_row_ = create_form_row(tr("ENDPOINT"), api_endpoint_);
    bl->addWidget(create_form_two_col(api_base_url_row_, api_endpoint_row_));

    api_method_ = new QComboBox;
    api_method_->addItems({"GET", "POST", "PUT", "DELETE", "PATCH"}); // HTTP verbs — protocol values
    api_auth_type_ = new QComboBox;
    // Auth types are matched by text against template configs — keep untranslated.
    api_auth_type_->addItems({"None", "API Key", "Bearer Token", "Basic Auth", "OAuth2"});
    api_method_row_ = create_form_row(tr("HTTP METHOD"), api_method_);
    api_auth_type_row_ = create_form_row(tr("AUTHENTICATION"), api_auth_type_);
    bl->addWidget(create_form_two_col(api_method_row_, api_auth_type_row_));

    api_auth_value_ = new QLineEdit;
    api_auth_value_->setPlaceholderText(tr("Token / API Key value"));
    api_auth_value_->setEchoMode(QLineEdit::Password);
    api_auth_value_row_ = create_form_row(tr("AUTH VALUE"), api_auth_value_);
    bl->addWidget(api_auth_value_row_);

    api_headers_ = new QPlainTextEdit;
    api_headers_->setPlaceholderText("Content-Type: application/json\nAccept: application/json");
    api_headers_->setMaximumHeight(60);
    api_headers_row_ = create_form_row(tr("HEADERS (one per line)"), api_headers_);
    bl->addWidget(api_headers_row_);

    api_body_ = new QPlainTextEdit;
    api_body_->setPlaceholderText("{\"symbol\": \"AAPL\"}");
    api_body_->setMaximumHeight(60);
    api_body_row_ = create_form_row(tr("REQUEST BODY (JSON)"), api_body_);
    bl->addWidget(api_body_row_);

    api_timeout_ = new QSpinBox;
    api_timeout_->setRange(1, 120);
    api_timeout_->setValue(30);
    api_timeout_->setSuffix(tr(" sec"));
    api_timeout_row_ = create_form_row(tr("TIMEOUT"), api_timeout_);
    bl->addWidget(api_timeout_row_);

    auto* test_row = new QWidget(this);
    auto* trl = new QHBoxLayout(test_row);
    trl->setContentsMargins(0, 0, 0, 0);
    trl->setSpacing(8);
    api_test_btn_ = new QPushButton(tr("TEST API REQUEST"));
    api_test_btn_->setObjectName("dmCalcBtn");
    api_test_btn_->setCursor(Qt::PointingHandCursor);
    connect(api_test_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_test_api);
    api_test_status_ = new QLabel;
    api_test_status_->setObjectName("dmInfoLabel");
    trl->addWidget(api_test_btn_);
    trl->addWidget(api_test_status_);
    trl->addStretch(1);
    bl->addWidget(test_row);

    vl->addWidget(body);
    return panel;
}

QWidget* DataMappingScreen::create_schema_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("2");
    icon->setObjectName("dmPanelHeaderIcon");
    schema_panel_title_ = new QLabel(tr("SCHEMA SELECTION"));
    schema_panel_title_->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(schema_panel_title_);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    schema_type_ = new QComboBox;
    schema_type_->addItems({tr("Predefined Schema"), tr("Custom Schema")});
    schema_type_row_ = create_form_row(tr("SCHEMA TYPE"), schema_type_);
    bl->addWidget(schema_type_row_);

    schema_select_ = new QComboBox;
    for (const auto& s : schemas()) {
        schema_select_->addItem(s.name); // schema names are data
    }
    connect(schema_select_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < schemas().size()) {
            schema_desc_->setText(schemas()[idx].description);
            const auto& fields = schemas()[idx].fields;
            schema_fields_table_->setRowCount(fields.size());
            for (int i = 0; i < fields.size(); ++i) {
                schema_fields_table_->setItem(i, 0, new QTableWidgetItem(fields[i].name));
                schema_fields_table_->setItem(i, 1, new QTableWidgetItem(fields[i].type));
                schema_fields_table_->setItem(i, 2, new QTableWidgetItem(fields[i].required ? tr("Yes") : tr("No")));
                schema_fields_table_->setItem(i, 3, new QTableWidgetItem(fields[i].description));
            }
            if (right_schema_info_)
                right_schema_info_->setText(tr("Schema: %1").arg(schemas()[idx].name));
            if (right_fields_info_)
                right_fields_info_->setText(tr("Fields: %1").arg(fields.size()));
        }
    });
    schema_select_row_ = create_form_row(tr("SELECT SCHEMA"), schema_select_);
    bl->addWidget(schema_select_row_);

    schema_desc_ = new QLabel(schemas().isEmpty() ? "" : schemas()[0].description);
    schema_desc_->setObjectName("dmInfoLabel");
    schema_desc_->setWordWrap(true);
    bl->addWidget(schema_desc_);

    schema_fields_table_ = new QTableWidget;
    schema_fields_table_->setColumnCount(4);
    schema_fields_table_->setHorizontalHeaderLabels({tr("Field"), tr("Type"), tr("Required"), tr("Description")});
    schema_fields_table_->horizontalHeader()->setStretchLastSection(true);
    schema_fields_table_->verticalHeader()->setVisible(false);
    schema_fields_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    schema_fields_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    schema_fields_table_->setMinimumHeight(200);

    if (!schemas().isEmpty()) {
        schema_select_->setCurrentIndex(0);
        emit schema_select_->currentIndexChanged(0);
    }

    bl->addWidget(schema_fields_table_);
    vl->addWidget(body);
    return panel;
}

QWidget* DataMappingScreen::create_field_mapping_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("3");
    icon->setObjectName("dmPanelHeaderIcon");
    field_panel_title_ = new QLabel(tr("FIELD MAPPING"));
    field_panel_title_->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(field_panel_title_);
    hl->addStretch(1);

    parser_engine_ = new QComboBox;
    // Parser names matched by text against template configs — keep untranslated.
    parser_engine_->addItems({"JSONPath", "JSONata", "JMESPath", "Direct", "JavaScript", "Regex"});
    parser_label_ = new QLabel(tr("Parser:"));
    hl->addWidget(parser_label_);
    hl->addWidget(parser_engine_);
    vl->addWidget(hdr);

    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);

    json_tree_ = new QTreeWidget;
    json_tree_->setHeaderLabels({tr("Key"), tr("Value"), tr("Type")});
    json_tree_->setColumnWidth(0, 160);
    json_tree_->setColumnWidth(1, 200);
    json_tree_->setMinimumWidth(250);
    split->addWidget(json_tree_);

    mapping_table_ = new QTableWidget;
    mapping_table_->setColumnCount(4);
    mapping_table_->setHorizontalHeaderLabels({tr("Target Field"), tr("Expression"), tr("Transform"), tr("Default")});
    mapping_table_->horizontalHeader()->setStretchLastSection(true);
    mapping_table_->verticalHeader()->setVisible(false);
    mapping_table_->setMinimumWidth(300);
    mapping_table_->setMinimumHeight(250);
    split->addWidget(mapping_table_);

    split->setSizes({350, 500});
    vl->addWidget(split, 1);

    return panel;
}

QWidget* DataMappingScreen::create_cache_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("4");
    icon->setObjectName("dmPanelHeaderIcon");
    cache_panel_title_ = new QLabel(tr("CACHE & SECURITY SETTINGS"));
    cache_panel_title_->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(cache_panel_title_);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    cache_enabled_ = new QComboBox;
    cache_enabled_->addItems({tr("Enabled"), tr("Disabled")});
    cache_enabled_row_ = create_form_row(tr("RESPONSE CACHING"), cache_enabled_);
    bl->addWidget(cache_enabled_row_);

    cache_ttl_ = new QSpinBox;
    cache_ttl_->setRange(0, 86400);
    cache_ttl_->setValue(300);
    cache_ttl_->setSuffix(tr(" sec"));
    cache_ttl_row_ = create_form_row(tr("CACHE TTL"), cache_ttl_);
    bl->addWidget(cache_ttl_row_);

    auto* sec_box = new QWidget(this);
    sec_box->setStyleSheet(
        QString("background: rgba(22,163,74,0.05); border: 1px solid %1; padding: 8px;").arg(colors::BORDER_DIM()));
    auto* sbl = new QVBoxLayout(sec_box);
    sbl->setSpacing(4);
    encryption_title_ = new QLabel(tr("ENCRYPTION"));
    encryption_title_->setObjectName("dmLabel");
    sbl->addWidget(encryption_title_);
    encryption_detail_ = new QLabel(tr("API credentials are encrypted with AES-256-GCM before storage.\n"
                                       "Sensitive data never stored in plaintext."));
    encryption_detail_->setObjectName("dmInfoLabel");
    encryption_detail_->setWordWrap(true);
    sbl->addWidget(encryption_detail_);
    bl->addWidget(sec_box);

    bl->addStretch(1);
    vl->addWidget(body);
    return panel;
}

QWidget* DataMappingScreen::create_test_save_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(this);
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("5");
    icon->setObjectName("dmPanelHeaderIcon");
    test_save_panel_title_ = new QLabel(tr("TEST & SAVE"));
    test_save_panel_title_->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(test_save_panel_title_);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    auto* test_row = new QWidget(this);
    auto* trl = new QHBoxLayout(test_row);
    trl->setContentsMargins(0, 0, 0, 0);
    trl->setSpacing(8);

    test_btn_ = new QPushButton(tr("RUN TEST"));
    test_btn_->setObjectName("dmCalcBtn");
    test_btn_->setCursor(Qt::PointingHandCursor);
    connect(test_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_test_mapping);

    test_status_ = new QLabel(tr("Not yet tested"));
    test_status_->setObjectName("dmInfoLabel");

    trl->addWidget(test_btn_);
    trl->addWidget(test_status_);
    trl->addStretch(1);
    bl->addWidget(test_row);

    test_output_ = new QTextEdit;
    test_output_->setObjectName("dmTestOutput");
    test_output_->setReadOnly(true);
    test_output_->setMinimumHeight(200);
    test_output_->setPlaceholderText(tr("Test results will appear here..."));
    bl->addWidget(test_output_);

    save_btn_ = new QPushButton(tr("SAVE MAPPING CONFIGURATION"));
    save_btn_->setObjectName("dmSaveBtn");
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setFixedHeight(36);
    save_btn_->setEnabled(false);
    connect(save_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_save_mapping);
    bl->addWidget(save_btn_);

    vl->addWidget(body);
    return panel;
}


QWidget* DataMappingScreen::create_list_view() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Toolbar
    auto* toolbar = new QWidget(this);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(0, 0, 0, 0);
    list_title_ = new QLabel(tr("SAVED MAPPINGS"));
    list_title_->setObjectName("dmPanelHeaderTitle");
    tbl->addWidget(list_title_);
    tbl->addStretch(1);

    list_run_btn_ = new QPushButton(tr("▶ RUN"));
    list_run_btn_->setObjectName("dmCalcBtn");
    list_run_btn_->setCursor(Qt::PointingHandCursor);
    connect(list_run_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_run_mapping);
    tbl->addWidget(list_run_btn_);

    list_del_btn_ = new QPushButton(tr("DELETE"));
    list_del_btn_->setObjectName("dmDestructiveBtn");
    list_del_btn_->setCursor(Qt::PointingHandCursor);
    connect(list_del_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_delete_mapping);
    tbl->addWidget(list_del_btn_);

    list_new_btn_ = new QPushButton(tr("+ NEW MAPPING"));
    list_new_btn_->setObjectName("dmCalcBtn");
    list_new_btn_->setCursor(Qt::PointingHandCursor);
    connect(list_new_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_new_mapping);
    tbl->addWidget(list_new_btn_);
    vl->addWidget(toolbar);

    mapping_list_ = new QListWidget;
    vl->addWidget(mapping_list_, 1);

    list_empty_ = new QLabel(tr("No mappings saved yet.\nClick CREATE to build your first data mapping."));
    list_empty_->setObjectName("dmEmptyState");
    list_empty_->setAlignment(Qt::AlignCenter);
    list_empty_->setWordWrap(true);
    vl->addWidget(list_empty_);

    scroll->setWidget(content);
    return scroll;
}

QWidget* DataMappingScreen::create_template_view() {
    auto* outer = new QWidget(this);
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);

    template_list_ = new QListWidget;
    for (int i = 0; i < templates().size(); ++i) {
        auto* item = new QListWidgetItem(templates()[i].name);
        item->setData(Qt::UserRole, i);
        item->setToolTip(templates()[i].description);
        template_list_->addItem(item);
    }
    connect(template_list_, &QListWidget::currentRowChanged, this, &DataMappingScreen::on_template_selected);
    split->addWidget(template_list_);

    auto* detail_scroll = new QScrollArea;
    detail_scroll->setWidgetResizable(true);
    auto* detail_content = new QWidget(this);
    auto* dvl = new QVBoxLayout(detail_content);
    dvl->setContentsMargins(16, 16, 16, 16);
    dvl->setSpacing(8);

    template_detail_ = new QLabel(tr("Select a template to view details"));
    template_detail_->setObjectName("dmInfoLabel");
    template_detail_->setWordWrap(true);
    template_detail_->setAlignment(Qt::AlignTop);
    dvl->addWidget(template_detail_);
    dvl->addStretch(1);

    template_use_btn_ = new QPushButton(tr("USE THIS TEMPLATE"));
    template_use_btn_->setObjectName("dmCalcBtn");
    template_use_btn_->setCursor(Qt::PointingHandCursor);
    auto* use_btn = template_use_btn_;
    connect(use_btn, &QPushButton::clicked, this, [this]() {
        int row = template_list_->currentRow();
        if (row >= 0 && row < templates().size()) {
            const auto& tmpl = templates()[row];
            api_name_->setText(tmpl.name);
            api_base_url_->setText(tmpl.base_url);
            api_endpoint_->setText(tmpl.endpoint);
            api_method_->setCurrentText(tmpl.method);
            api_auth_type_->setCurrentText(tmpl.auth_type);
            api_headers_->setPlainText(tmpl.headers);
            api_body_->setPlainText(tmpl.body);
            schema_select_->setCurrentText(tmpl.schema);
            parser_engine_->setCurrentText(tmpl.parser);

            mapping_table_->setRowCount(tmpl.field_mappings.size());
            for (int i = 0; i < tmpl.field_mappings.size(); ++i) {
                const auto& fm = tmpl.field_mappings[i];
                auto* name_item = new QTableWidgetItem(fm.target);
                name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
                mapping_table_->setItem(i, 0, name_item);
                mapping_table_->setItem(i, 1, new QTableWidgetItem(fm.expression));
                mapping_table_->setItem(i, 2, new QTableWidgetItem(fm.transform));
                mapping_table_->setItem(i, 3, new QTableWidgetItem(fm.default_val));
            }

            on_view_changed(2); // switch to create
            on_step_changed(0);
            LOG_INFO("DataMapping", "Template applied: " + tmpl.name);
        }
    });
    dvl->addWidget(use_btn);

    detail_scroll->setWidget(detail_content);
    split->addWidget(detail_scroll);
    split->setSizes({350, 500});

    auto* vl = new QVBoxLayout(outer);
    vl->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    auto* toolbar = new QWidget(this);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(16, 8, 16, 8);
    template_toolbar_title_ = new QLabel(tr("BROKER TEMPLATES"));
    template_toolbar_title_->setObjectName("dmPanelHeaderTitle");
    tbl->addWidget(template_toolbar_title_);
    tbl->addStretch(1);
    template_count_lbl_ = new QLabel(tr("%1 templates").arg(templates().size()));
    template_count_lbl_->setObjectName("dmInfoLabel");
    tbl->addWidget(template_count_lbl_);
    vl->addWidget(toolbar);
    vl->addWidget(split, 1);

    return outer;
}


QWidget* DataMappingScreen::create_nav_footer() {
    auto* bar = new QWidget(this);
    bar->setObjectName("dmNavFooter");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    prev_btn_ = new QPushButton(tr("PREVIOUS"));
    prev_btn_->setObjectName("dmSecondaryBtn");
    prev_btn_->setCursor(Qt::PointingHandCursor);
    prev_btn_->setEnabled(false);
    connect(prev_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_prev_step);

    step_label_ = new QLabel(tr("Step %1 of %2 — %3").arg(1).arg(5).arg(tr("API CONFIG")));
    step_label_->setObjectName("dmStatusText");
    step_label_->setAlignment(Qt::AlignCenter);

    next_btn_ = new QPushButton(tr("NEXT"));
    next_btn_->setObjectName("dmCalcBtn");
    next_btn_->setCursor(Qt::PointingHandCursor);
    connect(next_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_next_step);

    hl->addWidget(prev_btn_);
    hl->addStretch(1);
    hl->addWidget(step_label_);
    hl->addStretch(1);
    hl->addWidget(next_btn_);

    return bar;
}

QWidget* DataMappingScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("dmStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    status_version_ = new QLabel(tr("DATA MAPPING v1.0"));
    status_version_->setObjectName("dmStatusText");
    hl->addWidget(status_version_);
    hl->addStretch(1);

    status_view_ = new QLabel(tr("VIEW: %1").arg(tr("MAPPINGS")));
    status_view_->setObjectName("dmStatusText");
    hl->addWidget(status_view_);

    hl->addSpacing(16);
    status_step_ = new QLabel;
    status_step_->setObjectName("dmStatusHighlight");
    hl->addWidget(status_step_);

    return bar;
}


QWidget* DataMappingScreen::create_form_row(const QString& label_text, QWidget* input) {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    auto* lbl = new QLabel(label_text);
    lbl->setObjectName("dmLabel");
    vl->addWidget(lbl);
    vl->addWidget(input);
    return w;
}

QWidget* DataMappingScreen::create_form_two_col(QWidget* left, QWidget* right) {
    auto* w = new QWidget(this);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(left, 1);
    hl->addWidget(right, 1);
    return w;
}

} // namespace fincept::screens
