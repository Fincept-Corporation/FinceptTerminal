// tst_empty_state_spec.cpp — ui::EmptyState::Spec equality (EmptyState::apply() of the spec
// already on screen is a no-op, since 2026-11-17 round 6) and the remembered-spec rule apply()
// uses (empty_state_apply::LastApplied in AutoLoadPolicy.h). Header-only: no EmptyState is
// constructed, so no QApplication and no app sources are needed — Qt6::Widgets is linked only
// for the headers EmptyState.h includes.

#include "ui/widgets/EmptyState.h"

#include <QtTest>

#include <functional>
#include <type_traits>

using fincept::ui::EmptyState;
using fincept::ui::LoadingSkeleton;
using fincept::ui::empty_state_apply::LastApplied;
using Spec = EmptyState::Spec;

namespace {

/// A spec with every field away from its default.
Spec full_spec() {
    Spec s;
    s.glyph = QStringLiteral("!");
    s.glyph_tone = EmptyState::Tone::Danger;
    s.title = QStringLiteral("Could not load");
    s.message = QStringLiteral("3 of 12 symbols failed.");
    s.details = {{QStringLiteral("AAPL"), QStringLiteral("timeout")}, {QString(), QStringLiteral("a bullet")}};
    s.details_heading = QStringLiteral("FAILED");
    s.note = QStringLiteral("as of 14:02");
    s.note_tone = EmptyState::Tone::Info;
    s.primary = QStringLiteral("RETRY");
    s.secondary = QStringLiteral("DETAILS");
    s.illustration = true;
    s.illustration_shape = LoadingSkeleton::Shape::Table;
    s.technical = QStringLiteral("Traceback ...");
    s.primary_tooltip = QStringLiteral("Run again");
    s.secondary_tooltip = QStringLiteral("More");
    s.suggestions = {{QStringLiteral("MSFT"), QStringLiteral("Microsoft"), QStringLiteral("tip"), QStringLiteral("spend")}};
    s.suggestions_heading = QStringLiteral("OR TRY");
    s.details_clickable = true;
    s.primary_agent_risk = QStringLiteral("spend");
    s.secondary_agent_risk = QStringLiteral("delete");
    s.details_agent_risk = QStringLiteral("spend");
    s.details_agent_risks = {QStringLiteral("order"), QString()};
    return s;
}

} // namespace

class TestEmptyStateSpec : public QObject {
    Q_OBJECT
  private slots:
    void defaults_and_copies_are_equal();
    void every_field_counts();
    void suggestion_fields_count();
    void last_applied_rule_with_specs();
    // Round 6 (7): the agent-approval marker.
    void mark_agent_risk_sets_and_removes();
    // Round 7: markers of the clickable detail rows.
    void detail_row_kind_rule();
    void detail_rows_marked_like_buttons();
    void positional_initialisers_still_compile();
};

void TestEmptyStateSpec::detail_row_kind_rule() {
    const QString all = QStringLiteral("spend");
    const QStringList per_row = {QStringLiteral("order"), QString(), QStringLiteral("account")};
    // A row's own kind wins; an empty or missing entry falls back to the block's kind.
    QCOMPARE(EmptyState::detail_row_agent_risk(all, per_row, 0), QStringLiteral("order"));
    QCOMPARE(EmptyState::detail_row_agent_risk(all, per_row, 1), all);
    QCOMPARE(EmptyState::detail_row_agent_risk(all, per_row, 2), QStringLiteral("account"));
    QCOMPARE(EmptyState::detail_row_agent_risk(all, per_row, 3), all); // past the list
    QCOMPARE(EmptyState::detail_row_agent_risk(all, per_row, -1), all); // never out of range
    QCOMPARE(EmptyState::detail_row_agent_risk(all, {}, 0), all);
    // Nothing named anywhere: no marker.
    QVERIFY(EmptyState::detail_row_agent_risk(QString(), {}, 0).isEmpty());
    QVERIFY(EmptyState::detail_row_agent_risk(QString(), per_row, 1).isEmpty());
    QCOMPARE(EmptyState::detail_row_agent_risk(QString(), per_row, 2), QStringLiteral("account"));
    // The defaults of a Spec mark nothing.
    const Spec s;
    QVERIFY(s.details_agent_risk.isEmpty());
    QVERIFY(s.details_agent_risks.isEmpty());
    QVERIFY(EmptyState::detail_row_agent_risk(s.details_agent_risk, s.details_agent_risks, 0).isEmpty());
}

void TestEmptyStateSpec::detail_rows_marked_like_buttons() {
    // What EmptyState does for each row button (set_details / set_details_agent_risk):
    // mark_agent_risk(row, detail_row_agent_risk(block, per_row, i)) — plain QObjects here.
    const char* name = EmptyState::kAgentRiskProperty;
    QObject page;
    QObject row0(&page), row1(&page), row2(&page);
    QObject* rows[] = {&row0, &row1, &row2};
    const auto mark = [&rows](const QString& all, const QStringList& per_row) {
        for (int i = 0; i < 3; ++i)
            EmptyState::mark_agent_risk(rows[i], EmptyState::detail_row_agent_risk(all, per_row, i));
    };
    mark(QStringLiteral("order"), {QString(), QStringLiteral("money")});
    QCOMPARE(row0.property(name).toString(), QStringLiteral("order"));
    QCOMPARE(row1.property(name).toString(), QStringLiteral("money"));
    QCOMPARE(row2.property(name).toString(), QStringLiteral("order"));
    // The block's kind cleared: rows without their own kind lose the property (removed, not "").
    mark(QString(), {QString(), QStringLiteral("money")});
    QVERIFY(!row0.property(name).isValid());
    QVERIFY(!row0.dynamicPropertyNames().contains(QByteArray(name)));
    QCOMPARE(row1.property(name).toString(), QStringLiteral("money"));
    QVERIFY(!row2.property(name).isValid());
    // Everything cleared: no row carries a marker.
    mark(QString(), {});
    for (QObject* r : rows)
        QVERIFY(!r->property(name).isValid());
}

// Additive change: the details row type is unchanged (QPair<key, value>) and the new Spec
// fields are last, so existing aggregate initialisations keep compiling.
static_assert(std::is_aggregate_v<Spec>, "EmptyState::Spec must stay an aggregate");
static_assert(std::is_same_v<decltype(Spec::details), QList<QPair<QString, QString>>>,
              "Spec::details rows must stay QPair<key, value>");

void TestEmptyStateSpec::positional_initialisers_still_compile() {
    // The rows as before; the new kinds default to "no marker".
    Spec s;
    s.details = {{QStringLiteral("AAPL"), QStringLiteral("timeout")}, {QString(), QStringLiteral("a bullet")}};
    s.details_clickable = true;
    QVERIFY(s.details_agent_risk.isEmpty());
    QVERIFY(s.details_agent_risks.isEmpty());
    Spec marked = s;
    marked.details_agent_risk = QStringLiteral("order");
    QVERIFY(marked != s); // apply() of the marked spec is not skipped
    marked.details_agent_risk.clear();
    QVERIFY(marked == s);
}

void TestEmptyStateSpec::mark_agent_risk_sets_and_removes() {
    // Plain QObjects: the helper is header-inline (no widget, no QApplication).
    const char* name = EmptyState::kAgentRiskProperty;
    QCOMPARE(QByteArray(name), QByteArray("fincept_agent_risk")); // what mcp/core/UiRisk.cpp reads
    QObject container;
    QObject button(&container);
    container.setProperty(name, QStringLiteral("delete")); // a screen's marker on an ancestor

    EmptyState::mark_agent_risk(&button, QStringLiteral("spend"));
    QCOMPARE(button.property(name).toString(), QStringLiteral("spend"));
    EmptyState::mark_agent_risk(&button, QStringLiteral("spend")); // unchanged: no-op
    QCOMPARE(button.property(name).toString(), QStringLiteral("spend"));
    EmptyState::mark_agent_risk(&button, QStringLiteral("order"));
    QCOMPARE(button.property(name).toString(), QStringLiteral("order"));

    // Empty REMOVES it — never "" (an empty value would stop the ancestor walk).
    EmptyState::mark_agent_risk(&button, QString());
    QVERIFY(!button.property(name).isValid());
    QVERIFY(!button.dynamicPropertyNames().contains(QByteArray(name)));
    // The walk UiRisk does (first valid marker up the parent chain) now reaches the container.
    QString found;
    for (const QObject* p = &button; p; p = p->parent()) {
        const QVariant v = p->property(name);
        if (v.isValid()) {
            found = v.toString();
            break;
        }
    }
    QCOMPARE(found, QStringLiteral("delete"));
    EmptyState::mark_agent_risk(&button, QString()); // removing twice is harmless
    QVERIFY(!button.property(name).isValid());
    EmptyState::mark_agent_risk(nullptr, QStringLiteral("spend")); // no target: nothing happens
}

void TestEmptyStateSpec::defaults_and_copies_are_equal() {
    QVERIFY(Spec{} == Spec{});
    const Spec a = full_spec();
    const Spec b = a; // NOLINT(performance-unnecessary-copy-initialization)
    QVERIFY(a == b);
    QVERIFY(!(a != b));
    QVERIFY(!(a == Spec{}));
}

void TestEmptyStateSpec::every_field_counts() {
    // One mutation per field: each must make the spec differ (a field missing from
    // the comparison would let apply() skip a real change).
    const QList<std::function<void(Spec&)>> mutations = {
        [](Spec& s) { s.glyph = QStringLiteral("?"); },
        [](Spec& s) { s.glyph_tone = EmptyState::Tone::Success; },
        [](Spec& s) { s.title += QLatin1Char('.'); },
        [](Spec& s) { s.message.clear(); },
        [](Spec& s) { s.details[0].second = QStringLiteral("ok"); },
        [](Spec& s) { s.details.removeLast(); },
        [](Spec& s) { s.details_heading.clear(); },
        [](Spec& s) { s.note = QStringLiteral("as of 14:03"); },
        [](Spec& s) { s.note_tone = EmptyState::Tone::Warning; },
        [](Spec& s) { s.primary = QStringLiteral("RUN"); },
        [](Spec& s) { s.secondary.clear(); },
        [](Spec& s) { s.illustration = false; },
        [](Spec& s) { s.illustration_shape = LoadingSkeleton::Shape::List; },
        [](Spec& s) { s.technical += QLatin1Char('x'); },
        [](Spec& s) { s.primary_tooltip.clear(); },
        [](Spec& s) { s.secondary_tooltip = QStringLiteral("Less"); },
        [](Spec& s) { s.suggestions.clear(); },
        [](Spec& s) { s.suggestions_heading.clear(); },
        [](Spec& s) { s.details_clickable = false; },
        [](Spec& s) { s.primary_agent_risk.clear(); },
        [](Spec& s) { s.secondary_agent_risk = QStringLiteral("spend"); },
        [](Spec& s) { s.suggestions[0].agent_risk.clear(); },
        // Round 7: the detail rows' kinds count too.
        [](Spec& s) { s.details_agent_risk.clear(); },
        [](Spec& s) { s.details_agent_risk = QStringLiteral("order"); },
        [](Spec& s) { s.details_agent_risks[0] = QStringLiteral("money"); },
        [](Spec& s) { s.details_agent_risks.removeLast(); },
        [](Spec& s) { s.details_agent_risks.clear(); },
    };
    const Spec base = full_spec();
    for (int i = 0; i < mutations.size(); ++i) {
        Spec changed = base;
        mutations.at(i)(changed);
        QVERIFY2(!(changed == base), qPrintable(QStringLiteral("mutation %1 not seen").arg(i)));
        QVERIFY(changed != base);
    }
}

void TestEmptyStateSpec::suggestion_fields_count() {
    const EmptyState::Suggestion a{QStringLiteral("AAPL"), QStringLiteral("Apple"), QStringLiteral("tip")};
    QVERIFY(a == (EmptyState::Suggestion{QStringLiteral("AAPL"), QStringLiteral("Apple"), QStringLiteral("tip")}));
    QVERIFY(a != (EmptyState::Suggestion{QStringLiteral("AAPL"), QStringLiteral("Apple"), QString()}));
    QVERIFY(a != (EmptyState::Suggestion{QStringLiteral("AAPL"), QString(), QStringLiteral("tip")}));
    QVERIFY(a != (EmptyState::Suggestion{QStringLiteral("MSFT"), QStringLiteral("Apple"), QStringLiteral("tip")}));
    // Round 6 (7): the row's agent-approval kind counts too (and defaults to none).
    QVERIFY(a.agent_risk.isEmpty());
    QVERIFY(a != (EmptyState::Suggestion{QStringLiteral("AAPL"), QStringLiteral("Apple"), QStringLiteral("tip"),
                                         QStringLiteral("spend")}));
}

void TestEmptyStateSpec::last_applied_rule_with_specs() {
    LastApplied<Spec> memo;
    const Spec a = full_spec();
    Spec b = a;
    b.title = QStringLiteral("Something else");
    QVERIFY(!memo.valid());
    QVERIFY(memo.should_apply(a));  // first apply
    QVERIFY(!memo.should_apply(a)); // the same again: no-op
    QVERIFY(memo.should_apply(b));  // different
    QVERIFY(memo.should_apply(a));  // back to a: applies (b is on screen)
    QVERIFY(!memo.should_apply(a));
    memo.invalidate();              // a single setter changed the widget
    QVERIFY(memo.should_apply(a));  // the old spec applies again
    QVERIFY(!memo.should_apply(a));
}

QTEST_GUILESS_MAIN(TestEmptyStateSpec)
#include "tst_empty_state_spec.moc"
