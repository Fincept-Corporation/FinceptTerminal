// ToolRetriever.cpp — Okapi BM25 over the registered MCP tool catalog.

#include "mcp/ToolRetriever.h"

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

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

// ── Index build ─────────────────────────────────────────────────────────

void ToolRetriever::rebuild_index_locked() {
    docs_.clear();
    df_.clear();
    avg_doc_length_ = 0.0;

    // list_all_tools() returns disabled tools too — we want them in the
    // index so we can mark them "disabled" if needed, but for v1 we exclude
    // them at search time so they never surface to the LLM.
    const auto tool_list = McpProvider::instance().list_all_tools();
    docs_.reserve(tool_list.size());

    for (const auto& t : tool_list) {
        Doc d;
        d.name = t.name;
        d.category = t.category;
        d.description = t.description;
        d.is_destructive = t.is_destructive;

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
            d.name_token_set.insert(nt);

        const QJsonObject props = t.input_schema["properties"].toObject();
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

        d.all_tokens = std::move(tokens);
        d.length = d.all_tokens.size();

        // Update DF — count once per term per document.
        QSet<QString> seen;
        for (const auto& tok : d.all_tokens) {
            if (seen.contains(tok))
                continue;
            seen.insert(tok);
            df_[tok] = df_.value(tok, 0) + 1;
        }

        avg_doc_length_ += d.length;
        docs_.push_back(std::move(d));
    }

    if (!docs_.empty())
        avg_doc_length_ /= static_cast<double>(docs_.size());

    indexed_generation_ = McpProvider::instance().generation();

    LOG_INFO(TAG, QString("BM25 index built: %1 docs, %2 unique terms, avg_len=%3")
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

    // ── Query expansion via synonyms ───────────────────────────────────
    // Append synonym tokens AFTER the BM25 vocabulary has been used to
    // measure IDF, so synonym hits contribute to scoring without inflating
    // term frequencies in the index. The original token stays in q_tokens.
    {
        QStringList expanded;
        expanded.reserve(q_tokens.size() * 2);
        const auto& syn = query_synonyms();
        for (const auto& tok : q_tokens) {
            expanded.append(tok);
            auto it = syn.constFind(tok);
            if (it != syn.constEnd()) {
                for (const auto& s : it.value()) {
                    if (!expanded.contains(s))
                        expanded.append(s);
                }
            }
        }
        q_tokens = std::move(expanded);
    }

    const double N = static_cast<double>(docs_.size());

    // ── BM25 scoring loop ──────────────────────────────────────────────
    // For each tool doc, score = sum over query terms q of:
    //   IDF(q) * (TF(q,D) * (k1+1)) / (TF(q,D) + k1 * (1 - b + b * |D|/avgdl))
    //
    // IDF uses the Robertson-Spärck Jones formulation with the +1 floor
    // ("idf = log((N - df + 0.5) / (df + 0.5) + 1)") — keeps IDF non-negative
    // even for very common terms, which matters when the catalog is small.

    struct Scored { int doc_idx; double score; };
    std::vector<Scored> scored;
    scored.reserve(docs_.size());

    // Also disabled-tool exclusion: rebuild_index_locked includes ALL tools
    // (enabled + disabled) so the index doesn't churn whenever a tool is
    // toggled. We filter at search time instead.
    auto& provider = McpProvider::instance();

    for (std::size_t i = 0; i < docs_.size(); ++i) {
        const Doc& d = docs_[i];

        if (!category_filter.isEmpty() && d.category != category_filter)
            continue;
        if (!provider.is_tool_enabled(d.name))
            continue;

        double s = 0.0;
        int name_hits = 0;
        for (const auto& q_term : q_tokens) {
            const int df = df_.value(q_term, 0);
            if (df == 0)
                continue;

            // TF: count occurrences of q_term in this doc's token stream.
            // The token stream is small (typically <300 tokens) so a linear
            // scan beats hashing-per-term overhead.
            int tf = 0;
            for (const auto& tok : d.all_tokens) {
                if (tok == q_term)
                    ++tf;
            }
            if (tf == 0)
                continue;

            const double idf = std::log(((N - df + 0.5) / (df + 0.5)) + 1.0);
            const double norm = 1.0 - kB + kB * (static_cast<double>(d.length) / avg_doc_length_);
            const double tf_part = (tf * (kK1 + 1.0)) / (tf + kK1 * norm);
            s += idf * tf_part;

            // Exact-name bonus: query token literally appears in the tool's
            // name (after tokenisation). Counted once per query token, not
            // per TF — we already weight name tokens 3× during indexing.
            if (d.name_token_set.contains(q_term))
                ++name_hits;
        }

        if (s <= 0.0)
            continue;

        // ── Adjustments (applied after the BM25 sum) ──────────────────
        if (name_hits > 0)
            s += kNameMatchBonus * name_hits;
        if (penalize_destructive && d.is_destructive)
            s *= kDestructivePenalty;
        if (demoted_categories.contains(d.category))
            s *= kSpecialistPenalty;

        scored.push_back({static_cast<int>(i), s});
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

    LOG_INFO(TAG, QString("BM25 search: query=\"%1\" → %2 results (cap=%3)")
                      .arg(q.left(80))
                      .arg(out.size())
                      .arg(top_k));
    return out;
}

} // namespace fincept::mcp
