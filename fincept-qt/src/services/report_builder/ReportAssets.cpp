// ReportAssets.cpp — see ReportAssets.h.

#include "services/report_builder/ReportAssets.h"

#include "core/config/AppPaths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>

namespace fincept::services::report_assets {

namespace {

QString ras_ext_of(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
        return QStringLiteral("jpg");
    if (ext == QLatin1String("gif") || ext == QLatin1String("webp") || ext == QLatin1String("bmp") ||
        ext == QLatin1String("svg"))
        return ext;
    return QStringLiteral("png");
}

bool ras_is_safe_asset_name(const QString& name) {
    // <40 hex>.<ext> — never a path.
    if (name.size() < 42 || name.size() > 46 || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
        return false;
    for (int i = 0; i < 40; ++i) {
        const QChar ch = name.at(i);
        if (!(ch.isDigit() || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))))
            return false;
    }
    return name.at(40) == QLatin1Char('.');
}

bool ras_write(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(bytes) != bytes.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

} // namespace

QString assets_dir() {
    return AppPaths::data() + QStringLiteral("/report_assets");
}

bool has_picture(const report::ReportComponent& c) {
    return c.type == QLatin1String("image") || c.type == QLatin1String("cover");
}

QString picture_path(const report::ReportComponent& c) {
    const QString path = c.config.value(QStringLiteral("path"));
    if (!path.isEmpty() && QFileInfo::exists(path))
        return path;
    const QString asset = c.config.value(QLatin1String(kAssetKey));
    if (!asset.isEmpty() && ras_is_safe_asset_name(asset)) {
        const QString p = assets_dir() + QLatin1Char('/') + asset;
        if (QFileInfo::exists(p))
            return p;
    }
    return {};
}

bool picture_not_synced(const report::ReportComponent& c) {
    if (!has_picture(c))
        return false;
    const bool named = !c.config.value(QStringLiteral("path")).isEmpty() ||
                       !c.config.value(QLatin1String(kAssetKey)).isEmpty();
    return named && picture_path(c).isEmpty() && c.config.value(QLatin1String(kB64Key)).isEmpty();
}

int embed_for_sync(QJsonObject& root) {
    QJsonArray comps = root.value(QStringLiteral("components")).toArray();
    qint64 total = 0;
    // What is already embedded counts against the per-report budget.
    for (const QJsonValue& v : comps)
        total += v.toObject().value(QStringLiteral("config")).toObject().value(QLatin1String(kB64Key)).toString().size() * 3 / 4;
    int embedded = 0;
    bool changed = false;
    for (int i = 0; i < comps.size(); ++i) {
        QJsonObject obj = comps[i].toObject();
        const QString type = obj.value(QStringLiteral("type")).toString();
        if (type != QLatin1String("image") && type != QLatin1String("cover"))
            continue;
        QJsonObject cfg = obj.value(QStringLiteral("config")).toObject();
        if (!cfg.value(QLatin1String(kB64Key)).toString().isEmpty())
            continue;
        report::ReportComponent probe;
        probe.type = type;
        probe.config.insert(QStringLiteral("path"), cfg.value(QStringLiteral("path")).toString());
        probe.config.insert(QLatin1String(kAssetKey), cfg.value(QLatin1String(kAssetKey)).toString());
        const QString file = picture_path(probe);
        if (file.isEmpty())
            continue;
        const qint64 size = QFileInfo(file).size();
        if (size <= 0 || size > kMaxEmbedBytes || total + size > kMaxEmbedTotal)
            continue;
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = f.readAll();
        f.close();
        const QString asset = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex()) +
                              QLatin1Char('.') + ras_ext_of(file);
        // Keep a content-addressed copy here too, so the block survives the file moving.
        const QString local = assets_dir() + QLatin1Char('/') + asset;
        if (!QFileInfo::exists(local))
            ras_write(local, bytes);
        cfg.insert(QLatin1String(kAssetKey), asset);
        cfg.insert(QLatin1String(kB64Key), QString::fromLatin1(bytes.toBase64()));
        obj.insert(QStringLiteral("config"), cfg);
        comps[i] = obj;
        total += size;
        ++embedded;
        changed = true;
    }
    if (changed)
        root.insert(QStringLiteral("components"), comps);
    return embedded;
}

QString materialize_one(const report::ReportComponent& c) {
    const QString asset = c.config.value(QLatin1String(kAssetKey));
    const QString b64 = c.config.value(QLatin1String(kB64Key));
    if (asset.isEmpty() || b64.isEmpty() || !ras_is_safe_asset_name(asset))
        return {};
    const QString path = assets_dir() + QLatin1Char('/') + asset;
    if (QFileInfo::exists(path))
        return path;
    const QByteArray bytes = QByteArray::fromBase64(b64.toLatin1());
    // Only bytes that hash to their name are written (a damaged copy is not a picture).
    const QString sha = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
    if (bytes.isEmpty() || !asset.startsWith(sha))
        return {};
    return ras_write(path, bytes) ? path : QString();
}

int materialize(const report::ReportDocument& doc) {
    int written = 0;
    for (const report::ReportComponent& c : doc.components) {
        if (!has_picture(c) || c.config.value(QLatin1String(kB64Key)).isEmpty())
            continue;
        if (!picture_path(c).isEmpty())
            continue;
        if (!materialize_one(c).isEmpty())
            ++written;
    }
    return written;
}

} // namespace fincept::services::report_assets
