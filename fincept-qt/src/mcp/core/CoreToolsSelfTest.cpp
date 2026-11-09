// CoreToolsSelfTest.cpp — see CoreToolsSelfTest.h.

#include "mcp/core/CoreToolsSelfTest.h"

#include "mcp/core/UiEngine.h"
#include "mcp/server/CallApproval.h"
#include "mcp/server/ToolApprovalDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#ifdef HAS_QT_WEBENGINE
#    include "ui/charts/KLineChartWidget.h"

#    include <QWebEnginePage>
#    include <QWebEngineView>
#endif

#include <cstdio>
#include <memory>
#include <optional>

namespace fincept::mcp::core {

namespace core_selftest_detail {

#ifdef HAS_QT_WEBENGINE
// A page with every kind of thing an agent meets on the web: text, a button,
// a field, a password, a select, a control hidden in a closed menu, an order
// button, and a chart object shaped like KLineChart's.
const char* const kSelftestWebPage = R"HTML(<!doctype html><html><head><title>Selftest Page</title></head><body>
<h1>Quotes board</h1>
<p id="status">Idle</p>
<button onclick="document.getElementById('status').textContent = 'Clicked ' + (++window.clicks)">Go</button>
<div style="display:none"><button onclick="document.getElementById('status').textContent = 'Hidden ran'">Secret menu item</button></div>
<label>Symbol <input id="sym" type="text"></label>
<label>Password <input id="pw" type="password" value="hunter2"></label>
<select id="iv"><option>1D</option><option>1W</option><option>1M</option></select>
<button onclick="document.getElementById('status').textContent = 'ORDER PLACED'">Buy now</button>
<script>
window.clicks = 0;
var st = document.getElementById('status');
document.getElementById('sym').addEventListener('input', function () { st.textContent = 'Symbol ' + this.value; });
document.getElementById('iv').addEventListener('change', function () { st.textContent = 'Interval ' + this.value; });
var data = [];
for (var i = 0; i < 50; ++i)
  data.push({timestamp: 1700000000000 + i * 86400000, open: 100 + i, high: 101 + i, low: 99 + i, close: 100.5 + i, volume: 1000 + i});
window.chart = {
  getDataList: function () { return data; },
  getVisibleRange: function () { return {from: 20, to: 50, realFrom: 20, realTo: 50}; },
  getStyles: function () { return {candle: {type: 'candle_solid'}}; },
  getIndicatorByPaneId: function () {
    var inner = new Map();
    inner.set('MA', {name: 'MA', calcParams: [5, 10], figures: [{key: 'ma1', title: 'MA5: '}, {key: 'ma2', title: 'MA10: '}],
                     result: [{ma1: 140, ma2: 139}]});
    var panes = new Map();
    panes.set('candle_pane', inner);
    return panes;
  }
};
</script></body></html>)HTML";
#endif

void say(const QString& line) {
    std::fprintf(stdout, "%s\n", line.toUtf8().constData());
    std::fflush(stdout);
}

struct Tally {
    int pass = 0;
    int fail = 0;
    void check(bool ok, const QString& what, const QString& detail = {}) {
        ok ? ++pass : ++fail;
        say(QStringLiteral("  [%1] %2%3")
                .arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), what,
                     detail.isEmpty() ? QString()
                                      : QStringLiteral("  (") + detail.simplified().left(160) + QLatin1Char(')')));
    }
};

/// Run one engine call to completion, serving the event loop meanwhile.
ToolResult run(const std::function<void(ui::Reply)>& call, int timeout_ms = 15000) {
    auto result = std::make_shared<std::optional<ToolResult>>();
    call([result](ToolResult r) {
        if (!result->has_value())
            *result = std::move(r);
    });
    QElapsedTimer t;
    t.start();
    while (!result->has_value() && t.elapsed() < timeout_ms) {
        QEventLoop loop;
        QTimer::singleShot(5, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return result->has_value() ? **result : ToolResult::fail(QStringLiteral("selftest: no answer in time"));
}

void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QEventLoop loop;
        QTimer::singleShot(5, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

QString text_of(const ToolResult& r) {
    return r.success ? r.message : r.error;
}

/// The ref the snapshot gave to the first line holding all of `words`.
QString ref_on_line(const QString& snapshot, const QStringList& words) {
    for (const QString& line : snapshot.split(QLatin1Char('\n'))) {
        bool all = true;
        for (const QString& w : words)
            all = all && line.contains(w);
        if (!all)
            continue;
        const QString t = line.trimmed();
        const QString first = t.section(QLatin1Char(' '), 0, 0);
        if (first.size() > 1 && first[0] == QLatin1Char('e'))
            return first;
    }
    return {};
}

} // namespace core_selftest_detail

int run_core_tools_selftest() {
    using namespace core_selftest_detail;
    say(QStringLiteral("\n══════════════════════════════════════════════════════════════"));
    say(QStringLiteral("  CORE UI TOOLS SELF-TEST (engine on a real widget form)"));
    say(QStringLiteral("══════════════════════════════════════════════════════════════"));
    Tally tally;
    const QString origin = QStringLiteral("selftest");

    // ── The form ─────────────────────────────────────────────────────────────
    auto* form = new QDialog;
    form->setAttribute(Qt::WA_DeleteOnClose, false);
    form->setWindowTitle(QStringLiteral("Selftest Form"));
    auto* root = new QVBoxLayout(form);
    auto* fields = new QFormLayout;
    auto* symbol = new QLineEdit;
    fields->addRow(QStringLiteral("Symbol"), symbol);
    auto* interval = new QComboBox;
    interval->addItems({QStringLiteral("1D"), QStringLiteral("1W"), QStringLiteral("1M")});
    fields->addRow(QStringLiteral("Interval"), interval);
    auto* qty = new QSpinBox;
    qty->setRange(0, 100);
    fields->addRow(QStringLiteral("Quantity"), qty);
    auto* api_key = new QLineEdit;
    api_key->setEchoMode(QLineEdit::Password);
    api_key->setText(QStringLiteral("sk-live-SECRET-123"));
    fields->addRow(QStringLiteral("API key"), api_key);
    root->addLayout(fields);
    auto* auto_refresh = new QCheckBox(QStringLiteral("Auto refresh"));
    root->addWidget(auto_refresh);
    auto* status = new QLabel(QStringLiteral("Idle"));
    root->addWidget(status);
    auto* refresh = new QPushButton(QStringLiteral("Refresh"));
    root->addWidget(refresh);
    auto* wipe = new QPushButton(QStringLiteral("Delete everything"));
    root->addWidget(wipe);
    auto* ask = new QPushButton(QStringLiteral("Open question"));
    root->addWidget(ask);
    auto* place = new QPushButton(QStringLiteral("Place order"));
    root->addWidget(place);
    // No auto-default buttons: Enter in a field must not also press one (a
    // QDialog would otherwise forward an ignored Return to its default button).
    for (QPushButton* b : {refresh, wipe, ask, place})
        b->setAutoDefault(false);
    auto* tabs = new QTabWidget;
    tabs->addTab(new QLabel(QStringLiteral("overview page")), QStringLiteral("Overview"));
    tabs->addTab(new QLabel(QStringLiteral("details page")), QStringLiteral("Details"));
    root->addWidget(tabs);
    auto* table = new QTableWidget(3, 3);
    table->setHorizontalHeaderLabels({QStringLiteral("Symbol"), QStringLiteral("Last"), QStringLiteral("Change")});
    const char* rows[3][3] = {{"AAPL", "189.30", "+1.2%"}, {"MSFT", "412.10", "-0.4%"}, {"NVDA", "131.70", "+3.1%"}};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c)
            table->setItem(r, c, new QTableWidgetItem(QString::fromLatin1(rows[r][c])));
    }
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(table);

    int edited = 0, submitted = 0, activated = 0, refreshed = 0, wiped = 0, answered = 0, placed = 0;
    QObject::connect(symbol, &QLineEdit::textEdited, form, [&edited]() { ++edited; });
    QObject::connect(symbol, &QLineEdit::returnPressed, form, [&submitted]() { ++submitted; });
    QObject::connect(interval, &QComboBox::activated, form, [&activated]() { ++activated; });
    QObject::connect(refresh, &QPushButton::clicked, form, [&refreshed, status]() {
        ++refreshed;
        status->setText(QStringLiteral("Refreshed %1").arg(refreshed));
    });
    QObject::connect(wipe, &QPushButton::clicked, form, [&wiped]() { ++wiped; });
    QObject::connect(place, &QPushButton::clicked, form, [&placed]() { ++placed; });
    QObject::connect(ask, &QPushButton::clicked, form, [&answered, form]() {
        // exec(): a nested event loop — the call that clicked must not hang on it.
        QMessageBox::question(form, QStringLiteral("Continue?"), QStringLiteral("Proceed with the selftest?"));
        ++answered;
    });
    form->resize(520, 640);
    form->show();
    pump(100);

    const QString dref = ui::Refs::instance().ref_for(form, ui::RefKind::Dialog);
    const QJsonObject target{{QStringLiteral("target"), dref}};

    // ── Snapshot ─────────────────────────────────────────────────────────────
    ToolResult snap = run([&](ui::Reply r) { ui::ui_snapshot(target, std::move(r)); });
    const QString s = text_of(snap);
    tally.check(snap.success && s.contains(QStringLiteral("textbox \"Symbol\"")),
                QStringLiteral("a form field takes its label as its name"));
    tally.check(s.contains(QStringLiteral("combobox \"Interval\" = \"1D\"")),
                QStringLiteral("a dropdown shows its value"));
    tally.check(s.contains(QStringLiteral("checkbox \"Auto refresh\"")) && s.contains(QStringLiteral("unchecked")),
                QStringLiteral("a checkbox shows its state"));
    tally.check(s.contains(QStringLiteral("tab \"Overview\" [selected]")),
                QStringLiteral("tabs are elements, the current one selected"));
    tally.check(s.contains(QStringLiteral("table")) && s.contains(QStringLiteral("3 rows")) &&
                    s.contains(QStringLiteral("AAPL")),
                QStringLiteral("a table shows its size, headers and first rows"));
    tally.check(s.contains(QStringLiteral("secret")) && !s.contains(QStringLiteral("SECRET-123")),
                QStringLiteral("a password field is marked secret and its value is never shown"));
    tally.check(!s.contains(QStringLiteral("text \"Symbol\"")),
                QStringLiteral("a label used as a field name is not repeated"));
    const QString table_ref = ref_on_line(s, {QStringLiteral("table")});
    const QString key_ref = ref_on_line(s, {QStringLiteral("API key")});
    tally.check(!table_ref.isEmpty() && !key_ref.isEmpty(), QStringLiteral("controls carry refs"),
                table_ref + QLatin1Char(' ') + key_ref);

    // ── Acting ───────────────────────────────────────────────────────────────
    ToolResult r1 = run([&](ui::Reply r) {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("type")},
                               {QStringLiteral("name"), QStringLiteral("Symbol")},
                               {QStringLiteral("text"), QStringLiteral("AAPL")},
                               {QStringLiteral("submit"), true},
                               {QStringLiteral("target"), dref}},
                   origin, std::move(r));
    });
    tally.check(r1.success && symbol->text() == QLatin1String("AAPL") && edited > 0 && submitted == 1,
                QStringLiteral("type by name sets the text like a person (textEdited + Enter)"), text_of(r1));
    ToolResult r2 = run([&](ui::Reply r) {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("select")},
                               {QStringLiteral("name"), QStringLiteral("Interval")},
                               {QStringLiteral("text"), QStringLiteral("1W")},
                               {QStringLiteral("target"), dref}},
                   origin, std::move(r));
    });
    tally.check(r2.success && interval->currentText() == QLatin1String("1W") && activated == 1,
                QStringLiteral("select picks a dropdown option and emits activated"), text_of(r2));
    ToolResult r3 = run([&](ui::Reply r) {
        ui::ui_act(QJsonObject{{QStringLiteral("steps"),
                                QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("check")},
                                                       {QStringLiteral("name"), QStringLiteral("Auto refresh")}},
                                           QJsonObject{{QStringLiteral("action"), QStringLiteral("set_value")},
                                                       {QStringLiteral("name"), QStringLiteral("Quantity")},
                                                       {QStringLiteral("value"), 5}},
                                           QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                       {QStringLiteral("name"), QStringLiteral("Refresh")}},
                                           QJsonObject{{QStringLiteral("action"), QStringLiteral("select")},
                                                       {QStringLiteral("name"), QStringLiteral("Details")},
                                                       {QStringLiteral("role"), QStringLiteral("tab")}}}},
                               {QStringLiteral("target"), dref}},
                   origin, std::move(r));
    });
    tally.check(r3.success && auto_refresh->isChecked() && qty->value() == 5 && refreshed == 1 &&
                    tabs->currentIndex() == 1,
                QStringLiteral("a 4-step batch checks, sets a value, clicks and switches tab"), text_of(r3));
    tally.check(text_of(r3).contains(QStringLiteral("Refreshed 1")),
                QStringLiteral("the answer reports the new status text"));

    // ── Reading ──────────────────────────────────────────────────────────────
    ToolResult rd = run([&](ui::Reply r) {
        ui::ui_read(QJsonObject{{QStringLiteral("ref"), table_ref}, {QStringLiteral("query"), QStringLiteral("msft")}},
                    std::move(r));
    });
    tally.check(rd.success && text_of(rd).contains(QStringLiteral(":r1 | MSFT | 412.10")) &&
                    !text_of(rd).contains(QStringLiteral("AAPL")),
                QStringLiteral("ui_read filters table rows and gives row refs"), text_of(rd).left(120));
    ToolResult rc = run([&](ui::Reply r) {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                               {QStringLiteral("ref"), table_ref + QStringLiteral(":r2")}},
                   origin, std::move(r));
    });
    tally.check(rc.success && table->currentRow() == 2, QStringLiteral("clicking a row ref selects that row"),
                text_of(rc));
    ToolResult sec_read =
        run([&](ui::Reply r) { ui::ui_read(QJsonObject{{QStringLiteral("ref"), key_ref}}, std::move(r)); });
    tally.check(!sec_read.success && !text_of(sec_read).contains(QStringLiteral("SECRET")),
                QStringLiteral("a secret field cannot be read"));
    ToolResult sec_type = run([&](ui::Reply r) {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("type")},
                               {QStringLiteral("ref"), key_ref},
                               {QStringLiteral("text"), QStringLiteral("stolen")}},
                   origin, std::move(r));
    });
    tally.check(!sec_type.success && api_key->text() == QLatin1String("sk-live-SECRET-123"),
                QStringLiteral("a secret field cannot be typed into"));

    // ── Waits ────────────────────────────────────────────────────────────────
    ToolResult w1 = run([&](ui::Reply r) {
        ui::ui_wait(QJsonObject{{QStringLiteral("until"), QStringLiteral("text")},
                                {QStringLiteral("text"), QStringLiteral("Refreshed 1")},
                                {QStringLiteral("target"), dref}},
                    {}, std::move(r));
    });
    tally.check(w1.success, QStringLiteral("ui_wait until text holds at once when it is showing"), text_of(w1));
    QElapsedTimer wt;
    wt.start();
    ToolResult w2 = run([&](ui::Reply r) {
        ui::ui_wait(QJsonObject{{QStringLiteral("until"), QStringLiteral("text")},
                                {QStringLiteral("text"), QStringLiteral("never appears")},
                                {QStringLiteral("target"), dref},
                                {QStringLiteral("timeout_ms"), 400}},
                    {}, std::move(r));
    });
    tally.check(!w2.success && wt.elapsed() >= 350 && wt.elapsed() < 3000, QStringLiteral("ui_wait times out honestly"),
                text_of(w2));

    // ── A risky click asks first (denied here) ──────────────────────────────
    QTimer::singleShot(600, qApp, []() { ToolApprovalDialog::deny_all_pending(); });
    ToolResult risky = run(
        [&](ui::Reply r) {
            ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                   {QStringLiteral("name"), QStringLiteral("Delete everything")},
                                   {QStringLiteral("target"), dref}},
                       origin, std::move(r));
        },
        20000);
    tally.check(!risky.success && wiped == 0 && text_of(risky).contains(QStringLiteral("declined")),
                QStringLiteral("a delete asks the user and does nothing when declined"), text_of(risky));

    // ── A PAPER workflow run never places an order — and never asks ─────────
    // (The deny timer only cleans up if a prompt wrongly appears.)
    QTimer::singleShot(2500, qApp, []() { ToolApprovalDialog::deny_all_pending(); });
    QElapsedTimer pt;
    pt.start();
    ToolResult paper = run(
        [&](ui::Reply r) {
            ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                   {QStringLiteral("name"), QStringLiteral("Place order")},
                                   {QStringLiteral("target"), dref}},
                       origin + QLatin1String(kPaperRunOriginSuffix), std::move(r));
        },
        20000);
    tally.check(!paper.success && placed == 0 && text_of(paper).contains(QStringLiteral("PAPER")) &&
                    pt.elapsed() < 2000,
                QStringLiteral("a PAPER workflow run refuses an order outright, without asking"), text_of(paper));
    pump(2700); // let the clean-up timer fire before the next test

    // ── A click that opens a modal never hangs; the modal blocks the rest ───
    // The click runs QMessageBox::exec() — a nested event loop. The test driver
    // sits under that loop until the box closes, so the follow-up calls an agent
    // would make (a blocked click, then "No") are scheduled on timers and run
    // INSIDE the nested loop, exactly as an agent's next requests would.
    auto open_reply = std::make_shared<std::optional<ToolResult>>();
    auto open_at = std::make_shared<qint64>(-1);
    auto blocked = std::make_shared<std::optional<ToolResult>>();
    auto answer = std::make_shared<std::optional<ToolResult>>();
    QElapsedTimer mt;
    mt.start();
    QTimer::singleShot(700, qApp, [blocked, dref, origin]() {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                               {QStringLiteral("name"), QStringLiteral("Refresh")},
                               {QStringLiteral("target"), dref}},
                   origin, [blocked](ToolResult r) { *blocked = std::move(r); });
    });
    QTimer::singleShot(1200, qApp, [answer, origin]() {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                               {QStringLiteral("name"), QStringLiteral("No")},
                               {QStringLiteral("target"), QStringLiteral("dialog")}},
                   origin, [answer](ToolResult r) { *answer = std::move(r); });
    });
    ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                           {QStringLiteral("name"), QStringLiteral("Open question")},
                           {QStringLiteral("target"), dref}},
               origin, [open_reply, open_at, &mt](ToolResult r) {
                   *open_reply = std::move(r);
                   *open_at = mt.elapsed();
               });
    QElapsedTimer guard;
    guard.start();
    while ((!answer->has_value() || answered == 0) && guard.elapsed() < 15000)
        pump(20);
    tally.check(open_reply->has_value() && (*open_reply)->success &&
                    text_of(**open_reply).contains(QStringLiteral("dialog")) && *open_at >= 0 && *open_at < 1000,
                QStringLiteral("a click that opens a modal answers at once and reports the dialog"),
                open_reply->has_value() ? text_of(**open_reply) : QStringLiteral("no answer"));
    tally.check(blocked->has_value() && !(*blocked)->success && text_of(**blocked).contains(QStringLiteral("blocks")) &&
                    refreshed == 1,
                QStringLiteral("a modal dialog blocks clicks behind it"),
                blocked->has_value() ? text_of(**blocked) : QStringLiteral("no answer"));
    tally.check(answer->has_value() && (*answer)->success && answered == 1,
                QStringLiteral("answering the modal lets the original click finish"),
                answer->has_value() ? text_of(**answer) : QStringLiteral("no answer"));

    // ── Screenshot ───────────────────────────────────────────────────────────
    ToolResult shot = run([&](ui::Reply r) {
        ui::ui_screenshot(QJsonObject{{QStringLiteral("target"), dref}, {QStringLiteral("marks"), true}}, std::move(r));
    });
    const QJsonObject img = shot.content.at(0).toObject();
    tally.check(shot.success && img.value(QStringLiteral("type")).toString() == QLatin1String("image") &&
                    img.value(QStringLiteral("data")).toString().size() > 1000,
                QStringLiteral("ui_screenshot returns an image block"), text_of(shot));

    form->hide();
    form->deleteLater();
    pump(50);

    // ── A live blotter: paging, cell controls, rows that move ────────────────
    auto* blotter_form = new QDialog;
    blotter_form->setWindowTitle(QStringLiteral("Selftest Blotter"));
    auto* blotter_layout = new QVBoxLayout(blotter_form);
    auto* blotter = new QTableWidget(4, 14);
    QStringList blotter_headers{QStringLiteral("Symbol"), QStringLiteral("Side")};
    for (int c = 2; c < 13; ++c)
        blotter_headers << QStringLiteral("Q%1").arg(c - 1);
    blotter_headers << QStringLiteral("Action");
    blotter->setHorizontalHeaderLabels(blotter_headers);
    const QStringList syms{QStringLiteral("AAPL"), QStringLiteral("MSFT"), QStringLiteral("NVDA"),
                           QStringLiteral("TSLA")};
    int cancels[4] = {0, 0, 0, 0};
    for (int r = 0; r < 4; ++r) {
        blotter->setItem(r, 0, new QTableWidgetItem(syms[r]));
        blotter->setItem(r, 1, new QTableWidgetItem(r % 2 ? QStringLiteral("SELL") : QStringLiteral("BUY")));
        for (int c = 2; c < 13; ++c)
            blotter->setItem(r, c, new QTableWidgetItem(QString::number(r * 100 + c)));
        auto* cancel = new QPushButton(QStringLiteral("Cancel"));
        QObject::connect(cancel, &QPushButton::clicked, blotter_form, [&cancels, r]() { ++cancels[r]; });
        blotter->setCellWidget(r, 13, cancel);
    }
    blotter->horizontalHeader()->setDefaultSectionSize(60); // every column on screen
    blotter_layout->addWidget(blotter);
    auto* hit_me = new QPushButton(QStringLiteral("Hit me"));
    hit_me->setAutoDefault(false);
    int hits = 0;
    QObject::connect(hit_me, &QPushButton::clicked, blotter_form, [&hits]() { ++hits; });
    blotter_layout->addWidget(hit_me);
    blotter_form->resize(1100, 420);
    blotter_form->show();
    pump(100);
    const QString bref = ui::Refs::instance().ref_for(blotter_form, ui::RefKind::Dialog);
    const QString bsnap = text_of(
        run([&](ui::Reply r) { ui::ui_snapshot(QJsonObject{{QStringLiteral("target"), bref}}, std::move(r)); }));
    const QString blotter_ref = ref_on_line(bsnap, {QStringLiteral("table")});
    tally.check(bsnap.contains(QStringLiteral("+6 cols")),
                QStringLiteral("a preview says how many columns it left out"), bsnap.section(QLatin1Char('\n'), 1, 1));

    const QString page1 = text_of(run([&](ui::Reply r) {
        ui::ui_read(QJsonObject{{QStringLiteral("ref"), blotter_ref}, {QStringLiteral("limit"), 2}}, std::move(r));
    }));
    tally.check(page1.contains(QStringLiteral("columns 1–10 of 14")) &&
                    page1.contains(QStringLiteral("showing rows 0–1 of 4")) &&
                    page1.contains(QStringLiteral("offset=2")),
                QStringLiteral("ui_read pages rows and columns and says how to go on"), page1.right(160));
    const QString all_rows = text_of(run([&](ui::Reply r) {
        ui::ui_read(QJsonObject{{QStringLiteral("ref"), blotter_ref}, {QStringLiteral("col_offset"), 10}},
                    std::move(r));
    }));
    QString tsla_cancel;
    for (const QString& line : all_rows.split(QLatin1Char('\n'))) {
        if (line.contains(QStringLiteral(":r3 ")) && line.contains(QStringLiteral("button \"Cancel\"")))
            tsla_cancel = line.section(QLatin1Char('['), 1, 1).section(QLatin1Char(' '), 0, 0);
    }
    tally.check(!tsla_cancel.isEmpty(), QStringLiteral("a button inside a table row is listed with its own ref"),
                all_rows.section(QLatin1Char('\n'), 2, 3));
    ToolResult cell_click = run([&](ui::Reply r) {
        ui::ui_act(
            QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), tsla_cancel}},
            origin, std::move(r));
    });
    tally.check(cell_click.success && cancels[3] == 1 && cancels[0] + cancels[1] + cancels[2] == 0,
                QStringLiteral("clicking that ref presses the button in that row"), text_of(cell_click));

    // The rows were listed (r0 AAPL … r3 TSLA); now the view re-sorts.
    blotter->sortItems(0, Qt::DescendingOrder); // TSLA, NVDA, MSFT, AAPL
    pump(30);
    const QString moved = text_of(run([&](ui::Reply r) {
        ui::ui_read(QJsonObject{{QStringLiteral("ref"), blotter_ref + QStringLiteral(":r0")}}, std::move(r));
    }));
    tally.check(moved.contains(QStringLiteral("AAPL")) && moved.contains(QStringLiteral("listed it as r0")),
                QStringLiteral("a row ref follows its row when the table re-sorts"),
                moved.section(QLatin1Char('\n'), 0, 1));
    ToolResult side_click = run([&](ui::Reply r) {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                               {QStringLiteral("ref"), blotter_ref + QStringLiteral(":r1")},
                               {QStringLiteral("column"), QStringLiteral("Q1")}},
                   origin, std::move(r));
    });
    pump(30);
    const QTableWidgetItem* current = blotter->currentItem();
    tally.check(side_click.success && current && blotter->currentColumn() == 2 &&
                    blotter->item(blotter->currentRow(), 0)->text() == QLatin1String("MSFT"),
                QStringLiteral("column= clicks that cell of the row that was listed"), text_of(side_click));
    for (int r = 0; r < blotter->rowCount(); ++r) {
        if (blotter->item(r, 0)->text() == QLatin1String("MSFT")) {
            blotter->removeRow(r);
            break;
        }
    }
    pump(30);
    ToolResult gone = run([&](ui::Reply r) {
        ui::ui_read(QJsonObject{{QStringLiteral("ref"), blotter_ref + QStringLiteral(":r1")}}, std::move(r));
    });
    tally.check(!gone.success && text_of(gone).contains(QStringLiteral("no longer in that list")),
                QStringLiteral("a row that left the table is refused, not replaced by another"), text_of(gone));

    // A click at a point anywhere in the dialog lands on what is there.
    const QPoint at = hit_me->mapTo(blotter_form, hit_me->rect().center());
    ToolResult point_click = run([&](ui::Reply r) {
        ui::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                               {QStringLiteral("target"), bref},
                               {QStringLiteral("x"), at.x()},
                               {QStringLiteral("y"), at.y()}},
                   origin, std::move(r));
    });
    tally.check(point_click.success && hits == 1, QStringLiteral("a click at x/y presses the control under the point"),
                text_of(point_click));
    const QString found = text_of(run([&](ui::Reply r) {
        ui::ui_snapshot(
            QJsonObject{{QStringLiteral("target"), bref}, {QStringLiteral("query"), QStringLiteral("cancel")}},
            std::move(r));
    }));
    tally.check(found.contains(QStringLiteral("Cancel · r")), QStringLiteral("a query finds the buttons in table rows"),
                found.section(QLatin1Char('\n'), 1, 2));
    blotter_form->hide();
    blotter_form->deleteLater();
    pump(50);

#ifdef HAS_QT_WEBENGINE
    // ── A web page: read it, click / type / select in it, refuse its secrets ─
    bool web_runtime = false;
    {
        fincept::ui::KLineChartWidget probe; // never shown: no view is created
        web_runtime = probe.is_available();
    }
    if (!web_runtime) {
        say(QStringLiteral("  (web page checks skipped: no Qt WebEngine runtime next to the app)"));
    } else {
        auto* web_form = new QDialog;
        web_form->setWindowTitle(QStringLiteral("Selftest Web"));
        auto* web_layout = new QVBoxLayout(web_form);
        auto* page_view = new QWebEngineView;
        web_layout->addWidget(page_view);
        web_form->resize(900, 700);
        web_form->show();
        bool loaded = false;
        QObject::connect(page_view, &QWebEngineView::loadFinished, web_form, [&loaded](bool ok) { loaded = ok; });
        page_view->setHtml(QString::fromUtf8(kSelftestWebPage));
        QElapsedTimer load_clock;
        load_clock.start();
        while (!loaded && load_clock.elapsed() < 30000)
            pump(50);
        pump(300);
        tally.check(loaded, QStringLiteral("a web page loads"));
        const QString vref = ui::Refs::instance().ref_for(page_view, ui::RefKind::Element);
        const QString wref = ui::Refs::instance().ref_for(web_form, ui::RefKind::Dialog);
        auto read = [&](const QJsonObject& extra) {
            QJsonObject a = extra;
            a.insert(QStringLiteral("ref"), vref);
            return text_of(run([&](ui::Reply r) { ui::ui_read(a, std::move(r)); }));
        };
        auto page_text = [&]() { return read(QJsonObject{{QStringLiteral("path"), QStringLiteral("text")}}); };
        auto act = [&](const QJsonObject& a, const QString& who) {
            return run([&](ui::Reply r) { ui::ui_act(a, who, std::move(r)); }, 20000);
        };

        const QString overview = read({});
        tally.check(overview.contains(QStringLiteral("web page \"Selftest Page\"")) &&
                        overview.contains(QStringLiteral("Quotes board")) &&
                        overview.contains(QStringLiteral("button \"Go\"")) && overview.contains(QStringLiteral(":w")),
                    QStringLiteral("ui_read reads a web page: its text and its controls with refs"),
                    overview.section(QLatin1Char('\n'), 1, 4));
        tally.check(overview.contains(QStringLiteral("secret")) && !overview.contains(QStringLiteral("hunter2")),
                    QStringLiteral("a password on a page is marked secret and never read"));
        tally.check(overview.contains(QStringLiteral("chart · 50 candles")) &&
                        overview.contains(QStringLiteral("MA5 140")),
                    QStringLiteral("a web chart's data is summarised (range, last candle, indicators)"),
                    overview.section(QLatin1Char('\n'), 2, 4));
        tally.check(overview.contains(QStringLiteral("+1 hidden")),
                    QStringLiteral("controls the page keeps hidden are counted, not listed as if on screen"));

        const QString go_ref = ref_on_line(overview, {QStringLiteral("button \"Go\"")});
        ToolResult go = act(
            QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), go_ref}}, origin);
        pump(300);
        tally.check(go.success && page_text().contains(QStringLiteral("Clicked 1")),
                    QStringLiteral("clicking a web control's ref clicks it in the page"), text_of(go));

        const QString sym_ref = ref_on_line(overview, {QStringLiteral("textbox \"Symbol\"")});
        ToolResult typed = act(QJsonObject{{QStringLiteral("action"), QStringLiteral("type")},
                                           {QStringLiteral("ref"), sym_ref},
                                           {QStringLiteral("text"), QStringLiteral("AAPL")}},
                               origin);
        pump(300);
        tally.check(typed.success && page_text().contains(QStringLiteral("Symbol AAPL")),
                    QStringLiteral("typing into a web field types there"), text_of(typed));

        const QString pw_ref = ref_on_line(overview, {QStringLiteral("Password")});
        ToolResult pw = act(QJsonObject{{QStringLiteral("action"), QStringLiteral("type")},
                                        {QStringLiteral("ref"), pw_ref},
                                        {QStringLiteral("text"), QStringLiteral("guess")}},
                            origin);
        tally.check(!pw.success && text_of(pw).contains(QStringLiteral("secret")),
                    QStringLiteral("a web password field is never typed into"), text_of(pw));

        const QString iv_ref = ref_on_line(overview, {QStringLiteral("combobox")});
        ToolResult picked = act(QJsonObject{{QStringLiteral("action"), QStringLiteral("select")},
                                            {QStringLiteral("ref"), iv_ref},
                                            {QStringLiteral("text"), QStringLiteral("1W")}},
                                origin);
        pump(300);
        tally.check(picked.success && page_text().contains(QStringLiteral("Interval 1W")),
                    QStringLiteral("selecting a web dropdown's option picks it"), text_of(picked));

        const QString every = read(QJsonObject{{QStringLiteral("path"), QStringLiteral("controls")}});
        const QString hidden_ref = ref_on_line(every, {QStringLiteral("Secret menu item")});
        ToolResult hidden =
            act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), hidden_ref}},
                origin);
        pump(300);
        tally.check(hidden.success && page_text().contains(QStringLiteral("Hidden ran")),
                    QStringLiteral("a control in a closed menu is listed by path=controls and can be clicked"),
                    text_of(hidden));

        QTimer::singleShot(2500, qApp, []() { ToolApprovalDialog::deny_all_pending(); });
        const QString buy_ref = ref_on_line(overview, {QStringLiteral("Buy now")});
        ToolResult buy =
            act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), buy_ref}},
                origin + QLatin1String(kPaperRunOriginSuffix));
        pump(300);
        tally.check(!buy.success && text_of(buy).contains(QStringLiteral("PAPER")) &&
                        !page_text().contains(QStringLiteral("ORDER PLACED")),
                    QStringLiteral("a web \"Buy\" button is judged as an order (a PAPER run refuses it)"),
                    text_of(buy));
        pump(2300);

        // A click at a point of the page lands on what is there, and is named.
        QJsonObject where;
        run([&](ui::Reply r) {
            ui::run_web_script(page_view,
                               ui::web_target_script(
                                   QJsonObject{{QStringLiteral("mode"), QStringLiteral("control")},
                                               {QStringLiteral("i"), go_ref.section(QStringLiteral(":w"), 1).toInt()},
                                               {QStringLiteral("k"), QStringLiteral("button|Go")}}),
                               false, 5000, [&where, r](const QJsonObject& o, const QString&) {
                                   where = o;
                                   r(ToolResult::ok(QString()));
                               });
        });
        const double zoom = ui::web_zoom(page_view);
        ToolResult at_point =
            act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                            {QStringLiteral("ref"), vref},
                            {QStringLiteral("x"), qRound(where.value(QStringLiteral("x")).toDouble() * zoom)},
                            {QStringLiteral("y"), qRound(where.value(QStringLiteral("y")).toDouble() * zoom)}},
                origin);
        pump(300);
        tally.check(at_point.success && text_of(at_point).contains(QStringLiteral("\"Go\"")) &&
                        page_text().contains(QStringLiteral("Clicked 2")),
                    QStringLiteral("a click at x/y on a page presses what is there and names it"), text_of(at_point));

        const QString candles =
            read(QJsonObject{{QStringLiteral("path"), QStringLiteral("candles")}, {QStringLiteral("limit"), 5}});
        tally.check(candles.contains(QStringLiteral("c49:")) &&
                        candles.contains(QStringLiteral("showing candles 45–49 of 50")) &&
                        candles.contains(QStringLiteral("earlier:")),
                    QStringLiteral("a web chart's candles page from the newest, with the way back"),
                    candles.section(QLatin1Char('\n'), 0, 1));

        // ui_wait sees text that appears inside the page.
        page_view->page()->runJavaScript(QStringLiteral(
            "setTimeout(function () { document.getElementById('status').textContent = 'Ready later'; }, 600);"));
        ToolResult waited = run([&](ui::Reply r) {
            ui::ui_wait(QJsonObject{{QStringLiteral("until"), QStringLiteral("text")},
                                    {QStringLiteral("text"), QStringLiteral("Ready later")},
                                    {QStringLiteral("target"), wref},
                                    {QStringLiteral("timeout_ms"), 8000}},
                        {}, std::move(r));
        });
        tally.check(waited.success, QStringLiteral("ui_wait sees text appear inside a web page"), text_of(waited));

        // The page renames the button: its old ref is refused, not sent elsewhere.
        page_view->page()->runJavaScript(QStringLiteral("document.querySelector('button').textContent = 'Stop';"));
        pump(300);
        ToolResult stale = act(
            QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), go_ref}}, origin);
        tally.check(!stale.success && text_of(stale).contains(QStringLiteral("no longer on the page")),
                    QStringLiteral("a web control that changed is refused, not replaced"), text_of(stale));

        web_form->hide();
        web_form->deleteLater();
        pump(100);
    }
#endif

    say(QStringLiteral("──────────────────────────────────────────────────────────────"));
    say(QStringLiteral("  %1 passed, %2 failed").arg(tally.pass).arg(tally.fail));
    return tally.fail == 0 ? 0 : 1;
}

} // namespace fincept::mcp::core
