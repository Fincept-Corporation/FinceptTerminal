// src/screens/data_mapping/DataMappingScreen_Operations.cpp
//
// Test/save/run handlers and template lifecycle — on_test_api, on_test_mapping,
// on_save_mapping, on_run_mapping, on_edit_mapping, on_template_selected,
// on_new_mapping, on_delete_mapping, plus load_mappings_from_db and the
// wizard <-> DataMapping helpers (collect_field_mappings, build_mapping_from_form,
// reset_wizard_state, load_mapping_into_wizard).
//
// Part of the partial-class split of DataMappingScreen.cpp.

#include "core/logging/Logger.h"
#include "screens/data_mapping/DataMappingScreen.h"
#include "screens/data_mapping/DataMappingScreen_internal.h"
#include "services/data_normalization/DataMappingTestClient.h"
#include "services/data_normalization/DataNormalizationService.h"
#include "storage/repositories/DataMappingRepository.h"
#include "ui/theme/Theme.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPointer>
#include <QUuid>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::screens::data_mapping_internal;

namespace {

/// The engine evaluates JSONPath. "Direct" (plain key paths) and simple
/// JMESPath-style paths share that syntax; JSONata / JavaScript / Regex are
/// offered in the parser list but are not executed.
bool dm_parser_is_evaluated(const QString& parser) {
    return parser == QLatin1String("JSONPath") || parser == QLatin1String("Direct") ||
           parser == QLatin1String("JMESPath");
}

} // namespace

void DataMappingScreen::on_test_api() {
    using fincept::services::DataNormalizationService;

    const QString url = DataNormalizationService::join_url(api_base_url_->text(), api_endpoint_->text());
    if (url.isEmpty()) {
        api_test_status_->setText(tr("Enter a URL first"));
        return;
    }
    // HttpClient resolves anything that is not http(s) against the Fincept API and
    // attaches the user's Fincept session headers to it — never let a bare host through.
    if (!DataNormalizationService::is_http_url(url)) {
        api_test_status_->setText(tr("Base URL must start with http:// or https://"));
        return;
    }

    const QString method = api_method_->currentText();
    QJsonObject body;
    const bool has_body_verb = method == "POST" || method == "PUT" || method == "PATCH" || method == "DELETE";
    const QString body_text = api_body_->toPlainText().trimmed();
    if (has_body_verb && !body_text.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(body_text.toUtf8());
        if (!doc.isObject()) {
            // Used to be dropped silently, sending an empty body instead.
            api_test_status_->setText(tr("Request body must be a JSON object"));
            return;
        }
        body = doc.object();
    }

    api_test_btn_->setEnabled(false);
    api_test_status_->setText(tr("Testing..."));

    auto callback = [this](Result<QJsonDocument> result) {
        api_test_btn_->setEnabled(true);
        if (result.is_ok()) {
            sample_data_ = result.value();
            api_test_status_->setText(tr("SUCCESS — Sample data received"));
            api_test_status_->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(colors::POSITIVE()));
            LOG_INFO("DataMapping", "API test success");
        } else {
            api_test_status_->setText(tr("FAILED — %1").arg(QString::fromStdString(result.error())));
            api_test_status_->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(colors::NEGATIVE()));
            LOG_ERROR("DataMapping", "API test failed: " + QString::fromStdString(result.error()));
        }
    };

    using fincept::services::DataMappingTestClient;
    DataMappingTestClient::Method m = DataMappingTestClient::Method::Get;
    if (method == "POST" || method == "PUT" || method == "PATCH") {
        // PATCH historically routed through PUT — preserve that.
        m = (method == "POST") ? DataMappingTestClient::Method::Post : DataMappingTestClient::Method::Put;
    } else if (method == "DELETE") {
        m = DataMappingTestClient::Method::Delete;
    }
    // AUTH VALUE + HEADERS ride on this request (they were saved but never sent, so
    // an authenticated endpoint could not be tested at all).
    DataMappingTestClient::instance().test_api(
        m, url, body, this, callback,
        DataNormalizationService::build_request_headers(api_headers_->toPlainText(), api_auth_type_->currentText(),
                                                        api_auth_value_->text()));
}

void DataMappingScreen::on_test_mapping() {
    using fincept::services::DataNormalizationService;

    if (sample_data_.isNull()) {
        test_status_->setText(tr("No sample data — test API first (Step 1)"));
        return;
    }

    test_btn_->setEnabled(false);
    test_status_->setText(tr("Running test..."));

    const DataMapping dm = build_mapping_from_form();
    const QJsonArray mappings = collect_field_mappings();
    const bool has_mappings = !mappings.isEmpty();

    // Run the real engine over the sample (the same code RUN uses). This used to
    // report "TEST PASSED" whenever any expression cell had text in it, whether or
    // not it matched anything.
    const fincept::services::NormalizedRecord rec = DataNormalizationService::instance().normalize_raw(dm, sample_data_);

    QStringList warnings;
    if (!dm_parser_is_evaluated(dm.parser))
        warnings << tr("Parser \"%1\" is not executed — expressions are evaluated as JSONPath.").arg(dm.parser);
    for (const QJsonValue& v : mappings) {
        const QJsonObject fm = v.toObject();
        const QString target = fm["target"].toString();
        const QString expr = fm["expression"].toString();
        if (!DataNormalizationService::expression_supported(expr))
            warnings << tr("%1: unsupported expression \"%2\" (supported: $.a.b, $[0], [*], ['key']).")
                            .arg(target, expr);
        else if (!expr.isEmpty() && rec.normalized.value(target).isNull())
            warnings << tr("%1: expression matched nothing in the sample response.").arg(target);
        if (!DataNormalizationService::transform_supported(fm["transform"].toString()))
            warnings << tr("%1: unknown transform \"%2\" (ignored).").arg(target, fm["transform"].toString());
    }
    // The schema table flags more fields "Required" than the run-time validator enforces
    // (POSITION, ORDER, PORTFOLIO, INSTRUMENT). Surface the difference here instead of
    // letting a mapping that the schema calls incomplete read as clean.
    const int schema_idx = schema_select_->currentIndex();
    if (schema_idx >= 0 && schema_idx < schemas().size()) {
        for (const auto& field : schemas()[schema_idx].fields) {
            if (!field.required || rec.errors.contains(QStringLiteral("Missing required field: %1").arg(field.name)))
                continue; // optional, or already reported as a validation error
            const QJsonValue mapped = rec.normalized.value(field.name); // Undefined when the row was never mapped
            if (mapped.isNull() || mapped.isUndefined())
                warnings << tr("%1: required by the %2 schema but not mapped.").arg(field.name, dm.schema_name);
        }
    }

    QJsonObject summary;
    summary["name"] = dm.name;
    summary["schema"] = dm.schema_name;
    summary["parser"] = dm.parser;
    summary["cache_enabled"] = dm.cache_enabled;
    summary["cache_ttl"] = dm.cache_ttl;
    summary["field_mappings"] = mappings;
    summary["mapping_count"] = mappings.size();

    QJsonObject result;
    result["success"] = has_mappings && rec.errors.isEmpty();
    result["normalized"] = rec.normalized;
    result["validation_errors"] = QJsonArray::fromStringList(rec.errors);
    result["warnings"] = QJsonArray::fromStringList(warnings);
    result["config"] = summary;
    QJsonArray sample_keys;
    if (sample_data_.isObject()) {
        const QStringList keys = sample_data_.object().keys();
        for (const QString& k : keys)
            sample_keys.append(k);
    }
    result["sample_data_keys"] = sample_keys;

    test_output_->setPlainText(QJsonDocument(result).toJson(QJsonDocument::Indented));
    test_result_ = result;

    QString color;
    if (!has_mappings) {
        test_status_->setText(tr("TEST FAILED — No field mappings configured"));
        right_test_info_->setText(tr("Test: FAILED"));
        color = colors::NEGATIVE();
    } else if (rec.errors.isEmpty() && warnings.isEmpty()) {
        test_status_->setText(tr("TEST PASSED — %1 field(s) extracted").arg(rec.normalized.size()));
        right_test_info_->setText(tr("Test: PASSED"));
        color = colors::POSITIVE();
    } else {
        // Saving stays allowed: a sample response can legitimately lack a field
        // (empty result set, off-hours quote) that the live API will return.
        test_status_->setText(
            tr("TEST COMPLETED — %1 required field(s) missing, %2 warning(s)").arg(rec.errors.size()).arg(warnings.size()));
        right_test_info_->setText(tr("Test: ISSUES"));
        color = colors::WARNING();
    }
    test_status_->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(color));
    save_btn_->setEnabled(has_mappings);

    test_btn_->setEnabled(true);
    LOG_INFO("DataMapping", QString("Test: %1 mapping(s), %2 validation error(s), %3 warning(s)")
                                .arg(mappings.size())
                                .arg(rec.errors.size())
                                .arg(warnings.size()));
}

void DataMappingScreen::on_save_mapping() {
    using fincept::services::DataNormalizationService;

    const QString name = api_name_->text().trimmed();
    if (name.isEmpty()) {
        test_status_->setText(tr("Enter a mapping name first"));
        return;
    }

    DataMapping dm = build_mapping_from_form();
    if (!DataNormalizationService::is_http_url(DataNormalizationService::join_url(dm.base_url, dm.endpoint))) {
        test_status_->setText(tr("Base URL must start with http:// or https://"));
        return;
    }
    if (dm.field_mappings_json == QLatin1String("[]")) {
        test_status_->setText(tr("Save failed — no field mappings configured"));
        return;
    }
    if (!dm.body.isEmpty() && !QJsonDocument::fromJson(dm.body.toUtf8()).isObject()) {
        test_status_->setText(tr("Save failed — request body must be a JSON object"));
        return;
    }

    // SAVE on a mapping opened with EDIT overwrites it; a new mapping gets a fresh id.
    if (dm.id.isEmpty())
        dm.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto r = DataMappingRepository::instance().save(dm);
    if (r.is_err()) {
        LOG_ERROR("DataMapping", "Failed to save: " + QString::fromStdString(r.error()));
        test_status_->setText(tr("Save failed — database error"));
        return;
    }

    reset_wizard_state(); // a following CREATE must not inherit this mapping (or overwrite it)
    load_mappings_from_db();
    on_view_changed(0);
    LOG_INFO("DataMapping", "Mapping saved: " + name);
}

void DataMappingScreen::on_run_mapping() {
    if (run_in_flight_)
        return; // double-click on the list bypasses the disabled RUN button
    const int row = mapping_list_->currentRow();
    if (row < 0 || row >= saved_mappings_.size()) {
        QMessageBox::information(this, tr("Run Mapping"), tr("Select a saved mapping first."));
        return;
    }

    // A copy: the callback below can run synchronously (cache hit) and open a modal
    // box whose event loop may reload saved_mappings_ underneath a reference.
    const DataMapping dm = saved_mappings_[row];
    LOG_INFO("DataMapping", "Running mapping: " + dm.name);

    // Loading state — the fetch is a live network round-trip and RUN used to
    // give no feedback at all from the list view.
    run_in_flight_ = true;
    if (list_run_btn_) {
        list_run_btn_->setEnabled(false);
        list_run_btn_->setText(tr("RUNNING..."));
    }

    QPointer<DataMappingScreen> self = this;
    fincept::services::DataNormalizationService::instance().fetch_and_normalize(
        dm, [self, dm](bool ok, fincept::services::NormalizedRecord rec) {
            if (!self)
                return;
            self->run_in_flight_ = false;
            if (self->list_run_btn_) {
                self->list_run_btn_->setEnabled(true);
                self->list_run_btn_->setText(DataMappingScreen::tr("▶ RUN"));
            }
            if (ok) {
                const QString out = QJsonDocument(rec.normalized).toJson(QJsonDocument::Indented);
                self->test_output_->setPlainText(out);
                self->test_status_->setText(
                    DataMappingScreen::tr("RUN OK — %1 fields extracted").arg(rec.normalized.size()));
                LOG_INFO("DataMapping", "Run complete: " + dm.name);
                // test_output_ lives on the CREATE view; a run started from the
                // list view would otherwise write into a panel nobody can see.
                QMessageBox box(self);
                box.setIcon(QMessageBox::Information);
                box.setWindowTitle(DataMappingScreen::tr("Run: %1").arg(dm.name));
                box.setText(
                    DataMappingScreen::tr("%n field(s) extracted.", "", static_cast<int>(rec.normalized.size())));
                if (rec.from_cache)
                    box.setInformativeText(DataMappingScreen::tr(
                        "Served from the mapping's response cache (%1 s TTL) — no request was sent. Disable "
                        "caching or lower the TTL in the mapping's CACHE step to always fetch.")
                                               .arg(dm.cache_ttl));
                box.setDetailedText(out);
                box.exec();
            } else {
                const QString errs =
                    rec.errors.isEmpty() ? DataMappingScreen::tr("no error detail reported") : rec.errors.join("\n");
                self->test_status_->setText(DataMappingScreen::tr("RUN FAILED — %1").arg(rec.errors.join(", ")));
                LOG_WARN("DataMapping", "Run failed: " + errs);
                // Surface the actual upstream error, not just "failed".
                QMessageBox box(self);
                box.setIcon(QMessageBox::Warning);
                box.setWindowTitle(DataMappingScreen::tr("Run: %1").arg(dm.name));
                box.setText(DataMappingScreen::tr("The mapping did not produce a record."));
                box.setInformativeText(errs);
                // Whatever WAS extracted (a schema-validation failure still carries it) — the
                // fastest way to see which expression came up empty.
                if (!rec.normalized.isEmpty())
                    box.setDetailedText(QJsonDocument(rec.normalized).toJson(QJsonDocument::Indented));
                box.exec();
            }
        });
}

void DataMappingScreen::on_edit_mapping() {
    const int row = mapping_list_->currentRow();
    if (row < 0 || row >= saved_mappings_.size()) {
        QMessageBox::information(this, tr("Edit Mapping"), tr("Select a saved mapping first."));
        return;
    }
    load_mapping_into_wizard(saved_mappings_[row]);
    LOG_INFO("DataMapping", "Editing mapping: " + saved_mappings_[row].name);
}

void DataMappingScreen::on_template_selected(int index) {
    if (index < 0 || index >= templates().size())
        return;

    const auto& tmpl = templates()[index];
    QString detail = tr("NAME: %1\n\n"
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
                         .arg(tmpl.name, tmpl.broker, tmpl.schema, tmpl.verified ? tr("Yes") : tr("No"),
                              tmpl.description, tmpl.base_url, tmpl.endpoint, tmpl.method, tmpl.auth_type)
                         .arg(tmpl.field_mappings.size())
                         .arg(tmpl.parser, tmpl.tags.join(", "));

    template_detail_->setText(detail);
}

void DataMappingScreen::on_new_mapping() {
    reset_wizard_state();
    on_view_changed(2); // create
    on_step_changed(0);
}

void DataMappingScreen::on_delete_mapping() {
    const int row = mapping_list_->currentRow();
    if (row < 0 || row >= saved_mappings_.size()) {
        QMessageBox::information(this, tr("Delete Mapping"), tr("Select a saved mapping first."));
        return;
    }

    const QString id = saved_mappings_[row].id;
    const QString name = saved_mappings_[row].name;

    // Destructive and irreversible — it was previously a single unguarded click
    // next to RUN.
    if (QMessageBox::question(this, tr("Delete Mapping"),
                              tr("Delete the mapping \"%1\"?\n\nThis cannot be undone.").arg(name),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;

    auto r = DataMappingRepository::instance().remove(id);
    if (r.is_err()) {
        const QString err = QString::fromStdString(r.error());
        LOG_ERROR("DataMapping", "Failed to delete: " + err);
        QMessageBox::warning(this, tr("Delete Mapping"), tr("Could not delete \"%1\":\n\n%2").arg(name, err));
        return;
    }
    if (id == editing_mapping_id_)
        editing_mapping_id_.clear(); // SAVE would otherwise resurrect the deleted row under its old id

    load_mappings_from_db();
    LOG_INFO("DataMapping", "Mapping deleted: " + name);
}

void DataMappingScreen::load_mappings_from_db() {
    // Keep the caret on the same mapping across a reload (delete/save both
    // re-enter here and used to reset the selection to nothing).
    const QString previously_selected =
        (mapping_list_->currentRow() >= 0 && mapping_list_->currentRow() < saved_mappings_.size())
            ? saved_mappings_[mapping_list_->currentRow()].id
            : QString();

    saved_mappings_.clear();
    mapping_list_->clear();

    auto r = DataMappingRepository::instance().list_all();
    if (r.is_err()) {
        const QString err = QString::fromStdString(r.error());
        LOG_WARN("DataMapping", "Could not load mappings: " + err);
        if (list_empty_) {
            list_empty_->setText(tr("Could not load saved mappings:\n%1").arg(err));
            list_empty_->setVisible(true);
        }
        refresh_saved_mappings();
        return;
    }

    saved_mappings_ = r.value();
    int restore_row = -1;
    for (int i = 0; i < saved_mappings_.size(); ++i) {
        const auto& dm = saved_mappings_[i];
        mapping_list_->addItem(dm.name + " — " + dm.schema_name);
        if (!previously_selected.isEmpty() && dm.id == previously_selected)
            restore_row = i;
    }
    if (restore_row >= 0)
        mapping_list_->setCurrentRow(restore_row);

    if (status_mappings_) {
        status_mappings_->setText(tr("Saved: %1").arg(saved_mappings_.size()));
    }

    // The empty-state label used to sit permanently under a populated list.
    if (list_empty_) {
        list_empty_->setText(tr("No mappings saved yet.\nClick CREATE to build your first data mapping."));
        list_empty_->setVisible(saved_mappings_.isEmpty());
    }
    refresh_saved_mappings();
}

/// Enable/disable the list-view actions from the current selection.
void DataMappingScreen::refresh_saved_mappings() {
    const bool has_selection =
        mapping_list_ && mapping_list_->currentRow() >= 0 && mapping_list_->currentRow() < saved_mappings_.size();
    if (list_run_btn_ && !run_in_flight_)
        list_run_btn_->setEnabled(has_selection);
    if (list_edit_btn_)
        list_edit_btn_->setEnabled(has_selection);
    if (list_del_btn_)
        list_del_btn_->setEnabled(has_selection);
}

QJsonArray DataMappingScreen::collect_field_mappings() const {
    QJsonArray out;
    for (int r = 0; r < mapping_table_->rowCount(); ++r) {
        const auto* target = mapping_table_->item(r, 0);
        if (!target)
            continue;
        const auto* expr_item = mapping_table_->item(r, 1);
        const auto* trans_item = mapping_table_->item(r, 2);
        const auto* def_item = mapping_table_->item(r, 3);
        const QString expr = expr_item ? expr_item->text().trimmed() : QString();
        const QString transform = trans_item ? trans_item->text().trimmed() : QString();
        const QString default_val = def_item ? def_item->text().trimmed() : QString();
        // A row is a mapping when it has an expression OR a default to fall back
        // on (templates map e.g. "symbol" to a literal default only), and it must
        // carry transform/default through to the saved JSON — they were dropped
        // here, so saved mappings never applied them.
        if (expr.isEmpty() && default_val.isEmpty())
            continue;
        QJsonObject m;
        m["target"] = target->text();
        m["expression"] = expr;
        if (!transform.isEmpty())
            m["transform"] = transform;
        if (!default_val.isEmpty())
            m["default_val"] = default_val; // key read by DataNormalizationService::normalize_raw
        out.append(m);
    }
    return out;
}

DataMapping DataMappingScreen::build_mapping_from_form() const {
    DataMapping dm;
    dm.id = editing_mapping_id_;
    dm.name = api_name_->text().trimmed();
    dm.source_id = editing_source_id_;
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
    dm.field_mappings_json = QString::fromUtf8(QJsonDocument(collect_field_mappings()).toJson(QJsonDocument::Compact));
    return dm;
}

void DataMappingScreen::reset_wizard_state() {
    editing_mapping_id_.clear();
    editing_source_id_.clear();

    api_name_->clear();
    api_base_url_->clear();
    api_endpoint_->clear();
    api_auth_value_->clear();
    api_headers_->clear();
    api_body_->clear();
    api_method_->setCurrentIndex(0);
    api_auth_type_->setCurrentIndex(0);
    api_timeout_->setValue(30);
    api_test_status_->clear();

    schema_select_->setCurrentIndex(0);
    parser_engine_->setCurrentIndex(0);
    mapping_table_->setRowCount(0);
    json_tree_->clear();

    cache_enabled_->setCurrentIndex(0);
    cache_ttl_->setValue(300);

    sample_data_ = QJsonDocument();
    test_result_ = QJsonObject();
    test_output_->clear();
    test_status_->setText(tr("Not yet tested"));
    save_btn_->setEnabled(false);
    if (right_test_info_)
        right_test_info_->setText(tr("Test: --"));
}

void DataMappingScreen::load_mapping_into_wizard(const DataMapping& dm) {
    reset_wizard_state();
    editing_mapping_id_ = dm.id;
    editing_source_id_ = dm.source_id;

    api_name_->setText(dm.name);
    api_base_url_->setText(dm.base_url);
    api_endpoint_->setText(dm.endpoint);
    api_method_->setCurrentText(dm.method);
    api_auth_type_->setCurrentText(dm.auth_type);
    api_auth_value_->setText(dm.auth_token);
    api_headers_->setPlainText(dm.headers);
    api_body_->setPlainText(dm.body);
    parser_engine_->setCurrentText(dm.parser);
    cache_enabled_->setCurrentIndex(dm.cache_enabled ? 0 : 1);
    cache_ttl_->setValue(dm.cache_ttl);
    schema_select_->setCurrentText(dm.schema_name);

    // Rows come from the schema; overlay what the saved mapping defined.
    populate_mapping_list();
    const QJsonArray saved = QJsonDocument::fromJson(dm.field_mappings_json.toUtf8()).array();
    for (const QJsonValue& v : saved) {
        const QJsonObject fm = v.toObject();
        const QString target = fm["target"].toString();
        for (int r = 0; r < mapping_table_->rowCount(); ++r) {
            const auto* t = mapping_table_->item(r, 0);
            if (!t || t->text() != target)
                continue;
            mapping_table_->item(r, 1)->setText(fm["expression"].toString());
            mapping_table_->item(r, 2)->setText(fm["transform"].toString());
            mapping_table_->item(r, 3)->setText(fm.contains("default_val") ? fm["default_val"].toString()
                                                                           : fm["default"].toString());
            break;
        }
    }

    // The stored mapping was valid when it was saved, so SAVE is allowed straight
    // away (renaming, changing the cache TTL); re-test to check the field mapping.
    save_btn_->setEnabled(true);
    test_status_->setText(tr("Editing \"%1\" — SAVE overwrites it").arg(dm.name));

    on_view_changed(2); // create
    on_step_changed(0);
}

} // namespace fincept::screens
