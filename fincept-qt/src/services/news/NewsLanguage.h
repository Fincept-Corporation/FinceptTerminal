#pragma once
// NewsLanguage — language identification for news headlines and summaries.
//
// Non-Latin scripts are told apart by code-point ranges (CJK + kana = ja, Han =
// zh, Hangul = ko, Arabic, Hebrew, Cyrillic = ru / uk, Devanagari = hi, Thai,
// Greek). Latin-script text is scored against weighted stop-word profiles of
// en / fr / de / es / pt / it / nl / sv plus language-specific letters (ß, ñ, ç,
// ã ...). The feed's declared language is a prior: it wins when the text gives
// weak or close evidence (a four-word headline, names only).
//
// Pure Qt Core, no shared state beyond immutable tables: safe on any thread.

#include <QString>
#include <QStringList>

namespace fincept::services::news_lang {

struct LangGuess {
    QString code;            ///< ISO 639-1 ("en", "fr", "zh" ...); empty = undetermined
    double confidence = 0.0; ///< 0..1
    bool from_prior = false; ///< the feed's declared language decided it
};

/// Identify the language of `text`. `prior` is the feed's declared language
/// (ISO 639-1, may be empty). Without a prior, weak evidence on pure-ASCII text
/// reads as English (low confidence) and anything else as undetermined.
LangGuess detect(const QString& text, const QString& prior = {});

/// Languages offered by the feed's language filter, in display order.
const QStringList& filter_languages();

/// Whether headline tone is scored for `code`. The finance sentiment model is an
/// English lexicon, so only English (and undetermined text, which reads as
/// English) is scored; other languages are reported as "not scored".
bool sentiment_supported(const QString& code);

} // namespace fincept::services::news_lang
