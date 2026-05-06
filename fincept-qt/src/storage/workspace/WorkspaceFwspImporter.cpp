#include "storage/workspace/WorkspaceFwspImporter.h"

#include "core/config/AppPaths.h"
#include "core/identity/Uuid.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/LayoutTypes.h"
#include "core/logging/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept {

namespace {
constexpr const char* kTag = "FwspImporter";
constexpr const char* kFlagKey = "fwsp_import_done";
} // namespace

void WorkspaceFwspImporter::run_once_if_needed() {
    auto done = LayoutCatalog::instance().meta(QStringLiteral(kFlagKey));
    if (done.is_ok() && done.value() == QStringLiteral("1")) {
        // Already imported — fast path.
        return;
    }

    const QString dir_path = AppPaths::workspaces();
    QDir d(dir_path);
    if (!d.exists()) {
        // Nothing to import; mark done so we don't keep checking.
        LayoutCatalog::instance().set_meta(QStringLiteral(kFlagKey), QStringLiteral("1"));
        return;
    }

    int imported = 0;
    int skipped = 0;
    const auto files = d.entryInfoList(QStringList() << "*.fwsp", QDir::Files);
    for (const auto& fi : files) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) {
            ++skipped;
            continue;
        }
        const QByteArray bytes = f.readAll();
        f.close();

        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(bytes, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN(kTag, QString("Skipping %1: parse error: %2")
                              .arg(fi.fileName(), err.errorString()));
            ++skipped;
            continue;
        }

        const auto meta_obj = doc.object().value(QStringLiteral("metadata")).toObject();
        const QString name = meta_obj.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            ++skipped;
            continue;
        }

        layout::Workspace w;
        w.id = LayoutId::generate();
        w.name = name;
        w.kind = QStringLiteral("user");
        w.description = QStringLiteral(
            "(Imported from v3 — empty layout, please re-save under this name)");
        w.created_at_unix = QDateTime::currentSecsSinceEpoch();
        w.updated_at_unix = w.created_at_unix;
        // No frames / variants / link_state — user must re-save once they
        // have the desired layout open.

        if (LayoutCatalog::instance().save_workspace(w).is_ok())
            ++imported;
        else
            ++skipped;
    }

    LayoutCatalog::instance().set_meta(QStringLiteral(kFlagKey), QStringLiteral("1"));
    LOG_INFO(kTag, QString("Imported %1 legacy workspace name(s); %2 skipped")
                       .arg(imported).arg(skipped));
}

} // namespace fincept
