// src/screens/data_mapping/DataMappingScreen_Operations.cpp
//
// Test/save/run handlers and template lifecycle — on_test_api, on_test_mapping,
// on_save_mapping, on_run_mapping, on_template_selected, on_new_mapping,
// on_delete_mapping, plus load_mappings_from_db and build_mapping_config.
//
// Part of the partial-class split of DataMappingScreen.cpp.

#include "screens/data_mapping/DataMappingScreen.h"
#include "screens/data_mapping/DataMappingScreen_internal.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "services/data_normalization/DataNormalizationService.h"
#include "storage/repositories/DataMappingRepository.h"
#include "ui/theme/Theme.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidgetItem>
#include <QUuid>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::screens::data_mapping_internal;

void DataMappingScreen::on_test_api() {
    QString url = api_base_url_->text().trimmed() + api_endpoint_->text().trimmed();
    if (url.isEmpty()) {
        api_test_status_->setText("Enter a URL first");
        return;
    }

    api_test_btn_->setEnabled(false);
    api_test_status_->setText("Testing...");

    auto callback = [this](Result<QJsonDocument> result) {
        api_test_btn_->setEnabled(true);
        if (result.is_ok()) {
            sample_data_ = result.value();
            api_test_status_->setText("SUCCESS — Sample data received");
            api_test_status_->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(colors::POSITIVE()));
            LOG_INFO("DataMapping", "API test success");
        } else {
            api_test_status_->setText("FAILED — " + QString::fromStdString(result.error()));
            api_test_status_->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(colors::NEGATIVE()));
            LOG_ERROR("DataMapping", "API test failed: " + QString::fromStdString(result.error()));
        }
    };

    QString method = api_method_->currentText();
    if (method == "POST" || method == "PUT" || method == "PATCH") {
        QJsonObject body;
        if (!api_body_->toPlainText().trimmed().isEmpty()) {
            auto doc = QJsonDocument::fromJson(api_body_->toPlainText().toUtf8());
            if (doc.isObject())
                body = doc.object();
        }
        if (method == "POST")
            HttpClient::instance().post(url, body, callback, this);
        else
            HttpClient::instance().put(url, body, callback, this);
    } else {
        HttpClient::instance().get(url, callback, this);
    }
}

void DataMappingScreen::on_test_mapping() {
    if (sample_data_.isNull()) {
        test_status_->setText("No sample data — test API first (Step 1)");
        return;
    }

    test_btn_->setEnabled(false);
    test_status_->setText("Running test...");

    QJsonObject summary;
    summary["name"] = api_name_->text();
    summary["schema"] = schema_select_->currentText();
    summary["parser"] = parser_engine_->currentText();
    summary["cache_enabled"] = cache_enabled_->currentIndex() == 0;
    summary["cache_ttl"] = cache_ttl_->value();

    QJsonArray mappings;
    for (int r = 0; r < mapping_table_->rowCount(); ++r) {
        auto* expr_item = mapping_table_->item(r, 1);
        if (expr_item && !expr_item->text().trimmed().isEmpty()) {
            QJsonObject m;
            m["target"] = mapping_table_->item(r, 0)->text();
            m["expression"] = expr_item->text();
            auto* trans = mapping_table_->item(r, 2);
            if (trans && !trans->text().isEmpty())
                m["transform"] = trans->text();
            auto* def = mapping_table_->item(r, 3);
            if (def && !def->text().isEmpty())
                m["default"] = def->text();
            mappings.append(m);
        }
    }
    summary["field_mappings"] = mappings;
    summary["mapping_count"] = mappings.size();

    QJsonObject result;
    result["success"] = mappings.size() > 0;
    result["config"] = summary;
    result["sample_data_keys"] = QJsonArray();
    if (sample_data_.isObject()) {
        for (auto it = sample_data_.object().begin(); it != sample_data_.object().end(); ++it) {
            QJsonArray keys = result["sample_data_keys"].toArray();
            keys.append(it.key());
            result["sample_data_keys"] = keys;
        }
    }

    test_output_->setPlainText(QJsonDocument(result).toJson(QJsonDocument::Indented));
    test_result_ = result;

    bool success = mappings.size() > 0;
    if (success) {
        test_status_->setText("TEST PASSED");
        test_status_->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(colors::POSITIVE()));
        save_btn_->setEnabled(true);
        right_test_info_->setText("Test: PASSED");
    } else {
        test_status_->setText("TEST FAILED — No field mappings configured");
        test_status_->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(colors::NEGATIVE()));
        save_btn_->setEnabled(false);
        right_test_info_->setText("Test: FAILED");
    }

    test_btn_->setEnabled(true);
    LOG_INFO("DataMapping", "Test: " + QString(success ? "PASSED" : "FAILED"));
}

void DataMappingScreen::on_save_mapping() {
    const QString name = api_name_->text().trimmed();
    if (name.isEmpty()) {
        test_status_->setText("Enter a mapping name first");
        return;
    }

    QJsonObject config;
    build_mapping_config(config);

    DataMapping dm;
    dm.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dm.name = name;
    dm.schema_name = schema_select_->currentText();
    dm.base_url = api_base_url_->text().trimmed();
    dm.endpoint = api_endpoint_->text().trimmed();
    dm.method = api_method_->currentText();
    dm.auth_type = api_auth_type_->currentText();
    dm.auth_token = api_auth_value_->text().trimmed();
    dm.headers = api_headers_->toPlainText().trimmed();
    dm.body = api_body_->toPlainText().trimmed();
    dm.parser = parser_engine_->currentText();
    dm.cache_enabled = cache_enabled_->currentIndex() == 0;
    dm.cache_ttl = cache_ttl_->value();
    dm.field_mappings_json = QJsonDocument(config["field_mappings"].toArray()).toJson(QJsonDocument::Compact);

    auto r = DataMappingRepository::instance().save(dm);
    if (r.is_err()) {
        LOG_ERROR("DataMapping", "Failed to save: " + QString::fromStdString(r.error()));
        test_status_->setText("Save failed — database error");
        return;
    }

    load_mappings_from_db();
    on_view_changed(0);
    LOG_INFO("DataMapping", "Mapping saved: " + name);
}

void DataMappingScreen::on_run_mapping() {
    const int row = mapping_list_->currentRow();
    if (row < 0 || row >= saved_mappings_.size())
        return;

    const DataMapping& dm = saved_mappings_[row];
    LOG_INFO("DataMapping", "Running mapping: " + dm.name);

    QPointer<DataMappingScreen> self = this;
    fincept::services::DataNormalizationService::instance().fetch_and_normalize(
        dm, [self, dm](bool ok, fincept::services::NormalizedRecord rec) {
            if (!self)
                return;
            if (ok) {
                const QString out = QJsonDocument(rec.normalized).toJson(QJsonDocument::Indented);
                self->test_output_->setPlainText(out);
                self->test_status_->setText(QString("RUN OK — %1 fields extracted").arg(rec.normalized.size()));
                LOG_INFO("DataMapping", "Run complete: " + dm.name);
            } else {
                const QString errs = rec.errors.join(", ");
                self->test_status_->setText("RUN FAILED — " + errs);
                LOG_WARN("DataMapping", "Run failed: " + errs);
            }
        });
}

void DataMappingScreen::on_template_selected(int index) {
    if (index < 0 || index >= templates().size())
        return;

    const auto& tmpl = templates()[index];
    QString detail = QString("NAME: %1\n\n"
                             "PROVIDER: %2\n"
                             "SCHEMA: %3\n"
                             "VERIFIED: %4\n\n"
                             "DESCRIPTION:\n%5\n\n"
                             "API DETAILS:\n"
                             "  Base URL: %6\n"
                             "  Endpoint: %7\n"
                             "  Method: %8\n"
                             "  Auth: %9\n\n"
                             "FIELD MAPPINGS: %10 fields configured\n"
                             "PARSER: %11\n\n"
                             "TAGS: %12")
                         .arg(tmpl.name, tmpl.broker, tmpl.schema, tmpl.verified ? "Yes" : "No", tmpl.description,
                              tmpl.base_url, tmpl.endpoint, tmpl.method, tmpl.auth_type,
                              QString::number(tmpl.field_mappings.size()), tmpl.parser, tmpl.tags.join(", "));

    template_detail_->setText(detail);
}

void DataMappingScreen::on_new_mapping() {
    api_name_->clear();
    api_base_url_->clear();
    api_endpoint_->clear();
    api_auth_value_->clear();
    api_headers_->clear();
    api_body_->clear();
    api_test_status_->clear();
    sample_data_ = QJsonDocument();
    test_result_ = QJsonObject();
    test_output_->clear();
    test_status_->setText("Not yet tested");
    save_btn_->setEnabled(false);
    right_test_info_->setText("Test: --");

    on_view_changed(2); // create
    on_step_changed(0);
}

void DataMappingScreen::on_delete_mapping() {
    const int row = mapping_list_->currentRow();
    if (row < 0 || row >= saved_mappings_.size())
        return;

    const QString id = saved_mappings_[row].id;
    auto r = DataMappingRepository::instance().remove(id);
    if (r.is_err()) {
        LOG_ERROR("DataMapping", "Failed to delete: " + QString::fromStdString(r.error()));
        return;
    }

    load_mappings_from_db();
    LOG_INFO("DataMapping", "Mapping deleted");
}

void DataMappingScreen::load_mappings_from_db() {
    saved_mappings_.clear();
    mapping_list_->clear();

    auto r = DataMappingRepository::instance().list_all();
    if (r.is_err()) {
        LOG_WARN("DataMapping", "Could not load mappings: " + QString::fromStdString(r.error()));
        return;
    }

    saved_mappings_ = r.value();
    for (const auto& dm : saved_mappings_) {
        mapping_list_->addItem(dm.name + " — " + dm.schema_name);
    }

    if (status_mappings_) {
        status_mappings_->setText("Saved: " + QString::number(saved_mappings_.size()));
    }
}

void DataMappingScreen::build_mapping_config(QJsonObject& config) {
    config["name"] = api_name_->text();
    config["base_url"] = api_base_url_->text();
    config["endpoint"] = api_endpoint_->text();
    config["method"] = api_method_->currentText();
    config["auth_type"] = api_auth_type_->currentText();
    config["schema"] = schema_select_->currentText();
    config["parser"] = parser_engine_->currentText();
    config["cache_enabled"] = cache_enabled_->currentIndex() == 0;
    config["cache_ttl"] = cache_ttl_->value();

    QJsonArray mappings;
    for (int r = 0; r < mapping_table_->rowCount(); ++r) {
        auto* expr_item = mapping_table_->item(r, 1);
        if (expr_item && !expr_item->text().trimmed().isEmpty()) {
            QJsonObject m;
            m["target"] = mapping_table_->item(r, 0)->text();
            m["expression"] = expr_item->text();
            mappings.append(m);
        }
    }
    config["field_mappings"] = mappings;
}


} // namespace fincept::screens
