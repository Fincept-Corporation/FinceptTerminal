// C++ port of vaderSentiment 3.3.2 (MIT, Copyright (c) 2016 C.J. Hutto) — see VaderSentiment.h.
#include "services/news/VaderSentiment.h"

#include <QChar>
#include <QFile>
#include <QList>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace fincept::services {

// ── Python str semantics ────────────────────────────────────────────────────

namespace vader_pycompat_detail {

/// Python 3.11 carries Unicode 14.0 and Qt 6.8 Unicode 16.0: a character assigned after
/// 14.0 is unassigned to Python — no letter/number property and no case mapping.
bool known_to_python(char32_t c) {
    const QChar::UnicodeVersion v = QChar::unicodeVersion(c);
    return v != QChar::Unicode_Unassigned && v <= QChar::Unicode_14_0;
}

struct VaderCodeRange {
    char32_t first;
    char32_t last;
};

// Python's isupper()/islower() use the derived Uppercase/Lowercase properties (Lu/Ll plus
// Other_Uppercase/Other_Lowercase); QChar::isUpper()/isLower() test only Lu/Ll. These
// are the Unicode 14.0 Other_* ranges — e.g. circled letters, Roman numerals, ª º.
constexpr VaderCodeRange kOtherUppercase[] = {
    {0x2160, 0x216F}, {0x24B6, 0x24CF}, {0x1F130, 0x1F149}, {0x1F150, 0x1F169}, {0x1F170, 0x1F189},
};
constexpr VaderCodeRange kOtherLowercase[] = {
    {0x00AA, 0x00AA}, {0x00BA, 0x00BA}, {0x02B0, 0x02B8},   {0x02C0, 0x02C1},   {0x02E0, 0x02E4},   {0x0345, 0x0345},
    {0x037A, 0x037A}, {0x1D2C, 0x1D6A}, {0x1D78, 0x1D78},   {0x1D9B, 0x1DBF},   {0x2071, 0x2071},   {0x207F, 0x207F},
    {0x2090, 0x209C}, {0x2170, 0x217F}, {0x24D0, 0x24E9},   {0x2C7C, 0x2C7D},   {0xA69C, 0xA69D},   {0xA770, 0xA770},
    {0xA7F8, 0xA7F9}, {0xAB5C, 0xAB5F}, {0x10780, 0x10780}, {0x10783, 0x10785}, {0x10787, 0x107B0}, {0x107B2, 0x107BA},
};

template <std::size_t N>
bool in_code_ranges(const VaderCodeRange (&ranges)[N], char32_t c) {
    for (const auto& r : ranges) {
        if (c >= r.first && c <= r.last)
            return true;
    }
    return false;
}

bool has_uppercase_property(char32_t c) {
    return (QChar::isUpper(c) && known_to_python(c)) || in_code_ranges(kOtherUppercase, c);
}

bool has_lowercase_property(char32_t c) {
    return (QChar::isLower(c) && known_to_python(c)) || in_code_ranges(kOtherLowercase, c);
}

void append_code_point(QString& out, char32_t c) {
    if (QChar::requiresSurrogates(c)) {
        out.append(QChar(QChar::highSurrogate(c)));
        out.append(QChar(QChar::lowSurrogate(c)));
    } else {
        out.append(QChar(static_cast<char16_t>(c)));
    }
}

/// UTF-16 width of the code point starting at s[i]: 2 only for a well-formed pair.
qsizetype code_point_width(QStringView s, qsizetype i) {
    return (s[i].isHighSurrogate() && i + 1 < s.size() && s[i + 1].isLowSurrogate()) ? 2 : 1;
}

} // namespace vader_pycompat_detail

namespace pycompat {

bool is_space(char32_t c) {
    // Python also counts the information separators U+001C..U+001F as whitespace.
    return QChar::isSpace(c) || (c >= 0x1C && c <= 0x1F);
}

bool is_word_char(char32_t c) {
    return c == U'_' || (QChar::isLetterOrNumber(c) && vader_pycompat_detail::known_to_python(c));
}

bool is_alpha(char32_t c) {
    return QChar::isLetter(c) && vader_pycompat_detail::known_to_python(c);
}

bool is_upper_char(char32_t c) {
    return vader_pycompat_detail::has_uppercase_property(c);
}

bool str_isupper(QStringView s) {
    bool cased = false;
    for (qsizetype i = 0; i < s.size(); i += vader_pycompat_detail::code_point_width(s, i)) {
        const char32_t c = cp_at(s, i);
        if (vader_pycompat_detail::has_lowercase_property(c) ||
            (QChar::isTitleCase(c) && vader_pycompat_detail::known_to_python(c)))
            return false;
        if (!cased && vader_pycompat_detail::has_uppercase_property(c))
            cased = true;
    }
    return cased;
}

QString str_lower(QStringView s) {
    // Python maps a word-final capital sigma to U+03C2; this always yields U+03C3. Neither
    // is ASCII nor in any lexicon, so no rule built on the lowered text can tell them apart.
    QString out;
    out.reserve(s.size());
    for (qsizetype i = 0; i < s.size();) {
        const char16_t u = s[i].unicode();
        if (u < 0x80) {
            out.append(QChar(u >= u'A' && u <= u'Z' ? static_cast<char16_t>(u + 32) : u));
            ++i;
            continue;
        }
        const char32_t c = cp_at(s, i);
        i += vader_pycompat_detail::code_point_width(s, i);
        if (!vader_pycompat_detail::known_to_python(c)) {
            vader_pycompat_detail::append_code_point(out, c);
        } else if (c == 0x0130) {
            // LATIN CAPITAL LETTER I WITH DOT ABOVE: SpecialCasing's full lowercase form.
            out.append(QChar(u'i'));
            out.append(QChar(static_cast<char16_t>(0x0307)));
        } else {
            vader_pycompat_detail::append_code_point(out, QChar::toLower(c));
        }
    }
    return out;
}

QString str_strip(QStringView s) {
    // Every Python whitespace character is in the BMP, so testing UTF-16 units is exact.
    qsizetype begin = 0;
    qsizetype end = s.size();
    while (begin < end && is_space(s[begin].unicode()))
        ++begin;
    while (end > begin && is_space(s[end - 1].unicode()))
        --end;
    return s.sliced(begin, end - begin).toString();
}

QStringList str_split(QStringView s) {
    QStringList out;
    const qsizetype n = s.size();
    qsizetype i = 0;
    while (i < n) {
        while (i < n && is_space(s[i].unicode()))
            ++i;
        if (i >= n)
            break;
        qsizetype j = i;
        while (j < n && !is_space(s[j].unicode()))
            ++j;
        out.append(s.sliced(i, j - i).toString());
        i = j;
    }
    return out;
}

qsizetype cp_length(QStringView s) {
    qsizetype n = 0;
    for (qsizetype i = 0; i < s.size(); i += vader_pycompat_detail::code_point_width(s, i))
        ++n;
    return n;
}

char32_t cp_at(QStringView s, qsizetype i) {
    if (vader_pycompat_detail::code_point_width(s, i) == 2)
        return QChar::surrogateToUcs4(s[i], s[i + 1]);
    return s[i].unicode();
}

char32_t cp_before(QStringView s, qsizetype i) {
    if (s[i - 1].isLowSurrogate() && i >= 2 && s[i - 2].isHighSurrogate())
        return QChar::surrogateToUcs4(s[i - 2], s[i - 1]);
    return s[i - 1].unicode();
}

double round_half_even(double x) {
    // CPython float.__round__() without ndigits.
    double rounded = std::round(x);
    if (std::fabs(x - rounded) == 0.5)
        rounded = 2.0 * std::round(x / 2.0);
    return rounded;
}

double round_digits(double x, int ndigits) {
    if (!std::isfinite(x) || x == 0.0)
        return x;
    const double ax = std::fabs(x);
    if (ndigits < 0 || ndigits > 6 || ax >= 2147483648.0) {
        // Outside what this module rounds (scores, confidences, kilometres).
        const double scale = std::pow(10.0, ndigits);
        return std::nearbyint(x * scale) / scale;
    }
    // CPython rounds the EXACT binary value of x to `ndigits` decimals (ties to even) and
    // returns the double nearest that decimal. |x| = m * 2^e exactly, with m < 2^53; the
    // product m * 10^ndigits (< 2^73) is held in two 64-bit halves.
    int exp2 = 0;
    const double frac = std::frexp(ax, &exp2);
    const auto m = static_cast<std::uint64_t>(std::ldexp(frac, 53));
    const int shift = 53 - exp2; // >= 22 because ax < 2^31
    std::uint64_t pow10 = 1;
    for (int k = 0; k < ndigits; ++k)
        pow10 *= 10;
    const std::uint64_t lo_part = (m & 0xFFFFFFFFu) * pow10;
    const std::uint64_t hi_part = (m >> 32) * pow10;
    const std::uint64_t lo = lo_part + (hi_part << 32);
    const std::uint64_t hi = (hi_part >> 32) + (lo < lo_part ? 1u : 0u);

    const auto bit = [lo, hi](int k) -> bool {
        if (k < 64)
            return ((lo >> k) & 1u) != 0;
        return k < 128 && ((hi >> (k - 64)) & 1u) != 0;
    };
    const auto any_below = [lo, hi](int k) -> bool { // any of bits [0, k)
        if (k <= 0)
            return false;
        if (k < 64)
            return (lo & ((std::uint64_t{1} << k) - 1)) != 0;
        if (lo != 0)
            return true;
        if (k >= 128)
            return hi != 0;
        return k > 64 && (hi & ((std::uint64_t{1} << (k - 64)) - 1)) != 0;
    };

    std::uint64_t q = 0;
    if (shift < 64)
        q = (lo >> shift) | (hi << (64 - shift));
    else if (shift < 128)
        q = hi >> (shift - 64);
    if (bit(shift - 1) && (any_below(shift - 1) || (q & 1u) != 0))
        ++q; // above half, or exactly half and q is odd
    // q < 2^51 and 10^ndigits are exact doubles, so the division is correctly rounded —
    // the same double CPython's strtod returns for "<q>e-<ndigits>".
    return std::copysign(static_cast<double>(q) / static_cast<double>(pow10), x);
}

} // namespace pycompat

// ── VADER ───────────────────────────────────────────────────────────────────

namespace vader_detail {

// Empirically derived constants (vaderSentiment.py).
constexpr double kVaderBIncr = 0.293;
constexpr double kVaderBDecr = -0.293;
constexpr double kVaderCIncr = 0.733;
constexpr double kVaderNScalar = -0.74;

struct VaderWordValue {
    const char* word;
    double value;
};

constexpr const char* kVaderNegate[] = {
    "aint",    "arent",    "cannot",  "cant",    "couldnt", "darent",   "didnt",   "doesnt",    "ain't",    "aren't",
    "can't",   "couldn't", "daren't", "didn't",  "doesn't", "dont",     "hadnt",   "hasnt",     "havent",   "isnt",
    "mightnt", "mustnt",   "neither", "don't",   "hadn't",  "hasn't",   "haven't", "isn't",     "mightn't", "mustn't",
    "neednt",  "needn't",  "never",   "none",    "nope",    "nor",      "not",     "nothing",   "nowhere",  "oughtnt",
    "shant",   "shouldnt", "uhuh",    "wasnt",   "werent",  "oughtn't", "shan't",  "shouldn't", "uh-uh",    "wasn't",
    "weren't", "without",  "wont",    "wouldnt", "won't",   "wouldn't", "rarely",  "seldom",    "despite",
};

constexpr VaderWordValue kVaderBoosters[] = {
    {"absolutely", kVaderBIncr},    {"amazingly", kVaderBIncr},    {"awfully", kVaderBIncr},
    {"completely", kVaderBIncr},    {"considerable", kVaderBIncr}, {"considerably", kVaderBIncr},
    {"decidedly", kVaderBIncr},     {"deeply", kVaderBIncr},       {"effing", kVaderBIncr},
    {"enormous", kVaderBIncr},      {"enormously", kVaderBIncr},   {"entirely", kVaderBIncr},
    {"especially", kVaderBIncr},    {"exceptional", kVaderBIncr},  {"exceptionally", kVaderBIncr},
    {"extreme", kVaderBIncr},       {"extremely", kVaderBIncr},    {"fabulously", kVaderBIncr},
    {"flipping", kVaderBIncr},      {"flippin", kVaderBIncr},      {"frackin", kVaderBIncr},
    {"fracking", kVaderBIncr},      {"fricking", kVaderBIncr},     {"frickin", kVaderBIncr},
    {"frigging", kVaderBIncr},      {"friggin", kVaderBIncr},      {"fully", kVaderBIncr},
    {"fuckin", kVaderBIncr},        {"fucking", kVaderBIncr},      {"fuggin", kVaderBIncr},
    {"fugging", kVaderBIncr},       {"greatly", kVaderBIncr},      {"hella", kVaderBIncr},
    {"highly", kVaderBIncr},        {"hugely", kVaderBIncr},       {"incredible", kVaderBIncr},
    {"incredibly", kVaderBIncr},    {"intensely", kVaderBIncr},    {"major", kVaderBIncr},
    {"majorly", kVaderBIncr},       {"more", kVaderBIncr},         {"most", kVaderBIncr},
    {"particularly", kVaderBIncr},  {"purely", kVaderBIncr},       {"quite", kVaderBIncr},
    {"really", kVaderBIncr},        {"remarkably", kVaderBIncr},   {"so", kVaderBIncr},
    {"substantially", kVaderBIncr}, {"thoroughly", kVaderBIncr},   {"total", kVaderBIncr},
    {"totally", kVaderBIncr},       {"tremendous", kVaderBIncr},   {"tremendously", kVaderBIncr},
    {"uber", kVaderBIncr},          {"unbelievably", kVaderBIncr}, {"unusually", kVaderBIncr},
    {"utter", kVaderBIncr},         {"utterly", kVaderBIncr},      {"very", kVaderBIncr},
    {"almost", kVaderBDecr},        {"barely", kVaderBDecr},       {"hardly", kVaderBDecr},
    {"just enough", kVaderBDecr},   {"kind of", kVaderBDecr},      {"kinda", kVaderBDecr},
    {"kindof", kVaderBDecr},        {"kind-of", kVaderBDecr},      {"less", kVaderBDecr},
    {"little", kVaderBDecr},        {"marginal", kVaderBDecr},     {"marginally", kVaderBDecr},
    {"occasional", kVaderBDecr},    {"occasionally", kVaderBDecr}, {"partly", kVaderBDecr},
    {"scarce", kVaderBDecr},        {"scarcely", kVaderBDecr},     {"slight", kVaderBDecr},
    {"slightly", kVaderBDecr},      {"somewhat", kVaderBDecr},     {"sort of", kVaderBDecr},
    {"sorta", kVaderBDecr},         {"sortof", kVaderBDecr},       {"sort-of", kVaderBDecr},
};

// Special-case idioms and phrases containing lexicon words.
constexpr VaderWordValue kVaderSpecialCases[] = {
    {"the shit", 3},    {"the bomb", 3},         {"bad ass", 1.5},  {"badass", 1.5},        {"bus stop", 0.0},
    {"yeah right", -2}, {"kiss of death", -1.5}, {"to die for", 3}, {"beating heart", 3.5},
};

template <std::size_t N>
QHash<QString, double> vader_word_table(const VaderWordValue (&rows)[N]) {
    QHash<QString, double> table;
    for (const auto& row : rows)
        table.insert(QString::fromLatin1(row.word), row.value);
    return table;
}

const QHash<QString, double>& vader_boosters() {
    static const QHash<QString, double> table = vader_word_table(kVaderBoosters);
    return table;
}

const QHash<QString, double>& vader_special_cases() {
    static const QHash<QString, double> table = vader_word_table(kVaderSpecialCases);
    return table;
}

const QSet<QString>& vader_negations() {
    static const QSet<QString> words = [] {
        QSet<QString> s;
        for (const char* w : kVaderNegate)
            s.insert(QString::fromLatin1(w));
        return s;
    }();
    return words;
}

struct VaderLexicons {
    QHash<QString, double> lexicon;
    /// polarity_scores() looks emojis up one character at a time, so only the
    /// single-code-point entries of emoji_utf8_lexicon.txt can ever match.
    QHash<char32_t, QString> emojis;
    bool loaded = false;
};

/// `content.rstrip('\n').split('\n')`, each line `.strip().split('\t')[0:2]` — the parse
/// make_lex_dict() and make_emoji_dict() share. False when a line has fewer than two
/// fields (vaderSentiment raises there); the analyzer then reports !is_loaded().
template <typename Sink>
bool vader_parse_tab_file(const QString& path, bool skip_empty_lines, Sink sink) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QString content = QString::fromUtf8(file.readAll());
    while (content.endsWith(QLatin1Char('\n')))
        content.chop(1);
    const QList<QStringView> lines = QStringView(content).split(QLatin1Char('\n'));
    for (const QStringView line : lines) {
        if (skip_empty_lines && line.isEmpty())
            continue;
        const QString stripped = pycompat::str_strip(line);
        const QList<QStringView> fields = QStringView(stripped).split(QLatin1Char('\t'));
        if (fields.size() < 2 || !sink(fields[0], fields[1]))
            return false;
    }
    return true;
}

VaderLexicons load_vader_lexicons() {
    VaderLexicons data;
    const bool lexicon_ok = vader_parse_tab_file(QStringLiteral(":/vader/vader_lexicon.txt"), true,
                                                 [&data](QStringView word, QStringView measure) {
                                                     bool ok = false;
                                                     const double value = measure.toDouble(&ok);
                                                     if (ok)
                                                         data.lexicon.insert(word.toString(), value);
                                                     return ok;
                                                 });
    const bool emoji_ok = vader_parse_tab_file(
        QStringLiteral(":/vader/emoji_utf8_lexicon.txt"), false, [&data](QStringView emoji, QStringView description) {
            if (pycompat::cp_length(emoji) == 1)
                data.emojis.insert(pycompat::cp_at(emoji, 0), description.toString());
            return true;
        });
    data.loaded = lexicon_ok && emoji_ok;
    if (!data.loaded) {
        data.lexicon.clear();
        data.emojis.clear();
    }
    return data;
}

const VaderLexicons& vader_lexicons() {
    static const VaderLexicons data = load_vader_lexicons(); // thread-safe one-time load
    return data;
}

bool vader_is_ascii_punctuation(QChar c) {
    const char16_t u = c.unicode();
    return (u >= 0x21 && u <= 0x2F) || (u >= 0x3A && u <= 0x40) || (u >= 0x5B && u <= 0x60) || (u >= 0x7B && u <= 0x7E);
}

/// SentiText._strip_punc_if_word: strip string.punctuation from both ends unless that
/// leaves two characters or fewer (then it was probably an emoticon such as ":)").
QString vader_strip_punc_if_word(const QString& token) {
    qsizetype begin = 0;
    qsizetype end = token.size();
    while (begin < end && vader_is_ascii_punctuation(token[begin]))
        ++begin;
    while (end > begin && vader_is_ascii_punctuation(token[end - 1]))
        --end;
    const QStringView stripped = QStringView(token).sliced(begin, end - begin);
    if (pycompat::cp_length(stripped) <= 2)
        return token;
    return stripped.toString();
}

/// True if some but not all words are ALL CAPS.
bool vader_allcap_differential(const QStringList& words) {
    qsizetype allcap_words = 0;
    for (const QString& w : words) {
        if (pycompat::str_isupper(w))
            ++allcap_words;
    }
    const qsizetype cap_differential = words.size() - allcap_words;
    return cap_differential > 0 && cap_differential < words.size();
}

/// Does a preceding word boost, dampen or (via the caller) negate the valence?
double vader_scalar_inc_dec(const QString& word, const QString& word_lower, double valence, bool is_cap_diff) {
    double scalar = 0.0;
    const auto& boosters = vader_boosters();
    const auto it = boosters.constFind(word_lower);
    if (it != boosters.cend()) {
        scalar = *it;
        if (valence < 0)
            scalar *= -1;
        // booster/dampener in ALL CAPS while other words are not
        if (pycompat::str_isupper(word) && is_cap_diff) {
            if (valence > 0)
                scalar += kVaderCIncr;
            else
                scalar -= kVaderCIncr;
        }
    }
    return scalar;
}

bool vader_negated(const QString& word_lower) {
    return vader_negations().contains(word_lower) || word_lower.contains(QLatin1String("n't"));
}

double vader_negation_check(double valence, const QStringList& wl, qsizetype start_i, qsizetype i) {
    if (start_i == 0) {
        if (vader_negated(wl[i - 1])) // 1 word preceding the lexicon word
            valence = valence * kVaderNScalar;
    }
    if (start_i == 1) {
        if (wl[i - 2] == QLatin1String("never") &&
            (wl[i - 1] == QLatin1String("so") || wl[i - 1] == QLatin1String("this"))) {
            valence = valence * 1.25;
        } else if (wl[i - 2] == QLatin1String("without") && wl[i - 1] == QLatin1String("doubt")) {
            // "without doubt": not a negation
        } else if (vader_negated(wl[i - 2])) { // 2 words preceding
            valence = valence * kVaderNScalar;
        }
    }
    if (start_i == 2) {
        // As in vaderSentiment: the "so"/"this" test right before the word is NOT tied
        // to a preceding "never" (Python's and/or precedence).
        if ((wl[i - 3] == QLatin1String("never") &&
             (wl[i - 2] == QLatin1String("so") || wl[i - 2] == QLatin1String("this"))) ||
            (wl[i - 1] == QLatin1String("so") || wl[i - 1] == QLatin1String("this"))) {
            valence = valence * 1.25;
        } else if (wl[i - 3] == QLatin1String("without") &&
                   (wl[i - 2] == QLatin1String("doubt") || wl[i - 1] == QLatin1String("doubt"))) {
            // "without (a) doubt": not a negation
        } else if (vader_negated(wl[i - 3])) { // 3 words preceding
            valence = valence * kVaderNScalar;
        }
    }
    return valence;
}

double vader_special_idioms_check(double valence, const QStringList& wl, qsizetype i) {
    const QChar sp(QLatin1Char(' '));
    const QString onezero = wl[i - 1] + sp + wl[i];
    const QString twoonezero = wl[i - 2] + sp + wl[i - 1] + sp + wl[i];
    const QString twoone = wl[i - 2] + sp + wl[i - 1];
    const QString threetwoone = wl[i - 3] + sp + wl[i - 2] + sp + wl[i - 1];
    const QString threetwo = wl[i - 3] + sp + wl[i - 2];

    const auto& special = vader_special_cases();
    for (const QString* seq : {&onezero, &twoonezero, &twoone, &threetwoone, &threetwo}) {
        const auto it = special.constFind(*seq);
        if (it != special.cend()) {
            valence = *it;
            break;
        }
    }
    if (wl.size() - 1 > i) {
        const auto it = special.constFind(wl[i] + sp + wl[i + 1]);
        if (it != special.cend())
            valence = *it;
    }
    if (wl.size() - 1 > i + 1) {
        const auto it = special.constFind(wl[i] + sp + wl[i + 1] + sp + wl[i + 2]);
        if (it != special.cend())
            valence = *it;
    }
    // booster/dampener bi-grams such as 'sort of' or 'kind of'
    const auto& boosters = vader_boosters();
    for (const QString* n_gram : {&threetwoone, &threetwo, &twoone}) {
        const auto it = boosters.constFind(*n_gram);
        if (it != boosters.cend())
            valence = valence + *it;
    }
    return valence;
}

/// Contrastive "but": halves sentiment before it, boosts it after. Reproduces
/// vaderSentiment exactly — each value is re-located with list.index(), i.e. the FIRST
/// element equal to it, so repeated values all land on that first position.
void vader_but_check(const QStringList& wl, std::vector<double>& sentiments) {
    const qsizetype bi = wl.indexOf(QStringLiteral("but"));
    if (bi < 0)
        return;
    for (std::size_t k = 0; k < sentiments.size(); ++k) {
        const double sentiment = sentiments[k]; // never rewritten before step k (si <= step)
        std::size_t si = 0;
        while (sentiments[si] != sentiment)
            ++si;
        const auto pos = static_cast<qsizetype>(si);
        if (pos < bi)
            sentiments[si] = sentiment * 0.5;
        else if (pos > bi)
            sentiments[si] = sentiment * 1.5;
    }
}

double vader_punctuation_emphasis(const QString& text) {
    // up to 4 exclamation points
    const qsizetype ep_count = std::min<qsizetype>(text.count(QLatin1Char('!')), 4);
    const double ep_amplifier = static_cast<double>(ep_count) * 0.292;
    // 2 or 3+ question marks
    const qsizetype qm_count = text.count(QLatin1Char('?'));
    double qm_amplifier = 0.0;
    if (qm_count > 1)
        qm_amplifier = qm_count <= 3 ? static_cast<double>(qm_count) * 0.18 : 0.96;
    return ep_amplifier + qm_amplifier;
}

/// normalize(score, alpha=15). Separate statements keep compilers that contract
/// within an expression (Apple clang on arm64) from fusing score*score + alpha.
double vader_normalize(double score) {
    const double squared = score * score;
    const double denominator = std::sqrt(squared + 15.0);
    const double norm_score = score / denominator;
    if (norm_score < -1.0)
        return -1.0;
    if (norm_score > 1.0)
        return 1.0;
    return norm_score;
}

VaderScores vader_score_valence(const std::vector<double>& sentiments, const QString& text) {
    VaderScores out;
    if (sentiments.empty())
        return out;
    double sum_s = 0.0;
    for (const double s : sentiments)
        sum_s += s;
    const double punct_emph_amplifier = vader_punctuation_emphasis(text);
    if (sum_s > 0)
        sum_s += punct_emph_amplifier;
    else if (sum_s < 0)
        sum_s -= punct_emph_amplifier;
    const double compound = vader_normalize(sum_s);

    // separate positive versus negative sentiment scores
    double pos_sum = 0.0;
    double neg_sum = 0.0;
    int neu_count = 0;
    for (const double s : sentiments) {
        if (s > 0)
            pos_sum += (s + 1); // compensates for neutral words that are counted as 1
        if (s < 0)
            neg_sum += (s - 1);
        if (s == 0)
            neu_count += 1;
    }
    if (pos_sum > std::fabs(neg_sum))
        pos_sum += punct_emph_amplifier;
    else if (pos_sum < std::fabs(neg_sum))
        neg_sum -= punct_emph_amplifier;

    const double total = pos_sum + std::fabs(neg_sum) + neu_count;
    out.neg = pycompat::round_digits(std::fabs(neg_sum / total), 3);
    out.neu = pycompat::round_digits(std::fabs(neu_count / total), 3);
    out.pos = pycompat::round_digits(std::fabs(pos_sum / total), 3);
    out.compound = pycompat::round_digits(compound, 4);
    return out;
}

} // namespace vader_detail

VaderSentiment::VaderSentiment(const QHash<QString, double>& lexicon_overrides) {
    const auto& base = vader_detail::vader_lexicons();
    loaded_ = base.loaded;
    if (!loaded_)
        return;
    lexicon_ = base.lexicon;
    for (auto it = lexicon_overrides.cbegin(); it != lexicon_overrides.cend(); ++it)
        lexicon_.insert(it.key(), it.value());
}

VaderScores VaderSentiment::polarity_scores(const QString& text) const {
    using namespace vader_detail;
    // Emojis become their textual descriptions.
    const auto& emojis = vader_lexicons().emojis;
    QString text_no_emoji;
    text_no_emoji.reserve(text.size());
    bool prev_space = true;
    for (qsizetype i = 0; i < text.size();) {
        const char32_t c = pycompat::cp_at(text, i);
        const qsizetype width = c > 0xFFFF ? 2 : 1;
        const auto it = emojis.constFind(c);
        if (it != emojis.cend()) {
            if (!prev_space)
                text_no_emoji.append(QLatin1Char(' '));
            text_no_emoji.append(*it);
            prev_space = false;
        } else {
            text_no_emoji.append(QStringView(text).sliced(i, width));
            prev_space = c == U' ';
        }
        i += width;
    }
    const QString stripped = pycompat::str_strip(text_no_emoji);

    // SentiText: whitespace tokens with surrounding punctuation removed (emoticons kept).
    QStringList words = pycompat::str_split(stripped);
    for (QString& w : words)
        w = vader_strip_punc_if_word(w);
    const bool is_cap_diff = vader_allcap_differential(words);
    QStringList words_lower;
    words_lower.reserve(words.size());
    for (const QString& w : words)
        words_lower.append(pycompat::str_lower(w));

    const auto& boosters = vader_boosters();
    std::vector<double> sentiments;
    sentiments.reserve(static_cast<std::size_t>(words.size()));
    for (qsizetype i = 0; i < words.size(); ++i) {
        const QString& item_lower = words_lower[i];
        // lexicon words used as modifiers carry no valence of their own
        if (boosters.contains(item_lower) || (i < words.size() - 1 && item_lower == QLatin1String("kind") &&
                                              words_lower[i + 1] == QLatin1String("of"))) {
            sentiments.push_back(0.0);
            continue;
        }
        sentiments.push_back(sentiment_valence(words, words_lower, i, is_cap_diff));
    }
    vader_but_check(words_lower, sentiments);
    return vader_score_valence(sentiments, stripped);
}

double VaderSentiment::sentiment_valence(const QStringList& words, const QStringList& words_lower, qsizetype i,
                                         bool is_cap_diff) const {
    using namespace vader_detail;
    const QStringList& wl = words_lower;
    const auto it = lexicon_.constFind(wl[i]);
    if (it == lexicon_.cend())
        return 0.0;
    double valence = *it;
    const qsizetype n = words.size();

    // "no" as a negation of the next lexicon word rather than a lexicon item of its own
    if (wl[i] == QLatin1String("no") && i != n - 1 && lexicon_.contains(wl[i + 1]))
        valence = 0.0;
    if ((i > 0 && wl[i - 1] == QLatin1String("no")) || (i > 1 && wl[i - 2] == QLatin1String("no")) ||
        (i > 2 && wl[i - 3] == QLatin1String("no") &&
         (wl[i - 1] == QLatin1String("or") || wl[i - 1] == QLatin1String("nor")))) {
        valence = *it * kVaderNScalar;
    }

    // sentiment-laden word in ALL CAPS while others aren't
    if (pycompat::str_isupper(words[i]) && is_cap_diff) {
        if (valence > 0)
            valence += kVaderCIncr;
        else
            valence -= kVaderCIncr;
    }

    for (qsizetype start_i = 0; start_i < 3; ++start_i) {
        // Dampen the scalar of preceding words by their distance from this one.
        const qsizetype k = i - (start_i + 1);
        if (i > start_i && !lexicon_.contains(wl[k])) {
            double s = vader_scalar_inc_dec(words[k], wl[k], valence, is_cap_diff);
            if (start_i == 1 && s != 0)
                s = s * 0.95;
            if (start_i == 2 && s != 0)
                s = s * 0.9;
            valence = valence + s;
            valence = vader_negation_check(valence, wl, start_i, i);
            if (start_i == 2)
                valence = vader_special_idioms_check(valence, wl, i);
        }
    }
    return least_check(valence, wl, i);
}

double VaderSentiment::least_check(double valence, const QStringList& words_lower, qsizetype i) const {
    // negation case using "least"
    using vader_detail::kVaderNScalar;
    const QStringList& wl = words_lower;
    if (i > 1 && !lexicon_.contains(wl[i - 1]) && wl[i - 1] == QLatin1String("least")) {
        if (wl[i - 2] != QLatin1String("at") && wl[i - 2] != QLatin1String("very"))
            valence = valence * kVaderNScalar;
    } else if (i > 0 && !lexicon_.contains(wl[i - 1]) && wl[i - 1] == QLatin1String("least")) {
        valence = valence * kVaderNScalar;
    }
    return valence;
}

} // namespace fincept::services
