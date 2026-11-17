// ConnectionForm.cpp — see ConnectionForm.h.

#include "screens/data_sources/ConnectionForm.h"

#include "screens/data_sources/DataWorkspace.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens::datasources {

namespace {

bool cf_is_full_url(const QString& text) {
    const QUrl u(text, QUrl::TolerantMode);
    return u.isValid() && !u.scheme().isEmpty() && !u.host().isEmpty();
}

bool cf_is_port_field(const QString& name) {
    return name.compare(QLatin1String("port"), Qt::CaseInsensitive) == 0 || name.endsWith(QLatin1String("Port"));
}

QString cf_file_filter(const QString& connector_id) {
    static const QHash<QString, QString> kFilters = {
        {"csv", QStringLiteral("CSV / text (*.csv *.tsv *.txt)")},
        {"excel", QStringLiteral("Excel workbooks (*.xlsx *.xlsm *.xls)")},
        {"json", QStringLiteral("JSON (*.json *.jsonl *.ndjson)")},
        {"parquet", QStringLiteral("Parquet (*.parquet *.parq *.pq)")},
        {"geoparquet", QStringLiteral("Parquet (*.parquet *.geoparquet)")},
        {"feather", QStringLiteral("Feather / Arrow (*.feather *.arrow)")},
        {"arrow-ipc", QStringLiteral("Arrow IPC (*.arrow *.feather *.ipc *.arrows)")},
        {"xml", QStringLiteral("XML (*.xml)")},
        {"sqlite", QStringLiteral("SQLite databases (*.db *.sqlite *.sqlite3 *.db3)")},
    };
    const QString specific = kFilters.value(connector_id);
    return specific.isEmpty() ? QObject::tr("All files (*)") : specific + QStringLiteral(";;") + QObject::tr("All files (*)");
}

} // namespace

ConnectionForm::ConnectionForm(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(8);
}

QWidget* ConnectionForm::add_row(const QString& label, QWidget* input, bool required) {
    // A label with a buddy reads '&' as a mnemonic: escape it so "S&P" shows as written.
    auto* lbl = new QLabel(amp_safe(required ? label + QStringLiteral(" *") : label), body_);
    set_look(lbl, "field");
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl->setBuddy(input);
    grid_->addWidget(lbl, row_, 0);
    grid_->addWidget(input, row_, 1);
    ++row_;
    return input;
}

void ConnectionForm::set_connector(const ConnectorConfig* connector) {
    connector_ = connector;
    inputs_.clear();
    if (body_) {
        body_->hide();
        body_->deleteLater();
    }
    body_ = new QWidget(this);
    grid_ = new QGridLayout(body_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(12);
    grid_->setVerticalSpacing(8);
    grid_->setColumnStretch(1, 1);
    grid_->setColumnMinimumWidth(0, fontMetrics().horizontalAdvance(QStringLiteral("Connection name *")) + 8);
    row_ = 0;

    name_ = new QLineEdit(body_);
    name_->setAccessibleName(tr("Connection name"));
    add_row(tr("Connection name"), name_, false);
    connect(name_, &QLineEdit::textEdited, this, &ConnectionForm::touch);
    enabled_ = new QCheckBox(tr("Active"), body_);
    enabled_->setChecked(true);
    add_row(tr("Enabled"), enabled_, false);
    connect(enabled_, &QCheckBox::toggled, this, &ConnectionForm::touch);

    bool has_secret = false;
    if (connector_) {
        name_->setPlaceholderText(tr("%1 connection").arg(connector_->name));
        for (const FieldDef& f : connector_->fields) {
            QWidget* input = nullptr;
            QWidget* cell = nullptr;
            if (f.type == FieldType::Checkbox) {
                auto* check = new QCheckBox(f.required ? tr("Required") : tr("Optional"), body_);
                connect(check, &QCheckBox::toggled, this, &ConnectionForm::touch);
                input = check;
            } else if (f.type == FieldType::Select) {
                auto* combo = new QComboBox(body_);
                for (const SelectOption& o : f.options)
                    combo->addItem(o.label, o.value);
                connect(combo, &QComboBox::activated, this, &ConnectionForm::touch);
                input = combo;
            } else if (f.type == FieldType::Textarea) {
                auto* edit = new QPlainTextEdit(body_);
                edit->setPlaceholderText(f.placeholder);
                edit->setMaximumHeight(fontMetrics().lineSpacing() * 5 + 12);
                connect(edit, &QPlainTextEdit::textChanged, this, &ConnectionForm::touch);
                input = edit;
                if (f.name.contains(QLatin1String("credential"), Qt::CaseInsensitive) ||
                    f.name.compare(QLatin1String("headers"), Qt::CaseInsensitive) == 0)
                    has_secret = true;
            } else {
                const bool is_file = f.type == FieldType::File || f.name == QLatin1String("filepath");
                QWidget* row = is_file ? new QWidget(body_) : nullptr;
                auto* edit = new QLineEdit(row ? row : body_);
                edit->setPlaceholderText(f.placeholder);
                if (f.type == FieldType::Password) {
                    edit->setEchoMode(QLineEdit::Password);
                    edit->setAccessibleName(tr("%1 (secret)").arg(f.label));
                    has_secret = true;
                }
                if (f.type == FieldType::Number)
                    edit->setValidator(new QIntValidator(0, cf_is_port_field(f.name) ? 65535 : 999999999, edit));
                connect(edit, &QLineEdit::textEdited, this, &ConnectionForm::touch);
                input = edit;
                if (row) {
                    auto* hl = new QHBoxLayout(row);
                    hl->setContentsMargins(0, 0, 0, 0);
                    hl->setSpacing(6);
                    hl->addWidget(edit, 1);
                    auto* browse = new QPushButton(tr("BROWSE…"), row);
                    const QString filter = cf_file_filter(connector_->id);
                    connect(browse, &QPushButton::clicked, this, [this, edit, filter]() {
                        const QString cur = edit->text().trimmed();
                        const QString picked = QFileDialog::getOpenFileName(
                            this, tr("Select file"), cur.isEmpty() ? QString() : QFileInfo(cur).absolutePath(), filter);
                        if (!picked.isEmpty()) {
                            edit->setText(QDir::toNativeSeparators(picked));
                            touch();
                        }
                    });
                    hl->addWidget(browse);
                    cell = row;
                }
            }
            if (!f.placeholder.isEmpty() && f.type != FieldType::Password)
                input->setToolTip(tr("Example: %1").arg(f.placeholder));
            inputs_.insert(f.name, input);
            add_row(f.label, cell ? cell : input, f.required);
        }
    }
    tags_ = new QLineEdit(body_);
    tags_->setPlaceholderText(tr("Comma-separated, e.g. prod, research"));
    connect(tags_, &QLineEdit::textEdited, this, &ConnectionForm::touch);
    add_row(tr("Tags"), tags_, false);

    secret_note_ = new QLabel(body_);
    set_look(secret_note_, "note");
    secret_note_->setWordWrap(true);
    secret_note_->setText(tr("Secrets are stored encrypted on this computer (never in the connection table). EXPORT "
                             "leaves them out unless you choose to include them, encrypted with a passphrase."));
    secret_note_->setVisible(has_secret);
    grid_->addWidget(secret_note_, row_++, 0, 1, 2);
    layout()->addWidget(body_);
    dirty_ = false;
}

void ConnectionForm::set_field_value(const FieldDef& f, const QJsonValue& v) {
    QWidget* w = inputs_.value(f.name);
    if (auto* check = qobject_cast<QCheckBox*>(w)) {
        check->setChecked(v.isBool() ? v.toBool() : v.toVariant().toString() == QLatin1String("true"));
    } else if (auto* combo = qobject_cast<QComboBox*>(w)) {
        const int i = combo->findData(v.toVariant().toString());
        if (i >= 0)
            combo->setCurrentIndex(i);
    } else if (auto* text = qobject_cast<QPlainTextEdit*>(w)) {
        text->setPlainText(v.isObject() ? QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact))
                                        : v.toVariant().toString());
    } else if (auto* line = qobject_cast<QLineEdit*>(w)) {
        line->setText(v.isDouble() ? QString::number(v.toDouble(), 'g', 15) : v.toVariant().toString());
    }
}

void ConnectionForm::load(const DataSource& ds, bool duplicate) {
    loading_ = true;
    const QJsonObject cfg = QJsonDocument::fromJson(ds.config.toUtf8()).object();
    name_->setText(duplicate ? tr("Copy of %1").arg(ds.display_name) : ds.display_name);
    enabled_->setChecked(ds.enabled);
    tags_->setText(ds.tags);
    if (connector_) {
        for (const FieldDef& f : connector_->fields)
            set_field_value(f, cfg.contains(f.name) ? cfg.value(f.name) : QJsonValue(f.default_value));
    }
    loading_ = false;
    dirty_ = duplicate;
}

void ConnectionForm::load_defaults() {
    loading_ = true;
    name_->clear();
    enabled_->setChecked(true);
    tags_->clear();
    if (connector_) {
        for (const FieldDef& f : connector_->fields)
            set_field_value(f, QJsonValue(f.default_value));
    }
    loading_ = false;
    dirty_ = true;
}

QJsonObject ConnectionForm::current_config() const {
    QJsonObject cfg;
    if (!connector_)
        return cfg;
    for (const FieldDef& f : connector_->fields) {
        QWidget* w = inputs_.value(f.name);
        if (auto* line = qobject_cast<QLineEdit*>(w)) {
            const QString t = line->text().trimmed();
            if (f.type == FieldType::Number && !t.isEmpty())
                cfg.insert(f.name, t.toInt());
            else
                cfg.insert(f.name, t);
        } else if (auto* text = qobject_cast<QPlainTextEdit*>(w)) {
            cfg.insert(f.name, text->toPlainText().trimmed());
        } else if (auto* combo = qobject_cast<QComboBox*>(w)) {
            cfg.insert(f.name, combo->currentData().toString());
        } else if (auto* check = qobject_cast<QCheckBox*>(w)) {
            cfg.insert(f.name, check->isChecked());
        }
    }
    return cfg;
}

QString ConnectionForm::current_name() const {
    const QString n = name_ ? name_->text().trimmed() : QString();
    return n.isEmpty() && connector_ ? connector_->name : n;
}

bool ConnectionForm::collect(QJsonObject* config, QString* name, bool* enabled, QString* tags, QString* error) {
    if (!connector_) {
        *error = tr("Pick a connection type first");
        return false;
    }
    const QJsonObject cfg = current_config();
    for (const FieldDef& f : connector_->fields) {
        QWidget* w = inputs_.value(f.name);
        const QString text = cfg.value(f.name).toVariant().toString();
        auto fail = [&](const QString& msg) {
            *error = msg;
            if (w)
                w->setFocus(Qt::OtherFocusReason);
            return false;
        };
        if (f.required && f.type != FieldType::Checkbox && text.isEmpty())
            return fail(tr("%1 is required").arg(f.label));
        if (f.type == FieldType::Url && !text.isEmpty() && !cf_is_full_url(text))
            return fail(tr("%1 must be a full URL including the scheme, e.g. %2")
                            .arg(f.label, f.placeholder.isEmpty() ? QStringLiteral("https://host") : f.placeholder));
        if (f.type == FieldType::Number && cf_is_port_field(f.name) && !text.isEmpty()) {
            const int port = text.toInt();
            if (port < 1 || port > 65535)
                return fail(tr("%1 must be between 1 and 65535").arg(f.label));
        }
        if (f.type == FieldType::Textarea && f.label.contains(QLatin1String("JSON"), Qt::CaseInsensitive) &&
            !text.isEmpty() && !QJsonDocument::fromJson(text.toUtf8()).isObject())
            return fail(tr("%1 must be a valid JSON object").arg(f.label));
    }
    *config = cfg;
    *name = current_name();
    *enabled = enabled_->isChecked();
    *tags = tags_->text().trimmed();
    return true;
}

void ConnectionForm::touch() {
    if (loading_)
        return;
    dirty_ = true;
    emit edited();
}

} // namespace fincept::screens::datasources
