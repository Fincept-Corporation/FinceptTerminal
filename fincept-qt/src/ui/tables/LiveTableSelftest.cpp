// LiveTableSelftest — headless verification of ui::LiveTableModel.
//
// Also serves as the compile check for LiveTableModel.h and LiveTableDelegate.h:
// both are header-only, so without a TU that includes them they would never be
// compiled and a defect could sit there indefinitely looking finished.
//
// Run: FinceptTerminal.exe --selftest-live-table

#include "ui/tables/LiveTableSelftest.h"

#include "ui/tables/LiveTableDelegate.h" // compile-only: proves the delegate builds
#include "ui/tables/LiveTableModel.h"

// NOTE: deliberately no <QAbstractItemModelTester> and no <QSignalSpy> — both
// live in Qt6::Test, which the shipping binary does not link (see the
// find_package COMPONENTS list in CMakeLists.txt). Pulling a Qt module into the
// whole application for test-only conveniences is the wrong trade, so the model
// invariants are asserted by hand and signals are counted with plain connects.
#include <QStringList>
#include <QTextStream>
#include <QVector>

namespace fincept::ui {
namespace {

struct Pos {
    QString account;
    QString symbol;
    double qty = 0;
    double pnl = 0;
};

int g_failures = 0;

void check(bool ok, const QString& what) {
    QTextStream out(stdout);
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++g_failures;
}

using Model = LiveTableModel<Pos>;

void configure(Model& m) {
    m.set_headers({QStringLiteral("ACCOUNT"), QStringLiteral("SYMBOL"), QStringLiteral("QTY"),
                   QStringLiteral("PNL")});
    m.set_key([](const Pos& p) { return p.account + QLatin1Char('|') + p.symbol; });
    m.set_cell([](const Pos& p, int col, int role) -> QVariant {
        if (role == Qt::DisplayRole) {
            switch (col) {
            case 0:
                return p.account;
            case 1:
                return p.symbol;
            case 2:
                return QString::number(p.qty, 'f', 0);
            case 3:
                return QString::number(p.pnl, 'f', 2);
            default:
                break;
            }
        }
        if (role == Model::SortRole) {
            if (col == 2)
                return p.qty;
            if (col == 3)
                return p.pnl;
        }
        return {};
    });
}

QVector<Pos> book() {
    return {{"A1", "AAPL", 100, 250.0}, {"A1", "MSFT", 50, -75.5}, {"A2", "NVDA", 10, 1200.0}};
}

/// Minimal stand-in for QSignalSpy (Qt6::Test is not linked — see the include
/// note at the top). Records reset counts and the extent of each dataChanged.
struct SignalCounter {
    int resets = 0;
    int changes = 0;
    int last_top_row = -1, last_bottom_row = -1, last_left_col = -1, last_right_col = -1;

    explicit SignalCounter(QAbstractItemModel* m) {
        QObject::connect(m, &QAbstractItemModel::modelReset, [this]() { ++resets; });
        QObject::connect(m, &QAbstractItemModel::dataChanged,
                         [this](const QModelIndex& tl, const QModelIndex& br, const QList<int>&) {
                             ++changes;
                             last_top_row = tl.row();
                             last_bottom_row = br.row();
                             last_left_col = tl.column();
                             last_right_col = br.column();
                         });
    }
};

} // namespace

int run_live_table_selftest() {
    QTextStream out(stdout);
    out << "=== LiveTableModel selftest ===\n";

    // ── Shape ───────────────────────────────────────────────────────────────
    {
        Model m;
        configure(m);
        m.upsert(book());
        check(m.rowCount() == 3, "rowCount reflects upserted rows");
        check(m.columnCount() == 4, "columnCount follows headers");
        check(m.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString() == QLatin1String("SYMBOL"),
              "headerData returns the configured header");
        check(m.data(m.index(0, 1), Qt::DisplayRole).toString() == QLatin1String("AAPL"),
              "data() routes through the cell functor");
        check(m.data(m.index(2, 3), Model::SortRole).toDouble() == 1200.0, "SortRole carries the numeric key");
        check(m.data(m.index(0, 0), Model::KeyRole).toString() == QLatin1String("A1|AAPL"),
              "KeyRole exposes the stable row key");
        // Model invariants, asserted by hand (see the include note above).
        check(!m.index(0, 0).parent().isValid(), "a table model reports no parent");
        check(m.rowCount(m.index(0, 0)) == 0, "child rowCount under a valid index is 0");
        check(!m.data(m.index(99, 0), Qt::DisplayRole).isValid(), "out-of-range row yields an invalid QVariant");
        check(!m.data(QModelIndex(), Qt::DisplayRole).isValid(), "invalid index yields an invalid QVariant");
        check(!m.headerData(0, Qt::Vertical, Qt::DisplayRole).isValid(), "no vertical header is claimed");
    }

    // ── The point of the class: a value tick must NOT reset the model ───────
    {
        Model m;
        configure(m);
        m.upsert(book());

        SignalCounter spy(&m);

        auto ticked = book();
        ticked[1].pnl = -80.0; // one cell moves
        m.upsert(ticked);

        check(spy.resets == 0, "same key sequence does NOT reset the model (selection/scroll survive)");
        check(spy.changes == 1, "exactly one dataChanged for a single moved row");
        check(spy.last_top_row == 1 && spy.last_bottom_row == 1, "dataChanged targets only the row that moved");
        check(spy.last_left_col == 3 && spy.last_right_col == 3, "dataChanged spans only the column that moved");
        check(m.data(m.index(1, 3), Qt::DisplayRole).toString() == QLatin1String("-80.00"),
              "new value is adopted");
    }

    // ── An unchanged tick must emit nothing at all ──────────────────────────
    {
        Model m;
        configure(m);
        m.upsert(book());
        SignalCounter spy(&m);
        m.upsert(book()); // identical payload — the common idle case
        check(spy.changes == 0 && spy.resets == 0, "identical payload emits no signals (idle costs nothing)");
    }

    // ── Structural changes still reset, and the key index stays correct ─────
    {
        Model m;
        configure(m);
        m.upsert(book());
        SignalCounter spy(&m);

        auto smaller = book();
        smaller.removeAt(1); // position closed
        m.upsert(smaller);
        check(spy.resets == 1, "row removal resets the model");
        check(m.rowCount() == 2, "removed row is gone");
        check(m.index_of_key(QStringLiteral("A1|MSFT")) == -1, "key index drops the removed row");
        check(m.index_of_key(QStringLiteral("A2|NVDA")) == 1, "key index is rebuilt with correct positions");

        const Pos* p = m.row_for_key(QStringLiteral("A2|NVDA"));
        check(p != nullptr && p->qty == 10, "row_for_key returns the domain object, not parsed display text");
        check(m.row_at(m.index(0, 0)) != nullptr, "row_at resolves a valid index");
        check(m.row_at(QModelIndex()) == nullptr, "row_at rejects an invalid index");
    }

    // ── Filter ──────────────────────────────────────────────────────────────
    {
        Model m;
        configure(m);
        m.set_filter([](const Pos& p) { return p.qty != 0; });
        auto with_flat = book();
        with_flat.push_back({"A2", "FLAT", 0, 0});
        m.upsert(with_flat);
        check(m.rowCount() == 3, "filter drops rows before they reach the view");
    }

    // ── clear() ─────────────────────────────────────────────────────────────
    {
        Model m;
        configure(m);
        m.upsert(book());
        m.clear();
        check(m.rowCount() == 0, "clear empties the model");
        SignalCounter spy(&m);
        m.clear();
        check(spy.resets == 0, "clear on an empty model is a no-op");
    }

    out << (g_failures == 0 ? "=== LiveTableModel: ALL PASS ===\n"
                            : QStringLiteral("=== LiveTableModel: %1 FAILURE(S) ===\n").arg(g_failures));
    return g_failures == 0 ? 0 : 1;
}

} // namespace fincept::ui
