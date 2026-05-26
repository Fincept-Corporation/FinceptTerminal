#pragma once
// TranslationCacheRepository — persistent backing for runtime LLM-driven UI
// translations. Keyed by (sha256(source), target_lang); see migration v032.

#include "core/result/Result.h"
#include "storage/repositories/BaseRepository.h"

#include <QString>
#include <QVector>

#include <optional>

namespace fincept {

struct TranslationEntry {
    QString src_hash;
    QString target_lang;
    QString src_lang;
    QString domain;
    QString src_text;
    QString translated;
    QString provider;
    QString model;
    qint64 created_at = 0;
};

class TranslationCacheRepository : public BaseRepository<TranslationEntry> {
  public:
    static TranslationCacheRepository& instance();

    /// Stable 32-char hex (truncated sha256) of the source text. Caller can
    /// reuse this for in-memory dedupe, log keys, etc.
    static QString hash_source(const QString& source);

    /// Look up an existing translation. Returns nullopt on miss / error.
    std::optional<QString> lookup(const QString& src_hash, const QString& target_lang) const;

    /// Insert-or-replace. `provider`/`model` are stored for forensics; pass
    /// the live LlmService values where possible.
    Result<void> put(const QString& source, const QString& src_lang, const QString& target_lang,
                     const QString& domain, const QString& translated, const QString& provider,
                     const QString& model);

    /// Drop all rows in a domain (e.g. "news.headline") — used when the user
    /// switches LLM provider and wants to invalidate stale translations.
    Result<void> clear_domain(const QString& domain);

    /// Count rows for a given target language — cheap stats for the UI.
    int count(const QString& target_lang) const;

  private:
    TranslationCacheRepository() = default;
};

} // namespace fincept
