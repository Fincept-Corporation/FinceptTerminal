#include "screens/data_mapping/DataMappingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/data_normalization/DataNormalizationService.h"
#include "storage/repositories/DataMappingRepository.h"
#include "ui/theme/Theme.h"
#include "screens/data_mapping/DataMappingScreen_internal.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>

namespace {
using namespace fincept::ui;
static const QString kStyle =
    QStringLiteral("#dmScreen { background: %1; }"

                   "#dmHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#dmHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#dmHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
                   "#dmHeaderBadge { color: %6; font-size: 8px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.2); padding: 2px 6px; }"

                   "#dmViewBtn { background: transparent; color: %5; border: 1px solid %8; "
                   "  font-size: 9px; font-weight: 700; padding: 4px 12px; }"
                   "#dmViewBtn:hover { color: %4; }"
                   "#dmViewBtn[active=\"true\"] { background: %3; color: %1; border-color: %3; }"

                   "#dmStepBar { background: %7; border-bottom: 1px solid %8; }"
                   "#dmStepBtn { background: transparent; color: %5; border: none; "
                   "  font-size: 9px; font-weight: 700; padding: 6px 14px; border-bottom: 2px solid transparent; }"
                   "#dmStepBtn:hover { color: %4; }"
                   "#dmStepBtn[active=\"true\"] { color: %4; border-bottom-color: %3; }"
                   "#dmStepBtn[complete=\"true\"] { color: %6; }"

                   "#dmLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#dmRightPanel { background: %7; border-left: 1px solid %8; }"

                   "#dmPanelTitle { color: %5; font-size: 10px; font-weight: 700; "
                   "  letter-spacing: 0.5px; background: transparent; padding: 8px; "
                   "  border-bottom: 1px solid %8; }"

                   "#dmPanel { background: %7; border: 1px solid %8; }"
                   "#dmPanelHeader { background: %2; border-bottom: 1px solid %8; }"
                   "#dmPanelHeaderTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#dmPanelHeaderIcon { color: %3; font-size: 13px; background: transparent; }"

                   "#dmLabel { color: %5; font-size: 9px; font-weight: 700; "
                   "  letter-spacing: 0.5px; background: transparent; }"

                   "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QLineEdit:focus { border-color: %9; }"
                   "QPlainTextEdit { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                   "  selection-background-color: %3; }"
                   "QSpinBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QSpinBox::up-button, QSpinBox::down-button { width: 0; }"

                   "#dmCalcBtn { background: %3; color: %1; border: none; padding: 5px 16px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#dmCalcBtn:hover { background: #FF8800; }"
                   "#dmCalcBtn:disabled { background: %10; color: %11; }"
                   "#dmSecondaryBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 5px 16px; font-size: 9px; font-weight: 700; }"
                   "#dmSecondaryBtn:hover { color: %4; }"
                   "#dmSaveBtn { background: %6; color: %1; border: none; padding: 6px 20px; "
                   "  font-size: 10px; font-weight: 700; }"
                   "#dmSaveBtn:hover { background: %6; }"
                   "#dmSaveBtn:disabled { background: %10; color: %11; }"

                   "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: 11px; }"
                   "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

                   "QTreeWidget { background: %1; color: %4; border: none; font-size: 11px; }"
                   "QTreeWidget::item { padding: 2px 4px; }"
                   "QTreeWidget::item:selected { background: rgba(217,119,6,0.1); color: %3; }"

                   "QListWidget { background: %1; border: none; font-size: 11px; }"
                   "QListWidget::item { color: %5; padding: 6px 10px; border-bottom: 1px solid %8; }"
                   "QListWidget::item:hover { color: %4; background: %12; }"
                   "QListWidget::item:selected { color: %3; background: rgba(217,119,6,0.1); }"

                   "QTextEdit { background: %1; color: %13; border: none; font-size: 11px; }"

                   "#dmInfoLabel { color: %5; font-size: 9px; background: transparent; }"
                   "#dmInfoValue { color: %13; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#dmSuccessBadge { color: %6; font-size: 9px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.15); padding: 2px 6px; }"
                   "#dmErrorBadge { color: %14; font-size: 9px; font-weight: 700; "
                   "  background: rgba(220,38,38,0.15); padding: 2px 6px; }"

                   "#dmNavFooter { background: %2; border-top: 1px solid %8; }"
                   "#dmStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#dmStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#dmStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

                   "#dmEmptyState { color: %11; font-size: 13px; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollArea { background: %1; border: none; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::TEXT_DIM())       // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE())       // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

using namespace fincept::screens::data_mapping_internal;


DataMappingScreen::DataMappingScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("dmScreen");
    setStyleSheet(kStyle);
    setup_ui();
    LOG_INFO("DataMapping", "Screen constructed");
}


void DataMappingScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());
    root->addWidget(create_step_bar());
    root->addWidget(create_main_area(), 1);

    nav_footer_ = create_nav_footer();
    nav_footer_->hide();
    root->addWidget(nav_footer_);

    root->addWidget(create_status_bar());

    on_view_changed(0);
}

void DataMappingScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DataMappingScreen::retranslateUi() {
    // Header
    if (header_title_) header_title_->setText(tr("DATA MAPPING ENGINE"));
    if (header_sub_)   header_sub_->setText(tr("API CONFIGURATION & SCHEMA TRANSFORMATION"));
    if (header_badge_) header_badge_->setText(tr("7 SCHEMAS"));

    // View buttons (order: MAPPINGS, TEMPLATES, CREATE)
    const QStringList view_names = {tr("MAPPINGS"), tr("TEMPLATES"), tr("CREATE")};
    for (int i = 0; i < view_btns_.size() && i < view_names.size(); ++i)
        view_btns_[i]->setText(view_names[i]);

    // Step bar buttons ("1. API CONFIG" ...)
    const QStringList step_names = {tr("API CONFIG"), tr("SCHEMA"), tr("FIELD MAPPING"), tr("CACHE"), tr("TEST & SAVE")};
    for (int i = 0; i < step_btns_.size() && i < step_names.size(); ++i)
        step_btns_[i]->setText(QString("%1. %2").arg(i + 1).arg(step_names[i]));

    // Left wizard panel
    if (left_panel_title_) left_panel_title_->setText(tr("WIZARD STEPS"));
    const QStringList left_labels = {tr("API Configuration"), tr("Schema Selection"), tr("Field Mapping"),
                                     tr("Cache Settings"), tr("Test & Save")};
    const QStringList left_icons = {"1", "2", "3", "4", "5"};
    for (int i = 0; i < left_step_btns_.size() && i < left_labels.size(); ++i)
        left_step_btns_[i]->setText(left_icons[i] + "  " + left_labels[i]);
    if (quick_stats_title_)  quick_stats_title_->setText(tr("QUICK STATS"));
    if (schemas_count_lbl_)  schemas_count_lbl_->setText(tr("Schemas: %1").arg(7));
    if (parsers_count_lbl_)  parsers_count_lbl_->setText(tr("Parsers: %1").arg(6));
    if (status_mappings_)    status_mappings_->setText(tr("Saved: %1").arg(saved_mappings_.size()));

    // Right system panel
    if (right_panel_title_)     right_panel_title_->setText(tr("SYSTEM"));
    if (mapping_engine_title_)  mapping_engine_title_->setText(tr("MAPPING ENGINE"));
    if (parser_engines_title_)  parser_engines_title_->setText(tr("PARSER ENGINES"));
    if (security_title_)        security_title_->setText(tr("SECURITY"));
    if (current_mapping_title_) current_mapping_title_->setText(tr("CURRENT MAPPING"));

    // Helper: re-apply a form-row's label (each row holds exactly one QLabel).
    auto set_row_label = [](QWidget* row, const QString& text) {
        if (!row) return;
        if (auto* lbl = row->findChild<QLabel*>())
            lbl->setText(text);
    };

    // Step 0 — API config
    if (api_panel_title_) api_panel_title_->setText(tr("API CONFIGURATION"));
    set_row_label(api_name_row_, tr("MAPPING NAME"));
    set_row_label(api_base_url_row_, tr("BASE URL"));
    set_row_label(api_endpoint_row_, tr("ENDPOINT"));
    set_row_label(api_method_row_, tr("HTTP METHOD"));
    set_row_label(api_auth_type_row_, tr("AUTHENTICATION"));
    set_row_label(api_auth_value_row_, tr("AUTH VALUE"));
    set_row_label(api_headers_row_, tr("HEADERS (one per line)"));
    set_row_label(api_body_row_, tr("REQUEST BODY (JSON)"));
    set_row_label(api_timeout_row_, tr("TIMEOUT"));
    if (api_name_)    api_name_->setPlaceholderText(tr("e.g. Upstox OHLCV"));
    if (api_auth_value_) api_auth_value_->setPlaceholderText(tr("Token / API Key value"));
    if (api_timeout_) api_timeout_->setSuffix(tr(" sec"));
    if (api_test_btn_) api_test_btn_->setText(tr("TEST API REQUEST"));

    // Step 1 — schema
    if (schema_panel_title_) schema_panel_title_->setText(tr("SCHEMA SELECTION"));
    set_row_label(schema_type_row_, tr("SCHEMA TYPE"));
    set_row_label(schema_select_row_, tr("SELECT SCHEMA"));
    if (schema_type_ && schema_type_->count() >= 2) {
        schema_type_->setItemText(0, tr("Predefined Schema"));
        schema_type_->setItemText(1, tr("Custom Schema"));
    }
    if (schema_fields_table_)
        schema_fields_table_->setHorizontalHeaderLabels({tr("Field"), tr("Type"), tr("Required"), tr("Description")});

    // Step 2 — field mapping
    if (field_panel_title_) field_panel_title_->setText(tr("FIELD MAPPING"));
    if (parser_label_)      parser_label_->setText(tr("Parser:"));
    if (json_tree_)    json_tree_->setHeaderLabels({tr("Key"), tr("Value"), tr("Type")});
    if (mapping_table_)
        mapping_table_->setHorizontalHeaderLabels({tr("Target Field"), tr("Expression"), tr("Transform"), tr("Default")});

    // Step 3 — cache & security
    if (cache_panel_title_) cache_panel_title_->setText(tr("CACHE & SECURITY SETTINGS"));
    set_row_label(cache_enabled_row_, tr("RESPONSE CACHING"));
    set_row_label(cache_ttl_row_, tr("CACHE TTL"));
    if (cache_enabled_ && cache_enabled_->count() >= 2) {
        cache_enabled_->setItemText(0, tr("Enabled"));
        cache_enabled_->setItemText(1, tr("Disabled"));
    }
    if (cache_ttl_)         cache_ttl_->setSuffix(tr(" sec"));
    if (encryption_title_)  encryption_title_->setText(tr("ENCRYPTION"));
    if (encryption_detail_) encryption_detail_->setText(tr("API credentials are encrypted with AES-256-GCM before storage.\n"
                                                           "Sensitive data never stored in plaintext."));

    // Step 4 — test & save
    if (test_save_panel_title_) test_save_panel_title_->setText(tr("TEST & SAVE"));
    if (test_btn_)    test_btn_->setText(tr("RUN TEST"));
    if (test_output_) test_output_->setPlaceholderText(tr("Test results will appear here..."));
    if (save_btn_)    save_btn_->setText(tr("SAVE MAPPING CONFIGURATION"));

    // List view
    if (list_title_)   list_title_->setText(tr("SAVED MAPPINGS"));
    if (list_run_btn_) list_run_btn_->setText(tr("▶ RUN"));
    if (list_del_btn_) list_del_btn_->setText(tr("DELETE"));
    if (list_new_btn_) list_new_btn_->setText(tr("+ NEW MAPPING"));
    if (list_empty_)   list_empty_->setText(tr("No mappings saved yet.\nClick CREATE to build your first data mapping."));

    // Template view
    if (template_use_btn_)        template_use_btn_->setText(tr("USE THIS TEMPLATE"));
    if (template_toolbar_title_)  template_toolbar_title_->setText(tr("BROKER TEMPLATES"));
    if (template_count_lbl_)      template_count_lbl_->setText(tr("%1 templates").arg(templates().size()));

    // Footer + status bar (step/view labels re-derived from current state)
    if (prev_btn_)       prev_btn_->setText(tr("PREVIOUS"));
    if (next_btn_)       next_btn_->setText(tr("NEXT"));
    if (status_version_) status_version_->setText(tr("DATA MAPPING v1.0"));
    if (status_view_ && current_view_ >= 0 && current_view_ < view_names.size())
        status_view_->setText(tr("VIEW: %1").arg(view_names[current_view_]));
    if (current_view_ == 2)
        update_step_indicators(); // refreshes step_label_ + status_step_ in the new language
}

void DataMappingScreen::update_step_indicators() {
    const QStringList labels = {tr("API CONFIG"), tr("SCHEMA"), tr("FIELD MAPPING"), tr("CACHE"), tr("TEST & SAVE")};
    for (int i = 0; i < step_btns_.size(); ++i) {
        step_btns_[i]->setProperty("active", i == current_step_);
        step_btns_[i]->setProperty("complete", i < current_step_);
        step_btns_[i]->style()->unpolish(step_btns_[i]);
        step_btns_[i]->style()->polish(step_btns_[i]);
    }
    step_label_->setText(tr("Step %1 of %2 — %3").arg(current_step_ + 1).arg(5).arg(labels[current_step_]));
    prev_btn_->setEnabled(current_step_ > 0);
    next_btn_->setEnabled(current_step_ < 4);
    status_step_->setText(labels[current_step_]);
}

void DataMappingScreen::populate_json_tree(const QJsonValue& val, QTreeWidgetItem* parent, const QString& key) {
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(json_tree_);
    item->setText(0, key);

    if (val.isObject()) {
        item->setText(2, "object");
        auto obj = val.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            populate_json_tree(it.value(), item, it.key());
        }
    } else if (val.isArray()) {
        auto arr = val.toArray();
        item->setText(2, QString("array[%1]").arg(arr.size()));
        int limit = qMin(arr.size(), 10); // limit display
        for (int i = 0; i < limit; ++i) {
            populate_json_tree(arr[i], item, QString("[%1]").arg(i));
        }
    } else if (val.isDouble()) {
        item->setText(1, QString::number(val.toDouble(), 'g', 10));
        item->setText(2, "number");
        item->setForeground(1, QColor(colors::CYAN()));
    } else if (val.isBool()) {
        item->setText(1, val.toBool() ? "true" : "false");
        item->setText(2, "boolean");
        item->setForeground(1, QColor(colors::WARNING()));
    } else if (val.isNull()) {
        item->setText(1, "null");
        item->setText(2, "null");
        item->setForeground(1, QColor(colors::TEXT_DIM()));
    } else {
        item->setText(1, val.toString());
        item->setText(2, "string");
    }
}

void DataMappingScreen::populate_mapping_list() {
    int schema_idx = schema_select_->currentIndex();
    if (schema_idx < 0 || schema_idx >= schemas().size())
        return;

    const auto& fields = schemas()[schema_idx].fields;
    mapping_table_->setRowCount(fields.size());
    for (int i = 0; i < fields.size(); ++i) {
        auto* name_item = new QTableWidgetItem(fields[i].name);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        mapping_table_->setItem(i, 0, name_item);
        mapping_table_->setItem(i, 1, new QTableWidgetItem("")); // expression
        mapping_table_->setItem(i, 2, new QTableWidgetItem("")); // transform
        mapping_table_->setItem(i, 3, new QTableWidgetItem("")); // default
    }
}


void DataMappingScreen::on_view_changed(int view) {
    current_view_ = view;
    view_stack_->setCurrentIndex(view);

    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }

    const QStringList names = {tr("MAPPINGS"), tr("TEMPLATES"), tr("CREATE")};
    status_view_->setText(tr("VIEW: %1").arg(names[view]));

    // CREATE is index 2 — show step bar + nav footer only there.
    nav_footer_->setVisible(view == 2); // only in create mode

    if (view == 2) {
        update_step_indicators();
    }

    LOG_INFO("DataMapping", "View: " + names[view]);
    ScreenStateManager::instance().notify_changed(this);
}

void DataMappingScreen::on_step_changed(int step) {
    if (step < 0 || step > 4)
        return;
    current_step_ = step;
    step_stack_->setCurrentIndex(step);
    update_step_indicators();

    if (step == 2) {
        populate_mapping_list();
        if (!sample_data_.isNull()) {
            json_tree_->clear();
            populate_json_tree(sample_data_.isObject() ? QJsonValue(sample_data_.object())
                                                       : QJsonValue(sample_data_.array()),
                               nullptr, "root");
            json_tree_->expandToDepth(1);
        }
    }

    LOG_INFO("DataMapping", "Step: " + QString::number(step + 1));
}

void DataMappingScreen::on_next_step() {
    if (current_step_ < 4) {
        on_step_changed(current_step_ + 1);
    }
}

void DataMappingScreen::on_prev_step() {
    if (current_step_ > 0) {
        on_step_changed(current_step_ - 1);
    }
}

void DataMappingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load_mappings_from_db();
}

QVariantMap DataMappingScreen::save_state() const {
    QVariantMap state{{"view", current_view_}};
    if (api_name_) state["api_name"] = api_name_->text();
    if (api_base_url_) state["api_base_url"] = api_base_url_->text();
    if (api_endpoint_) state["api_endpoint"] = api_endpoint_->text();
    if (api_auth_value_) state["api_auth_value"] = api_auth_value_->text();
    if (api_headers_) state["api_headers"] = api_headers_->toPlainText();
    if (api_body_) state["api_body"] = api_body_->toPlainText();
    if (test_output_ && !test_output_->toPlainText().isEmpty()) {
        const QString out = test_output_->toPlainText();
        if (out.size() < 200000)
            state["test_output"] = out;
    }
    return state;
}

void DataMappingScreen::restore_state(const QVariantMap& state) {
    const int view = state.value("view", 0).toInt();
    // Restore list/templates view only — never resume a partial wizard.
    if (view == 0 || view == 1)
        on_view_changed(view);
    if (api_name_ && state.contains("api_name"))
        api_name_->setText(state.value("api_name").toString());
    if (api_base_url_ && state.contains("api_base_url"))
        api_base_url_->setText(state.value("api_base_url").toString());
    if (api_endpoint_ && state.contains("api_endpoint"))
        api_endpoint_->setText(state.value("api_endpoint").toString());
    if (api_auth_value_ && state.contains("api_auth_value"))
        api_auth_value_->setText(state.value("api_auth_value").toString());
    if (api_headers_ && state.contains("api_headers"))
        api_headers_->setPlainText(state.value("api_headers").toString());
    if (api_body_ && state.contains("api_body"))
        api_body_->setPlainText(state.value("api_body").toString());
    if (test_output_ && state.contains("test_output"))
        test_output_->setPlainText(state.value("test_output").toString());
}

} // namespace fincept::screens
