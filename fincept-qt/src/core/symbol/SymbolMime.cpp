#include "core/symbol/SymbolMime.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::symbol_mime {

QMimeData* pack(const SymbolRef& ref, SymbolGroup source_group) {
    QJsonObject o = ref.to_json();
    if (source_group != SymbolGroup::None)
        o["source_group"] = QString(symbol_group_letter(source_group));
    auto* m = new QMimeData;
    m->setData(kMimeType, QJsonDocument(o).toJson(QJsonDocument::Compact));
    // Also set plain text so the drag can be dropped into generic text fields
    // (Excel, chat boxes, external apps) — the symbol alone is the useful bit.
    m->setText(ref.symbol);
    return m;
}

SymbolRef unpack(const QMimeData* mime) {
    if (!mime || !mime->hasFormat(kMimeType))
        return {};
    const QByteArray blob = mime->data(kMimeType);
    const QJsonDocument doc = QJsonDocument::fromJson(blob);
    if (!doc.isObject())
        return {};
    return SymbolRef::from_json(doc.object());
}

SymbolGroup unpack_source_group(const QMimeData* mime) {
    if (!mime || !mime->hasFormat(kMimeType))
        return SymbolGroup::None;
    const QJsonDocument doc = QJsonDocument::fromJson(mime->data(kMimeType));
    if (!doc.isObject())
        return SymbolGroup::None;
    const QString letter = doc.object().value("source_group").toString();
    if (letter.isEmpty())
        return SymbolGroup::None;
    return symbol_group_from_char(letter.at(0));
}

bool has_symbol(const QMimeData* mime) {
    return mime && mime->hasFormat(kMimeType);
}

} // namespace fincept::symbol_mime
