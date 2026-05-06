// ConnectionConfigDialog.cpp — modal connection editor for the Data Sources
// screen. Self-contained: builds its own form, validates, saves, and returns
// the resulting connection ID.

#include "screens/data_sources/ConnectionConfigDialog.h"

#include "screens/data_sources/DataSourcesHelpers.h"
#include "storage/repositories/DataSourceRepository.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens::datasources {

namespace col = fincept::ui::colors;

QString show_connection_config_dialog(QWidget* parent, const ConnectorConfig& config,
                                      const QString& edit_id, bool duplicate) {
    const bool editing = !edit_id.isEmpty() && !duplicate;

    DataSource existing;
    QJsonObject existing_cfg;
    bool existing_loaded = false;

    if (!edit_id.isEmpty()) {
        auto result = DataSourceRepository::instance().get(edit_id);
        if (result.is_ok()) {
            existing = result.value();
            existing_cfg = QJsonDocument::fromJson(existing.config.toUtf8()).object();
            existing_loaded = true;
        }
    }

    QString saved_id;

    QDialog dlg(parent);
    dlg.setWindowTitle(editing     ? QString("Edit — %1").arg(config.name)
                       : duplicate ? QString("Clone — %1").arg(config.name)
                                   : QString("Configure — %1").arg(config.name));
    dlg.resize(560, 620);
    dlg.setModal(true);
    dlg.setStyleSheet(
        QString("QDialog { background:%1; color:%2; }"
                "QLabel { color:%3; font-size:12px; background:transparent; font-family:'Consolas','Courier "
                "New',monospace; }"
                "QLineEdit, QTextEdit, QComboBox { background:%4; border:1px solid %5; color:%2;"
                "  padding:6px 10px; font-size:13px; font-family:'Consolas','Courier New',monospace; }"
                "QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border-color:%6; }"
                "QCheckBox { color:%2; font-size:13px; font-family:'Consolas','Courier New',monospace; }"
                "QPushButton { padding:7px 18px; font-size:12px; font-weight:700;"
                "  font-family:'Consolas','Courier New',monospace; }")
            .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::TEXT_SECONDARY(),
                 col::BG_BASE(), col::BORDER_DIM(), col::AMBER()));

    auto* root_vl = new QVBoxLayout(&dlg);
    root_vl->setContentsMargins(0, 0, 0, 0);
    root_vl->setSpacing(0);

    // Dialog header
    auto* dlg_hdr = new QWidget(&dlg);
    dlg_hdr->setFixedHeight(56);
    dlg_hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* dlg_hdr_hl = new QHBoxLayout(dlg_hdr);
    dlg_hdr_hl->setContentsMargins(16, 0, 16, 0);
    dlg_hdr_hl->setSpacing(12);

    auto* code_lbl = new QLabel(connector_code(config));
    code_lbl->setAlignment(Qt::AlignCenter);
    code_lbl->setFixedSize(36, 36);
    code_lbl->setStyleSheet(QString("background:%1;color:%2;border:1px solid %3;font-size:14px;font-weight:700;")
                                .arg(config.color, col::TEXT_PRIMARY(), col::BORDER_DIM()));
    dlg_hdr_hl->addWidget(code_lbl);

    auto* title_vl = new QVBoxLayout;
    title_vl->setContentsMargins(0, 0, 0, 0);
    title_vl->setSpacing(2);

    auto* dlg_title = new QLabel(editing     ? QString("Edit  %1").arg(config.name)
                                 : duplicate ? QString("Clone  %1").arg(config.name)
                                             : config.name);
    dlg_title->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:700;background:transparent;").arg(col::AMBER()));
    title_vl->addWidget(dlg_title);

    auto* dlg_sub = new QLabel(config.description);
    dlg_sub->setWordWrap(true);
    dlg_sub->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;").arg(col::TEXT_SECONDARY()));
    title_vl->addWidget(dlg_sub);

    dlg_hdr_hl->addLayout(title_vl, 1);
    root_vl->addWidget(dlg_hdr);

    // Scrollable form body
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background:%1; border:none; }").arg(col::BG_SURFACE()));

    auto* body = new QWidget(&dlg);
    auto* body_vl = new QVBoxLayout(body);
    body_vl->setContentsMargins(20, 20, 20, 20);
    body_vl->setSpacing(16);

    auto* form = new QGridLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setColumnStretch(1, 1);
    form->setColumnMinimumWidth(0, 130);

    int row = 0;

    auto* name_lbl = new QLabel("Connection Name");
    name_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addWidget(name_lbl, row, 0);

    auto* name_edit = new QLineEdit;
    name_edit->setPlaceholderText(config.name + " Connection");
    name_edit->setText(
        existing_loaded ? (duplicate ? QString("Copy of %1").arg(existing.display_name) : existing.display_name) : "");
    name_edit->setFixedHeight(34);
    form->addWidget(name_edit, row, 1);
    ++row;

    auto* enabled_lbl = new QLabel("Enable Connection");
    enabled_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addWidget(enabled_lbl, row, 0);

    auto* enabled_check = new QCheckBox("Active");
    enabled_check->setChecked(existing_loaded ? existing.enabled : true);
    form->addWidget(enabled_check, row, 1);
    ++row;

    QMap<QString, QWidget*> field_widgets;

    for (const auto& field : config.fields) {
        auto* lbl = new QLabel(field.label + (field.required ? " *" : ""));
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->addWidget(lbl, row, 0);

        QWidget* input = nullptr;
        if (field.type == FieldType::Checkbox) {
            auto* check = new QCheckBox(field.required ? "Required" : "Optional");
            const bool value = existing_cfg.contains(field.name) ? existing_cfg.value(field.name).toBool()
                                                                 : (field.default_value == "true");
            check->setChecked(value);
            input = check;
        } else if (field.type == FieldType::Select) {
            auto* combo = new QComboBox;
            for (const auto& option : field.options)
                combo->addItem(option.label, option.value);
            const QString current = existing_cfg.contains(field.name)
                                        ? existing_cfg.value(field.name).toString()
                                        : field.default_value;
            const int index = combo->findData(current);
            if (index >= 0) combo->setCurrentIndex(index);
            input = combo;
        } else if (field.type == FieldType::Textarea) {
            auto* edit = new QTextEdit;
            edit->setMaximumHeight(90);
            edit->setPlaceholderText(field.placeholder);
            edit->setPlainText(existing_cfg.contains(field.name) ? existing_cfg.value(field.name).toString()
                                                                 : field.default_value);
            input = edit;
        } else {
            auto* edit = new QLineEdit;
            edit->setPlaceholderText(field.placeholder);
            edit->setFixedHeight(34);
            if (field.type == FieldType::Password) edit->setEchoMode(QLineEdit::Password);
            if (field.type == FieldType::Number)   edit->setValidator(new QIntValidator(0, 999999999, edit));
            const QString text = existing_cfg.contains(field.name)
                                     ? existing_cfg.value(field.name).toVariant().toString()
                                     : field.default_value;
            edit->setText(text);
            input = edit;
        }

        field_widgets[field.name] = input;
        form->addWidget(input, row, 1);
        ++row;
    }

    // Tags field
    auto* tags_lbl = new QLabel("Tags");
    tags_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addWidget(tags_lbl, row, 0);

    auto* tags_edit = new QLineEdit;
    tags_edit->setPlaceholderText("Comma-separated tags, e.g. prod, live, trading");
    tags_edit->setFixedHeight(34);
    tags_edit->setText(existing_loaded ? existing.tags : "");
    form->addWidget(tags_edit, row, 1);
    ++row;

    body_vl->addLayout(form);

    auto* note = new QLabel("Fields marked with * are required.");
    note->setWordWrap(true);
    note->setStyleSheet(
        QString("color:%1;font-size:11px;font-style:italic;background:transparent;").arg(col::TEXT_TERTIARY()));
    body_vl->addWidget(note);
    body_vl->addStretch();

    scroll->setWidget(body);
    root_vl->addWidget(scroll, 1);

    // Dialog footer
    auto* footer = new QWidget(&dlg);
    footer->setFixedHeight(54);
    footer->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* footer_hl = new QHBoxLayout(footer);
    footer_hl->setContentsMargins(16, 0, 16, 0);
    footer_hl->setSpacing(8);

    auto* status = new QLabel;
    status->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(col::TEXT_SECONDARY()));
    footer_hl->addWidget(status, 1);

    auto* cancel = new QPushButton("Cancel");
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;}"
                                  "QPushButton:hover{background:%3;color:%4;}")
                              .arg(col::BG_BASE(), col::TEXT_SECONDARY(), col::BORDER_MED(), col::TEXT_PRIMARY()));
    footer_hl->addWidget(cancel);

    auto* save = new QPushButton(editing ? "Update Connection" : "Save Connection");
    save->setCursor(Qt::PointingHandCursor);
    save->setDefault(true);
    save->setAutoDefault(true);
    save->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;border:1px solid %2;}"
                                "QPushButton:hover{background:%1;color:%3;}")
                            .arg(col::AMBER(), col::AMBER_DIM(), col::BG_BASE()));
    footer_hl->addWidget(save);

    root_vl->addWidget(footer);

    name_edit->setFocus();

    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dlg, [&, existing_loaded]() {
        QJsonObject cfg_json;

        for (const auto& field : config.fields) {
            QWidget* widget = field_widgets.value(field.name, nullptr);
            QString text_value;

            if (auto* line_edit = qobject_cast<QLineEdit*>(widget)) {
                text_value = line_edit->text().trimmed();
                if (field.type == FieldType::Number)
                    cfg_json[field.name] = text_value.toInt();
                else
                    cfg_json[field.name] = text_value;
            } else if (auto* text_edit = qobject_cast<QTextEdit*>(widget)) {
                text_value = text_edit->toPlainText().trimmed();
                cfg_json[field.name] = text_value;
            } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
                text_value = combo->currentData().toString();
                cfg_json[field.name] = text_value;
            } else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
                cfg_json[field.name] = check->isChecked();
            }

            if (field.required && field.type != FieldType::Checkbox && text_value.isEmpty()) {
                status->setText("Missing required field: " + field.label);
                status->setStyleSheet(
                    QString("color:%1;font-size:12px;font-weight:700;background:transparent;").arg(col::NEGATIVE()));
                return;
            }
        }

        DataSource ds = (existing_loaded && !duplicate) ? existing : DataSource{};
        ds.id    = (existing_loaded && !duplicate) ? existing.id : QUuid::createUuid().toString(QUuid::WithoutBraces);
        ds.alias = (existing_loaded && !duplicate) && !existing.alias.isEmpty()
                       ? existing.alias
                       : (config.id + "_" + ds.id.left(8));
        ds.display_name = name_edit->text().trimmed().isEmpty() ? config.name : name_edit->text().trimmed();
        ds.description  = config.description;
        ds.type         = persistence_type(config);
        ds.provider     = config.id;
        ds.category     = category_str(config.category);
        ds.config       = QJsonDocument(cfg_json).toJson(QJsonDocument::Compact);
        ds.enabled      = enabled_check->isChecked();
        ds.tags         = tags_edit->text().trimmed();

        const auto result = DataSourceRepository::instance().save(ds);
        if (result.is_err()) {
            status->setText("Failed to save: " + QString::fromStdString(result.error()));
            status->setStyleSheet(
                QString("color:%1;font-size:12px;font-weight:700;background:transparent;").arg(col::NEGATIVE()));
            return;
        }

        saved_id = ds.id;
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted)
        return saved_id;
    return {};
}

} // namespace fincept::screens::datasources
