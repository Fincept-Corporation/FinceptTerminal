#include "screens/alpha_arena/LeaderboardCards.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::screens::alpha_arena {
namespace {

enum Col { ColRank = 0, ColChip, ColModel, ColEquity, ColPnlPct, ColUpnl, ColPos, ColTok, ColSt, ColAct, ColCount };

constexpr int kRowHeight = 22;

QTableWidgetItem* cell(const QString& text, const QColor& fg, Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
    auto* it = new QTableWidgetItem(text);
    it->setForeground(fg);
    it->setTextAlignment(align);
    return it;
}

// `kind` selects the colour via a dynamic-property rule in the table-level
// stylesheet. rebuild() runs on every marks tick (~1 Hz) for every agent, so a
// per-button setStyleSheet() meant N*2 full CSS re-parses per tick; the rules
// now live in one stylesheet set once in the constructor.
QPushButton* icon_btn(const QString& glyph, const QString& tip, const char* kind) {
    auto* b = new QPushButton(glyph);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(20, 18);
    b->setFlat(true);
    b->setObjectName(QStringLiteral("arenaActBtn"));
    b->setProperty("act", QString::fromLatin1(kind));
    return b;
}

QString fmt_usd(double v) {
    return QLocale(QLocale::English, QLocale::UnitedStates).toString(v, 'f', 0);
}

} // namespace

LeaderboardCards::LeaderboardCards(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    table_ = new QTableWidget(0, ColCount);
    table_->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("●"), tr("MODEL"), tr("EQUITY $"),
                                       tr("PnL %"), tr("uPnL $"), tr("POS"), tr("TOK"), tr("ST"), tr("ACT")});
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kRowHeight);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);
    table_->setSortingEnabled(false); // caller order = rank (sorted by equity)
    // Includes the ACT cell-widget rules — cell widgets are descendants of the
    // table, so this one stylesheet covers every row's buttons and container.
    table_->setStyleSheet("QTableWidget{background:#0D0D0D;border:none;font-size:11px;"
                          " font-family:'Consolas','Courier New',monospace;}"
                          "QTableWidget::item{padding:0 6px;border-bottom:1px solid #1C1C1C;}"
                          "QHeaderView::section{background:#141414;color:#777;border:none;"
                          " border-bottom:1px solid #2A2A2A;padding:2px 6px;font-size:10px;font-weight:700;}"
                          "QWidget#arenaAct{background:#0D0D0D;}"
                          "QWidget#arenaAct[sel=\"true\"]{background:#1F1A10;}"
                          "QPushButton#arenaActBtn{background:transparent;border:none;font-size:11px;padding:0;}"
                          "QPushButton#arenaActBtn:hover{background:#2A2A2A;}"
                          "QPushButton#arenaActBtn[act=\"halt\"]{color:#E0A800;}"
                          "QPushButton#arenaActBtn[act=\"resume\"]{color:#26A65B;}"
                          "QPushButton#arenaActBtn[act=\"kill\"]{color:#E0245E;}");
    auto* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(ColModel, QHeaderView::Stretch);
    hh->setHighlightSections(false);

    // Row click (outside the ACT buttons — those eat the press) toggles
    // selection. Deferred: the screen handler re-filters the panel grid and may
    // rebuild this board; let the click handler unwind first.
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int col) {
        if (col == ColAct || row < 0 || row >= cards_.size())
            return;
        const QString next = selected_ == cards_[row].agent_id ? QString() : cards_[row].agent_id;
        QMetaObject::invokeMethod(this, [this, next]() { emit agent_selected(next); }, Qt::QueuedConnection);
    });
    connect(this, &LeaderboardCards::agent_selected, this, [this](const QString& id) {
        selected_ = id;
        rebuild();
    });
    v->addWidget(table_, 1);
}

void LeaderboardCards::set_data(QVector<AgentCardData> cards) {
    cards_ = std::move(cards);
    rebuild();
}

void LeaderboardCards::rebuild() {
    table_->setRowCount(cards_.size());
    for (int i = 0; i < cards_.size(); ++i) {
        const auto& d = cards_[i];
        const bool selected = d.agent_id == selected_;
        const QColor pnl_fg = d.pnl_pct >= 0 ? QColor("#26A65B") : QColor("#E0245E");
        const QColor upnl_fg = d.upnl >= 0 ? QColor("#26A65B") : QColor("#E0245E");

        table_->setItem(i, ColRank, cell(QString::number(i + 1), QColor("#777"), Qt::AlignCenter));
        table_->setItem(i, ColChip, cell(QStringLiteral("●"), d.color, Qt::AlignCenter));
        table_->setItem(i, ColModel,
                        cell(d.name, selected ? QColor("#FF8800") : QColor("#EEE"), Qt::AlignLeft | Qt::AlignVCenter));
        table_->setItem(i, ColEquity, cell(fmt_usd(d.equity), QColor("#FFF")));
        table_->setItem(
            i, ColPnlPct,
            cell(QStringLiteral("%1%2%").arg(d.pnl_pct >= 0 ? "+" : "").arg(QString::number(d.pnl_pct, 'f', 2)),
                 pnl_fg));
        table_->setItem(
            i, ColUpnl,
            cell(QStringLiteral("%1%2").arg(d.upnl >= 0 ? "+" : "").arg(QString::number(d.upnl, 'f', 2)), upnl_fg));
        table_->setItem(i, ColPos, cell(QString::number(d.open_positions), QColor("#DDD"), Qt::AlignCenter));
        table_->setItem(i, ColTok,
                        cell(d.tokens >= 1000 ? QStringLiteral("%1k").arg(QString::number(d.tokens / 1000.0, 'f', 1))
                                              : QString::number(d.tokens),
                             QColor("#999")));
        const bool active = d.status == QLatin1String("active");
        const bool circuit = d.status == QLatin1String("halted_circuit");
        auto* st = cell(active    ? QStringLiteral("▶")
                        : circuit ? QStringLiteral("⚡")
                                  : QStringLiteral("⏸"),
                        active    ? QColor("#26A65B")
                        : circuit ? QColor("#E0A800")
                                  : QColor("#E0245E"),
                        Qt::AlignCenter);
        st->setToolTip(d.status);
        table_->setItem(i, ColSt, st);

        // Per-row selection tint (set after all items exist).
        const QColor bg = selected ? QColor("#1F1A10") : QColor("#0D0D0D");
        for (int c = 0; c < ColCount; ++c)
            if (auto* it = table_->item(i, c))
                it->setBackground(bg);

        // ACT: halt/resume toggle + kill. Emissions are deferred — handlers
        // refresh the dashboard, which calls set_data → rebuild and would
        // delete the clicked button under its own clicked() handler.
        auto* act = new QWidget;
        act->setObjectName(QStringLiteral("arenaAct"));
        act->setProperty("sel", selected);
        auto* h = new QHBoxLayout(act);
        h->setContentsMargins(2, 0, 2, 0);
        h->setSpacing(2);
        const QString id = d.agent_id;
        auto* toggle = icon_btn(active ? QStringLiteral("⏸") : QStringLiteral("▶"),
                                active ? tr("Halt agent") : tr("Resume agent"), active ? "halt" : "resume");
        connect(toggle, &QPushButton::clicked, this, [this, id, active]() {
            QMetaObject::invokeMethod(
                this,
                [this, id, active]() {
                    if (active)
                        emit halt_requested(id);
                    else
                        emit resume_requested(id);
                },
                Qt::QueuedConnection);
        });
        auto* kill = icon_btn(QStringLiteral("✖"), tr("Close positions & halt (kill)"), "kill");
        connect(kill, &QPushButton::clicked, this, [this, id]() {
            QMetaObject::invokeMethod(this, [this, id]() { emit kill_requested(id); }, Qt::QueuedConnection);
        });
        h->addWidget(toggle);
        h->addWidget(kill);
        table_->setCellWidget(i, ColAct, act);
        table_->setRowHeight(i, kRowHeight);
    }
}

} // namespace fincept::screens::alpha_arena
