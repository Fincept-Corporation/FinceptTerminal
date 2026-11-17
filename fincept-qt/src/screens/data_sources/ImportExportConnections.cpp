// ImportExportConnections.cpp — see ImportExportConnections.h.

#include "screens/data_sources/ImportExportConnections.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourceTypes.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "services/data_sources/ConnectionTransfer.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens::datasources {

namespace dsrc = fincept::services::datasources;

namespace {

constexpr const char* kIeTag = "DataSources";
constexpr qint64 kIeMaxFileBytes = 32 * 1024 * 1024;

/// Run `work` on a worker thread and `then(result)` on the GUI thread while `ctx` lives.
template <typename R, typename Work, typename Then>
void ie_async(QObject* ctx, Work work, Then then) {
    QPointer<QObject> guard(ctx);
    (void)QtConcurrent::run([guard, work, then]() {
        R r = work();
        QMetaObject::invokeMethod(
            qApp,
            [guard, then, r = std::move(r)]() {
                if (guard)
                    then(r);
            },
            Qt::QueuedConnection);
    });
}

QString ie_default_dir() {
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return docs.isEmpty() ? QDir::homePath() : docs;
}

/// Number / Checkbox fields stored as the form stores them (a hand-edited file writes "5432", "true").
void ie_coerce_config_types(QJsonObject& cfg, const ConnectorConfig& connector) {
    for (const auto& field : connector.fields) {
        if (!cfg.contains(field.name) || !cfg.value(field.name).isString())
            continue;
        const QString text = cfg.value(field.name).toString().trimmed();
        if (field.type == FieldType::Number) {
            bool ok = false;
            const int n = text.toInt(&ok);
            if (ok)
                cfg[field.name] = n;
        } else if (field.type == FieldType::Checkbox) {
            const QString t = text.toLower();
            cfg[field.name] = (t == QLatin1String("true") || t == QLatin1String("1") || t == QLatin1String("yes"));
        }
    }
}

/// Export choices: leave secrets out, or include them encrypted with a passphrase.
bool ie_ask_export_options(QWidget* parent, dsrc::ExportOptions* opts) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Export connections"));
    auto* vl = new QVBoxLayout(&dlg);
    auto* intro = new QLabel(QObject::tr("Connection settings are exported. What about their secrets (passwords, API "
                                         "keys, tokens, auth headers)?"),
                             &dlg);
    intro->setWordWrap(true);
    vl->addWidget(intro);
    auto* leave_out = new QRadioButton(QObject::tr("Leave secrets out (recommended) — re-enter them after import"), &dlg);
    auto* encrypt = new QRadioButton(QObject::tr("Include secrets, encrypted with a passphrase"), &dlg);
    leave_out->setChecked(true);
    vl->addWidget(leave_out);
    vl->addWidget(encrypt);
    auto* pass = new QLineEdit(&dlg);
    pass->setEchoMode(QLineEdit::Password);
    pass->setPlaceholderText(QObject::tr("Passphrase (at least 8 characters)"));
    auto* pass2 = new QLineEdit(&dlg);
    pass2->setEchoMode(QLineEdit::Password);
    pass2->setPlaceholderText(QObject::tr("Repeat the passphrase"));
    auto* note = new QLabel(QObject::tr("The passphrase is not stored anywhere: without it the secrets in the file "
                                        "cannot be read — by anyone, including you."),
                            &dlg);
    note->setWordWrap(true);
    auto* error = new QLabel(&dlg);
    error->setWordWrap(true);
    vl->addWidget(pass);
    vl->addWidget(pass2);
    vl->addWidget(note);
    vl->addWidget(error);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Export…"));
    vl->addWidget(buttons);
    auto sync = [=]() {
        const bool on = encrypt->isChecked();
        pass->setEnabled(on);
        pass2->setEnabled(on);
        note->setEnabled(on);
    };
    sync();
    QObject::connect(encrypt, &QRadioButton::toggled, &dlg, sync);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (encrypt->isChecked()) {
            if (pass->text().size() < 8) {
                error->setText(QObject::tr("The passphrase needs at least 8 characters."));
                pass->setFocus();
                return;
            }
            if (pass->text() != pass2->text()) {
                error->setText(QObject::tr("The two passphrases differ."));
                pass2->setFocus();
                return;
            }
        }
        dlg.accept();
    });
    if (dlg.exec() != QDialog::Accepted)
        return false;
    opts->include_secrets = encrypt->isChecked();
    opts->passphrase = opts->include_secrets ? pass->text() : QString();
    return true;
}

enum class PassChoice { WithSecrets, WithoutSecrets, Cancel };

PassChoice ie_ask_passphrase(QWidget* parent, const QString& error_text, QString* passphrase) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Import connections"));
    auto* vl = new QVBoxLayout(&dlg);
    auto* intro = new QLabel(QObject::tr("This file holds encrypted secrets. Enter the passphrase it was exported "
                                         "with, or import the connections without their secrets."),
                             &dlg);
    intro->setWordWrap(true);
    vl->addWidget(intro);
    auto* pass = new QLineEdit(&dlg);
    pass->setEchoMode(QLineEdit::Password);
    vl->addWidget(pass);
    if (!error_text.isEmpty()) {
        auto* err = new QLabel(error_text, &dlg);
        err->setWordWrap(true);
        vl->addWidget(err);
    }
    auto* buttons = new QDialogButtonBox(&dlg);
    auto* with = buttons->addButton(QObject::tr("Import with secrets"), QDialogButtonBox::AcceptRole);
    auto* without = buttons->addButton(QObject::tr("Import without secrets"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    vl->addWidget(buttons);
    PassChoice choice = PassChoice::Cancel;
    QObject::connect(with, &QPushButton::clicked, &dlg, [&]() {
        choice = PassChoice::WithSecrets;
        dlg.accept();
    });
    QObject::connect(without, &QPushButton::clicked, &dlg, [&]() {
        choice = PassChoice::WithoutSecrets;
        dlg.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return PassChoice::Cancel;
    *passphrase = pass->text();
    return choice;
}

struct ReadOutcome {
    dsrc::ImportResult result;
    QByteArray bytes;
    QString io_error;
};

struct SaveOutcome {
    int imported = 0;
    int skipped = 0;
    int unknown = 0;
    QStringList errors;
};

void ie_confirm_and_save(QWidget* parent, const QString& path, const dsrc::ImportResult& res, TransferDone done) {
    if (res.connections.isEmpty()) {
        done(false, 0, QObject::tr("The file has no connections to import."));
        return;
    }
    QString detail = QObject::tr("Import %n connection(s) from %1?", "", int(res.connections.size()))
                         .arg(QDir::toNativeSeparators(path));
    if (res.secrets_restored > 0)
        detail += QStringLiteral("\n") + QObject::tr("%n secret(s) restored.", "", res.secrets_restored);
    if (res.secrets_missing > 0)
        detail += QStringLiteral("\n") + QObject::tr("%n secret(s) are not in the file — re-enter them after import.", "",
                                                     res.secrets_missing);
    detail += QStringLiteral("\n\n") + QObject::tr("Only import files you created or trust: a connection can point at "
                                                   "any host.");
    if (QMessageBox::question(parent, QObject::tr("Import connections"), detail, QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Yes) {
        done(false, 0, QString());
        return;
    }
    const QVector<dsrc::ImportedConnection> items = res.connections;
    const int skipped_in_file = res.skipped;
    ie_async<SaveOutcome>(
        parent,
        [items, skipped_in_file]() {
            SaveOutcome out;
            out.skipped = skipped_in_file;
            for (const dsrc::ImportedConnection& ic : items) {
                const ConnectorConfig* connector = find_connector_config(ic.connection.provider);
                if (!connector) {
                    ++out.unknown;
                    ++out.skipped;
                    continue;
                }
                DataSource ds;
                ds.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                ds.alias = connector->id + QLatin1Char('_') + ds.id.left(8);
                ds.display_name = ic.connection.display_name.isEmpty() ? connector->name : ic.connection.display_name;
                ds.description = ic.connection.description.isEmpty() ? connector->description : ic.connection.description;
                ds.type = persistence_type(*connector);
                ds.provider = connector->id;
                ds.category = category_str(connector->category);
                QJsonObject cfg = ic.connection.config;
                ie_coerce_config_types(cfg, *connector);
                ds.config = QString::fromUtf8(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
                ds.enabled = ic.connection.enabled;
                ds.tags = ic.connection.tags;
                const auto r = DataSourceRepository::instance().save(ds);
                if (r.is_ok()) {
                    ++out.imported;
                } else {
                    ++out.skipped;
                    out.errors << QString::fromStdString(r.error());
                }
            }
            return out;
        },
        [done, path](const SaveOutcome& out) {
            LOG_INFO(kIeTag, QStringLiteral("Import: %1 imported, %2 skipped").arg(out.imported).arg(out.skipped));
            if (out.imported > 0)
                fincept::services::FileManagerService::instance().import_file(path, QStringLiteral("data_sources"));
            QString msg = QObject::tr("Imported %1 connection(s), skipped %2.").arg(out.imported).arg(out.skipped);
            if (out.unknown > 0)
                msg += QStringLiteral(" ") + QObject::tr("%1 named a connection type this version does not have.").arg(out.unknown);
            if (!out.errors.isEmpty())
                msg += QStringLiteral(" ") + out.errors.first();
            done(out.imported > 0, out.imported, msg);
        });
}

void ie_read_and_continue(QWidget* parent, const QString& path, const QByteArray& bytes, const QString& passphrase,
                          bool skip_secrets, int attempt, TransferDone done) {
    ie_async<dsrc::ImportResult>(
        parent, [bytes, passphrase, skip_secrets]() { return dsrc::read_connections_document(bytes, passphrase, skip_secrets); },
        [parent, path, bytes, attempt, done](const dsrc::ImportResult& res) {
            QPointer<QWidget> p(parent);
            if (!p)
                return;
            if (res.needs_passphrase || res.wrong_passphrase) {
                if (attempt >= 5) {
                    done(false, 0, QObject::tr("Wrong passphrase — nothing was imported."));
                    return;
                }
                QString pass;
                const PassChoice c = ie_ask_passphrase(parent, res.wrong_passphrase ? res.error : QString(), &pass);
                if (c == PassChoice::Cancel) {
                    done(false, 0, QString());
                    return;
                }
                ie_read_and_continue(parent, path, bytes, pass, c == PassChoice::WithoutSecrets, attempt + 1, done);
                return;
            }
            if (!res.ok) {
                done(false, 0, res.error);
                return;
            }
            ie_confirm_and_save(parent, path, res, done);
        });
}

} // namespace

void export_connections(QWidget* parent, TransferDone done) {
    dsrc::ExportOptions opts;
    if (!ie_ask_export_options(parent, &opts))
        return;
    const QString path = QFileDialog::getSaveFileName(parent, QObject::tr("Export connections"),
                                                      QDir(ie_default_dir()).filePath(QStringLiteral("fincept_connections.json")),
                                                      QObject::tr("JSON files (*.json);;All files (*)"));
    if (path.isEmpty())
        return;
    struct ExportOutcome {
        dsrc::ExportResult result;
        QString error;
    };
    ie_async<ExportOutcome>(
        parent,
        [opts, path]() {
            ExportOutcome out;
            const auto all = DataSourceRepository::instance().list_all();
            if (all.is_err()) {
                out.error = QObject::tr("Could not read the connections: %1").arg(QString::fromStdString(all.error()));
                return out;
            }
            QVector<dsrc::TransferConnection> conns;
            int unreadable = 0;
            for (const DataSource& ds : all.value()) {
                dsrc::TransferConnection c;
                c.id = ds.id;
                c.alias = ds.alias;
                c.display_name = ds.display_name;
                c.description = ds.description;
                c.type = ds.type;
                c.provider = ds.provider;
                c.category = ds.category;
                c.tags = ds.tags;
                c.enabled = ds.enabled;
                c.config = QJsonDocument::fromJson(ds.config.toUtf8()).object();
                if (const ConnectorConfig* cfg = find_connector_config(ds.provider))
                    for (const FieldDef& f : cfg->fields)
                        if (f.type == FieldType::Password)
                            c.secret_fields << f.name;
                if (ds.secrets_unreadable)
                    ++unreadable;
                conns.append(c);
            }
            out.result = dsrc::export_connections_document(conns, opts);
            if (!out.result.ok) {
                out.error = out.result.error;
                return out;
            }
            QSaveFile f(path);
            if (!f.open(QIODevice::WriteOnly) || f.write(out.result.json) != out.result.json.size() || !f.commit()) {
                out.error = QObject::tr("Could not write %1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
                return out;
            }
            if (unreadable > 0 && opts.include_secrets)
                out.error = QObject::tr("%n connection(s) were exported without secrets: secure storage could not be "
                                        "read for them.",
                                        "", unreadable);
            return out;
        },
        [done, path, opts](const ExportOutcome& out) {
            if (!out.result.ok) {
                done(false, 0, out.error);
                return;
            }
            LOG_INFO(kIeTag, QStringLiteral("Exported %1 connection(s) (%2)")
                                 .arg(out.result.connections)
                                 .arg(opts.include_secrets ? QStringLiteral("secrets encrypted") : QStringLiteral("secrets left out")));
            // Only a file without secrets is indexed in the file manager (whose tools read its index).
            if (!opts.include_secrets)
                fincept::services::FileManagerService::instance().import_file(path, QStringLiteral("data_sources"));
            // One .arg() call each: a chained call re-scans the text and would fill a "%3" in the path.
            QString msg = opts.include_secrets
                              ? QObject::tr("Exported %1 connection(s) to %2 · %3 secret(s) encrypted with your passphrase.")
                                    .arg(QString::number(out.result.connections), QDir::toNativeSeparators(path),
                                         QString::number(out.result.secrets_encrypted))
                              : QObject::tr("Exported %1 connection(s) to %2 · %3 secret(s) left out.")
                                    .arg(QString::number(out.result.connections), QDir::toNativeSeparators(path),
                                         QString::number(out.result.secrets_excluded));
            if (!out.error.isEmpty())
                msg += QStringLiteral(" ") + out.error;
            done(true, out.result.connections, msg);
        });
}

void import_connections(QWidget* parent, TransferDone done) {
    const QString path = QFileDialog::getOpenFileName(parent, QObject::tr("Import connections"), ie_default_dir(),
                                                      QObject::tr("JSON files (*.json);;All files (*)"));
    if (path.isEmpty())
        return;
    ie_async<ReadOutcome>(
        parent,
        [path]() {
            ReadOutcome out;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) {
                out.io_error = QObject::tr("Could not open %1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
                return out;
            }
            if (f.size() > kIeMaxFileBytes) {
                out.io_error = QObject::tr("%1 is too large for a connections file").arg(QDir::toNativeSeparators(path));
                return out;
            }
            out.bytes = f.readAll();
            out.result = dsrc::read_connections_document(out.bytes, QString());
            return out;
        },
        [parent, path, done](const ReadOutcome& out) {
            if (!out.io_error.isEmpty()) {
                done(false, 0, out.io_error);
                return;
            }
            if (out.result.needs_passphrase) {
                ie_read_and_continue(parent, path, out.bytes, QString(), false, 0, done);
                return;
            }
            if (!out.result.ok) {
                done(false, 0, out.result.error);
                return;
            }
            ie_confirm_and_save(parent, path, out.result, done);
        });
}

bool download_connector_template(QWidget* parent) {
    const QString path = QFileDialog::getSaveFileName(parent, QObject::tr("Save connection template"),
                                                      QDir(ie_default_dir()).filePath(QStringLiteral("fincept_connections_template.json")),
                                                      QObject::tr("JSON files (*.json);;All files (*)"));
    if (path.isEmpty())
        return false;
    QJsonArray out;
    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        QJsonObject config_obj;
        for (const auto& field : cfg.fields) {
            // Never seed a secret with a placeholder that looks like a real value.
            if (field.type == FieldType::Password) {
                config_obj[field.name] = QString();
                continue;
            }
            const QString val = !field.default_value.isEmpty() ? field.default_value : field.placeholder;
            if (field.type == FieldType::Checkbox) {
                config_obj[field.name] = (field.default_value == QLatin1String("true"));
                continue;
            }
            if (field.type == FieldType::Number) {
                bool ok = false;
                const int n = val.toInt(&ok);
                config_obj[field.name] = ok ? n : 0;
                continue;
            }
            config_obj[field.name] = val;
        }
        QJsonObject entry;
        entry["provider"] = cfg.id;
        entry["display_name"] = cfg.name;
        entry["description"] = cfg.description;
        entry["type"] = persistence_type(cfg);
        entry["category"] = category_str(cfg.category);
        entry["enabled"] = false;
        entry["tags"] = QString();
        entry["config"] = config_obj;
        out.append(entry);
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(parent, QObject::tr("Save connection template"),
                             QObject::tr("Could not write to:\n%1\n\n%2").arg(path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        QMessageBox::warning(parent, QObject::tr("Save connection template"),
                             QObject::tr("Could not write to:\n%1\n\n%2").arg(path, file.errorString()));
        return false;
    }
    LOG_INFO(kIeTag, QStringLiteral("Template written: %1 connection types to %2").arg(out.size()).arg(path));
    return true;
}

} // namespace fincept::screens::datasources
