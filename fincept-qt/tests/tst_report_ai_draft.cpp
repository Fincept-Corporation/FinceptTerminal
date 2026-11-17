// tst_report_ai_draft.cpp — Report Builder AI drafting (RB-7).
//
// Pure half (core/report/ReportAiDraft.h): facts from stats / table / chart / market_data /
// sparkline blocks, the prompt (rules, text, every fact), the number check (invented
// numbers flagged; facts in other formats, source-text numbers, years, quarter labels,
// ordinals, variables and dates handled), the paragraph diff and partial accept, the word
// diff, titles. Session (services/report_builder/ReportAiSession) over a FAKE backend:
// prepare sends nothing, streaming, sentinels, cancel, time-out, retry with a new id,
// accept / accept part / title / summary emit exactly one ApplyResult, discard emits none,
// needs-setup sends nothing. Links ReportAiSession.cpp (+ moc) and the logger — never
// LlmService.

#include "core/report/ReportAiDraft.h"
#include "services/report_builder/ReportAiSession.h"

#include <QSignalSpy>
#include <QtTest>

#include <cmath>
#include <limits>

using fincept::report::ReportComponent;
using fincept::report::ReportDocument;
using fincept::services::AiBackendResult;
using fincept::services::AiModelInfo;
using fincept::services::AiRequest;
using fincept::services::ApplyResult;
using fincept::services::ReportAiBackend;
using fincept::services::ReportAiSession;
namespace ai = fincept::report::ai;
namespace rep = fincept::report;

namespace {

ReportComponent comp(int id, const QString& type, const QString& content = {},
                     const QMap<QString, QString>& cfg = {}) {
    ReportComponent c;
    c.id = id;
    c.type = type;
    c.content = content;
    c.config = cfg;
    return c;
}

const QString kTargetText = QStringLiteral("Apple trades at a premium. Revenue grew 6% in 2023.");

ReportDocument sample_doc(bool with_cover = true) {
    ReportDocument d;
    d.metadata.title = QStringLiteral("Apple review");
    if (with_cover)
        d.components << comp(1, QStringLiteral("cover"), {}, {{QStringLiteral("title"), QStringLiteral("Apple")}});
    d.components << comp(2, QStringLiteral("heading"), QStringLiteral("Valuation"), {{QStringLiteral("level"), QStringLiteral("1")}});
    d.components << comp(3, QStringLiteral("stats_block"), {},
                         {{QStringLiteral("title"), QStringLiteral("AAPL key statistics")},
                          {QStringLiteral("data"), QStringLiteral("Market cap: 2.95T USD\nP/E (trailing): 28.41\n"
                                                                  "Dividend yield: 0.55%\nRevenue growth (YoY): 12.30%")},
                          {QStringLiteral("source_label"), QStringLiteral("Yahoo Finance")},
                          {QStringLiteral("as_of"), QStringLiteral("2026-11-17T13:02:00Z")},
                          {QStringLiteral("bind"), rep::info_binding(QStringLiteral("AAPL")).to_string()}});
    d.components << comp(4, QStringLiteral("text"), kTargetText);
    d.components << comp(5, QStringLiteral("table"), {},
                         {{QStringLiteral("title"), QStringLiteral("Income statement")},
                          {QStringLiteral("csv"),
                           rep::table::encode({{"Line item", "FY2023", "FY2024"},
                                               {"Total Revenue", "383,285", "391,035"},
                                               {"Net Income", "96,995", "93,736"},
                                               {"Notes", "n/a", QStringLiteral("—")}})},
                          {QStringLiteral("note"), QStringLiteral("Values in millions of the reporting currency.")}});
    d.components << comp(6, QStringLiteral("heading"), QStringLiteral("Market"), {{QStringLiteral("level"), QStringLiteral("1")}});
    d.components << comp(7, QStringLiteral("market_data"), {},
                         {{QStringLiteral("symbol"), QStringLiteral("AAPL")},
                          {QStringLiteral("name"), QStringLiteral("Apple Inc.")},
                          {QStringLiteral("price"), QStringLiteral("189.50")},
                          {QStringLiteral("change"), QStringLiteral("1.23")},
                          {QStringLiteral("change_pct"), QStringLiteral("0.65")},
                          {QStringLiteral("high"), QStringLiteral("190.10")},
                          {QStringLiteral("low"), QStringLiteral("187.20")},
                          {QStringLiteral("volume"), QStringLiteral("52123456")},
                          {QStringLiteral("currency"), QStringLiteral("USD")}});
    d.components << comp(8, QStringLiteral("chart"), {},
                         {{QStringLiteral("title"), QStringLiteral("AAPL 6M")},
                          {QStringLiteral("labels"), QStringLiteral("1 Jun,1 Jul,1 Aug,1 Sep")},
                          {QStringLiteral("data"), QStringLiteral("150,160,145,180")},
                          {QStringLiteral("chart_type"), QStringLiteral("line")}});
    d.components << comp(9, QStringLiteral("chart"), {},
                         {{QStringLiteral("title"), QStringLiteral("Sample")},
                          {QStringLiteral("data"), QStringLiteral("1,2,3")},
                          {QStringLiteral("sample"), QStringLiteral("1")}});
    d.components << comp(10, QStringLiteral("sparkline"), {},
                         {{QStringLiteral("title"), QStringLiteral("MSFT")},
                          {QStringLiteral("current"), QStringLiteral("410.20")},
                          {QStringLiteral("change_pct"), QStringLiteral("+1.5")}});
    d.components << comp(11, QStringLiteral("text"), QStringLiteral("Closing remarks."));
    d.next_id = 12;
    return d;
}

ai::Fact fact(const QString& id, const QString& label, const QString& value,
              double number = std::numeric_limits<double>::quiet_NaN(), const QString& unit = {},
              const QString& as_of = {}) {
    ai::Fact f;
    f.id = id;
    f.label = label;
    f.value = value;
    f.number = number;
    f.unit = unit;
    f.source = QStringLiteral("Test");
    f.as_of = as_of;
    return f;
}

QStringList flag_texts(const QVector<ai::NumberFlag>& flags) {
    QStringList out;
    for (const ai::NumberFlag& f : flags)
        out << f.text;
    return out;
}

const ai::Fact* find_fact(const QVector<ai::Fact>& facts, const QString& label_part) {
    for (const ai::Fact& f : facts)
        if (f.label.contains(label_part))
            return &f;
    return nullptr;
}

/// A backend that records requests; the test drives chunks and results by hand.
class FakeBackend : public ReportAiBackend {
  public:
    struct Call {
        quint64 id = 0;
        QString system;
        QString user;
        std::function<void(quint64, const QString&)> on_chunk;
        std::function<void(quint64, const AiBackendResult&)> on_done;
    };
    QVector<Call> calls;
    QVector<quint64> cancelled;
    AiModelInfo info{QStringLiteral("Test model"), QStringLiteral("0 credits · billed to your Test API key"), false, {}};

    void start(quint64 id, const QString& system, const QString& user,
               std::function<void(quint64, const QString&)> on_chunk,
               std::function<void(quint64, const AiBackendResult&)> on_done) override {
        calls.append({id, system, user, std::move(on_chunk), std::move(on_done)});
    }
    void cancel(quint64 id) override { cancelled.append(id); }
    AiModelInfo model_info() const override { return info; }

    void chunk(int call, const QString& text) { calls[call].on_chunk(calls[call].id, text); }
    void finish(int call, const QString& text) {
        AiBackendResult r;
        r.ok = true;
        r.text = text;
        r.route_label = QStringLiteral("Test model");
        calls[call].on_done(calls[call].id, r);
    }
    void fail(int call, const QString& error) {
        AiBackendResult r;
        r.error = error;
        calls[call].on_done(calls[call].id, r);
    }
};

AiRequest block_request(ai::Action a, const QString& text = kTargetText) {
    AiRequest r;
    r.action = a;
    r.block_id = 4;
    r.block_text = text;
    r.doc = sample_doc();
    r.doc.components[r.doc.index_of(4)].content = text;
    return r;
}

/// Prepare + send + one chunk + finish: the session ends in Proposal (or Failed).
void run_to_proposal(ReportAiSession& s, FakeBackend& fb, const AiRequest& req, const QString& reply) {
    s.prepare(req);
    QVERIFY(s.send());
    fb.chunk(static_cast<int>(fb.calls.size()) - 1, reply);
    fb.finish(static_cast<int>(fb.calls.size()) - 1, reply);
}

} // namespace

class TestReportAiDraft : public QObject {
    Q_OBJECT

  private slots:
    // ── Pure: facts ──
    void facts_from_blocks();
    void facts_cap_and_document_order();
    // ── Pure: prompt ──
    void prompt_carries_rules_text_and_facts();
    void prompt_for_report_actions();
    // ── Pure: number check ──
    void invented_numbers_flagged();
    void fact_numbers_in_other_formats_accepted();
    void source_text_numbers_accepted();
    void years_and_fiscal_labels();
    void quarter_labels_ordinals_variables_excused();
    void dates_checked_as_a_whole();
    // ── Pure: diff, titles ──
    void diff_and_partial_apply();
    void diff_delete_and_merge();
    void word_diff();
    void titles_parsed();
    // ── Session ──
    void prepare_sends_nothing();
    void send_streams_and_drops_sentinels();
    void cancel_drops_late_chunks();
    void timeout_fails_and_cancels();
    void retry_uses_new_id();
    void accept_all_emits_once();
    void accept_hunks_partial();
    void selection_splice();
    void use_title_and_summary();
    void discard_emits_nothing();
    void needs_setup_sends_nothing();
    void too_long_and_empty_and_no_change();
};

// ── Facts ───────────────────────────────────────────────────────────────────

void TestReportAiDraft::facts_from_blocks() {
    const ReportDocument doc = sample_doc();
    const QVector<ai::Fact> facts = ai::collect_facts(doc, 4);
    // stats 4 + table 4 (the "Notes" row has no numbers) + quote 6 + chart 5 + sparkline 2;
    // the sample chart gives none.
    QCOMPARE(facts.size(), 21);
    for (int i = 0; i < facts.size(); ++i)
        QCOMPARE(facts[i].id, QStringLiteral("F%1").arg(i + 1));

    // Nearest first: the stats block above in the same section, then the table below it,
    // then the other section by distance (quote, chart, sparkline).
    QCOMPARE(facts.first().block_id, 3);
    QCOMPARE(facts[4].block_id, 5);
    QCOMPARE(facts[8].block_id, 7);
    QCOMPARE(facts[14].block_id, 8);
    QCOMPARE(facts.last().block_id, 10);
    for (const ai::Fact& f : facts)
        QVERIFY(f.block_id != 9 && f.block_id != 4);

    const ai::Fact* pe = find_fact(facts, QStringLiteral("P/E (trailing)"));
    QVERIFY(pe);
    QCOMPARE(pe->value, QStringLiteral("28.41"));
    QCOMPARE(pe->number, 28.41);
    QVERIFY(pe->label.startsWith(QStringLiteral("AAPL key statistics")));
    QVERIFY(pe->source.contains(QStringLiteral("Yahoo Finance")));
    QVERIFY(pe->as_of.contains(QStringLiteral("2026")));
    const ai::Fact* yield = find_fact(facts, QStringLiteral("Dividend yield"));
    QVERIFY(yield);
    QCOMPARE(yield->unit, QStringLiteral("%"));

    const ai::Fact* rev = find_fact(facts, QStringLiteral("Total Revenue — FY2024"));
    QVERIFY(rev);
    QCOMPARE(rev->value, QStringLiteral("391,035"));
    QCOMPARE(rev->number, 391035.0);
    QCOMPARE(rev->unit, QStringLiteral("millions"));
    QVERIFY(rev->label.startsWith(QStringLiteral("Income statement")));
    QVERIFY(!find_fact(facts, QStringLiteral("Notes")));

    const ai::Fact* price = find_fact(facts, QStringLiteral("AAPL (Apple Inc.) price"));
    QVERIFY(price);
    QCOMPARE(price->value, QStringLiteral("189.50 USD"));
    QCOMPARE(price->number, 189.5);
    const ai::Fact* chg = find_fact(facts, QStringLiteral("AAPL (Apple Inc.) change %"));
    QVERIFY(chg);
    QCOMPARE(chg->value, QStringLiteral("+0.65%"));
    QVERIFY(find_fact(facts, QStringLiteral("volume")));

    const ai::Fact* first = find_fact(facts, QStringLiteral("first value (1 Jun)"));
    QVERIFY(first);
    QCOMPARE(first->number, 150.0);
    const ai::Fact* low = find_fact(facts, QStringLiteral("low (1 Aug)"));
    QVERIFY(low);
    QCOMPARE(low->number, 145.0);
    const ai::Fact* period = find_fact(facts, QStringLiteral("change over the period (1 Jun to 1 Sep)"));
    QVERIFY(period);
    QCOMPARE(period->value, QStringLiteral("+20.00%"));

    const ai::Fact* spark = find_fact(facts, QStringLiteral("MSFT change %"));
    QVERIFY(spark);
    QCOMPARE(spark->value, QStringLiteral("+1.5%"));
    QVERIFY(find_fact(facts, QStringLiteral("MSFT current")));
}

void TestReportAiDraft::facts_cap_and_document_order() {
    const ReportDocument doc = sample_doc();
    QCOMPARE(ai::collect_facts(doc, 4, 3).size(), 3);
    QCOMPARE(ai::collect_facts(doc, 4, 0).size(), 0);
    // No target: document order.
    const QVector<ai::Fact> all = ai::collect_facts(doc, 0);
    QCOMPARE(all.size(), 21);
    QCOMPARE(all.first().block_id, 3);
    QCOMPARE(all.last().block_id, 10);
    // Target in the second section: the quote (above, same section) comes first.
    QCOMPARE(ai::collect_facts(doc, 11).first().block_id, 10);
}

// ── Prompt ──────────────────────────────────────────────────────────────────

void TestReportAiDraft::prompt_carries_rules_text_and_facts() {
    const ReportDocument doc = sample_doc();
    const QVector<ai::Fact> facts = ai::collect_facts(doc, 4);
    const ai::Prompt p = ai::build_prompt(ai::Action::Rewrite, {}, kTargetText, facts, doc, 4);
    QVERIFY(p.system.contains(QStringLiteral("ONLY numbers")));
    QVERIFY(p.system.contains(QStringLiteral("[n/a]")));
    QVERIFY(p.system.contains(QStringLiteral("{{variable}}")));
    QVERIFY(p.system.contains(QStringLiteral("investment advice")));
    QVERIFY(p.system.contains(QStringLiteral("finished text only")));
    QVERIFY(p.user.contains(kTargetText));
    QVERIFY(p.user.contains(QStringLiteral("SECTION HEADING: Valuation")));
    QCOMPARE(p.heading, QStringLiteral("Valuation"));
    QCOMPARE(p.fact_count, 21);
    QCOMPARE(p.text_chars, static_cast<int>(kTargetText.size()));
    for (const ai::Fact& f : facts) {
        QVERIFY2(p.user.contains(f.id + QStringLiteral(" | ") + f.label + QStringLiteral(" | ") + f.value),
                 qPrintable(f.id));
    }
    QVERIFY(p.user.contains(QStringLiteral("F1 | AAPL key statistics — Market cap | 2.95T USD | Yahoo Finance")));

    const ai::Prompt tone = ai::build_prompt(ai::Action::ChangeTone, QStringLiteral("confident"), kTargetText, {}, doc, 4);
    QVERIFY(tone.user.contains(QStringLiteral("confident tone")));
    QVERIFY(tone.user.contains(QStringLiteral("FACTS: none")));
    QCOMPARE(tone.fact_count, 0);

    const ai::Prompt sel = ai::build_prompt(ai::Action::Shorten, {}, QStringLiteral("Apple trades"), {}, doc, 4, true);
    QVERIFY(sel.user.contains(QStringLiteral("passage")));

    const ai::Prompt draft = ai::build_prompt(ai::Action::DraftFromData, {}, {}, facts, doc, 4);
    QVERIFY(!draft.user.contains(QStringLiteral("BEGIN TEXT")));
    QVERIFY(draft.user.contains(QStringLiteral("from the FACTS")));
    QCOMPARE(draft.text_chars, 0);
}

void TestReportAiDraft::prompt_for_report_actions() {
    const ReportDocument doc = sample_doc();
    const QVector<ai::Fact> facts = ai::collect_facts(doc, 0);
    const ai::Prompt sum = ai::build_prompt(ai::Action::SummariseReport, {}, {}, facts, doc);
    QVERIFY(sum.user.contains(QStringLiteral("OUTLINE:")));
    QVERIFY(sum.user.contains(QStringLiteral("Valuation")));
    QVERIFY(sum.user.contains(QStringLiteral("Market")));
    QVERIFY(sum.user.contains(QStringLiteral("[Valuation] ") + kTargetText));
    QVERIFY(sum.user.contains(QStringLiteral("Title: Apple review")));
    QVERIFY(sum.text_chars > 0);
    QVERIFY(sum.text_chars <= 6000);
    QVERIFY(sum.user.contains(QStringLiteral("executive summary")));

    const ai::Prompt titles = ai::build_prompt(ai::Action::SuggestTitle, {}, {}, facts, doc);
    QVERIFY(titles.user.contains(QStringLiteral("exactly 3 titles")));
    QVERIFY(titles.user.contains(QStringLiteral("one per line")));

    // The excerpt is capped.
    ReportDocument big = doc;
    for (int i = 0; i < 40; ++i)
        big.components << comp(100 + i, QStringLiteral("text"), QString(500, QLatin1Char('x')));
    QVERIFY(ai::report_excerpt(big).size() <= 6000);
}

// ── Number check ────────────────────────────────────────────────────────────

void TestReportAiDraft::invented_numbers_flagged() {
    const ReportDocument doc = sample_doc();
    const QVector<ai::Fact> facts = ai::collect_facts(doc, 4);
    const QString draft = QStringLiteral("Revenue reached $412.7 billion while the P/E stayed at 28.41x and margins "
                                         "were 52.3%.");
    const QVector<ai::NumberFlag> flags = ai::check_numbers(draft, facts, kTargetText);
    QCOMPARE(flag_texts(flags), (QStringList{QStringLiteral("$412.7 billion"), QStringLiteral("52.3%")}));
    for (const ai::NumberFlag& f : flags) {
        QCOMPARE(draft.mid(f.start, f.length), f.text);
        QCOMPARE(f.reason, QStringLiteral("not in the report's data"));
    }
    // Rounded table values at another scale are fine; an unknown one is not.
    QVERIFY(ai::check_numbers(QStringLiteral("Revenue of $391.0 billion and net income of $93.7bn."), facts, {}).isEmpty());
    QCOMPARE(flag_texts(ai::check_numbers(QStringLiteral("Net income was $95.2bn."), facts, {})),
             QStringList{QStringLiteral("$95.2bn")});
}

void TestReportAiDraft::fact_numbers_in_other_formats_accepted() {
    const QVector<ai::Fact> facts = {
        fact(QStringLiteral("F1"), QStringLiteral("Revenue"), QStringLiteral("1,200,000,000"), 1.2e9),
        fact(QStringLiteral("F2"), QStringLiteral("Margin"), QStringLiteral("12.30%"), 12.3, QStringLiteral("%")),
        fact(QStringLiteral("F3"), QStringLiteral("Price"), QStringLiteral("1234.5"), 1234.5),
        fact(QStringLiteral("F4"), QStringLiteral("Yield"), QStringLiteral("0.034"), 0.034),
        fact(QStringLiteral("F5"), QStringLiteral("Net change"), QStringLiteral("-1,234"), -1234),
        fact(QStringLiteral("F6"), QStringLiteral("EV/EBITDA"), QStringLiteral("14.2x"), 14.2),
    };
    const QString draft = QStringLiteral("Revenue of $1.2B (also $1.2 billion, 1.2bn and 1,200 million), a 12.3% margin, "
                                         "a price of 1,234.5 (about 1,235), a 3.4% yield, a net change of (1,234) "
                                         "and 14.2x EV/EBITDA.");
    const QVector<ai::NumberFlag> flags = ai::check_numbers(draft, facts, {});
    QVERIFY2(flags.isEmpty(), qPrintable(flag_texts(flags).join(QStringLiteral(" / "))));
    // Rounding is only tolerated to the decimals the draft printed.
    QCOMPARE(flag_texts(ai::check_numbers(QStringLiteral("a 12.4% margin and $1.3B revenue"), facts, {})),
             (QStringList{QStringLiteral("12.4%"), QStringLiteral("$1.3B")}));
}

void TestReportAiDraft::source_text_numbers_accepted() {
    // Rewrite: the original text's own numbers may stay.
    QVERIFY(ai::check_numbers(QStringLiteral("In 2023, revenue grew 6%."), {}, kTargetText).isEmpty());
    QCOMPARE(flag_texts(ai::check_numbers(QStringLiteral("In 2023, revenue grew 7%."), {}, kTargetText)),
             QStringList{QStringLiteral("7%")});
}

void TestReportAiDraft::years_and_fiscal_labels() {
    const ReportDocument doc = sample_doc();
    const QVector<ai::Fact> facts = ai::collect_facts(doc, 4); // the table has FY2023 / FY2024
    QVERIFY(ai::check_numbers(QStringLiteral("In FY2024 revenue rose; FY24 and 2024 too."), facts, {}).isEmpty());
    QCOMPARE(flag_texts(ai::check_numbers(QStringLiteral("Unlike FY2019 and 2019, FY2024 was strong."), facts, {})),
             (QStringList{QStringLiteral("FY2019"), QStringLiteral("2019")}));
    // A fiscal year from the source text is fine.
    QVERIFY(ai::check_numbers(QStringLiteral("FY2023 was weaker."), {}, kTargetText).isEmpty());
    QCOMPARE(flag_texts(ai::check_numbers(QStringLiteral("CY2025 will tell."), {}, kTargetText)),
             QStringList{QStringLiteral("CY2025")});
}

void TestReportAiDraft::quarter_labels_ordinals_variables_excused() {
    QVERIFY(ai::check_numbers(QStringLiteral("Q3 and H1 results improved, 3Q24 too; Q4 and H2 follow."), {}, {}).isEmpty());
    QVERIFY(ai::check_numbers(QStringLiteral("1. Growth\n2) Margins\n  3. Outlook"), {}, {}).isEmpty());
    QVERIFY(ai::check_numbers(QStringLiteral("Target {{fy2024_target}} for {{q3 period}}."), {}, {}).isEmpty());
    // Mid-line "1." is a number, not list numbering.
    QCOMPARE(flag_texts(ai::check_numbers(QStringLiteral("Growth was 4. Margins held."), {}, {})),
             QStringList{QStringLiteral("4")});
}

void TestReportAiDraft::dates_checked_as_a_whole() {
    const QVector<ai::Fact> facts = {fact(QStringLiteral("F1"), QStringLiteral("Price"), QStringLiteral("189.50"),
                                          189.5, {}, QStringLiteral("17 Nov 2026 14:02"))};
    QVERIFY(ai::check_numbers(QStringLiteral("As of 17 November 2026 at 14:02 the price was $189.50 (Nov 17, 2026)."),
                              facts, {})
                .isEmpty());
    QVERIFY(ai::check_numbers(QStringLiteral("On 17 November the stock closed in 2026."), facts, {}).isEmpty());
    // The as-of time never excuses a stray small number, and another date is flagged.
    QCOMPARE(flag_texts(ai::check_numbers(QStringLiteral("A 2% rise on 18 November 2026; 17 analysts agree."),
                                          facts, {})),
             (QStringList{QStringLiteral("2%"), QStringLiteral("18 November 2026"), QStringLiteral("17")}));
}

// ── Diff, titles ────────────────────────────────────────────────────────────

void TestReportAiDraft::diff_and_partial_apply() {
    const QString old_text = QStringLiteral("Alpha.\n\nBeta.\n\nGamma.");
    const QString new_text = QStringLiteral("Alpha.\n\nBeta two.\n\nGamma.\n\nDelta.");
    const QVector<ai::Hunk> h = ai::diff_paragraphs(old_text, new_text);
    QCOMPARE(h.size(), 4);
    QCOMPARE(h[0].kind, ai::Hunk::Kind::Same);
    QCOMPARE(h[1].kind, ai::Hunk::Kind::Replace);
    QCOMPARE(h[1].old_text, QStringLiteral("Beta."));
    QCOMPARE(h[1].new_text, QStringLiteral("Beta two."));
    QCOMPARE(new_text.mid(h[1].new_start, h[1].new_text.size()), h[1].new_text);
    QCOMPARE(h[2].kind, ai::Hunk::Kind::Same);
    QCOMPARE(h[3].kind, ai::Hunk::Kind::Insert);
    QCOMPARE(new_text.mid(h[3].new_start, h[3].new_text.size()), QStringLiteral("Delta."));
    QCOMPARE(ai::change_hunks(h), (QVector<int>{1, 3}));

    QCOMPARE(ai::apply_hunks(h, {}), old_text);
    QCOMPARE(ai::apply_hunks(h, {1}), QStringLiteral("Alpha.\n\nBeta two.\n\nGamma."));
    QCOMPARE(ai::apply_hunks(h, {3}), QStringLiteral("Alpha.\n\nBeta.\n\nGamma.\n\nDelta."));
    QCOMPARE(ai::apply_hunks(h, {1, 3}), new_text);
    QCOMPARE(ai::apply_hunks(h, {0, 2}), old_text); // accepting a Same hunk changes nothing
}

void TestReportAiDraft::diff_delete_and_merge() {
    const QVector<ai::Hunk> del = ai::diff_paragraphs(QStringLiteral("A\n\nB"), QStringLiteral("A"));
    QCOMPARE(del.size(), 2);
    QCOMPARE(del[1].kind, ai::Hunk::Kind::Delete);
    QCOMPARE(ai::apply_hunks(del, {}), QStringLiteral("A\n\nB"));
    QCOMPARE(ai::apply_hunks(del, {1}), QStringLiteral("A"));

    const QVector<ai::Hunk> merged = ai::diff_paragraphs(QStringLiteral("A\n\nB\n\nC"), QStringLiteral("A\n\n\nB\n\nX"));
    QCOMPARE(merged.size(), 2);
    QCOMPARE(merged[0].kind, ai::Hunk::Kind::Same);
    QCOMPARE(merged[0].old_text, QStringLiteral("A\n\nB"));
    QCOMPARE(merged[1].kind, ai::Hunk::Kind::Replace);

    // From nothing (an executive summary): every paragraph is an insertion.
    const QVector<ai::Hunk> ins = ai::diff_paragraphs(QString(), QStringLiteral("One.\n\nTwo."));
    QCOMPARE(ins.size(), 2);
    QCOMPARE(ins[0].kind, ai::Hunk::Kind::Insert);
    QCOMPARE(ins[1].kind, ai::Hunk::Kind::Insert);
}

void TestReportAiDraft::word_diff() {
    const QString a = QStringLiteral("the quick brown fox, 1,234.5 units");
    const QString b = QStringLiteral("the slow brown fox, 1,240.0 units");
    const QVector<ai::WordOp> ops = ai::diff_words(a, b);
    QVERIFY(!ops.isEmpty());
    QString old_side, new_side;
    int deletions = 0, insertions = 0;
    for (const ai::WordOp& op : ops) {
        if (op.kind != ai::WordOp::Kind::Insert)
            old_side += op.text;
        if (op.kind != ai::WordOp::Kind::Delete)
            new_side += op.text;
        deletions += op.kind == ai::WordOp::Kind::Delete ? 1 : 0;
        insertions += op.kind == ai::WordOp::Kind::Insert ? 1 : 0;
    }
    QCOMPARE(old_side, a);
    QCOMPARE(new_side, b);
    QCOMPARE(deletions, 2); // "quick", "1,234.5" — a number stays one token
    QCOMPARE(insertions, 2);
    QCOMPARE(ops.first().kind, ai::WordOp::Kind::Same);
    // Too long to align: empty (the caller shows both paragraphs whole).
    QVERIFY(ai::diff_words(QString(200, QLatin1Char('a')).replace(QLatin1Char('a'), QStringLiteral("a ")),
                           QString(200, QLatin1Char('b')).replace(QLatin1Char('b'), QStringLiteral("b ")), 1000)
                .isEmpty());
}

void TestReportAiDraft::titles_parsed() {
    const QStringList t = ai::parse_titles(
        QStringLiteral("Here are three titles:\n1. \"Alpha Rising\"\n2) **Beta Case**\n- ‘Gamma’\nDelta"));
    QCOMPARE(t, (QStringList{QStringLiteral("Alpha Rising"), QStringLiteral("Beta Case"), QStringLiteral("Gamma")}));
    QCOMPARE(ai::parse_titles(QStringLiteral("```\nOne\nTwo\n```")), (QStringList{QStringLiteral("One"), QStringLiteral("Two")}));
    QCOMPARE(ai::parse_titles(QStringLiteral("2026 Outlook: Apple")), QStringList{QStringLiteral("2026 Outlook: Apple")});
}

// ── Session ─────────────────────────────────────────────────────────────────

void TestReportAiDraft::prepare_sends_nothing() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    QSignalSpy states(&s, &ReportAiSession::state_changed);
    s.prepare(block_request(ai::Action::Rewrite));
    QCOMPARE(fb.calls.size(), 0);
    QCOMPARE(s.state(), ReportAiSession::State::Ready);
    QCOMPARE(states.count(), 1);
    QCOMPARE(s.facts().size(), 21);
    QCOMPARE(s.prompt().fact_count, 21);
    QCOMPARE(s.prompt().text_chars, static_cast<int>(kTargetText.size()));
    QVERIFY(s.prompt().user.contains(kTargetText));
    QCOMPARE(s.sent_text(), kTargetText);
    QCOMPARE(s.model_info().label, QStringLiteral("Test model"));
    QCOMPARE(fb.calls.size(), 0); // asking for the model sends nothing either
}

void TestReportAiDraft::send_streams_and_drops_sentinels() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    s.prepare(block_request(ai::Action::Rewrite));
    QSignalSpy streamed(&s, &ReportAiSession::stream_updated);
    QSignalSpy ready(&s, &ReportAiSession::proposal_ready);
    QVERIFY(s.send());
    QCOMPARE(s.state(), ReportAiSession::State::Streaming);
    QCOMPARE(fb.calls.size(), 1);
    QCOMPARE(fb.calls[0].id, s.request_id());
    QVERIFY(fb.calls[0].system.contains(QStringLiteral("ONLY numbers")));
    QVERIFY(fb.calls[0].user.contains(kTargetText));
    QVERIFY(!s.send()); // already streaming

    fb.chunk(0, QStringLiteral("Draft that "));
    fb.chunk(0, QStringLiteral("\x01__TOOL_CALL_CLEAR__"));
    QCOMPARE(s.stream_text(), QString());
    fb.chunk(0, QStringLiteral("Apple trades "));
    fb.chunk(0, QStringLiteral("\x02__THINK__considering"));
    fb.chunk(0, QStringLiteral("\x03__TOOL__Searching\n"));
    fb.chunk(0, QStringLiteral("at 28.41x earnings with 99.9% margins. Revenue grew 6% in 2023."));
    QCOMPARE(s.stream_text(), QStringLiteral("Apple trades at 28.41x earnings with 99.9% margins. Revenue grew 6% in 2023."));
    QCOMPARE(streamed.count(), 4); // three text chunks and the clear; thinking / tool chunks add none
    QCOMPARE(streamed.last().at(0).toString(), s.stream_text());
    QCOMPARE(s.state(), ReportAiSession::State::Streaming);

    fb.finish(0, s.stream_text());
    QCOMPARE(s.state(), ReportAiSession::State::Proposal);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(s.proposal(), s.stream_text());
    QCOMPARE(flag_texts(s.flags()), QStringList{QStringLiteral("99.9%")});
    QCOMPARE(s.route_label(), QStringLiteral("Test model"));
    QVERIFY(!s.hunks().isEmpty());
    QVERIFY(!s.no_change());
}

void TestReportAiDraft::cancel_drops_late_chunks() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    s.prepare(block_request(ai::Action::Rewrite));
    QVERIFY(s.send());
    fb.chunk(0, QStringLiteral("Part"));
    s.cancel();
    QCOMPARE(s.state(), ReportAiSession::State::Cancelled);
    QCOMPARE(fb.cancelled, QVector<quint64>{fb.calls[0].id});
    QVERIFY(s.error().contains(QStringLiteral("nothing was changed")));
    QSignalSpy streamed(&s, &ReportAiSession::stream_updated);
    QSignalSpy ready(&s, &ReportAiSession::proposal_ready);
    fb.chunk(0, QStringLiteral(" late"));
    fb.finish(0, QStringLiteral("Late answer."));
    QCOMPARE(streamed.count(), 0);
    QCOMPARE(ready.count(), 0);
    QCOMPARE(s.state(), ReportAiSession::State::Cancelled);
    QVERIFY(s.proposal().isEmpty());
}

void TestReportAiDraft::timeout_fails_and_cancels() {
    QCOMPARE(ReportAiSession::timeout_message(120000),
             QStringLiteral("No answer within 2 minutes — nothing was changed."));
    QCOMPARE(ReportAiSession::timeout_message(ReportAiSession::kDefaultTimeoutMs),
             QStringLiteral("No answer within 2 minutes — nothing was changed."));
    FakeBackend fb;
    ReportAiSession s(&fb);
    QCOMPARE(s.timeout_ms(), 120000);
    s.set_timeout_ms(60);
    s.prepare(block_request(ai::Action::Rewrite));
    QVERIFY(s.send());
    QTRY_COMPARE_WITH_TIMEOUT(s.state(), ReportAiSession::State::Failed, 5000);
    QCOMPARE(fb.cancelled, QVector<quint64>{fb.calls[0].id});
    QVERIFY(s.error().startsWith(QStringLiteral("No answer within")));
    QVERIFY(s.error().contains(QStringLiteral("nothing was changed")));
    fb.finish(0, QStringLiteral("Too late."));
    QCOMPARE(s.state(), ReportAiSession::State::Failed);
}

void TestReportAiDraft::retry_uses_new_id() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    s.prepare(block_request(ai::Action::Rewrite));
    QVERIFY(s.send());
    fb.fail(0, QStringLiteral("You're out of Fincept credits."));
    QCOMPARE(s.state(), ReportAiSession::State::Failed);
    QCOMPARE(s.error(), QStringLiteral("You're out of Fincept credits."));
    QVERIFY(s.retry());
    QCOMPARE(fb.calls.size(), 2);
    QVERIFY(fb.calls[1].id != fb.calls[0].id);
    QCOMPARE(fb.calls[1].user, fb.calls[0].user); // the same prompt
    QCOMPARE(s.request_id(), fb.calls[1].id);
    QVERIFY(s.error().isEmpty());

    fb.chunk(0, QStringLiteral("old"));
    QCOMPARE(s.stream_text(), QString());
    fb.chunk(1, QStringLiteral("New answer."));
    QCOMPARE(s.stream_text(), QStringLiteral("New answer."));
    fb.finish(0, QStringLiteral("old"));
    QCOMPARE(s.state(), ReportAiSession::State::Streaming);
    fb.finish(1, QStringLiteral("New answer."));
    QCOMPARE(s.state(), ReportAiSession::State::Proposal);
    QCOMPARE(s.proposal(), QStringLiteral("New answer."));
    // Retry from a proposal: again a new id.
    QVERIFY(s.retry());
    QCOMPARE(fb.calls.size(), 3);
    QVERIFY(s.proposal().isEmpty());
}

void TestReportAiDraft::accept_all_emits_once() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    QVector<ApplyResult> applied;
    QObject::connect(&s, &ReportAiSession::apply_requested, &s, [&applied](const ApplyResult& r) { applied << r; });
    const QString reply = QStringLiteral("Apple trades at 28.41x earnings. Revenue grew 6% in 2023.");
    run_to_proposal(s, fb, block_request(ai::Action::Rewrite), reply);
    QCOMPARE(s.state(), ReportAiSession::State::Proposal);
    QVERIFY(s.flags().isEmpty());
    QVERIFY(s.accept_all());
    QCOMPARE(applied.size(), 1);
    QCOMPARE(applied[0].kind, ApplyResult::Kind::ReplaceText);
    QCOMPARE(applied[0].block_id, 4);
    QCOMPARE(applied[0].new_content, reply);
    QCOMPARE(applied[0].old_content, kTargetText);
    QCOMPARE(applied[0].undo_text, QStringLiteral("AI: rewrite"));
    QCOMPARE(s.state(), ReportAiSession::State::Idle);
    QVERIFY(!s.accept_all());
    QVERIFY(!s.accept_hunks({1}));
    QCOMPARE(applied.size(), 1);
}

void TestReportAiDraft::accept_hunks_partial() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    QVector<ApplyResult> applied;
    QObject::connect(&s, &ReportAiSession::apply_requested, &s, [&applied](const ApplyResult& r) { applied << r; });
    const QString text = QStringLiteral("Alpha.\n\nBeta.\n\nGamma.");
    const QString reply = QStringLiteral("Alpha.\n\nBeta two.\n\nGamma.\n\nDelta.");
    run_to_proposal(s, fb, block_request(ai::Action::Expand, text), reply);
    QCOMPARE(s.hunks().size(), 4);
    QVERIFY(!s.accept_hunks({}));
    QVERIFY(!s.accept_hunks({0, 2})); // Same hunks only
    QCOMPARE(applied.size(), 0);
    QVERIFY(s.accept_hunks({1}));
    QCOMPARE(applied.size(), 1);
    QCOMPARE(applied[0].kind, ApplyResult::Kind::ReplaceText);
    QCOMPARE(applied[0].new_content, QStringLiteral("Alpha.\n\nBeta two.\n\nGamma."));
    QCOMPARE(applied[0].old_content, text);
    QVERIFY(!s.accept_hunks({3}));
    QCOMPARE(applied.size(), 1);

    // Every change accepted: the proposal exactly as the model wrote it.
    run_to_proposal(s, fb, block_request(ai::Action::Expand, text), reply);
    QVERIFY(s.accept_hunks({1, 3}));
    QCOMPARE(applied.size(), 2);
    QCOMPARE(applied[1].new_content, reply);
}

void TestReportAiDraft::selection_splice() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    const QString text = QStringLiteral("Hello world. Second sentence.");
    AiRequest req = block_request(ai::Action::Rewrite, text);
    req.sel_start = 0;
    req.sel_end = 12; // "Hello world."
    s.prepare(req);
    QVERIFY(s.has_selection());
    QCOMPARE(s.sent_text(), QStringLiteral("Hello world."));
    QVERIFY(s.prompt().user.contains(QStringLiteral("passage")));
    QVERIFY(!s.prompt().user.contains(QStringLiteral("Second sentence")));
    QVERIFY(s.send());
    fb.finish(0, QStringLiteral("Hi planet.\n"));
    QCOMPARE(s.proposal(), QStringLiteral("Hi planet. Second sentence."));
    QCOMPARE(s.original(), text);

    // The selection's own trailing space stays.
    req.sel_end = 13; // "Hello world. "
    s.prepare(req);
    QVERIFY(s.send());
    fb.finish(1, QStringLiteral("Hi planet."));
    QCOMPARE(s.proposal(), QStringLiteral("Hi planet. Second sentence."));

    // An invalid selection means the whole block.
    req.sel_start = 5;
    req.sel_end = 500;
    s.prepare(req);
    QVERIFY(!s.has_selection());
    QCOMPARE(s.sent_text(), text);
}

void TestReportAiDraft::use_title_and_summary() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    QVector<ApplyResult> applied;
    QObject::connect(&s, &ReportAiSession::apply_requested, &s, [&applied](const ApplyResult& r) { applied << r; });

    AiRequest titles;
    titles.action = ai::Action::SuggestTitle;
    titles.doc = sample_doc();
    run_to_proposal(s, fb, titles, QStringLiteral("1. Apple at a Premium\n2. Growth Slows\n3. Valuation Check"));
    QCOMPARE(s.state(), ReportAiSession::State::Proposal);
    QCOMPARE(s.titles(), (QStringList{QStringLiteral("Apple at a Premium"), QStringLiteral("Growth Slows"),
                                      QStringLiteral("Valuation Check")}));
    QVERIFY(s.prompt().user.contains(QStringLiteral("OUTLINE:")));
    QVERIFY(!s.accept_hunks({0}));
    QVERIFY(!s.use_title(QStringLiteral("  ")));
    QVERIFY(s.use_title(QStringLiteral("Growth  Slows")));
    QCOMPARE(applied.size(), 1);
    QCOMPARE(applied[0].kind, ApplyResult::Kind::SetTitle);
    QCOMPARE(applied[0].title, QStringLiteral("Growth Slows"));
    QVERIFY(!s.use_title(QStringLiteral("Again")));
    QCOMPARE(applied.size(), 1);

    // use_title is only for titles.
    run_to_proposal(s, fb, block_request(ai::Action::Rewrite), QStringLiteral("Apple trades at a premium."));
    QVERIFY(!s.use_title(QStringLiteral("Title")));

    // Summary: a level-1 heading and a text block, after the cover.
    AiRequest sum;
    sum.action = ai::Action::SummariseReport;
    sum.doc = sample_doc();
    run_to_proposal(s, fb, sum, QStringLiteral("Apple trades at 28.41x earnings."));
    QVERIFY(s.flags().isEmpty());
    QCOMPARE(s.hunks().size(), 1);
    QCOMPARE(s.hunks()[0].kind, ai::Hunk::Kind::Insert);
    QVERIFY(s.accept_all());
    QCOMPARE(applied.size(), 2);
    const ApplyResult& r = applied[1];
    QCOMPARE(r.kind, ApplyResult::Kind::InsertBlocks);
    QCOMPARE(r.after_block_id, 1);
    QCOMPARE(r.blocks.size(), 2);
    QCOMPARE(r.blocks[0].type, QStringLiteral("heading"));
    QCOMPARE(r.blocks[0].content, QStringLiteral("Executive summary"));
    QCOMPARE(r.blocks[0].config.value(QStringLiteral("level")), QStringLiteral("1"));
    QCOMPARE(r.blocks[1].type, QStringLiteral("text"));
    QCOMPARE(r.blocks[1].content, QStringLiteral("Apple trades at 28.41x earnings."));
    QCOMPARE(r.blocks[0].id, 0);

    // No front matter: at the start of the report.
    sum.doc = sample_doc(false);
    run_to_proposal(s, fb, sum, QStringLiteral("Short."));
    QVERIFY(s.accept_all());
    QCOMPARE(applied.size(), 3);
    QCOMPARE(applied[2].after_block_id, 0);
}

void TestReportAiDraft::discard_emits_nothing() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    int applied = 0;
    QObject::connect(&s, &ReportAiSession::apply_requested, &s, [&applied](const ApplyResult&) { ++applied; });
    run_to_proposal(s, fb, block_request(ai::Action::Rewrite), QStringLiteral("Something else."));
    s.discard();
    QCOMPARE(s.state(), ReportAiSession::State::Idle);
    QVERIFY(s.proposal().isEmpty());
    QVERIFY(!s.accept_all());
    QVERIFY(!s.send()); // nothing prepared
    QCOMPARE(applied, 0);

    // Discard while streaming cancels the request; its late result is ignored.
    s.prepare(block_request(ai::Action::Rewrite));
    QVERIFY(s.send());
    const int call = static_cast<int>(fb.calls.size()) - 1;
    s.discard();
    QCOMPARE(fb.cancelled.last(), fb.calls[call].id);
    fb.finish(call, QStringLiteral("Late."));
    QCOMPARE(s.state(), ReportAiSession::State::Idle);
    QCOMPARE(applied, 0);
}

void TestReportAiDraft::needs_setup_sends_nothing() {
    FakeBackend fb;
    fb.info.needs_setup = true;
    fb.info.setup_reason = QStringLiteral("Sign in to Fincept, or set up your own model in Settings › LLM Configuration.");
    ReportAiSession s(&fb);
    s.prepare(block_request(ai::Action::Rewrite));
    QCOMPARE(s.state(), ReportAiSession::State::Ready);
    QVERIFY(s.model_info().needs_setup);
    QVERIFY(!s.send());
    QVERIFY(!s.retry());
    QCOMPARE(fb.calls.size(), 0);
    // Once set up, the same prepared request goes out.
    fb.info.needs_setup = false;
    QVERIFY(s.send());
    QCOMPARE(fb.calls.size(), 1);
}

void TestReportAiDraft::too_long_and_empty_and_no_change() {
    FakeBackend fb;
    ReportAiSession s(&fb);
    s.prepare(block_request(ai::Action::Rewrite, QString(ReportAiSession::kMaxTextChars + 1, QLatin1Char('a'))));
    QCOMPARE(s.state(), ReportAiSession::State::Failed);
    QVERIFY(s.error().contains(QStringLiteral("too long")));
    QVERIFY(!s.send());
    QCOMPARE(fb.calls.size(), 0);

    // An empty answer fails without a proposal.
    s.prepare(block_request(ai::Action::Rewrite));
    QVERIFY(s.send());
    fb.finish(0, QStringLiteral("   "));
    QCOMPARE(s.state(), ReportAiSession::State::Failed);
    QVERIFY(s.error().contains(QStringLiteral("empty")));

    // Nothing to fix: a proposal with no change cannot be accepted.
    int applied = 0;
    QObject::connect(&s, &ReportAiSession::apply_requested, &s, [&applied](const ApplyResult&) { ++applied; });
    run_to_proposal(s, fb, block_request(ai::Action::FixGrammar), kTargetText);
    QCOMPARE(s.state(), ReportAiSession::State::Proposal);
    QVERIFY(s.no_change());
    QVERIFY(!s.accept_all());
    QCOMPARE(applied, 0);

    // A fenced reply is unwrapped.
    run_to_proposal(s, fb, block_request(ai::Action::Rewrite), QStringLiteral("```markdown\nClean text.\n```"));
    QCOMPARE(s.proposal(), QStringLiteral("Clean text."));
}

QTEST_GUILESS_MAIN(TestReportAiDraft)
#include "tst_report_ai_draft.moc"
