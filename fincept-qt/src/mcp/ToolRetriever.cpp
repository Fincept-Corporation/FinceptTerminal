// ToolRetriever.cpp — Okapi BM25 over the registered MCP tool catalog.

#include "mcp/ToolRetriever.h"

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace fincept::mcp {

static constexpr const char* TAG = "ToolRetriever";

ToolRetriever& ToolRetriever::instance() {
    static ToolRetriever s;
    return s;
}

void ToolRetriever::invalidate() {
    QMutexLocker lock(&mutex_);
    indexed_generation_ = static_cast<quint64>(-1);
}

// ── Stop words ──────────────────────────────────────────────────────────
//
// Conservative list — only common English function words. Generic command
// verbs ("get", "set", "list", "show", "run") WERE in this list, but the
// quality battery revealed that stripping them collapses verb-cued queries:
// "show me the watchlist" became indistinguishable from "watchlist", so all
// four watchlist tools tied and `delete_watchlist` won on alphabetical order.
// We now keep verb tokens; BM25's IDF naturally down-weights them since
// they appear across many tools.
static const QSet<QString>& stop_word_set() {
    static const QSet<QString> kStop = {
        "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
        "has", "he", "in", "is", "it", "its", "of", "on", "or", "that",
        "the", "to", "was", "were", "will", "with", "this", "these", "those",
        "i", "me", "my", "we", "us", "you", "your", "they", "them",
        "but", "if", "so", "do", "does", "did", "have", "had", "having",
        "can", "could", "should", "would", "may", "might", "must",
        "what", "which", "who", "whom", "whose", "where", "when", "why", "how",
        "all", "any", "both", "each", "few", "more", "most", "other", "some",
        "such", "no", "not", "only", "own", "same", "than", "too", "very",
        "just", "also", "now", "then", "here", "there",
    };
    return kStop;
}

// ── Query-side synonyms ─────────────────────────────────────────────────
//
// Applied ONLY when tokenising the query, never the indexed docs. Expands
// natural-language phrasings into the vocabulary actually used in tool
// names ("price" → "quote", "buy/sell" → "order/trade", "tab/screen" →
// "navigate"). Each entry adds tokens; the original token stays in the
// stream so a literal match still scores.
//
// Keep this map small and audit-friendly — anything broader belongs in a
// proper embedding model. Domain words first; UI/navigation verbs second.
static const QHash<QString, QStringList>& query_synonyms() {
    static const QHash<QString, QStringList> kSyn = {
        // Markets / equity
        {"price",    {"quote"}},
        {"prices",   {"quote", "historical"}},
        {"ticker",   {"symbol"}},
        {"company",  {"symbol", "equity"}},
        {"chart",    {"ohlc"}},
        // Trading
        {"buy",      {"order", "trade", "place"}},
        {"sell",     {"order", "trade", "place"}},
        {"order",    {"trade"}},
        {"trade",    {"order"}},
        // Navigation / UI
        {"tab",      {"navigate", "screen"}},
        {"screen",   {"navigate", "tab"}},
        {"page",     {"navigate", "tab"}},
        {"open",     {"navigate"}},
        {"switch",   {"navigate"}},
        {"window",   {"panel"}},
        // Portfolio
        {"pnl",      {"profit", "portfolio"}},
        {"profit",   {"portfolio", "pnl"}},
        {"holdings", {"portfolio", "holding"}},
        // Filings
        {"filings",  {"edgar"}},
        {"filing",   {"edgar"}},
        {"sec",      {"edgar"}},
        {"earnings", {"financials"}},
        // Strategy / analytics
        {"strategy", {"backtest"}},
        {"sharpe",   {"risk", "metrics"}},
        {"var",      {"risk"}},
        // Notes
        {"memo",     {"note"}},
        {"journal",  {"note"}},
    };
    return kSyn;
}

// Read-style verbs vs mutate-style verbs. Used by the destructive-tool
// penalty: if the query reads like an inspection ("show me X", "list X")
// and contains no mutate verb, score destructive tools at half weight so
// `delete_watchlist` stops outranking `add_to_watchlist` for "show me the
// watchlist".
static const QSet<QString>& read_verbs() {
    static const QSet<QString> kRead = {
        "show", "list", "view", "find", "get", "fetch", "lookup",
        "describe", "search", "display", "see", "read", "inspect",
        "explore", "what", "which", "tell",
    };
    return kRead;
}
static const QSet<QString>& mutate_verbs() {
    static const QSet<QString> kMutate = {
        "delete", "remove", "drop", "clear",
        "add", "create", "insert", "append", "import",
        "place", "submit", "send", "post",
        "set", "update", "modify", "edit", "rename",
        "cancel", "stop", "kill",
        "save", "store", "export", "write",
    };
    return kMutate;
}

// Specialist categories — these have many tools that keyword-match generic
// queries (e.g. quant-lab's 100 tools mention "factor/risk/model") and
// drown out simpler tools. Each entry maps category → cue tokens that
// signal genuine intent for that specialist. If a query contains NO cue
// for a specialist, every tool in it is demoted at scoring time.
static const QHash<QString, QSet<QString>>& specialist_categories() {
    static const QHash<QString, QSet<QString>> kSpec = {
        {"quant-lab",         {"quant", "factor", "backtest", "alpha", "ic",
                               "rl", "ml", "ml-ops", "alphalens", "qlib"}},
        {"surface-analytics", {"surface", "vol", "volatility", "skew",
                               "databento", "iv"}},
        {"alt-investments",   {"alt", "junk", "convertible", "preferred",
                               "private", "venture", "hedge", "yield"}},
    };
    return kSpec;
}
static constexpr double kSpecialistPenalty = 0.7;
static constexpr double kDestructivePenalty = 0.5;
static constexpr double kNameMatchBonus = 2.0;

bool ToolRetriever::is_stop_word(const QString& token) {
    return stop_word_set().contains(token);
}

QStringList ToolRetriever::tokenise(const QString& text) {
    // Split on non-word characters AND underscores, lowercase, drop tokens
    // shorter than 2 chars. Splitting on underscores is essential because
    // tool names are snake_case: "report_apply_template" → ["report","apply","template"].
    // Without this, the entire snake_case name would be one token and
    // individual-word queries ("apply template") would miss the name-match bonus.
    QStringList out;
    static const QRegularExpression kSplit("[\\W_]+");
    const auto raw = text.toLower().split(kSplit, Qt::SkipEmptyParts);
    out.reserve(raw.size());
    for (const auto& w : raw) {
        if (w.size() < 2)
            continue;
        if (is_stop_word(w))
            continue;
        out.append(w);
    }
    return out;
}

QString ToolRetriever::stem(const QString& word) {
    if (word.length() <= 2)
        return word;

    QString w = word.toLower();

    // 1. Plurals and common suffixes
    if (w.endsWith(QLatin1String("ies"))) {
        if (w.length() > 4) {
            w.chop(3);
            w.append(QLatin1Char('y'));
        }
    } else if (w.endsWith(QLatin1String("sses"))) {
        w.chop(2); // "classes" -> "class"
    } else if (w.endsWith(QLatin1String("es"))) {
        if (w.length() > 3) {
            w.chop(2);
        }
    } else if (w.endsWith(QLatin1Char('s'))) {
        if (!w.endsWith(QLatin1String("ss")) && !w.endsWith(QLatin1String("us")) && !w.endsWith(QLatin1String("is"))) {
            w.chop(1);
        }
    }

    // 2. Gerunds and past participles
    if (w.endsWith(QLatin1String("ing"))) {
        if (w.length() > 5) {
            w.chop(3);
        }
    } else if (w.endsWith(QLatin1String("ed"))) {
        if (w.length() > 4) {
            w.chop(2);
        }
    }

    // 3. Common derivations
    if (w.endsWith(QLatin1String("ational"))) {
        w.chop(7);
        w.append(QLatin1String("ate"));
    } else if (w.endsWith(QLatin1String("tional"))) {
        w.chop(6);
        w.append(QLatin1String("tion"));
    } else if (w.endsWith(QLatin1String("ly"))) {
        if (w.length() > 4) w.chop(2);
    } else if (w.endsWith(QLatin1String("ment"))) {
        if (w.length() > 5) w.chop(4);
    }

    // 4. Strip trailing silent 'e' to unify tenses (e.g. "create"/"creating" -> "creat", "price"/"pricing" -> "pric")
    if (w.endsWith(QLatin1Char('e')) && w.length() > 3) {
        w.chop(1);
    }

    return w;
}

int ToolRetriever::levenshtein_distance(const QString& s1, const QString& s2) {
    const int len1 = s1.length();
    const int len2 = s2.length();
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;

    std::vector<int> col(len2 + 1);
    std::vector<int> prev_col(len2 + 1);

    for (int i = 0; i <= len2; ++i) {
        prev_col[i] = i;
    }

    for (int i = 0; i < len1; ++i) {
        col[0] = i + 1;
        for (int j = 0; j < len2; ++j) {
            const int cost = (s1[i] == s2[j]) ? 0 : 1;
            col[j + 1] = std::min({ col[j] + 1, prev_col[j + 1] + 1, prev_col[j] + cost });
        }
        col.swap(prev_col);
    }
    return prev_col[len2];
}

QString ToolRetriever::classify_category(const QStringList& query_stems) {
    static const QHash<QString, QSet<QString>> kCategorySignals = {
        {"watchlist",      {"watchlist", "watch", "track"}},
        {"paper-trading",  {"trad", "buy", "sell", "order", "portfolio", "pnl", "hold", "position", "broker"}},
        {"news",           {"new", "feed", "rss", "headlin", "articl", "file", "edgar"}},
        {"report-builder", {"report", "builder", "generat", "document", "templat", "pdf"}},
        {"quant-lab",      {"quant", "factor", "backtest", "alpha", "risk", "metric", "sharp", "var"}},
        {"markets",        {"market", "quot", "pric", "stock", "equity", "ticker", "chart", "ohlc"}},
        {"notes",          {"not", "memo", "mind", "journal", "writ"}},
        {"file_manager",   {"fil", "folder", "directory", "path", "open", "read", "writ"}},
        {"settings",       {"sett", "config", "setup", "credential", "api", "key"}}
    };

    QHash<QString, int> category_scores;
    for (const auto& stem : query_stems) {
        for (auto it = kCategorySignals.constBegin(); it != kCategorySignals.constEnd(); ++it) {
            if (it.value().contains(stem)) {
                category_scores[it.key()] += 1;
            }
        }
    }

    QString best_category;
    int max_score = 0;
    for (auto it = category_scores.constBegin(); it != category_scores.constEnd(); ++it) {
        if (it.value() > max_score) {
            max_score = it.value();
            best_category = it.key();
        }
    }

    return best_category;
}

// ── Index build ─────────────────────────────────────────────────────────

void ToolRetriever::rebuild_index_locked() {
    docs_.clear();
    df_.clear();
    inverted_index_.clear();
    vocabulary_.clear();
    avg_doc_length_ = 0.0;

    // Index the unified catalog — internal registry tools PLUS tools
    // discovered from running external MCP servers (McpService merges the
    // two, cached 5 s). Building only over McpProvider::list_all_tools()
    // left external tools invisible to Tool RAG's tool_list. Disabled
    // internal tools are excluded upstream by list_tools(); the
    // is_tool_enabled() check at search time still guards against tools
    // disabled after the index was built (external names always pass it).
    const auto tool_list = McpService::instance().get_all_tools();
    docs_.reserve(tool_list.size());

    for (const auto& t : tool_list) {
        Doc d;
        d.name = t.name;
        d.category = t.category;
        d.description = t.description;
        d.is_destructive = t.is_destructive;

        // Construct the full raw text for phrase matching
        QStringList raw_parts;
        raw_parts.append(t.name);
        raw_parts.append(t.description);
        const QJsonObject props = t.input_schema["properties"].toObject();
        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            raw_parts.append(it.key());
            const QString p_desc = it.value().toObject()["description"].toString();
            if (!p_desc.isEmpty())
                raw_parts.append(p_desc);
        }
        d.raw_text = raw_parts.join(" ").toLower();

        // ── Build the weighted token stream ────────────────────────────
        // Token weighting via repetition. BM25's TF saturates with k1 so
        // raw repetition gives a ~log-shaped boost — exactly what we want
        // for "name token is much more important than description token"
        // without inventing a per-field BM25F variant.
        //
        //   name tokens  → weight 3 (most discriminative)
        //   param names  → weight 2
        //   description  → weight 1
        //
        QStringList tokens;
        const auto name_tok = tokenise(t.name);
        for (int i = 0; i < 3; ++i)
            tokens.append(name_tok);
        for (const auto& nt : name_tok)
            d.name_stems.insert(stem(nt));

        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            const QStringList p_tok = tokenise(it.key());
            for (int i = 0; i < 2; ++i)
                tokens.append(p_tok);
            // Param description text — single weight.
            const QString p_desc = it.value().toObject()["description"].toString();
            if (!p_desc.isEmpty())
                tokens.append(tokenise(p_desc));
        }

        tokens.append(tokenise(t.description));

        d.length = tokens.size();
        avg_doc_length_ += d.length;

        // Populate term_tf, vocabulary_, df_, and inverted_index_
        const int doc_idx = static_cast<int>(docs_.size());
        QSet<QString> seen_stems;
        for (const auto& tok : tokens) {
            QString stemmed = stem(tok);
            d.term_tf[stemmed] = d.term_tf.value(stemmed, 0) + 1;
            vocabulary_.insert(stemmed);

            if (!seen_stems.contains(stemmed)) {
                seen_stems.insert(stemmed);
                df_[stemmed] = df_.value(stemmed, 0) + 1;
                inverted_index_[stemmed].append(doc_idx);
            }
        }

        docs_.push_back(std::move(d));
    }

    if (!docs_.empty())
        avg_doc_length_ /= static_cast<double>(docs_.size());

    indexed_generation_ = McpProvider::instance().generation();

    LOG_INFO(TAG, QString("Semantic-Enhanced BM25 index built: %1 docs, %2 unique terms, avg_len=%3")
                      .arg(docs_.size())
                      .arg(df_.size())
                      .arg(avg_doc_length_, 0, 'f', 1));
}

// ── Search ──────────────────────────────────────────────────────────────

std::vector<ToolMatch> ToolRetriever::search(const QString& query, int top_k,
                                              const QString& category_filter) {
    const QString q = query.trimmed();
    if (q.isEmpty())
        return {};

    // Clamp top_k. Anthropic's recommended band is 3-5; we let callers ask
    // up to 20 for power-user / agent-orchestration cases.
    top_k = std::clamp(top_k, 1, 20);

    QMutexLocker lock(&mutex_);

    // Lazy index rebuild on generation change. Cheap check (one quint64 cmp).
    const quint64 cur_gen = McpProvider::instance().generation();
    if (cur_gen != indexed_generation_)
        rebuild_index_locked();

    if (docs_.empty())
        return {};

    QStringList q_tokens = tokenise(q);
    if (q_tokens.isEmpty())
        return {};

    // ── Query intent detection (before synonym expansion) ──────────────
    // Look for read-style verbs vs mutate-style verbs in the *raw* query
    // tokens. If the user clearly wants a read action ("show me X", "list
    // X", "find X") and never says delete/place/cancel/etc, we'll halve
    // the score of every destructive tool below — this stops things like
    // `delete_watchlist` outranking `add_to_watchlist` on "show watchlist".
    bool has_read_verb = false;
    bool has_mutate_verb = false;
    for (const auto& tok : q_tokens) {
        if (read_verbs().contains(tok))   has_read_verb = true;
        if (mutate_verbs().contains(tok)) has_mutate_verb = true;
    }
    const bool penalize_destructive = has_read_verb && !has_mutate_verb;

    // ── Semantic Category Classifier ───────────────────────────────────
    QStringList q_stems;
    q_stems.reserve(q_tokens.size());
    for (const auto& tok : q_tokens) {
        q_stems.append(stem(tok));
    }
    QString target_category = classify_category(q_stems);

    // ── Specialist-category gating ─────────────────────────────────────
    // For each specialist category, decide whether ANY query token is a
    // genuine cue (e.g. "quant"/"backtest" for quant-lab). Categories with
    // no cue get demoted at scoring time. This stops the 100 quant_* tools
    // from drowning out a plain "compute Sharpe ratio" query.
    QSet<QString> demoted_categories;
    {
        const QSet<QString> q_set(q_tokens.begin(), q_tokens.end());
        for (auto it = specialist_categories().constBegin();
             it != specialist_categories().constEnd(); ++it) {
            bool has_cue = false;
            for (const auto& cue : it.value()) {
                if (q_set.contains(cue)) { has_cue = true; break; }
            }
            if (!has_cue)
                demoted_categories.insert(it.key());
        }
    }

    // ── Query expansion via stemmed synonyms ───────────────────────────
    QStringList expanded_stems;
    {
        const auto& syn = query_synonyms();
        for (const auto& tok : q_tokens) {
            QString s_tok = stem(tok);
            if (!expanded_stems.contains(s_tok))
                expanded_stems.append(s_tok);

            auto it = syn.constFind(tok);
            if (it != syn.constEnd()) {
                for (const auto& s : it.value()) {
                    QString s_syn = stem(s);
                    if (!expanded_stems.contains(s_syn))
                        expanded_stems.append(s_syn);
                }
            }
        }
    }

    // ── Fuzzy spelling correction for unmatched stems ──────────────────
    QHash<QString, double> query_term_weights; // term_stem -> weight
    QStringList final_query_stems;

    for (const auto& term : expanded_stems) {
        if (df_.contains(term)) {
            query_term_weights[term] = 1.0;
            if (!final_query_stems.contains(term))
                final_query_stems.append(term);
        } else {
            // Find closest candidate term in vocabulary using Levenshtein distance
            QString best_match;
            int best_dist = 999;
            for (const auto& vocab_term : vocabulary_) {
                if (std::abs(vocab_term.length() - term.length()) > 2)
                    continue;
                int dist = levenshtein_distance(term, vocab_term);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_match = vocab_term;
                } else if (dist == best_dist && !best_match.isEmpty()) {
                    // Stable tie-breaker: prefer shorter length, then lexicographical order
                    if (vocab_term.length() < best_match.length()) {
                        best_match = vocab_term;
                    } else if (vocab_term.length() == best_match.length()) {
                        if (vocab_term < best_match) {
                            best_match = vocab_term;
                        }
                    }
                }
            }
            // Accept if distance is 1 (or 2 for words of 5 or more chars)
            if (best_dist == 1 || (best_dist == 2 && term.length() >= 5)) {
                query_term_weights[best_match] = 0.75;
                if (!final_query_stems.contains(best_match))
                    final_query_stems.append(best_match);
            }
        }
    }

    // ── Inverted Index retrieval ───────────────────────────────────────
    // Fast O(M) lookup to score only documents containing at least one query term/stem
    QSet<int> candidate_doc_indices;
    for (const auto& term : final_query_stems) {
        auto it = inverted_index_.constFind(term);
        if (it != inverted_index_.constEnd()) {
            for (int idx : it.value()) {
                candidate_doc_indices.insert(idx);
            }
        }
    }

    const double N = static_cast<double>(docs_.size());

    struct Scored { int doc_idx; double score; };
    std::vector<Scored> scored;
    scored.reserve(candidate_doc_indices.size());

    auto& provider = McpProvider::instance();

    for (int doc_idx : candidate_doc_indices) {
        const Doc& d = docs_[doc_idx];

        if (!category_filter.isEmpty() && d.category != category_filter)
            continue;
        if (!provider.is_tool_enabled(d.name))
            continue;

        double s = 0.0;
        int name_hits = 0;
        for (const auto& q_term : final_query_stems) {
            const int df = df_.value(q_term, 0);
            if (df == 0)
                continue;

            // O(1) pre-computed term-frequency lookup
            int tf = d.term_tf.value(q_term, 0);
            if (tf == 0)
                continue;

            const double idf = std::log(((N - df + 0.5) / (df + 0.5)) + 1.0);
            const double norm = 1.0 - kB + kB * (static_cast<double>(d.length) / avg_doc_length_);
            const double tf_part = (tf * (kK1 + 1.0)) / (tf + kK1 * norm);
            double term_weight = query_term_weights.value(q_term, 1.0);
            s += idf * tf_part * term_weight;

            // Exact stemmed name match bonus
            if (d.name_stems.contains(q_term))
                ++name_hits;
        }

        if (s <= 0.0)
            continue;

        // ── Contiguous Phrase Match Bonus ──────────────────────────────
        double phrase_bonus = 1.0;
        QString raw_q = q.toLower();
        if (d.raw_text.contains(raw_q)) {
            phrase_bonus = 2.0; // Significant bonus for perfect phrase matching
        } else {
            // Check if at least 2 words appear contiguously
            if (q_tokens.size() >= 2) {
                for (int j = 0; j < q_tokens.size() - 1; ++j) {
                    QString bigram = q_tokens[j] + " " + q_tokens[j+1];
                    if (d.raw_text.contains(bigram)) {
                        phrase_bonus = std::max(phrase_bonus, 1.5);
                    }
                }
            }
        }
        s *= phrase_bonus;

        // ── Adjustments (applied after phrase bonus) ──────────────────
        if (name_hits > 0)
            s += kNameMatchBonus * name_hits;
        if (penalize_destructive && d.is_destructive)
            s *= kDestructivePenalty;
        if (demoted_categories.contains(d.category))
            s *= kSpecialistPenalty;

        // ── Semantic Category Pre-routing Boost ───────────────────────
        if (!target_category.isEmpty() && d.category == target_category) {
            s *= 1.8;
        }

        scored.push_back({doc_idx, s});
    }

    // Top-K via partial sort — O(N log K) instead of O(N log N).
    if (static_cast<int>(scored.size()) > top_k) {
        std::partial_sort(scored.begin(), scored.begin() + top_k, scored.end(),
                          [](const Scored& a, const Scored& b) { return a.score > b.score; });
        scored.resize(top_k);
    } else {
        std::sort(scored.begin(), scored.end(),
                  [](const Scored& a, const Scored& b) { return a.score > b.score; });
    }

    // ── Build result ───────────────────────────────────────────────────
    std::vector<ToolMatch> out;
    out.reserve(scored.size());
    for (const auto& s : scored) {
        const Doc& d = docs_[s.doc_idx];
        ToolMatch m;
        m.name = d.name;
        m.category = d.category;
        // Truncate long descriptions to keep tool_list payload small. Full
        // schema is a tool_describe() call away.
        m.description = d.description.left(200);
        if (d.description.size() > 200)
            m.description += QStringLiteral("…");
        m.is_destructive = d.is_destructive;
        m.score = s.score;
        out.push_back(std::move(m));
    }

    LOG_INFO(TAG, QString("Semantic-Enhanced search: query=\"%1\" → %2 results (cap=%3)")
                      .arg(q.left(80))
                      .arg(out.size())
                      .arg(top_k));
    return out;
}

} // namespace fincept::mcp
