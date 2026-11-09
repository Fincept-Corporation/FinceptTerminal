#pragma once
// C++ port of vaderSentiment 3.3.2 — SentimentIntensityAnalyzer.polarity_scores.
// VADER: Copyright (c) 2016 C.J. Hutto, MIT License. The lexicons ship unmodified as
// the :/vader resource together with that licence (resources/vader/LICENSE.txt).
//
// The port reproduces the Python implementation's arithmetic and its quirks (the
// `but` re-weighting that looks values up by equality, the "never so/this" check that
// also fires without "never", ...) so scores match it bit for bit — see
// pycompat below for the Python `str` semantics the rules depend on.

#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace fincept::services {

/// The Python 3.11 `str` / `re` / `round` behaviour (Unicode 14.0 data) that the VADER
/// port and NewsNlpEngine reproduce. Positions are UTF-16 indexes; "code point" helpers
/// decode surrogate pairs the way Python indexes characters.
namespace pycompat {

bool is_space(char32_t c);       // str.isspace() of one character
bool is_word_char(char32_t c);   // `re` \w for str patterns
bool is_alpha(char32_t c);       // str.isalpha() of one character
bool is_upper_char(char32_t c);  // str.isupper() of one character
bool str_isupper(QStringView s); // str.isupper()
QString str_lower(QStringView s);
QString str_strip(QStringView s);     // str.strip()
QStringList str_split(QStringView s); // str.split()
qsizetype cp_length(QStringView s);   // len(str)
/// Code point starting at UTF-16 index `i` (a lone surrogate is returned as itself).
char32_t cp_at(QStringView s, qsizetype i);
/// Code point that ends just before UTF-16 index `i` (`i` > 0).
char32_t cp_before(QStringView s, qsizetype i);
/// round(x, ndigits): x rounded half-to-even on its exact binary value, as CPython does.
double round_digits(double x, int ndigits);
/// round(x) for a float: nearest integer, ties to even.
double round_half_even(double x);

} // namespace pycompat

/// polarity_scores() output; neg/neu/pos rounded to 3 places, compound to 4.
struct VaderScores {
    double neg = 0.0;
    double neu = 0.0;
    double pos = 0.0;
    double compound = 0.0;
};

/// Immutable after construction, so one instance may be shared across threads.
/// The lexicon files are parsed once per process, on first use.
class VaderSentiment {
  public:
    /// `lexicon_overrides` are applied over vader_lexicon.txt like
    /// `analyzer.lexicon.update(...)` (later keys win).
    explicit VaderSentiment(const QHash<QString, double>& lexicon_overrides = {});

    /// False when the lexicon resource could not be read (polarity_scores then
    /// scores every text as neutral).
    bool is_loaded() const { return loaded_; }

    VaderScores polarity_scores(const QString& text) const;

  private:
    double sentiment_valence(const QStringList& words, const QStringList& words_lower, qsizetype i,
                             bool is_cap_diff) const;
    double least_check(double valence, const QStringList& words_lower, qsizetype i) const;

    QHash<QString, double> lexicon_;
    bool loaded_ = false;
};

} // namespace fincept::services
