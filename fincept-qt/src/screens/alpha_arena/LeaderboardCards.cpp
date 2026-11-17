#include "screens/alpha_arena/LeaderboardCards.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include "services/alpha_arena/ArenaMetrics.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::screens::alpha_arena {
namespace {
using namespace ui::colors;

enum Col {
    ColRank = 0,
    ColChip,
    ColModel,
    ColEquity,
    ColPnlPct,
    ColDd,
    ColSharpe,
    ColUpnl,
    ColPos,
    ColTok,
    ColSt,
    ColAct,
    ColCount
};

// Row height for the board's text size (22 px at the default font size).
int row_height() {
    return qMax(22, QFontMetrics(ui::fonts::font(-3)).height() + 6);
}

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
    b->setMinimumSize(20, 18);
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
                                       tr("RET %"), tr("MAX DD %"), tr("SHARPE"), tr("uPnL $"), tr("POS"), tr("TOK"),
                                       tr("ST"), tr("ACT")});
    // Say what each figure is — a professional reads "Sharpe" and asks "of what?".
    const QStringList header_tips = {
        tr("Rank by equity"),
        QString(),
        tr("Model"),
        tr("Account equity: free balance + margin + unrealised PnL (net of funding)"),
        tr("Return on starting capital, fees and funding included"),
        tr("Largest peak-to-trough fall of equity, as % of the peak (starting capital counts as the first peak)"),
        tr("Annualised Sharpe ratio of per-round returns (risk-free rate 0, 365-day year). Shown once %1 rounds "
           "have settled.")
            .arg(fincept::arena::kArenaMinSharpeReturns + 1),
        tr("Unrealised PnL of open positions, net of accrued funding"),
        tr("Open positions"),
        tr("LLM tokens used (prompt + completion)"),
        tr("Status: ▶ active · ⏸ halted by user · ⚡ halted by circuit breaker"),
        tr("Halt / resume · close positions & halt"),
    };
    for (int c = 0; c < ColCount && c < header_tips.size(); ++c)
        if (auto* h = table_->horizontalHeaderItem(c))
            h->setToolTip(header_tips[c]);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(row_height());
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);
    table_->setSortingEnabled(false); // caller order = rank (sorted by equity)
    // Includes the ACT cell-widget rules — cell widgets are descendants of the
    // table, so this one stylesheet covers every row's buttons and container.
    // Halt is a caution, resume a good state, kill destructive — state colours.
    table_->setStyleSheet(QString("QTableWidget{background:%1;border:none;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                                  "QTableWidget::item{padding:0 6px;border-bottom:1px solid %2;}"
                                  "QHeaderView::section{background:%3;color:%4;border:none;"
                                  " border-bottom:1px solid %5;padding:2px 6px;font-size:" + fincept::ui::fonts::px(-4) +
                                  "px;font-weight:700;}"
                                  "QWidget#arenaAct{background:%1;}"
                                  "QWidget#arenaAct[sel=\"true\"]{background:%6;}"
                                  "QPushButton#arenaActBtn{background:transparent;border:none;font-size:" +
                                  fincept::ui::fonts::px(-3) + "px;padding:0;}"
                                  "QPushButton#arenaActBtn:hover{background:%7;}"
                                  "QPushButton#arenaActBtn[act=\"halt\"]{color:%8;}"
                                  "QPushButton#arenaActBtn[act=\"resume\"]{color:%9;}"
                                  "QPushButton#arenaActBtn[act=\"kill\"]{color:%10;}")
                              .arg(BG_SURFACE(), BORDER_DIM(), BG_RAISED(), TEXT_SECONDARY(), BORDER_MED(), ACCENT_BG(),
                                   BG_HOVER(), WARNING(), SUCCESS())
                              .arg(DANGER()));
    // Interactive + one fit after a fill (rebuild): a ResizeToContents header
    // re-measured every cell on every ~1 Hz marks tick while rows were written.
    auto* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
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

    // Cell colours are item brushes and the row height follows the font size.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        table_->verticalHeader()->setDefaultSectionSize(row_height());
        rebuild();
    });
    // A font change settles after the theme pass: fit the columns once then.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        refit_ = true;
        rebuild();
    });
}

void LeaderboardCards::set_data(QVector<AgentCardData> cards) {
    cards_ = std::move(cards);
    rebuild();
}

void LeaderboardCards::set_controls_enabled(bool on) {
    if (controls_enabled_ == on)
        return;
    controls_enabled_ = on;
    table_->setColumnHidden(ColAct, !on);
    rebuild();
}

void LeaderboardCards::rebuild() {
    const int row_h = row_height();
    table_->setRowCount(cards_.size());
    for (int i = 0; i < cards_.size(); ++i) {
        const auto& d = cards_[i];
        const bool selected = d.agent_id == selected_;
        const QColor up(POSITIVE());
        const QColor down(NEGATIVE());
        const QColor pnl_fg = d.pnl_pct >= 0 ? up : down;
        const QColor upnl_fg = d.upnl >= 0 ? up : down;

        table_->setItem(i, ColRank, cell(QString::number(i + 1), QColor(TEXT_TERTIARY()), Qt::AlignCenter));
        // The agent's provider brand colour, made legible on the theme.
        table_->setItem(i, ColChip, cell(QStringLiteral("●"), legible_color(d.color), Qt::AlignCenter));
        table_->setItem(i, ColModel,
                        cell(d.name, selected ? QColor(AMBER()) : QColor(TEXT_PRIMARY()), Qt::AlignLeft | Qt::AlignVCenter));
        table_->setItem(i, ColEquity, cell(fmt_usd(d.equity), QColor(TEXT_PRIMARY())));
        table_->setItem(
            i, ColPnlPct,
            cell(QStringLiteral("%1%2%").arg(d.pnl_pct >= 0 ? "+" : "").arg(QString::number(d.pnl_pct, 'f', 2)),
                 pnl_fg));
        table_->setItem(i, ColDd,
                        cell(d.max_dd_pct > 0.005 ? QStringLiteral("-%1%").arg(QString::number(d.max_dd_pct, 'f', 2))
                                                  : QStringLiteral("0.00%"),
                             d.max_dd_pct > 0.005 ? down : QColor(TEXT_SECONDARY())));
        auto* sh = cell(d.sharpe_valid ? QString::number(d.sharpe, 'f', 2) : QStringLiteral("—"),
                        !d.sharpe_valid ? QColor(TEXT_TERTIARY()) : d.sharpe >= 0 ? up : down);
        sh->setToolTip(d.sharpe_valid
                           ? tr("From %n per-round return(s)", "", d.sharpe_returns)
                           : tr("Needs at least %1 per-round returns (%2 so far)")
                                 .arg(fincept::arena::kArenaMinSharpeReturns)
                                 .arg(d.sharpe_returns));
        table_->setItem(i, ColSharpe, sh);
        table_->setItem(
            i, ColUpnl,
            cell(QStringLiteral("%1%2").arg(d.upnl >= 0 ? "+" : "").arg(QString::number(d.upnl, 'f', 2)), upnl_fg));
        table_->setItem(i, ColPos, cell(QString::number(d.open_positions), QColor(TEXT_PRIMARY()), Qt::AlignCenter));
        table_->setItem(i, ColTok,
                        cell(d.tokens >= 1000 ? QStringLiteral("%1k").arg(QString::number(d.tokens / 1000.0, 'f', 1))
                                              : QString::number(d.tokens),
                             QColor(TEXT_SECONDARY())));
        const bool active = d.status == QLatin1String("active");
        const bool circuit = d.status == QLatin1String("halted_circuit");
        auto* st = cell(active    ? QStringLiteral("▶")
                        : circuit ? QStringLiteral("⚡")
                                  : QStringLiteral("⏸"),
                        active    ? QColor(SUCCESS())
                        : circuit ? QColor(WARNING())
                                  : QColor(DANGER()),
                        Qt::AlignCenter);
        st->setToolTip(d.status);
        table_->setItem(i, ColSt, st);

        // Per-row selection tint (set after all items exist); other rows show
        // the table background.
        if (selected) {
            const QColor bg(ACCENT_BG());
            for (int c = 0; c < ColCount; ++c)
                if (auto* it = table_->item(i, c))
                    it->setBackground(bg);
        }

        // ACT: halt/resume toggle + kill. Emissions are deferred — handlers
        // refresh the dashboard, which calls set_data → rebuild and would
        // delete the clicked button under its own clicked() handler.
        if (!controls_enabled_) {
            table_->removeCellWidget(i, ColAct);
            table_->setRowHeight(i, row_h);
            continue;
        }
        auto* act = new QWidget;
        act->setObjectName(QStringLiteral("arenaAct"));
        act->setProperty("sel", selected);
        auto* h = new QHBoxLayout(act);
        h->setContentsMargins(2, 0, 2, 0);
        h->setSpacing(2);
        const QString id = d.agent_id;
        auto* toggle = icon_btn(active ? QStringLiteral("⏸") : QStringLiteral("▶"),
                                active ? tr("Halt agent") : tr("Resume agent"), active ? "halt" : "resume");
        toggle->setProperty("fincept_agent_risk", "order"); // agents: halts / resumes this agent's trading
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
        kill->setProperty("fincept_agent_risk", "order"); // agents: closes this agent's positions and halts it
        connect(kill, &QPushButton::clicked, this, [this, id]() {
            QMetaObject::invokeMethod(this, [this, id]() { emit kill_requested(id); }, Qt::QueuedConnection);
        });
        h->addWidget(toggle);
        h->addWidget(kill);
        table_->setCellWidget(i, ColAct, act);
        table_->setRowHeight(i, row_h);
    }
    // Fit once after the fill (ColModel stretches): first fill, another number
    // of agents, or a font change. Value-only ticks keep the widths.
    if (refit_ || fitted_rows_ != static_cast<int>(cards_.size())) {
        for (int c = 0; c < ColCount; ++c)
            if (c != ColModel)
                table_->resizeColumnToContents(c);
        fitted_rows_ = static_cast<int>(cards_.size());
        refit_ = false;
    }
}

} // namespace fincept::screens::alpha_arena
