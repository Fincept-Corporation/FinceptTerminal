#include "storage/repositories/TranslationCacheRepository.h"

#include <QCryptographicHash>

namespace fincept {

TranslationCacheRepository& TranslationCacheRepository::instance() {
    static TranslationCacheRepository s;
    return s;
}

QString TranslationCacheRepository::hash_source(const QString& source) {
    const QByteArray h = QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(h.toHex().left(32));
}

std::optional<QString> TranslationCacheRepository::lookup(const QString& src_hash,
                                                          const QString& target_lang) const {
    auto r = db().execute(
        "SELECT translated FROM translation_cache WHERE src_hash = ? AND target_lang = ?",
        {src_hash, target_lang});
    if (r.is_err())
        return std::nullopt;
    auto& q = r.value();
    if (!q.next())
        return std::nullopt;
    return q.value(0).toString();
}

Result<void> TranslationCacheRepository::put(const QString& source, const QString& src_lang,
                                             const QString& target_lang, const QString& domain,
                                             const QString& translated, const QString& provider,
                                             const QString& model) {
    return exec_write(
        "INSERT OR REPLACE INTO translation_cache "
        "(src_hash, target_lang, src_lang, domain, src_text, translated, provider, model) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        {hash_source(source), target_lang, src_lang, domain, source, translated, provider, model});
}

Result<void> TranslationCacheRepository::clear_domain(const QString& domain) {
    return exec_write("DELETE FROM translation_cache WHERE domain = ?", {domain});
}

int TranslationCacheRepository::count(const QString& target_lang) const {
    auto r = db().execute("SELECT COUNT(*) FROM translation_cache WHERE target_lang = ?",
                          {target_lang});
    if (r.is_err())
        return 0;
    auto& q = r.value();
    if (!q.next())
        return 0;
    return q.value(0).toInt();
}

} // namespace fincept
