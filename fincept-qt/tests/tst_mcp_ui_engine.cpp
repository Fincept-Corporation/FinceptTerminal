// tst_mcp_ui_engine.cpp — the MCP UI engine (src/mcp/core/Ui*.cpp) on real
// widgets, offscreen, with the terminal replaced by tests/stubs/mcp_ui_host_fake.
//
// Covers: stable refs and stale refs (destroyed between observe and act, a click
// that deletes its own button, a model reset between read and act), every editor
// family and its validation, combo / menu / table-cell paths, a virtual 100k-row
// table read in pages, batches (stop on error; wait and read steps), bounded
// waits, a modal dialog opened by a click, native-dialog suppression, approval of
// dangerous controls through every generic path and the one-order-step rule,
// PAPER runs, secrets, incremental snapshots, size budgets, structured error codes
// and that no exception escapes a handler.

#include "../tests/stubs/mcp_ui_host_fake.h"
#include "core/agent/AgentActionScope.h"
#include "core/agent/AgentRiskMarkers.h"
#include "mcp/core/UiRisk.h"
#include "mcp/ResultStore.h"
#include "mcp/core/UiDispatch.h"
#include "mcp/core/UiEngine.h"
#include "mcp/core/UiErrors.h"
#include "mcp/server/CallApproval.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFuture>
#include <QFormLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>
#include <thread>
#include <optional>
#include <stdexcept>

using namespace fincept::mcp;
using namespace fincept::mcp::core;
namespace eng = fincept::mcp::core::ui;

/// Counts as busy (the engine recognises "…LoadingOverlay" classes).
class FakeLoadingOverlay : public QWidget {
    Q_OBJECT
  public:
    using QWidget::QWidget;
};

namespace {

const QString kOrigin = QStringLiteral("test-agent");

ToolResult run(const std::function<void(eng::Reply)>& call, int timeout_ms = 20000) {
    auto result = std::make_shared<std::optional<ToolResult>>();
    call([result](ToolResult r) {
        if (!result->has_value())
            *result = std::move(r);
    });
    QElapsedTimer t;
    t.start();
    while (!result->has_value() && t.elapsed() < timeout_ms)
        QTest::qWait(5);
    return result->has_value() ? **result : ToolResult::fail(QStringLiteral("test: no answer in time"));
}

QString text_of(const ToolResult& r) {
    return r.success ? r.message : r.error;
}

ToolResult act(const QJsonObject& a, const QString& origin = kOrigin) {
    return run([&](eng::Reply r) { eng::ui_act(a, origin, std::move(r)); });
}

ToolResult snapshot(const QJsonObject& a) {
    return run([&](eng::Reply r) { eng::ui_snapshot(a, std::move(r)); });
}

ToolResult read(const QJsonObject& a) {
    return run([&](eng::Reply r) { eng::ui_read(a, std::move(r)); });
}

ToolResult wait_for(const QJsonObject& a, eng::CancelCheck cancelled = {}) {
    return run([&](eng::Reply r) { eng::ui_wait(a, std::move(cancelled), std::move(r)); });
}

/// The ref at the start of the first line holding every word.
QString ref_on_line(const QString& text, const QStringList& words) {
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        bool all = true;
        for (const QString& w : words)
            all = all && line.contains(w);
        if (!all)
            continue;
        const QString first = line.trimmed().section(QLatin1Char(' '), 0, 0);
        if (first.size() > 1 && QStringLiteral("epdmw").contains(first[0]))
            return first;
    }
    return {};
}

bool has_code(const ToolResult& r, const char* code) {
    return !r.success && r.error.startsWith(QString::fromLatin1(code) + QStringLiteral(": "));
}

/// A terminal window with one panel; widgets go into `panel_layout`.
struct Screen {
    QPointer<QWidget> frame;
    QPointer<QWidget> panel;
    QVBoxLayout* layout = nullptr;
    QString panel_ref;

    explicit Screen(const QString& id = QStringLiteral("markets")) {
        frame = new QWidget;
        frame->setProperty("fincept_test_frame", true);
        frame->setWindowTitle(QStringLiteral("Test Terminal"));
        auto* outer = new QVBoxLayout(frame);
        panel = new QWidget(frame);
        panel->setProperty("fincept_test_panel", id);
        panel->setWindowTitle(id.at(0).toUpper() + id.mid(1));
        outer->addWidget(panel);
        layout = new QVBoxLayout(panel);
    }
    void show(int w = 900, int h = 700) {
        frame->resize(w, h);
        frame->show();
        frame->activateWindow();
        QTest::qWait(30);
        panel_ref = eng::Refs::instance().ref_for(panel, eng::RefKind::Panel);
    }
    QJsonObject target() const { return QJsonObject{{QStringLiteral("target"), panel_ref}}; }
    ~Screen() { delete frame.data(); }
};

/// A 100k-row model computed on demand (nothing stored, no item widgets).
class BigModel : public QAbstractTableModel {
  public:
    int rows = 100000;
    int rowCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : rows; }
    int columnCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : 4; }
    QVariant data(const QModelIndex& i, int role) const override {
        if (role != Qt::DisplayRole && role != Qt::EditRole)
            return {};
        switch (i.column()) {
            case 0:
                return QStringLiteral("S%1").arg(i.row());
            case 1:
                return role == Qt::EditRole ? QVariant(100.0 + i.row() / 100.0)
                                            : QVariant(QStringLiteral("$%1").arg(100.0 + i.row() / 100.0, 0, 'f', 2));
            case 2:
                return QStringLiteral("%1%").arg((i.row() % 7) - 3);
            default:
                return QStringLiteral("%1K").arg(i.row() % 1000);
        }
    }
    QVariant headerData(int s, Qt::Orientation o, int role) const override {
        if (o != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        static const char* h[] = {"Symbol", "Last", "Chg", "Volume"};
        return QString::fromLatin1(h[s]);
    }
};

/// A model that throws when asked to — for the "nothing escapes" checks.
class ThrowingModel : public QStandardItemModel {
  public:
    using QStandardItemModel::QStandardItemModel;
    bool throw_on_data = false;
    QVariant data(const QModelIndex& i, int role) const override {
        if (throw_on_data)
            throw std::runtime_error("model data failed");
        return QStandardItemModel::data(i, role);
    }
    bool setData(const QModelIndex&, const QVariant&, int) override { throw std::runtime_error("model refused"); }
};

QStandardItemModel* make_table_model(QObject* parent, const QStringList& headers, const QList<QStringList>& rows) {
    auto* m = new QStandardItemModel(static_cast<int>(rows.size()), static_cast<int>(headers.size()), parent);
    m->setHorizontalHeaderLabels(headers);
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < headers.size(); ++c)
            m->setItem(r, c, new QStandardItem(rows[r].value(c)));
    }
    return m;
}

} // namespace

class TestMcpUiEngine : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        test_host::set_verdict(ToolApprovalVerdict::Denied);
        test_host::approvals().clear();
        test_host::set_locked(false);
    }

    // ── Refs ────────────────────────────────────────────────────────────────

    void refs_stay_stable_across_relayout() {
        Screen s;
        auto* b = new QPushButton(QStringLiteral("Refresh"));
        s.layout->addWidget(b);
        s.show();
        const QString first = text_of(snapshot(s.target()));
        const QString r1 = ref_on_line(first, {QStringLiteral("button \"Refresh\"")});
        QVERIFY2(!r1.isEmpty(), qPrintable(first));
        s.frame->resize(1200, 800);
        QFont f = s.frame->font();
        f.setPointSize(f.pointSize() + 4);
        s.frame->setFont(f);
        QTest::qWait(30);
        const QString second = text_of(snapshot(s.target()));
        QCOMPARE(ref_on_line(second, {QStringLiteral("button \"Refresh\"")}), r1);
    }

    void stale_ref_names_nearest_match_and_never_acts() {
        Screen s;
        auto* old = new QPushButton(QStringLiteral("Refresh"));
        s.layout->addWidget(old);
        s.show();
        const QString r1 = ref_on_line(text_of(snapshot(s.target())), {QStringLiteral("button \"Refresh\"")});
        delete old;
        auto* fresh = new QPushButton(QStringLiteral("Refresh"));
        int clicks = 0;
        connect(fresh, &QPushButton::clicked, [&clicks]() { ++clicks; });
        s.layout->addWidget(fresh);
        QTest::qWait(20);
        const ToolResult r = act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), r1}});
        QVERIFY2(has_code(r, "stale_ref"), qPrintable(text_of(r)));
        QVERIFY2(r.error.contains(QStringLiteral("the same control now:")) &&
                     r.error.contains(QStringLiteral("button \"Refresh\"")),
                 qPrintable(r.error));
        QCOMPARE(clicks, 0);
        // The hint's ref works.
        const QString hinted = r.error.section(QStringLiteral("now: "), 1).section(QLatin1Char(' '), 0, 0);
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), hinted}}).success);
        QCOMPARE(clicks, 1);
    }

    void click_that_deletes_its_own_button_or_panel() {
        Screen s;
        auto* tip = new QPushButton(QStringLiteral("Hide tip"));
        connect(tip, &QPushButton::clicked, tip, &QObject::deleteLater);
        auto* done = new QPushButton(QStringLiteral("Done"));
        QPointer<QWidget> panel = s.panel;
        connect(done, &QPushButton::clicked, s.panel, [panel]() {
            if (panel)
                panel->deleteLater();
        });
        s.layout->addWidget(tip);
        s.layout->addWidget(done);
        s.show();
        QPointer<QPushButton> tip_guard(tip);
        const ToolResult r1 =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Hide tip")}});
        QVERIFY2(r1.success, qPrintable(text_of(r1)));
        QTest::qWait(20);
        QVERIFY(tip_guard.isNull());
        const ToolResult r2 =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Done")}});
        QVERIFY2(r2.success, qPrintable(text_of(r2)));
        QTest::qWait(20);
        QVERIFY(panel.isNull());
        // The window is still readable afterwards.
        QVERIFY(snapshot({{QStringLiteral("target"), QStringLiteral("window")}}).success);
    }

    void model_reset_between_read_and_act() {
        Screen s;
        auto* view = new QTableView;
        auto* model = make_table_model(view, {QStringLiteral("Symbol"), QStringLiteral("Note")},
                                       {{QStringLiteral("AAPL"), QStringLiteral("a")},
                                        {QStringLiteral("MSFT"), QStringLiteral("b")},
                                        {QStringLiteral("NVDA"), QStringLiteral("c")}});
        view->setModel(model);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        s.layout->addWidget(view);
        s.show();
        const QString tref = ref_on_line(text_of(snapshot(s.target())), {QStringLiteral("table")});
        QVERIFY(read({{QStringLiteral("ref"), tref}}).success);
        // Rebuilt with MSFT first: r1 (listed as MSFT) follows MSFT.
        model->removeRows(0, model->rowCount());
        for (const QString& sym : {QStringLiteral("MSFT"), QStringLiteral("TSLA")})
            model->appendRow({new QStandardItem(sym), new QStandardItem(QStringLiteral("x"))});
        const ToolResult moved =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), tref + QStringLiteral(":r1")}});
        QVERIFY2(moved.success, qPrintable(text_of(moved)));
        QCOMPARE(view->currentIndex().row(), 0);
        // NVDA (r2) is gone: refused, never another row.
        const ToolResult gone =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), tref + QStringLiteral(":r2")}});
        QVERIFY2(has_code(gone, "stale_ref") && gone.error.contains(QStringLiteral("no longer in that list")),
                 qPrintable(text_of(gone)));
        QCOMPARE(view->currentIndex().row(), 0);
    }

    // ── Editors ─────────────────────────────────────────────────────────────

    void editors_text_fields_validate() {
        Screen s;
        auto* form = new QFormLayout;
        auto* qty = new QLineEdit;
        qty->setValidator(new QIntValidator(0, 999, qty));
        form->addRow(QStringLiteral("Qty"), qty);
        auto* code = new QLineEdit;
        code->setMaxLength(3);
        form->addRow(QStringLiteral("Code"), code);
        auto* when = new QLineEdit;
        when->setInputMask(QStringLiteral("99-99"));
        form->addRow(QStringLiteral("Slot"), when);
        auto* notes = new QPlainTextEdit;
        form->addRow(QStringLiteral("Notes"), notes);
        auto* ticker = new QLineEdit; // normalises what it gets, as symbol fields do
        connect(ticker, &QLineEdit::textEdited, ticker, [ticker](const QString& t) {
            if (t != t.toUpper())
                ticker->setText(t.toUpper());
        });
        form->addRow(QStringLiteral("Ticker"), ticker);
        s.layout->addLayout(form);
        s.show();
        auto type = [](const QString& name, const QString& text) {
            return act({{QStringLiteral("action"), QStringLiteral("type")},
                        {QStringLiteral("name"), name},
                        {QStringLiteral("text"), text}});
        };
        QVERIFY(has_code(type(QStringLiteral("Qty"), QStringLiteral("abc")), "invalid_value"));
        QVERIFY(qty->text().isEmpty());
        QVERIFY(type(QStringLiteral("Qty"), QStringLiteral("42")).success);
        QCOMPARE(qty->text(), QStringLiteral("42"));
        QVERIFY(has_code(type(QStringLiteral("Code"), QStringLiteral("12345")), "invalid_value"));
        QVERIFY(type(QStringLiteral("Slot"), QStringLiteral("1234")).success);
        QCOMPARE(when->displayText(), QStringLiteral("12-34"));
        QVERIFY2(has_code(type(QStringLiteral("Slot"), QStringLiteral("1")), "invalid_value"),
                 qPrintable(when->displayText()));
        QVERIFY(type(QStringLiteral("Notes"), QStringLiteral("line one")).success);
        QCOMPARE(notes->toPlainText(), QStringLiteral("line one"));
        const ToolResult up = type(QStringLiteral("Ticker"), QStringLiteral("aapl"));
        QVERIFY2(up.success && up.message.contains(QStringLiteral("\"AAPL\"")), qPrintable(text_of(up)));
    }

    void editors_numbers_dates_ranges_refused_not_clamped() {
        Screen s;
        auto* form = new QFormLayout;
        auto* spin = new QSpinBox;
        spin->setRange(0, 100);
        form->addRow(QStringLiteral("Quantity"), spin);
        auto* dspin = new QDoubleSpinBox;
        dspin->setDecimals(2);
        dspin->setRange(0, 10);
        form->addRow(QStringLiteral("Weight"), dspin);
        auto* date = new QDateEdit(QDate(2026, 1, 1));
        date->setDateRange(QDate(2020, 1, 1), QDate(2030, 12, 31));
        form->addRow(QStringLiteral("Start"), date);
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 10);
        form->addRow(QStringLiteral("Level"), slider);
        s.layout->addLayout(form);
        s.show();
        auto set = [](const QString& name, const QJsonValue& v) {
            return act({{QStringLiteral("action"), QStringLiteral("set_value")},
                        {QStringLiteral("name"), name},
                        {QStringLiteral("value"), v}});
        };
        QVERIFY(has_code(set(QStringLiteral("Quantity"), 500), "invalid_value"));
        QCOMPARE(spin->value(), 0);
        QVERIFY(set(QStringLiteral("Quantity"), 42).success);
        QCOMPARE(spin->value(), 42);
        const ToolResult w = set(QStringLiteral("Weight"), 1.234);
        QVERIFY2(w.success && w.message.contains(QStringLiteral("keeps 2 decimals")), qPrintable(text_of(w)));
        QVERIFY(set(QStringLiteral("Start"), QStringLiteral("2026-11-08")).success);
        QCOMPARE(date->date(), QDate(2026, 11, 8));
        QVERIFY(has_code(set(QStringLiteral("Start"), QStringLiteral("1999-01-01")), "invalid_value"));
        QVERIFY(has_code(set(QStringLiteral("Start"), QStringLiteral("not a date")), "invalid_value"));
        QVERIFY(has_code(set(QStringLiteral("Level"), 11), "invalid_value"));
        QVERIFY(set(QStringLiteral("Level"), 7).success);
        QCOMPARE(slider->value(), 7);
    }

    void editors_choices_are_exact_or_ambiguous() {
        Screen s;
        auto* form = new QFormLayout;
        auto* interval = new QComboBox;
        interval->addItem(QStringLiteral("1D"), QStringLiteral("d"));
        interval->addItem(QStringLiteral("1W"), QStringLiteral("w"));
        interval->addItem(QStringLiteral("1M"), QStringLiteral("m"));
        form->addRow(QStringLiteral("Interval"), interval);
        s.layout->addLayout(form);
        auto* check = new QCheckBox(QStringLiteral("Auto refresh"));
        s.layout->addWidget(check);
        auto* group = new QGroupBox(QStringLiteral("Bars"));
        auto* gl = new QVBoxLayout(group);
        auto* daily = new QRadioButton(QStringLiteral("Daily"));
        auto* daily_close = new QRadioButton(QStringLiteral("Daily close"));
        auto* weekly = new QRadioButton(QStringLiteral("Weekly"));
        gl->addWidget(daily);
        gl->addWidget(daily_close);
        gl->addWidget(weekly);
        s.layout->addWidget(group);
        s.show();
        auto select = [](const QString& name, const QString& text) {
            return act({{QStringLiteral("action"), QStringLiteral("select")},
                        {QStringLiteral("name"), name},
                        {QStringLiteral("text"), text}});
        };
        QVERIFY(select(QStringLiteral("Interval"), QStringLiteral("1W")).success);
        QCOMPARE(interval->currentIndex(), 1);
        QVERIFY(select(QStringLiteral("Interval"), QStringLiteral("#2")).success);
        QCOMPARE(interval->currentIndex(), 2);
        QVERIFY(select(QStringLiteral("Interval"), QStringLiteral("d")).success); // by data
        QCOMPARE(interval->currentIndex(), 0);
        const ToolResult amb = select(QStringLiteral("Interval"), QStringLiteral("1"));
        QVERIFY2(has_code(amb, "ambiguous") && amb.error.contains(QStringLiteral("1W")), qPrintable(text_of(amb)));
        QVERIFY(has_code(select(QStringLiteral("Interval"), QStringLiteral("Year")), "not_found"));
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("check")}, {QStringLiteral("name"), QStringLiteral("Auto refresh")}})
                    .success);
        QVERIFY(check->isChecked());
        QVERIFY(select(QStringLiteral("Bars"), QStringLiteral("Daily")).success);
        QVERIFY(daily->isChecked() && !daily_close->isChecked());
        QVERIFY(select(QStringLiteral("Bars"), QStringLiteral("week")).success);
        QVERIFY(weekly->isChecked());
        QVERIFY(has_code(select(QStringLiteral("Bars"), QStringLiteral("Dai")), "ambiguous"));
    }

    void table_cell_edit_through_the_model() {
        Screen s;
        auto* view = new QTableView;
        auto* model = new QStandardItemModel(2, 2, view);
        model->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Weight")});
        for (int r = 0; r < 2; ++r) {
            model->setItem(r, 0, new QStandardItem(r ? QStringLiteral("Bonds") : QStringLiteral("Equity")));
            auto* w = new QStandardItem;
            w->setData(0.25, Qt::EditRole);
            model->setItem(r, 1, w);
        }
        model->item(0, 0)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled); // Name is not editable
        view->setModel(model);
        s.layout->addWidget(view);
        s.show();
        const QString tref = ref_on_line(text_of(snapshot(s.target())), {QStringLiteral("table")});
        const ToolResult ok = act({{QStringLiteral("action"), QStringLiteral("set_value")},
                                   {QStringLiteral("ref"), tref + QStringLiteral(":r1")},
                                   {QStringLiteral("column"), QStringLiteral("Weight")},
                                   {QStringLiteral("value"), 0.5}});
        QVERIFY2(ok.success, qPrintable(text_of(ok)));
        QCOMPARE(model->item(1, 1)->data(Qt::EditRole).typeId(), static_cast<int>(QMetaType::Double));
        QCOMPARE(model->item(1, 1)->data(Qt::EditRole).toDouble(), 0.5);
        QVERIFY(has_code(act({{QStringLiteral("action"), QStringLiteral("type")},
                              {QStringLiteral("ref"), tref + QStringLiteral(":r1")},
                              {QStringLiteral("column"), QStringLiteral("Weight")},
                              {QStringLiteral("text"), QStringLiteral("heavy")}}),
                         "invalid_value"));
        QVERIFY(has_code(act({{QStringLiteral("action"), QStringLiteral("type")},
                              {QStringLiteral("ref"), tref + QStringLiteral(":r0")},
                              {QStringLiteral("column"), QStringLiteral("Name")},
                              {QStringLiteral("text"), QStringLiteral("x")}}),
                         "unsupported"));
    }

    void menu_button_opens_menu_and_item_is_operable() {
        Screen s;
        auto* b = new QPushButton(QStringLiteral("Views"));
        auto* menu = new QMenu(b);
        int beta = 0;
        menu->addAction(QStringLiteral("Alpha"));
        connect(menu->addAction(QStringLiteral("Beta")), &QAction::triggered, [&beta]() { ++beta; });
        b->setMenu(menu);
        s.layout->addWidget(b);
        s.show();
        // The click opens the menu (an exec()'d popup): the answer lists its
        // items; the next call — made while the menu is open — picks one.
        auto second = std::make_shared<std::optional<ToolResult>>();
        QTimer::singleShot(700, qApp, [second]() {
            eng::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                    {QStringLiteral("name"), QStringLiteral("Beta")},
                                    {QStringLiteral("role"), QStringLiteral("menuitem")}},
                        kOrigin, [second](ToolResult r) { *second = std::move(r); });
        });
        const ToolResult first =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Views")}});
        QElapsedTimer t;
        t.start();
        while (!second->has_value() && t.elapsed() < 10000)
            QTest::qWait(10);
        QVERIFY2(first.success && first.message.contains(QStringLiteral("menu")) &&
                     first.message.contains(QStringLiteral("\"Beta\"")),
                 qPrintable(text_of(first)));
        QVERIFY2(second->has_value() && (*second)->success, second->has_value() ? qPrintable(text_of(**second)) : "none");
        QTest::qWait(50);
        QCOMPARE(beta, 1);
    }

    // ── Tables ──────────────────────────────────────────────────────────────

    void virtual_100k_row_table_reads_in_bounded_pages() {
        Screen s;
        auto* view = new QTableView;
        auto* model = new BigModel;
        model->setParent(view);
        view->setModel(model);
        s.layout->addWidget(view);
        s.show();
        QElapsedTimer t;
        t.start();
        const QString snap = text_of(snapshot(s.target()));
        const qint64 snap_ms = t.elapsed();
        QVERIFY2(snap.contains(QStringLiteral("100000 rows")), qPrintable(snap.left(400)));
        const QString tref = ref_on_line(snap, {QStringLiteral("table")});
        t.restart();
        const ToolResult page = read({{QStringLiteral("ref"), tref},
                                      {QStringLiteral("offset"), 99990},
                                      {QStringLiteral("limit"), 10}});
        const qint64 page_ms = t.elapsed();
        QVERIFY2(page.success && page.message.contains(tref + QStringLiteral(":r99990 | S99990")) &&
                     page.message.contains(QStringLiteral("showing rows 99990–99999 of 100000")),
                 qPrintable(text_of(page)));
        t.restart();
        const ToolResult found = read({{QStringLiteral("ref"), tref}, {QStringLiteral("query"), QStringLiteral("Symbol:S99999")}});
        const qint64 query_ms = t.elapsed();
        QVERIFY(found.success);
        QVERIFY2(found.message.contains(QStringLiteral(":r99999")) ||
                     found.message.contains(QStringLiteral("searched the first")),
                 qPrintable(found.message.left(400)));
        qInfo("100k rows: snapshot %lld ms, page at 99990 %lld ms, column query %lld ms", snap_ms, page_ms, query_ms);
        QVERIFY2(snap_ms < 1500 && page_ms < 1500 && query_ms < 2500, "a 100k-row read must stay bounded");
        // The page is acted on by the listed row.
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("click")},
                     {QStringLiteral("ref"), tref + QStringLiteral(":r99995")}})
                    .success);
        QCOMPARE(view->currentIndex().row(), 99995);
    }

    void table_json_has_header_typed_cells_and_units() {
        Screen s;
        auto* view = new QTableView;
        auto* model = new BigModel;
        model->rows = 30;
        model->setParent(view);
        view->setModel(model);
        s.layout->addWidget(view);
        s.show();
        const QString tref = ref_on_line(text_of(snapshot(s.target())), {QStringLiteral("table")});
        const ToolResult r = read({{QStringLiteral("ref"), tref}, {QStringLiteral("format"), QStringLiteral("json")},
                                   {QStringLiteral("limit"), 3}});
        QVERIFY2(r.success, qPrintable(text_of(r)));
        const QJsonObject o = QJsonDocument::fromJson(r.message.toUtf8()).object();
        QCOMPARE(o.value(QStringLiteral("ref")).toString(), tref);
        QCOMPARE(o.value(QStringLiteral("rows")).toInt(), 30);
        QCOMPARE(o.value(QStringLiteral("cols")).toArray().at(0).toString(), QStringLiteral("#"));
        QCOMPARE(o.value(QStringLiteral("units")).toObject().value(QStringLiteral("Last")).toString(), QStringLiteral("$"));
        QCOMPARE(o.value(QStringLiteral("units")).toObject().value(QStringLiteral("Chg")).toString(), QStringLiteral("%"));
        const QJsonArray row1 = o.value(QStringLiteral("data")).toArray().at(1).toArray();
        QCOMPARE(row1.at(0).toInt(), 1);                  // row number → ref e…:r1
        QCOMPARE(row1.at(1).toString(), QStringLiteral("S1"));
        QCOMPARE(row1.at(2).toDouble(), 100.01);          // raw number, not "$100.01"
        QCOMPARE(row1.at(3).toDouble(), -2.0);            // "-2%"
        QCOMPARE(row1.at(4).toDouble(), 1000.0);          // "1K"
        QCOMPARE(o.value(QStringLiteral("next")).toInt(), 3);
    }

    // ── Batches and waits ───────────────────────────────────────────────────

    void batch_stops_at_the_first_failure() {
        Screen s;
        auto* form = new QFormLayout;
        auto* sym = new QLineEdit;
        form->addRow(QStringLiteral("Symbol"), sym);
        s.layout->addLayout(form);
        auto* refresh = new QPushButton(QStringLiteral("Refresh"));
        int refreshed = 0;
        connect(refresh, &QPushButton::clicked, [&refreshed]() { ++refreshed; });
        s.layout->addWidget(refresh);
        s.show();
        const ToolResult r = act({{QStringLiteral("steps"),
                                   QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("type")},
                                                          {QStringLiteral("name"), QStringLiteral("Symbol")},
                                                          {QStringLiteral("text"), QStringLiteral("AAPL")}},
                                              QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                          {QStringLiteral("name"), QStringLiteral("No such button")}},
                                              QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                          {QStringLiteral("name"), QStringLiteral("Refresh")}}}}});
        QVERIFY2(has_code(r, "not_found") && r.error.contains(QStringLiteral("step 2 of 3 failed")) &&
                     r.error.contains(QStringLiteral("1. type")) && r.error.contains(QStringLiteral("not run")),
                 qPrintable(text_of(r)));
        QCOMPARE(sym->text(), QStringLiteral("AAPL"));
        QCOMPARE(refreshed, 0);
        // An unknown action refuses the whole batch before anything runs.
        sym->clear();
        const ToolResult bad = act({{QStringLiteral("steps"),
                                     QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("type")},
                                                            {QStringLiteral("name"), QStringLiteral("Symbol")},
                                                            {QStringLiteral("text"), QStringLiteral("X")}},
                                                QJsonObject{{QStringLiteral("action"), QStringLiteral("teleport")}}}}});
        QVERIFY(has_code(bad, "invalid_value"));
        QVERIFY(sym->text().isEmpty());
    }

    void batch_sets_clicks_waits_and_reads_in_one_call() {
        Screen s;
        auto* form = new QFormLayout;
        auto* sym = new QLineEdit;
        form->addRow(QStringLiteral("Symbol"), sym);
        s.layout->addLayout(form);
        auto* run_btn = new QPushButton(QStringLiteral("Run"));
        auto* status = new QLabel(QStringLiteral("Idle"));
        auto* table = new QTableWidget(0, 2);
        table->setHorizontalHeaderLabels({QStringLiteral("Symbol"), QStringLiteral("Price")});
        s.layout->addWidget(run_btn);
        s.layout->addWidget(status);
        s.layout->addWidget(table);
        connect(run_btn, &QPushButton::clicked, [sym, status, table]() {
            status->setText(QStringLiteral("Loading…"));
            QTimer::singleShot(400, status, [sym, status, table]() {
                table->setRowCount(2);
                table->setItem(0, 0, new QTableWidgetItem(sym->text()));
                table->setItem(0, 1, new QTableWidgetItem(QStringLiteral("189.30")));
                table->setItem(1, 0, new QTableWidgetItem(QStringLiteral("MSFT")));
                table->setItem(1, 1, new QTableWidgetItem(QStringLiteral("412.10")));
                status->setText(QStringLiteral("Loaded 2 rows"));
            });
        });
        s.show();
        const QString tref = ref_on_line(text_of(snapshot(s.target())), {QStringLiteral("table")});
        const QJsonObject batch_args{{QStringLiteral("steps"),
                                   QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("type")},
                                                          {QStringLiteral("name"), QStringLiteral("Symbol")},
                                                          {QStringLiteral("text"), QStringLiteral("AAPL")}},
                                              QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                          {QStringLiteral("name"), QStringLiteral("Run")}},
                                              QJsonObject{{QStringLiteral("action"), QStringLiteral("wait")},
                                                          {QStringLiteral("until"), QStringLiteral("text")},
                                                          {QStringLiteral("text"), QStringLiteral("Loaded")}},
                                              QJsonObject{{QStringLiteral("action"), QStringLiteral("read")},
                                                          {QStringLiteral("ref"), tref}}}}};
        const ToolResult r = act(batch_args);
        QVERIFY2(r.success && r.message.contains(QStringLiteral("3. wait text")) &&
                     r.message.contains(tref + QStringLiteral(":r0 | AAPL | 189.30")),
                 qPrintable(text_of(r)));

        // The same task as separate calls (type, click, wait, read), for the record.
        sym->clear();
        status->setText(QStringLiteral("Idle"));
        table->setRowCount(0);
        int separate_bytes = 0;
        const QList<QJsonObject> separate_acts = {
            {{QStringLiteral("action"), QStringLiteral("type")}, {QStringLiteral("name"), QStringLiteral("Symbol")},
             {QStringLiteral("text"), QStringLiteral("AAPL")}},
            {{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Run")}}};
        for (const QJsonObject& a : separate_acts) {
            const ToolResult x = act(a);
            QVERIFY(x.success);
            separate_bytes += static_cast<int>(x.message.toUtf8().size() + QJsonDocument(a).toJson(QJsonDocument::Compact).size());
        }
        const QJsonObject w{{QStringLiteral("until"), QStringLiteral("text")}, {QStringLiteral("text"), QStringLiteral("Loaded")}};
        const ToolResult wr = wait_for(w);
        QVERIFY(wr.success);
        separate_bytes += static_cast<int>(wr.message.toUtf8().size() + QJsonDocument(w).toJson(QJsonDocument::Compact).size());
        const QJsonObject rd{{QStringLiteral("ref"), tref}};
        const ToolResult rr = read(rd);
        QVERIFY(rr.success);
        separate_bytes += static_cast<int>(rr.message.toUtf8().size() + QJsonDocument(rd).toJson(QJsonDocument::Compact).size());
        qInfo("set+click+wait+read: one batched call %d bytes (request + answer); as 4 separate calls %d bytes",
              static_cast<int>(r.message.toUtf8().size() + QJsonDocument(batch_args).toJson(QJsonDocument::Compact).size()),
              separate_bytes);
    }

    void waits_are_bounded_and_cancellable() {
        Screen s;
        auto* overlay_host = new QWidget;
        s.layout->addWidget(overlay_host);
        auto* overlay = new FakeLoadingOverlay(overlay_host);
        overlay->resize(50, 50);
        s.show();
        QElapsedTimer t;
        t.start();
        const ToolResult never = wait_for({{QStringLiteral("until"), QStringLiteral("text")},
                                           {QStringLiteral("text"), QStringLiteral("never shows")},
                                           {QStringLiteral("timeout_ms"), 400}});
        QVERIFY2(has_code(never, "timeout") && t.elapsed() >= 350 && t.elapsed() < 3000, qPrintable(text_of(never)));
        const ToolResult busy = wait_for({{QStringLiteral("until"), QStringLiteral("idle")}, {QStringLiteral("timeout_ms"), 400}});
        QVERIFY2(has_code(busy, "timeout") && busy.error.contains(QStringLiteral("loading indicator")),
                 qPrintable(text_of(busy)));
        overlay->hide();
        QVERIFY(wait_for({{QStringLiteral("until"), QStringLiteral("idle")}, {QStringLiteral("timeout_ms"), 2000}}).success);
        QElapsedTimer c;
        c.start();
        const ToolResult cancelled = wait_for({{QStringLiteral("until"), QStringLiteral("time")}, {QStringLiteral("timeout_ms"), 20000}},
                                              [&c]() { return c.elapsed() > 300; });
        QVERIFY2(has_code(cancelled, "cancelled") && c.elapsed() < 3000, qPrintable(text_of(cancelled)));
    }

    void modal_dialog_opened_by_a_click_is_reported_and_operable() {
        Screen s;
        auto* ask = new QPushButton(QStringLiteral("Ask"));
        auto* other = new QPushButton(QStringLiteral("Other"));
        int answered = 0, others = 0;
        connect(ask, &QPushButton::clicked, s.frame, [&answered, &s]() {
            QMessageBox::question(s.frame, QStringLiteral("Continue?"), QStringLiteral("Proceed with the test?"));
            ++answered;
        });
        connect(other, &QPushButton::clicked, [&others]() { ++others; });
        s.layout->addWidget(ask);
        s.layout->addWidget(other);
        s.show();
        auto blocked = std::make_shared<std::optional<ToolResult>>();
        auto answer = std::make_shared<std::optional<ToolResult>>();
        QTimer::singleShot(700, qApp, [blocked]() {
            eng::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                    {QStringLiteral("name"), QStringLiteral("Other")}},
                        kOrigin, [blocked](ToolResult r) { *blocked = std::move(r); });
        });
        QTimer::singleShot(1300, qApp, [answer]() {
            eng::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                    {QStringLiteral("name"), QStringLiteral("No")},
                                    {QStringLiteral("target"), QStringLiteral("dialog")}},
                        kOrigin, [answer](ToolResult r) { *answer = std::move(r); });
        });
        QElapsedTimer t;
        t.start();
        const ToolResult opened =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Ask")}});
        const qint64 opened_ms = t.elapsed();
        while ((!answer->has_value() || answered == 0) && t.elapsed() < 15000)
            QTest::qWait(10);
        QVERIFY2(opened.success && opened.message.contains(QStringLiteral("dialog")) &&
                     opened.message.contains(QStringLiteral("opened")),
                 qPrintable(text_of(opened)));
        QVERIFY2(opened_ms < 3000 || answered, "the click must answer while the dialog is still open");
        QVERIFY2(blocked->has_value() && has_code(**blocked, "obscured") && others == 0,
                 blocked->has_value() ? qPrintable(text_of(**blocked)) : "none");
        QVERIFY2(answer->has_value() && (*answer)->success && answered == 1,
                 answer->has_value() ? qPrintable(text_of(**answer)) : "none");
    }

    void file_dialogs_open_as_qt_dialogs_during_an_action() {
        Screen s;
        auto* b = new QPushButton(QStringLiteral("Browse"));
        bool seen = false;
        connect(b, &QPushButton::clicked, [&seen]() { seen = QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs); });
        s.layout->addWidget(b);
        s.show();
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Browse")}})
                    .success);
        QVERIFY(seen);
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs)); // restored
    }

    // ── Safety ──────────────────────────────────────────────────────────────

    void approval_is_asked_on_every_generic_path_to_an_order() {
        Screen s;
        int fired = 0;
        auto add = [&](const QString& text) {
            auto* b = new QPushButton(text);
            b->setAutoDefault(false);
            connect(b, &QPushButton::clicked, [&fired]() { ++fired; });
            s.layout->addWidget(b);
            return b;
        };
        add(QStringLiteral("Place order"));
        add(QStringLiteral("CANCEL ALL"));
        add(QStringLiteral("EXIT ALL"));
        QPushButton* go = add(QStringLiteral("Go"));
        go->setProperty("fincept_agent_risk", QStringLiteral("order")); // a screen's declarative marker
        add(QStringLiteral("Refresh"));
        auto* ticket = new QWidget;
        ticket->setProperty("fincept_agent_risk", QStringLiteral("order"));
        ticket->setProperty("fincept_agent_risk_inherit", true); // the whole ticket
        auto* tl = new QFormLayout(ticket);
        auto* qty = new QLineEdit;
        tl->addRow(QStringLiteral("Ticket qty"), qty);
        connect(qty, &QLineEdit::returnPressed, [&fired]() { ++fired; });
        s.layout->addWidget(ticket);
        auto* blotter = new QTableWidget(1, 3);
        blotter->setHorizontalHeaderLabels({QStringLiteral("Symbol"), QStringLiteral("Side"), QStringLiteral("Action")});
        blotter->setItem(0, 0, new QTableWidgetItem(QStringLiteral("TSLA")));
        blotter->setItem(0, 1, new QTableWidgetItem(QStringLiteral("SELL")));
        auto* row_cancel = new QPushButton(QStringLiteral("Cancel"));
        connect(row_cancel, &QPushButton::clicked, [&fired]() { ++fired; });
        blotter->setCellWidget(0, 2, row_cancel);
        s.layout->addWidget(blotter);
        s.show();

        auto expect_asked = [&](const QJsonObject& a, const char* what) {
            const int before = static_cast<int>(test_host::approvals().size());
            const ToolResult r = act(a);
            QVERIFY2(has_code(r, "denied"), qPrintable(QStringLiteral("%1: %2").arg(QString::fromLatin1(what), text_of(r))));
            QVERIFY2(test_host::approvals().size() == before + 1, what);
            QVERIFY2(fired == 0, what);
        };
        auto click = [](const QString& name) {
            return QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), name}};
        };
        expect_asked(click(QStringLiteral("Place order")), "click by name");
        expect_asked(click(QStringLiteral("CANCEL ALL")), "cancel all");
        expect_asked(click(QStringLiteral("EXIT ALL")), "exit all positions");
        expect_asked(click(QStringLiteral("Go")), "declarative marker");
        // A click at x/y that lands on "Place order" is a click on it.
        QPushButton* place = nullptr;
        for (QPushButton* b : s.panel->findChildren<QPushButton*>()) {
            if (b->text() == QLatin1String("Place order"))
                place = b;
        }
        const QPoint at = place->mapTo(s.panel, place->rect().center());
        expect_asked({{QStringLiteral("action"), QStringLiteral("click")},
                      {QStringLiteral("target"), s.panel_ref},
                      {QStringLiteral("x"), at.x()},
                      {QStringLiteral("y"), at.y()}},
                     "click at a point");
        // Space on the focused order button.
        place->setFocus();
        expect_asked({{QStringLiteral("action"), QStringLiteral("press")},
                      {QStringLiteral("ref"), eng::Refs::instance().ref_for(place, eng::RefKind::Element)},
                      {QStringLiteral("text"), QStringLiteral("Space")}},
                     "space on the button");
        // Enter in a field of a marked order ticket.
        expect_asked({{QStringLiteral("action"), QStringLiteral("type")},
                      {QStringLiteral("name"), QStringLiteral("Ticket qty")},
                      {QStringLiteral("text"), QStringLiteral("5")},
                      {QStringLiteral("submit"), true}},
                     "enter in a marked ticket");
        // "Cancel" on an order row cancels that order.
        expect_asked({{QStringLiteral("action"), QStringLiteral("click")},
                      {QStringLiteral("ref"), eng::Refs::instance().ref_for(row_cancel, eng::RefKind::Element)}},
                     "row cancel on a SELL row");
        // A plain button asks nothing.
        const int before = static_cast<int>(test_host::approvals().size());
        QVERIFY(act(click(QStringLiteral("Refresh"))).success);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), before);
    }

    void order_path_one_order_step_per_call_and_no_auto_confirm() {
        test_host::set_verdict(ToolApprovalVerdict::Approved);
        Screen s;
        int bought = 0, confirmed = 0, yes = 0;
        auto* buy = new QPushButton(QStringLiteral("BUY"));
        auto* confirm = new QPushButton(QStringLiteral("Confirm order"));
        buy->setAutoDefault(false);
        confirm->setAutoDefault(false);
        QPointer<QDialog> confirm_dialog;
        connect(buy, &QPushButton::clicked, s.frame, [&]() {
            ++bought;
            // The app's own confirmation (non-modal here): what was prepared.
            confirm_dialog = new QDialog(s.frame);
            confirm_dialog->setWindowTitle(QStringLiteral("Confirm live order"));
            auto* l = new QVBoxLayout(confirm_dialog);
            l->addWidget(new QLabel(QStringLiteral("LIVE BUY 1 AAPL at market")));
            auto* ok = new QPushButton(QStringLiteral("Yes"));
            ok->setAutoDefault(false);
            l->addWidget(ok);
            connect(ok, &QPushButton::clicked, confirm_dialog.data(), [&yes]() { ++yes; });
            confirm_dialog->show();
        });
        connect(confirm, &QPushButton::clicked, [&confirmed]() { ++confirmed; });
        s.layout->addWidget(buy);
        s.layout->addWidget(confirm);
        s.show();
        // One call may not place AND confirm.
        const ToolResult batch = act({{QStringLiteral("steps"),
                                       QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                              {QStringLiteral("name"), QStringLiteral("BUY")}},
                                                  QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                              {QStringLiteral("name"), QStringLiteral("Confirm order")},
                                                              {QStringLiteral("target"), s.panel_ref}}}}});
        QVERIFY2(has_code(batch, "needs_approval") && batch.error.contains(QStringLiteral("one ui_act call holds at most one order step")),
                 qPrintable(text_of(batch)));
        QCOMPARE(bought, 1);
        QCOMPARE(confirmed, 0);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), 1); // the second was never offered
        // The app's own confirmation is never auto-confirmed by the BUY approval:
        // answering it asks the user again, with the dialog's text in front of them.
        QVERIFY(confirm_dialog);
        const ToolResult answer = act({{QStringLiteral("action"), QStringLiteral("click")},
                                       {QStringLiteral("name"), QStringLiteral("Yes")},
                                       {QStringLiteral("target"), QStringLiteral("dialog")}});
        QVERIFY2(answer.success, qPrintable(text_of(answer)));
        QCOMPARE(yes, 1);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), 2);
        QVERIFY(test_host::approvals().last().live_trading);
        QVERIFY(test_host::approvals().last().args.value(QStringLiteral("dialog says")).toString().contains(QStringLiteral("live buy 1 aapl")));
        delete confirm_dialog.data();
    }

    void paper_run_refuses_orders_without_asking() {
        Screen s;
        auto* place = new QPushButton(QStringLiteral("Place order"));
        int placed = 0;
        connect(place, &QPushButton::clicked, [&placed]() { ++placed; });
        s.layout->addWidget(place);
        s.show();
        const ToolResult r = act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Place order")}},
                                 kOrigin + QLatin1String(kPaperRunOriginSuffix));
        QVERIFY2(has_code(r, "denied") && r.error.contains(QStringLiteral("PAPER")), qPrintable(text_of(r)));
        QCOMPARE(placed, 0);
        QVERIFY(test_host::approvals().isEmpty());
    }

    void secrets_are_never_returned_or_typed() {
        Screen s;
        auto* form = new QFormLayout;
        auto* pw = new QLineEdit(QStringLiteral("hunter2"));
        pw->setEchoMode(QLineEdit::Password);
        form->addRow(QStringLiteral("Password"), pw);
        auto* key = new QLineEdit(QStringLiteral("sk-live-123"));
        key->setObjectName(QStringLiteral("api_key"));
        form->addRow(QStringLiteral("Key"), key);
        s.layout->addLayout(form);
        auto* vault = new QWidget;
        vault->setProperty("fincept_secret", true);
        auto* vl = new QVBoxLayout(vault);
        auto* token = new QLabel(QStringLiteral("TOKEN-XYZ-789"));
        vl->addWidget(token);
        s.layout->addWidget(vault);
        s.show();
        const QString snap = text_of(snapshot(s.target()));
        for (const char* secret : {"hunter2", "sk-live-123", "TOKEN-XYZ"})
            QVERIFY2(!snap.contains(QString::fromLatin1(secret)), secret);
        const QString pw_ref = ref_on_line(snap, {QStringLiteral("Password")});
        const ToolResult rd = read({{QStringLiteral("ref"), pw_ref}});
        QVERIFY(has_code(rd, "denied") && !rd.error.contains(QStringLiteral("hunter2")));
        QVERIFY(has_code(read({{QStringLiteral("ref"), eng::Refs::instance().ref_for(token, eng::RefKind::Element)}}), "denied"));
        const ToolResult typed = act({{QStringLiteral("action"), QStringLiteral("type")},
                                      {QStringLiteral("ref"), eng::Refs::instance().ref_for(key, eng::RefKind::Element)},
                                      {QStringLiteral("text"), QStringLiteral("stolen")}});
        QVERIFY(has_code(typed, "denied"));
        QCOMPARE(key->text(), QStringLiteral("sk-live-123"));
        // A wait cannot be used to probe a secret either.
        QVERIFY(has_code(wait_for({{QStringLiteral("until"), QStringLiteral("text")},
                                   {QStringLiteral("text"), QStringLiteral("hunter2")},
                                   {QStringLiteral("timeout_ms"), 300}}),
                         "timeout"));
    }

    // ── Snapshots and size ──────────────────────────────────────────────────

    void snapshot_since_lists_only_changes() {
        Screen s;
        auto* status = new QLabel(QStringLiteral("Status: idle"));
        s.layout->addWidget(status);
        for (int i = 0; i < 20; ++i)
            s.layout->addWidget(new QPushButton(QStringLiteral("Button %1").arg(i)));
        s.show();
        const QString full = text_of(snapshot(s.target()));
        const QString id = full.section(QStringLiteral("snap "), 1, 1).section(QLatin1Char('\n'), 0, 0).trimmed();
        QVERIFY2(id.startsWith(QLatin1Char('s')), qPrintable(full.left(200)));
        status->setText(QStringLiteral("Status: done"));
        const QString changes = text_of(snapshot({{QStringLiteral("target"), s.panel_ref}, {QStringLiteral("since"), id}}));
        QVERIFY2(changes.contains(QStringLiteral("changes since ") + id) &&
                     changes.contains(QStringLiteral("+ text \"Status: done\"")) &&
                     changes.contains(QStringLiteral("unchanged")) && !changes.contains(QStringLiteral("Button 7")),
                 qPrintable(changes));
        QVERIFY(changes.size() * 3 < full.size());
        qInfo("snapshot %d bytes; after one change, since= answers %d bytes", static_cast<int>(full.toUtf8().size()),
              static_cast<int>(changes.toUtf8().size()));
    }

    void size_budgets_for_a_busy_screen_and_a_500_row_table() {
        Screen s;
        auto* form = new QFormLayout;
        for (int i = 0; i < 30; ++i)
            form->addRow(QStringLiteral("Field %1").arg(i), new QLineEdit(QStringLiteral("value %1").arg(i)));
        s.layout->addLayout(form);
        for (int i = 0; i < 10; ++i)
            s.layout->addWidget(new QPushButton(QStringLiteral("Action %1").arg(i)));
        for (int i = 0; i < 6; ++i) {
            auto* tile = new QWidget;
            auto* tl = new QVBoxLayout(tile);
            tl->addWidget(new QLabel(QStringLiteral("METRIC %1").arg(QChar('A' + i))));
            tl->addWidget(new QLabel(QStringLiteral("$%1.5B").arg(i + 1)));
            s.layout->addWidget(tile);
        }
        auto* view = new QTableView;
        auto* model = new BigModel;
        model->rows = 500;
        model->setParent(view);
        view->setModel(model);
        s.layout->addWidget(view);
        s.show(1280, 1600);
        const QString snap = text_of(snapshot(s.target()));
        const int snap_bytes = static_cast<int>(snap.toUtf8().size());
        const QString tref = ref_on_line(snap, {QStringLiteral("table")});
        const ToolResult text500 = read({{QStringLiteral("ref"), tref}, {QStringLiteral("limit"), 500}});
        const ToolResult json500 = read({{QStringLiteral("ref"), tref}, {QStringLiteral("limit"), 500},
                                         {QStringLiteral("format"), QStringLiteral("json")}});
        const int text_bytes = static_cast<int>(text500.message.toUtf8().size());
        const int json_bytes = static_cast<int>(json500.message.toUtf8().size());
        qInfo("busy screen snapshot %d bytes; 500-row table: text %d bytes, json %d bytes", snap_bytes, text_bytes,
              json_bytes);
        QVERIFY2(snap.contains(QStringLiteral("METRIC A: $1.5B")), "a metric tile reads as one line");
        QVERIFY2(snap_bytes < 6500, "busy-screen snapshot outgrew its budget");
        QVERIFY2(text_bytes < 30000, "500-row text read outgrew its budget");
        QVERIFY2(json_bytes < 26000, "500-row json read outgrew its budget");
        QVERIFY(json_bytes < text_bytes);
    }

    // ── Errors ──────────────────────────────────────────────────────────────

    void failures_carry_a_code_and_ambiguity_is_never_guessed() {
        Screen s;
        auto* a = new QPushButton(QStringLiteral("Refresh"));
        auto* b = new QPushButton(QStringLiteral("Refresh"));
        int ca = 0, cb = 0;
        connect(a, &QPushButton::clicked, [&ca]() { ++ca; });
        connect(b, &QPushButton::clicked, [&cb]() { ++cb; });
        s.layout->addWidget(a);
        s.layout->addWidget(b);
        s.show();
        const ToolResult amb =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Refresh")}});
        QVERIFY2(has_code(amb, "ambiguous") && amb.error.count(QStringLiteral("button \"Refresh\"")) >= 2,
                 qPrintable(text_of(amb)));
        QCOMPARE(ca + cb, 0);
        const QRegularExpression coded(QStringLiteral("^[a-z_]{4,20}: "));
        const QList<ToolResult> failures = {
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), QStringLiteral("zz9")}}),
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("ref"), QStringLiteral("e99999999")}}),
            act({{QStringLiteral("action"), QStringLiteral("fly")}}),
            act(QJsonObject{}),
            read(QJsonObject{}),
            read({{QStringLiteral("ref"), QStringLiteral("res_ffffff")}}),
            wait_for({{QStringLiteral("until"), QStringLiteral("forever")}}),
            snapshot({{QStringLiteral("target"), QStringLiteral("no_such_screen")}}),
        };
        for (const ToolResult& r : failures)
            QVERIFY2(!r.success && coded.match(r.error).hasMatch(), qPrintable(r.error));
        test_host::set_locked(true);
        QVERIFY(has_code(act({{QStringLiteral("action"), QStringLiteral("click")},
                              {QStringLiteral("ref"), eng::Refs::instance().ref_for(a, eng::RefKind::Element)}}),
                         "busy"));
        test_host::set_locked(false);
    }

    void obscured_controls_are_refused_until_uncovered() {
        Screen s;
        auto* b = new QPushButton(QStringLiteral("Submit form"));
        int clicks = 0;
        connect(b, &QPushButton::clicked, [&clicks]() { ++clicks; });
        s.layout->addWidget(b);
        s.show();
        auto* cover = new FakeLoadingOverlay(s.panel);
        cover->setGeometry(s.panel->rect());
        cover->show();
        cover->raise();
        QTest::qWait(20);
        const ToolResult r =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Submit form")}});
        QVERIFY2(has_code(r, "obscured"), qPrintable(text_of(r)));
        QCOMPARE(clicks, 0);
        cover->hide();
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Submit form")}})
                    .success);
        QCOMPARE(clicks, 1);
    }

    // ── Agent attribution (core/agent/AgentActionScope.h) ───────────────────

    void agent_scope_covers_the_click_and_its_queued_follow_up_until_settled() {
        namespace ag = fincept::core::agent;
        Screen s;
        auto* rec = new QPushButton(QStringLiteral("Record"));
        QString sync, deferred, late = QStringLiteral("unset");
        connect(rec, &QPushButton::clicked, [&]() {
            sync = ag::current_agent_order_source();
            QTimer::singleShot(0, [&deferred]() { deferred = ag::current_agent_order_source(); });
            QTimer::singleShot(1200, [&late]() { late = ag::current_agent_order_source(); });
        });
        s.layout->addWidget(rec);
        s.show();
        QVERIFY(!ag::current_agent_actor());
        const ToolResult r = act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Record")}},
                                 QStringLiteral("Claude Code · terminal AI (paper run)"));
        QVERIFY2(r.success, qPrintable(text_of(r)));
        QCOMPARE(sync, QStringLiteral("agent:claude_code"));
        QCOMPARE(deferred, QStringLiteral("agent:claude_code")); // queued by the handler, before the step settled
        // Settled: nothing is marked any more, and nothing is left held.
        QVERIFY(!ag::current_agent_actor());
        QCOMPARE(ag::active_agent_holds(), 0);
        QTest::qWait(1300);
        QCOMPARE(late, QString()); // work long after the step is not the agent's
        // The run id travels with the actor.
        const auto actor = eng::agent_actor_for_origin(QStringLiteral("Fincept agent: Daily brief"));
        QCOMPARE(actor.client_id, QStringLiteral("Fincept agent"));
        QCOMPARE(actor.run_id, QStringLiteral("Daily brief"));
    }

    void agent_actor_is_the_transport_client_id_not_the_label() {
        // The MCP transport puts the caller's stable ids in scope (ScopedCallClient)
        // on the thread that dispatches; the handler wrapper carries them to the GUI
        // thread and the step runs as that client and run — whatever the label says.
        namespace ag = fincept::core::agent;
        Screen s;
        auto* rec = new QPushButton(QStringLiteral("Record"));
        QString source, run_id;
        connect(rec, &QPushButton::clicked, [&]() {
            source = ag::current_agent_order_source();
            if (const auto a = ag::current_agent_actor())
                run_id = a->run_id;
        });
        s.layout->addWidget(rec);
        s.show();
        const AsyncToolHandler h = ui_gui_handler(
            QStringLiteral("ui_act"), [](const QJsonObject& a, const QString& o, eng::CancelCheck c, eng::Reply r) {
                eng::ui_act(a, o, std::move(c), std::move(r));
            });
        const QJsonObject click{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Record")}};
        auto call_from_worker = [&](const QString& label, const QString& client, const QString& run) {
            auto promise = std::make_shared<QPromise<ToolResult>>();
            QFuture<ToolResult> f = promise->future();
            promise->start();
            std::thread worker([&h, &click, promise, label, client, run]() {
                ScopedCallOrigin origin(label);
                ScopedCallClient caller(client, run);
                h(click, ToolContext{}, promise);
            });
            worker.join();
            QElapsedTimer t;
            t.start();
            while (!f.isFinished() && t.elapsed() < 10000)
                QTest::qWait(5);
            return f.isFinished() && f.resultCount() == 1 ? f.result() : ToolResult::fail(QStringLiteral("no answer"));
        };
        // A run-token call from Claude Code: the profile id and the run, not the label.
        ToolResult r = call_from_worker(QStringLiteral("some client · terminal AI"), QStringLiteral("claude_code"),
                                        QStringLiteral("run-7"));
        QVERIFY2(r.success, qPrintable(text_of(r)));
        QCOMPARE(source, QStringLiteral("agent:claude_code"));
        QCOMPARE(run_id, QStringLiteral("run-7"));
        // A client identified by initialize.clientInfo (no run): its profile id.
        r = call_from_worker(QStringLiteral("Claude Code"), QStringLiteral("codex"), QString());
        QVERIFY2(r.success, qPrintable(text_of(r)));
        QCOMPARE(source, QStringLiteral("agent:codex"));
        QCOMPARE(run_id, QString());
        // No stable id (an older caller): the label still decides.
        r = call_from_worker(QStringLiteral("Grok · terminal AI"), QString(), QString());
        QVERIFY2(r.success, qPrintable(text_of(r)));
        QCOMPARE(source, QStringLiteral("agent:grok"));
        QVERIFY(!ag::current_agent_actor());
        QCOMPARE(ScopedCallClient::client_id(), QString()); // nothing left on the GUI thread
    }

    void approval_request_carries_kind_mode_and_ids() {
        // What the approval dialog is told (mcp/server/ApprovalPolicy.h): the kind,
        // LIVE / PAPER only as the screen shows it, the caller's stable ids, the screen.
        Screen s;
        auto* mode = new QLabel(QStringLiteral("PAPER")); // the screen's own mode indicator
        auto* place = new QPushButton(QStringLiteral("Place order"));
        place->setProperty("fincept_agent_risk", QStringLiteral("order"));
        auto* fetch = new QPushButton(QStringLiteral("Fetch quotes"));
        fetch->setProperty("fincept_agent_risk", QStringLiteral("spend"));
        s.layout->addWidget(mode);
        s.layout->addWidget(place);
        s.layout->addWidget(fetch);
        s.show();
        auto click = [](const QString& name) {
            return act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), name}});
        };
        {
            ScopedCallClient caller(QStringLiteral("claude_code"), QStringLiteral("run-9"));
            QVERIFY(has_code(click(QStringLiteral("Place order")), "denied"));
        }
        ToolApprovalRequest r = test_host::approvals().last();
        QCOMPARE(approval::kind_of(r), QStringLiteral("order"));
        QVERIFY(r.trading_mode == ApprovalTradingMode::Paper);
        QCOMPARE(r.client_id, QStringLiteral("claude_code"));
        QCOMPARE(r.run_id, QStringLiteral("run-9"));
        QVERIFY(!r.screen.isEmpty());
        QVERIFY(!approval::present(r).grant_allowed);
        // The control says LIVE while the screen shows PAPER: unclear, never "PAPER".
        place->setAccessibleDescription(QStringLiteral("LIVE BUY LIMIT 1 AAPL"));
        QVERIFY(has_code(click(QStringLiteral("Place order")), "denied"));
        QVERIFY(test_host::approvals().last().trading_mode == ApprovalTradingMode::Unknown);
        mode->setText(QStringLiteral("LIVE"));
        QVERIFY(has_code(click(QStringLiteral("Place order")), "denied"));
        QVERIFY(test_host::approvals().last().trading_mode == ApprovalTradingMode::Live);
        // A spend: its own kind; a timed grant may be offered. No stable ids → the label's.
        QVERIFY(has_code(click(QStringLiteral("Fetch quotes")), "denied"));
        r = test_host::approvals().last();
        QCOMPARE(approval::kind_of(r), QStringLiteral("spend"));
        QVERIFY(approval::present(r).grant_allowed);
        QCOMPARE(r.client_id, QStringLiteral("test-agent"));
        QCOMPARE(r.run_id, QString());
    }

    void agent_scope_ends_on_timeout_cancel_and_destruction_and_is_capped() {
        namespace ag = fincept::core::agent;
        const ag::AgentActor actor{QStringLiteral("codex"), QString()};
        {
            ag::AgentActionHold hold(actor, 50);
            QVERIFY(hold.active());
            QCOMPARE(ag::current_agent_order_source(), QStringLiteral("agent:codex"));
            QTest::qWait(80); // never released: its own deadline ends it
            QVERIFY(!hold.active());
            QVERIFY(!ag::current_agent_actor());
        }
        {
            auto leaked = std::make_unique<ag::AgentActionHold>(actor, 60000);
            QVERIFY(leaked->active());
            leaked.reset(); // destroyed = released
            QCOMPARE(ag::active_agent_holds(), 0);
        }
        {
            std::vector<std::unique_ptr<ag::AgentActionHold>> many;
            for (int i = 0; i < ag::AgentActionHold::kMaxHolds + 8; ++i)
                many.push_back(std::make_unique<ag::AgentActionHold>(actor, 60000));
            QCOMPARE(ag::active_agent_holds(), ag::AgentActionHold::kMaxHolds); // the oldest were dropped
            QVERIFY(!many.front()->active() && many.back()->active());
        }
        QCOMPARE(ag::active_agent_holds(), 0);
        // A batch the caller cancels after its first step leaves nothing marked.
        Screen s;
        int clicks = 0;
        auto* b = new QPushButton(QStringLiteral("Record"));
        connect(b, &QPushButton::clicked, [&clicks]() { ++clicks; });
        s.layout->addWidget(b);
        s.show();
        const ToolResult r = run([&](eng::Reply reply) {
            eng::ui_act(QJsonObject{{QStringLiteral("steps"),
                                     QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                            {QStringLiteral("name"), QStringLiteral("Record")}},
                                                QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                            {QStringLiteral("name"), QStringLiteral("Record")}}}}},
                        kOrigin, [&clicks]() { return clicks >= 1; }, std::move(reply));
        });
        QVERIFY2(has_code(r, "cancelled"), qPrintable(text_of(r)));
        QCOMPARE(clicks, 1);
        QVERIFY(!ag::current_agent_actor());
        QCOMPARE(ag::active_agent_holds(), 0);
    }

    void agent_scopes_nest_and_each_batch_step_is_marked() {
        namespace ag = fincept::core::agent;
        {
            ag::AgentActionScope outer({QStringLiteral("outer"), QString()});
            QCOMPARE(ag::current_agent_order_source(), QStringLiteral("agent:outer"));
            {
                ag::AgentActionScope inner({QStringLiteral("inner"), QString()});
                QCOMPARE(ag::current_agent_order_source(), QStringLiteral("agent:inner"));
            }
            QCOMPARE(ag::current_agent_order_source(), QStringLiteral("agent:outer"));
        }
        QCOMPARE(ag::current_agent_order_source(), QString());
        Screen s;
        QStringList seen;
        for (const QString& name : {QStringLiteral("First"), QStringLiteral("Second")}) {
            auto* b = new QPushButton(name);
            connect(b, &QPushButton::clicked, [&seen]() { seen << ag::current_agent_order_source(); });
            s.layout->addWidget(b);
        }
        s.show();
        QVERIFY(act({{QStringLiteral("steps"),
                      QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("First")}},
                                 QJsonObject{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Second")}}}}},
                    QStringLiteral("codex-mcp-client"))
                    .success);
        QCOMPARE(seen, (QStringList{QStringLiteral("agent:codex_mcp_client"), QStringLiteral("agent:codex_mcp_client")}));
    }

    void agent_order_source_is_sanitised() {
        namespace ag = fincept::core::agent;
        QCOMPARE(ag::agent_order_source(QStringLiteral("Claude Code")), QStringLiteral("agent:claude_code"));
        QCOMPARE(ag::agent_order_source(QStringLiteral("  Codex-MCP  Client! ")), QStringLiteral("agent:codex_mcp_client"));
        QCOMPARE(ag::agent_order_source(QStringLiteral("__gemini__cli__")), QStringLiteral("agent:gemini_cli"));
        QCOMPARE(ag::agent_order_source(QStringLiteral("Ünïcödé · ✓")), QStringLiteral("agent:n_c_d"));
        QCOMPARE(ag::agent_order_source(QString()), QStringLiteral("agent:unknown"));
        QCOMPARE(ag::agent_order_source(QStringLiteral("!!!")), QStringLiteral("agent:unknown"));
        const QString long_id = ag::agent_order_source(QString(100, QLatin1Char('x')));
        QCOMPARE(long_id, QStringLiteral("agent:") + QString(40, QLatin1Char('x')));
        static const QRegularExpression shape(QStringLiteral("^agent:[a-z0-9_]+$"));
        QVERIFY(shape.match(ag::agent_order_source(QStringLiteral("AI assistant"))).hasMatch());
    }

    void a_person_s_click_is_never_an_agent_s() {
        namespace ag = fincept::core::agent;
        Screen s;
        auto* agent_btn = new QPushButton(QStringLiteral("Record"));
        auto* person_btn = new QPushButton(QStringLiteral("Mine"));
        QString person_seen = QStringLiteral("unset");
        connect(person_btn, &QPushButton::clicked, [&person_seen]() { person_seen = ag::current_agent_order_source(); });
        s.layout->addWidget(agent_btn);
        s.layout->addWidget(person_btn);
        s.show();
        // No agent action at all.
        QTest::mouseClick(person_btn, Qt::LeftButton);
        QCOMPARE(person_seen, QString());
        // A person clicking while an agent's step is still settling.
        person_seen = QStringLiteral("unset");
        QTimer::singleShot(60, person_btn, [person_btn]() { QTest::mouseClick(person_btn, Qt::LeftButton); });
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Record")}}).success);
        QCOMPARE(person_seen, QString());
        QCOMPARE(ag::active_agent_holds(), 0);
    }

    void agent_scope_covers_the_continuation_after_a_dialog_the_agent_answers() {
        namespace ag = fincept::core::agent;
        Screen s;
        auto* ask = new QPushButton(QStringLiteral("Ask"));
        QString inside_dialog = QStringLiteral("unset"), continuation = QStringLiteral("unset");
        QTimer* background = new QTimer(s.frame);
        connect(ask, &QPushButton::clicked, s.frame, [&]() {
            QMessageBox::question(s.frame, QStringLiteral("Continue?"), QStringLiteral("Place it?"));
            continuation = ag::current_agent_order_source(); // the screen's "confirmed → place" path
        });
        // Unrelated work that runs while the dialog's loop runs is not the agent's.
        background->setInterval(900);
        background->setSingleShot(true);
        connect(background, &QTimer::timeout, [&inside_dialog]() { inside_dialog = ag::current_agent_order_source(); });
        s.layout->addWidget(ask);
        s.show();
        auto answer = std::make_shared<std::optional<ToolResult>>();
        QTimer::singleShot(300, qApp, [background]() { background->start(); });
        QTimer::singleShot(1400, qApp, [answer]() {
            eng::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                    {QStringLiteral("name"), QStringLiteral("No")},
                                    {QStringLiteral("target"), QStringLiteral("dialog")}},
                        kOrigin, [answer](ToolResult r) { *answer = std::move(r); });
        });
        const ToolResult opened =
            act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Ask")}});
        QElapsedTimer t;
        t.start();
        while ((continuation == QLatin1String("unset") || !answer->has_value()) && t.elapsed() < 10000)
            QTest::qWait(10);
        QVERIFY(opened.success);
        QVERIFY(answer->has_value() && (*answer)->success);
        QCOMPARE(inside_dialog, QString());                      // after the step settled, inside the dialog's loop
        QCOMPARE(continuation, QStringLiteral("agent:test_agent")); // the agent answered: the follow-through is its own
        QVERIFY(!ag::current_agent_actor()); // both steps settled
        QCOMPARE(ag::active_agent_holds(), 0);
    }

    // ── Markers: kinds, inheritance, keys, cells, dialogs ───────────────────

    void marker_kinds_and_container_inheritance() {
        QVERIFY(eng::risk_for_kind(QStringLiteral("code")).risky);
        QVERIFY(eng::risk_for_kind(QStringLiteral("CODE")).why.contains(QStringLiteral("runs code")));
        QVERIFY(eng::is_order_kind(eng::risk_for_kind(QStringLiteral("trade"))));
        QCOMPARE(eng::risk_for_kind(QStringLiteral("delete")).kind, QStringLiteral("destructive"));
        QVERIFY(eng::risk_for_kind(QStringLiteral("something-new")).risky); // unknown → still asks
        QVERIFY(!eng::risk_for_kind(QStringLiteral("none")).risky);

        Screen s;
        auto* tile = new QWidget;
        tile->setProperty("fincept_agent_risk", QStringLiteral("spend")); // the tile only, no inherit
        auto* tl = new QVBoxLayout(tile);
        auto* fetch = new QPushButton(QStringLiteral("Fetch"));
        tl->addWidget(fetch);
        s.layout->addWidget(tile);
        auto* run_code = new QPushButton(QStringLiteral("Execute"));
        run_code->setProperty("fincept_agent_risk", QStringLiteral("code"));
        s.layout->addWidget(run_code);
        s.show();
        auto click = [](const QString& name) {
            return act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), name}});
        };
        QVERIFY(click(QStringLiteral("Fetch")).success); // a container's marker does not reach inside
        QVERIFY(test_host::approvals().isEmpty());
        tile->setProperty("fincept_agent_risk_inherit", true);
        QVERIFY(has_code(click(QStringLiteral("Fetch")), "denied"));
        QCOMPARE(static_cast<int>(test_host::approvals().size()), 1);
        const ToolResult code = click(QStringLiteral("Execute"));
        QVERIFY(has_code(code, "denied"));
        QVERIFY(test_host::approvals().last().tool.contains(QStringLiteral("runs code")));
    }

    void keyboard_paths_are_judged_by_markers() {
        Screen s;
        int fired = 0;
        auto* form = new QFormLayout;
        auto* field = new QLineEdit;
        form->addRow(QStringLiteral("Note"), field);
        auto* prompt = new QLineEdit;
        prompt->setProperty("fincept_agent_risk", QStringLiteral("spend")); // Enter here sends a billed request
        connect(prompt, &QLineEdit::returnPressed, [&fired]() { ++fired; });
        form->addRow(QStringLiteral("Prompt"), prompt);
        s.layout->addLayout(form);
        auto* send = new QPushButton(QStringLiteral("Send"));
        send->setShortcut(QKeySequence(QStringLiteral("Ctrl+Return"))); // the ticket's own shortcut
        send->setProperty("fincept_agent_risk", QStringLiteral("order"));
        connect(send, &QPushButton::clicked, [&fired]() { ++fired; });
        s.layout->addWidget(send);
        auto* editor = new QPlainTextEdit;
        editor->setProperty("fincept_agent_risk_keys", QStringLiteral("Ctrl+Shift+R=code;Delete=delete"));
        s.layout->addWidget(editor);
        auto* run_action = new QAction(QStringLiteral("Recalc"), s.frame);
        run_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
        run_action->setProperty("fincept_agent_risk", QStringLiteral("code"));
        connect(run_action, &QAction::triggered, [&fired]() { ++fired; });
        s.frame->addAction(run_action);
        auto* holder = new QPushButton(QStringLiteral("Purge"));
        holder->setProperty("fincept_agent_risk", QStringLiteral("delete"));
        auto* purge_key = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), holder);
        connect(purge_key, &QShortcut::activated, [&fired]() { ++fired; });
        s.layout->addWidget(holder);
        s.show();
        const QString field_ref = eng::Refs::instance().ref_for(field, eng::RefKind::Element);
        const QString editor_ref = eng::Refs::instance().ref_for(editor, eng::RefKind::Element);
        auto press = [](const QString& ref, const QString& key) {
            return act({{QStringLiteral("action"), QStringLiteral("press")},
                        {QStringLiteral("ref"), ref},
                        {QStringLiteral("text"), key}});
        };
        auto asked = [](const ToolResult& r, int before) {
            return has_code(r, "denied") && test_host::approvals().size() == before + 1;
        };
        int n = 0;
        QVERIFY2(asked(press(field_ref, QStringLiteral("Ctrl+Return")), n++), "a button's own shortcut");
        QVERIFY2(asked(press(field_ref, QStringLiteral("Ctrl+R")), n++), "a marked QAction's shortcut");
        QVERIFY2(asked(press(field_ref, QStringLiteral("Ctrl+K")), n++), "a QShortcut on a marked button");
        QVERIFY2(asked(press(editor_ref, QStringLiteral("Delete")), n++), "a declared keyPressEvent key");
        QVERIFY2(asked(press(editor_ref, QStringLiteral("Ctrl+Shift+R")), n++), "a declared chord");
        QVERIFY2(asked(act({{QStringLiteral("action"), QStringLiteral("type")},
                            {QStringLiteral("name"), QStringLiteral("Prompt")},
                            {QStringLiteral("text"), QStringLiteral("hello")},
                            {QStringLiteral("submit"), true}}),
                       n++),
                 "submitting a marked field");
        QCOMPARE(fired, 0);
        // An undeclared key on the same editor, and typing without submit, ask nothing.
        QVERIFY(press(editor_ref, QStringLiteral("F2")).success);
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("type")},
                     {QStringLiteral("name"), QStringLiteral("Prompt")},
                     {QStringLiteral("text"), QStringLiteral("draft")}})
                    .success);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), n);
        // Enter in a plain field judges the window's default button.
        auto* go = new QPushButton(QStringLiteral("Go"));
        go->setDefault(true);
        go->setProperty("fincept_agent_risk", QStringLiteral("spend"));
        s.layout->addWidget(go);
        QTest::qWait(20);
        QVERIFY2(asked(press(field_ref, QStringLiteral("Enter")), n++), "the window's default button");
    }

    void table_cells_and_columns_carry_risk() {
        namespace ag = fincept::core::agent;
        Screen s;
        auto* view = new QTableView;
        auto* model = make_table_model(view, {QStringLiteral("Name"), QStringLiteral("Action"), QStringLiteral("Weight")},
                                       {{QStringLiteral("Alpha"), QStringLiteral("Close"), QStringLiteral("1")},
                                        {QStringLiteral("Beta"), QStringLiteral("Close"), QStringLiteral("2")}});
        model->item(1, 2)->setData(QStringLiteral("delete"), ag::kAgentRiskRole);
        view->setModel(model);
        view->setProperty("fincept_agent_risk_columns", QStringLiteral("Action=order"));
        s.layout->addWidget(view);
        s.show();
        const QString tref = ref_on_line(text_of(snapshot(s.target())), {QStringLiteral("table")});
        auto cell = [&](const QString& action, int row, const QString& column) {
            QJsonObject a{{QStringLiteral("action"), action},
                          {QStringLiteral("ref"), tref + QStringLiteral(":r%1").arg(row)},
                          {QStringLiteral("column"), column}};
            if (action == QLatin1String("set_value"))
                a.insert(QStringLiteral("value"), QStringLiteral("5"));
            return act(a);
        };
        QVERIFY(cell(QStringLiteral("click"), 0, QStringLiteral("Name")).success);
        QVERIFY(test_host::approvals().isEmpty());
        const ToolResult dbl = cell(QStringLiteral("double_click"), 0, QStringLiteral("Action"));
        QVERIFY2(has_code(dbl, "denied") && test_host::approvals().last().live_trading, qPrintable(text_of(dbl)));
        QVERIFY(has_code(cell(QStringLiteral("set_value"), 1, QStringLiteral("Weight")), "denied")); // the cell's role
        QCOMPARE(model->item(1, 2)->text(), QStringLiteral("2"));
        QVERIFY(cell(QStringLiteral("set_value"), 0, QStringLiteral("Weight")).success); // an unmarked cell
        QCOMPARE(model->item(0, 2)->data(Qt::EditRole).toString(), QStringLiteral("5"));
    }

    void a_dialog_opened_by_a_marked_step_inherits_it() {
        test_host::set_verdict(ToolApprovalVerdict::Approved);
        Screen s;
        int cleared = 0, ordered = 0;
        auto* clear = new QPushButton(QStringLiteral("Tidy board"));
        clear->setProperty("fincept_agent_risk", QStringLiteral("delete"));
        connect(clear, &QPushButton::clicked, s.frame, [&]() {
            if (QMessageBox::question(s.frame, QStringLiteral("Sure?"), QStringLiteral("Proceed?")) == QMessageBox::Yes)
                ++cleared;
        });
        auto* go = new QPushButton(QStringLiteral("Go"));
        go->setProperty("fincept_agent_risk", QStringLiteral("order"));
        connect(go, &QPushButton::clicked, s.frame, [&]() {
            if (QMessageBox::question(s.frame, QStringLiteral("Sure?"), QStringLiteral("Proceed?")) == QMessageBox::Yes)
                ++ordered;
        });
        s.layout->addWidget(clear);
        s.layout->addWidget(go);
        s.show();
        // Click the opener; while its dialog is open, answer "Yes" in a second call.
        auto open_and_answer = [&](const QString& opener) {
            auto answer = std::make_shared<std::optional<ToolResult>>();
            QTimer::singleShot(900, qApp, [answer]() {
                eng::ui_act(QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                        {QStringLiteral("name"), QStringLiteral("Yes")},
                                        {QStringLiteral("target"), QStringLiteral("dialog")}},
                            kOrigin, [answer](ToolResult r) { *answer = std::move(r); });
            });
            const ToolResult first =
                act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), opener}});
            QElapsedTimer t;
            t.start();
            while (!answer->has_value() && t.elapsed() < 10000)
                QTest::qWait(10);
            return std::make_pair(first, answer->value_or(ToolResult::fail(QStringLiteral("no answer"))));
        };
        // Not an order: the opener's approval covers its own confirmation.
        const auto del = open_and_answer(QStringLiteral("Tidy board"));
        QVERIFY2(del.first.success && del.second.success, qPrintable(text_of(del.second)));
        QCOMPARE(cleared, 1);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), 1);
        // An order: its confirmation — a neutral "Proceed? Yes" — is an order too,
        // and the user approves it again.
        const auto ord = open_and_answer(QStringLiteral("Go"));
        QVERIFY2(ord.first.success && ord.second.success, qPrintable(text_of(ord.second)));
        QCOMPARE(ordered, 1);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), 3);
        QVERIFY(test_host::approvals().last().live_trading);
    }

    void a_web_chart_s_ticket_is_judged_by_its_marked_submit() {
        // A chart page's "Buy" is judged by its words; the ticket it opens is a
        // native dialog whose submit carries the screen's marker — and THAT is
        // what the user approves before anything is sent.
        test_host::set_verdict(ToolApprovalVerdict::Approved);
        Screen s;
        auto* chart = new QWidget; // stands in for the KLineChart page
        auto* cl = new QVBoxLayout(chart);
        auto* buy = new QPushButton(QStringLiteral("Buy"));
        cl->addWidget(buy);
        s.layout->addWidget(chart);
        int sent = 0;
        QPointer<QDialog> ticket;
        connect(buy, &QPushButton::clicked, s.frame, [&]() {
            ticket = new QDialog(s.frame);
            ticket->setWindowTitle(QStringLiteral("Ticket"));
            auto* tl = new QVBoxLayout(ticket);
            tl->addWidget(new QLabel(QStringLiteral("1 AAPL at market")));
            auto* box = new QDialogButtonBox(ticket);
            QPushButton* submit = box->addButton(QStringLiteral("Send"), QDialogButtonBox::AcceptRole);
            submit->setProperty("fincept_agent_risk", QStringLiteral("order"));
            box->addButton(QDialogButtonBox::Cancel);
            connect(submit, &QPushButton::clicked, [&sent]() { ++sent; });
            tl->addWidget(box);
            ticket->show();
        });
        s.show();
        QVERIFY(act({{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("name"), QStringLiteral("Buy")}}).success);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), 1); // "Buy" by its words
        QVERIFY(ticket);
        const ToolResult r = act({{QStringLiteral("action"), QStringLiteral("click")},
                                  {QStringLiteral("name"), QStringLiteral("Send")},
                                  {QStringLiteral("target"), QStringLiteral("dialog")}});
        QVERIFY2(r.success, qPrintable(text_of(r)));
        QCOMPARE(sent, 1);
        QCOMPARE(static_cast<int>(test_host::approvals().size()), 2); // the submit asked again
        QVERIFY(test_host::approvals().last().live_trading);
        delete ticket.data();
    }

    void no_exception_escapes_a_handler() {
        // The dispatch wrapper the terminal uses: an engine function that throws.
        AsyncToolHandler h = ui_gui_handler(QStringLiteral("ui_read"),
                                            [](const QJsonObject&, const QString&, eng::CancelCheck, eng::Reply) {
                                                throw std::runtime_error("engine bug");
                                            });
        auto promise = std::make_shared<QPromise<ToolResult>>();
        QFuture<ToolResult> f = promise->future();
        promise->start();
        h(QJsonObject{}, ToolContext{}, promise);
        QVERIFY(f.isFinished() && f.resultCount() == 1);
        QVERIFY2(f.result().error.startsWith(QStringLiteral("internal: ")), qPrintable(f.result().error));

        // A model that throws while its cells are read, and one that throws on edit.
        Screen s;
        auto* view = new QTableView;
        auto* model = new ThrowingModel(1, 2, view);
        model->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Value")});
        model->setItem(0, 0, new QStandardItem(QStringLiteral("x")));
        model->setItem(0, 1, new QStandardItem(QStringLiteral("1")));
        view->setModel(model);
        s.layout->addWidget(view);
        s.show();
        const QString tref = ref_on_line(text_of(snapshot(s.target())), {QStringLiteral("table")});
        AsyncToolHandler reader = ui_gui_handler(QStringLiteral("ui_read"),
                                                 [](const QJsonObject& a, const QString&, eng::CancelCheck, eng::Reply r) {
                                                     eng::ui_read(a, std::move(r));
                                                 });
        model->throw_on_data = true;
        auto p2 = std::make_shared<QPromise<ToolResult>>();
        QFuture<ToolResult> f2 = p2->future();
        p2->start();
        reader(QJsonObject{{QStringLiteral("ref"), tref}}, ToolContext{}, p2);
        model->throw_on_data = false;
        QVERIFY(f2.isFinished());
        QVERIFY2(f2.result().error.startsWith(QStringLiteral("internal: ")), qPrintable(f2.result().error));
        const ToolResult edit = act({{QStringLiteral("action"), QStringLiteral("set_value")},
                                     {QStringLiteral("ref"), tref + QStringLiteral(":r0")},
                                     {QStringLiteral("column"), QStringLiteral("Value")},
                                     {QStringLiteral("value"), QStringLiteral("2")}});
        QVERIFY2(has_code(edit, "internal"), qPrintable(text_of(edit)));
    }
};

QTEST_MAIN(TestMcpUiEngine)
#include "tst_mcp_ui_engine.moc"
