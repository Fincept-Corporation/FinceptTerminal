// NewsLanguage.cpp — see NewsLanguage.h.

#include "services/news/NewsLanguage.h"

#include <QHash>
#include <QSet>
#include <QVector>

#include <algorithm>

namespace fincept::services::news_lang {

namespace {

/// Latin-script languages with a stop-word profile, in table order.
constexpr int kNewsLangLatinCount = 8;
const char* const kNewsLangLatinCodes[kNewsLangLatinCount] = {"en", "fr", "de", "es", "pt", "it", "nl", "sv"};

// High-frequency function words plus headline vocabulary of each language.
// Words shared by several languages ("de", "la", "in", "a") count for each of
// them with a weight of 1 / (number of languages that list them).
const char* const kNewsLangWordsEn =
    "the of and to in for on with at by from as is are was were be been has have had will would "
    "it its this that these those after over into amid says said say new not no but or than more "
    "up down out against about why how what who when where could may might should can "
    "stocks shares market markets rate rates bank banks oil prices price deal deals year week "
    "report reports profit profits sales growth investors trade economy government first record "
    "billion million rises rise falls fall jumps slips gains losses hits cuts raises warns "
    "plans seeks set sees ahead amid despite while under between before since";
const char* const kNewsLangWordsFr =
    "le la les des du de un une et en est dans pour sur au aux par avec que qui pas plus "
    "ce cette ces son sa ses leur leurs mais ou où sont été être fait selon après face contre "
    "chez sans entre vers aussi encore très bourse marché marchés taux banque prix hausse baisse "
    "milliards millions euros entreprises gouvernement économie croissance résultats chiffre "
    "affaires annonce nouvelle nouveau premier ministre l' d' qu' n' s' c'";
const char* const kNewsLangWordsDe =
    "der die das und mit für auf ist im den von zu nach bei wie ein eine einen einem einer will soll sollen "
    "muss kann dass wegen chef "
    "nicht auch sich des dem aus um wird werden wurde sind hat haben noch nur über unter gegen "
    "vor beim zum zur seit bis mehr neue neuen neuer erste ersten so am als oder aber wenn "
    "börse aktie aktien markt märkte zinsen bank banken preise milliarden millionen euro "
    "unternehmen regierung wirtschaft wachstum gewinn umsatz prozent jahr steigt fällt";
const char* const kNewsLangWordsEs =
    "el la los las de del que y en por para con una un se su sus al es más como pero sin a no lo hoy según "
    "llega aún "
    "sobre entre tras ante hasta desde este esta estos estas ya muy fue ser son ha han hay "
    "nuevo nueva bolsa mercado mercados tasas banco precios alza baja millones euros dólares "
    "empresas gobierno economía crecimiento resultados ventas año inversores acciones";
const char* const kNewsLangWordsPt =
    "o a os as de do da dos das e em um uma para com por no na nos nas que não mais ao aos "
    "se seu sua seus suas é foi ser são tem têm como mas sem sobre entre após até desde "
    "este esta isso já muito novo nova bolsa mercado mercados juros banco preços alta queda "
    "bilhões milhões reais dólar empresas governo economia crescimento resultados vendas ano "
    "investidores ações";
const char* const kNewsLangWordsIt =
    "il lo la i gli le di del della dello dei degli delle e in un una per con su che è non "
    "più al alla alle dal dalla nel nella sono ha hanno anche come ma tra fra dopo verso "
    "contro questo questa nuovo nuova borsa mercato mercati tassi banca prezzi rialzo calo "
    "miliardi milioni euro aziende governo economia crescita risultati vendite anno";
const char* const kNewsLangWordsNl =
    "de het een en van in op te voor met is dat die niet aan bij om naar zijn ook als maar "
    "of uit door over na tot nog wel wordt worden werd heeft hebben er dan deze dit meer kan wil zonder "
    "neemt geen naar onder tussen weer vanwege topman "
    "nieuwe beurs markt markten rente bank banken prijzen miljard miljoen euro bedrijven "
    "regering economie groei winst omzet jaar kabinet";
const char* const kNewsLangWordsSv =
    "och att det som en ett är på för med av till den har inte om de var men från efter "
    "kan ska nya sig vid över mot under sin sina också bara när där mer än år "
    "börsen aktier marknaden räntan banken priser miljarder miljoner kronor företag regeringen "
    "ekonomin tillväxt vinst";

struct NewsLangTables {
    QHash<QString, QVector<int>> word_langs; // word -> languages listing it
};

const NewsLangTables& news_lang_tables() {
    static const NewsLangTables tables = [] {
        NewsLangTables t;
        const char* const lists[kNewsLangLatinCount] = {kNewsLangWordsEn, kNewsLangWordsFr, kNewsLangWordsDe,
                                                        kNewsLangWordsEs, kNewsLangWordsPt, kNewsLangWordsIt,
                                                        kNewsLangWordsNl, kNewsLangWordsSv};
        for (int lang = 0; lang < kNewsLangLatinCount; ++lang) {
            const QStringList words = QString::fromUtf8(lists[lang]).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString& w : words) {
                auto& v = t.word_langs[w];
                if (!v.contains(lang))
                    v.append(lang);
            }
        }
        return t;
    }();
    return tables;
}

int news_lang_index(const QString& code) {
    for (int i = 0; i < kNewsLangLatinCount; ++i)
        if (code == QLatin1String(kNewsLangLatinCodes[i]))
            return i;
    return -1;
}

struct NewsScriptCounts {
    int letters = 0;
    int latin = 0;
    int han = 0;
    int kana = 0;
    int hangul = 0;
    int arabic = 0;
    int hebrew = 0;
    int cyrillic = 0;
    int ukrainian_marks = 0; // і ї є ґ
    int devanagari = 0;
    int thai = 0;
    int greek = 0;
    bool non_ascii_latin = false;
};

NewsScriptCounts news_count_scripts(const QString& text) {
    NewsScriptCounts c;
    for (qsizetype i = 0; i < text.size(); ++i) {
        char32_t cp = text.at(i).unicode();
        if (QChar::isHighSurrogate(cp) && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            cp = QChar::surrogateToUcs4(text.at(i), text.at(i + 1));
            ++i;
        }
        if (!QChar::isLetter(cp))
            continue;
        ++c.letters;
        if (cp < 0x0250) {
            ++c.latin;
            if (cp > 0x7F)
                c.non_ascii_latin = true;
        } else if ((cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0xF900 && cp <= 0xFAFF) ||
                   (cp >= 0x20000 && cp <= 0x2A6DF)) {
            ++c.han;
        } else if (cp >= 0x3040 && cp <= 0x30FF) {
            ++c.kana;
        } else if ((cp >= 0xAC00 && cp <= 0xD7AF) || (cp >= 0x1100 && cp <= 0x11FF) || (cp >= 0x3130 && cp <= 0x318F)) {
            ++c.hangul;
        } else if ((cp >= 0x0600 && cp <= 0x06FF) || (cp >= 0x0750 && cp <= 0x077F) || (cp >= 0xFB50 && cp <= 0xFEFF)) {
            ++c.arabic;
        } else if (cp >= 0x0590 && cp <= 0x05FF) {
            ++c.hebrew;
        } else if (cp >= 0x0400 && cp <= 0x04FF) {
            ++c.cyrillic;
            if (cp == 0x0456 || cp == 0x0457 || cp == 0x0454 || cp == 0x0491 || cp == 0x0406 || cp == 0x0407 ||
                cp == 0x0404 || cp == 0x0490)
                ++c.ukrainian_marks;
        } else if (cp >= 0x0900 && cp <= 0x097F) {
            ++c.devanagari;
        } else if (cp >= 0x0E00 && cp <= 0x0E7F) {
            ++c.thai;
        } else if (cp >= 0x0370 && cp <= 0x03FF) {
            ++c.greek;
        }
    }
    return c;
}

/// Script-decided language, or empty when the text is (mostly) Latin script.
QString news_script_language(const NewsScriptCounts& c, double* confidence) {
    if (c.letters == 0)
        return {};
    const double n = static_cast<double>(c.letters);
    auto share = [n](int k) { return static_cast<double>(k) / n; };
    // Japanese mixes kanji with kana; any real share of kana decides it.
    if (share(c.kana) > 0.05) {
        *confidence = std::min(1.0, 0.6 + share(c.kana + c.han));
        return QStringLiteral("ja");
    }
    struct Cand {
        int count;
        const char* code;
    };
    const Cand cands[] = {{c.han, "zh"},          {c.hangul, "ko"}, {c.arabic, "ar"}, {c.hebrew, "he"},
                          {c.cyrillic, "ru"},     {c.devanagari, "hi"}, {c.thai, "th"}, {c.greek, "el"}};
    const Cand* best = nullptr;
    for (const auto& cand : cands)
        if (!best || cand.count > best->count)
            best = &cand;
    if (!best || share(best->count) < 0.25)
        return {};
    *confidence = std::min(1.0, 0.5 + share(best->count) / 2.0);
    if (qstrcmp(best->code, "ru") == 0 && c.ukrainian_marks >= 2)
        return QStringLiteral("uk");
    return QString::fromLatin1(best->code);
}

/// Lower-case word tokens of Latin-script text (apostrophe elisions kept as
/// their own token: "l'économie" -> "l'", "économie").
QStringList news_latin_tokens(const QString& text) {
    QStringList out;
    QString cur;
    const QString lower = text.toLower();
    for (qsizetype i = 0; i < lower.size(); ++i) {
        const QChar ch = lower.at(i);
        if (ch.isLetter()) {
            cur.append(ch);
            continue;
        }
        if ((ch == QLatin1Char('\'') || ch == QChar(0x2019)) && !cur.isEmpty() && cur.size() <= 2) {
            out.append(cur + QLatin1Char('\''));
            cur.clear();
            continue;
        }
        if (!cur.isEmpty()) {
            out.append(cur);
            cur.clear();
        }
    }
    if (!cur.isEmpty())
        out.append(cur);
    return out;
}

/// Letters that occur in (nearly) only one of the profiled languages.
void news_letter_hints(const QString& text, double scores[kNewsLangLatinCount]) {
    for (const QChar ch : text) {
        switch (ch.toLower().unicode()) {
            case 0x00DF: // ß
                scores[2] += 1.5;
                break;
            case 0x00E4: // ä — de / sv
            case 0x00F6: // ö
                scores[2] += 0.5;
                scores[7] += 0.5;
                break;
            case 0x00FC: // ü
                scores[2] += 1.0;
                break;
            case 0x00E5: // å
                scores[7] += 1.5;
                break;
            case 0x00F1: // ñ
            case 0x00BF: // ¿
            case 0x00A1: // ¡
                scores[3] += 1.5;
                break;
            case 0x00E3: // ã
            case 0x00F5: // õ
                scores[4] += 1.5;
                break;
            case 0x00E7: // ç — fr / pt
                scores[1] += 0.5;
                scores[4] += 0.5;
                break;
            case 0x00E8: // è
            case 0x00EA: // ê — fr (pt has ê too)
            case 0x00EB: // ë
            case 0x0153: // œ
            case 0x00EE: // î
            case 0x00EF: // ï
                scores[1] += 0.6;
                break;
            case 0x00F2: // ò
            case 0x00EC: // ì
                scores[5] += 1.0;
                break;
            default:
                break;
        }
    }
}

} // namespace

LangGuess detect(const QString& text, const QString& prior_in) {
    const QString prior = prior_in.trimmed().toLower();
    const NewsScriptCounts sc = news_count_scripts(text);
    if (sc.letters == 0)
        return {prior, prior.isEmpty() ? 0.0 : 0.5, !prior.isEmpty()};

    double script_conf = 0.0;
    const QString script_lang = news_script_language(sc, &script_conf);
    if (!script_lang.isEmpty()) {
        // A feed declared "zh-TW"/"zh" etc. — the script decides; the prior only
        // confirms. (Chinese text with a Japanese prior: Japanese without kana is
        // unusual but possible in short headlines — trust the feed then.)
        if (script_lang == QLatin1String("zh") && prior == QLatin1String("ja"))
            return {prior, 0.6, true};
        return {script_lang, script_conf, false};
    }

    const auto& tables = news_lang_tables();
    double scores[kNewsLangLatinCount] = {0, 0, 0, 0, 0, 0, 0, 0};
    const QStringList tokens = news_latin_tokens(text);
    for (const QString& tok : tokens) {
        const auto it = tables.word_langs.constFind(tok);
        if (it == tables.word_langs.constEnd())
            continue;
        const double w = 1.0 / static_cast<double>(it->size());
        for (int lang : *it)
            scores[lang] += w;
    }
    news_letter_hints(text, scores);

    int best = 0;
    int second = -1;
    for (int i = 1; i < kNewsLangLatinCount; ++i) {
        if (scores[i] > scores[best]) {
            second = best;
            best = i;
        } else if (second < 0 || scores[i] > scores[second]) {
            second = i;
        }
    }
    const double best_score = scores[best];
    const double second_score = second >= 0 ? scores[second] : 0.0;
    const int prior_idx = news_lang_index(prior);
    const bool prior_is_latin_profiled = prior_idx >= 0;

    // Weak evidence (names and numbers only): the feed's language decides.
    if (best_score < 1.0) {
        if (!prior.isEmpty())
            return {prior, 0.5, true};
        if (!sc.non_ascii_latin && tokens.size() >= 2)
            return {QStringLiteral("en"), 0.3, false};
        return {};
    }
    // A profiled feed language yields only to clear evidence: a single shared
    // or borrowed word ("will" in a German headline, "AI" read as Italian "ai")
    // must not relabel an outlet's article.
    if (prior_is_latin_profiled && prior_idx != best) {
        const double prior_score = scores[prior_idx];
        if (best_score < 2.5 || best_score < 2.0 * (prior_score + 0.5))
            return {prior, 0.55, true};
    }
    // A Latin-script feed in a language without a profile (pl, tr, id, vi ...):
    // only clear evidence for a profiled language overrides it.
    if (!prior.isEmpty() && !prior_is_latin_profiled && best_score < 3.0)
        return {prior, 0.5, true};

    const double margin = best_score - second_score;
    double confidence = std::clamp(0.45 + margin / (best_score + 1.0), 0.3, 0.98);
    if (prior_idx == best)
        confidence = std::min(0.99, confidence + 0.15);
    return {QString::fromLatin1(kNewsLangLatinCodes[best]), confidence, false};
}

const QStringList& filter_languages() {
    static const QStringList langs = {QStringLiteral("en"), QStringLiteral("fr"), QStringLiteral("de"),
                                      QStringLiteral("es"), QStringLiteral("pt"), QStringLiteral("it"),
                                      QStringLiteral("nl"), QStringLiteral("sv"), QStringLiteral("zh"),
                                      QStringLiteral("ja"), QStringLiteral("ko"), QStringLiteral("hi"),
                                      QStringLiteral("ar"), QStringLiteral("he"), QStringLiteral("ru"),
                                      QStringLiteral("th")};
    return langs;
}

bool sentiment_supported(const QString& code) {
    return code == QLatin1String("en");
}

} // namespace fincept::services::news_lang
