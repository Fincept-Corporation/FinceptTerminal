// StatePanels.cpp — implementations of PositionsPanel, HitlPanel, RiskPanel,
// AuditPanel. See StatePanels.h for the contract.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 7.

#include "screens/alpha_arena/panels/StatePanels.h"

#include "services/alpha_arena/AlphaArenaEngine.h"
#include "services/alpha_arena/AlphaArenaRepo.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStringList>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::alpha_arena {

using fincept::services::alpha_arena::AlphaArenaEngine;
using fincept::services::alpha_arena::AlphaArenaRepo;
using fincept::services::alpha_arena::AgentRow;
using fincept::services::alpha_arena::EventRow;
using fincept::services::alpha_arena::Position;

namespace {

QString fmt_dollars(double v) {
    return QStringLiteral("$%1").arg(v, 0, 'f', 2);
}
QString fmt_signed_pct(double frac) {
    const QString sign = (frac >= 0.0) ? QStringLiteral("+") : QString();
    return sign + QString::number(frac * 100.0, 'f', 2) + QStringLiteral("%");
}

} // namespace

// ── PositionsPanel ──────────────────────────────────────────────────────────

PositionsPanel::PositionsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("aaPositionsPanel");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* hdr = new QWidget;
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 6, 12, 6);
    auto* title = new QLabel(QStringLiteral("OPEN POSITIONS"));
    title->setObjectName("aaPanelTitle");
    hl->addWidget(title);
    hl->addStretch(1);
    root->addWidget(hdr);

    table_ = new QTableWidget(this);
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels(
        {"AGENT", "COIN", "QTY", "ENTRY", "MARK", "uPnL", "LEV", "LIQ DIST"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    root->addWidget(table_, 1);
}

void PositionsPanel::set_competition(const QString& competition_id) {
    competition_id_ = competition_id;
    table_->setRowCount(0);
}

void PositionsPanel::refresh() {
    if (competition_id_.isEmpty()) {
        table_->setRowCount(0);
        return;
    }
    auto& repo = AlphaArenaRepo::instance();
    auto agents = repo.agents_for(competition_id_);
    if (agents.is_err()) {
        table_->setRowCount(0);
        return;
    }

    QVector<QPair<AgentRow, Position>> rows;
    for (const auto& a : agents.value()) {
        auto pos_r = repo.open_positions_for(a.id);
        if (pos_r.is_err()) continue;
        for (const auto& p : pos_r.value()) {
            rows.append({a, p});
        }
    }

    table_->setSortingEnabled(false);
    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& [agent, p] = rows[i];

        auto* it_agent = new QTableWidgetItem(agent.display_name);
        it_agent->setForeground(QColor(agent.color_hex));
        table_->setItem(i, 0, it_agent);

        table_->setItem(i, 1, new QTableWidgetItem(p.coin));

        auto* it_qty = new QTableWidgetItem(QString::number(p.qty, 'f', 6));
        it_qty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it_qty->setForeground(QColor(p.qty >= 0 ? fincept::ui::colors::POSITIVE()
                                                 : fincept::ui::colors::NEGATIVE()));
        table_->setItem(i, 2, it_qty);

        auto* it_entry = new QTableWidgetItem(fmt_dollars(p.entry));
        it_entry->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 3, it_entry);

        auto* it_mark = new QTableWidgetItem(fmt_dollars(p.mark));
        it_mark->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 4, it_mark);

        auto* it_pnl = new QTableWidgetItem(fmt_dollars(p.unrealized_pnl));
        it_pnl->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it_pnl->setForeground(QColor(p.unrealized_pnl >= 0 ? fincept::ui::colors::POSITIVE()
                                                            : fincept::ui::colors::NEGATIVE()));
        table_->setItem(i, 5, it_pnl);

        auto* it_lev = new QTableWidgetItem(QString::number(p.leverage) + QStringLiteral("x"));
        it_lev->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 6, it_lev);

        const double dist = (p.mark > 0.0 && p.liq_price > 0.0)
                                ? std::fabs(p.mark - p.liq_price) / p.mark : 0.0;
        auto* it_dist = new QTableWidgetItem(fmt_signed_pct(dist));
        it_dist->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (dist < 0.05) it_dist->setForeground(QColor(fincept::ui::colors::NEGATIVE()));
        table_->setItem(i, 7, it_dist);
    }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

// ── HitlPanel ───────────────────────────────────────────────────────────────

HitlPanel::HitlPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("aaHitlPanel");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* hdr = new QLabel(QStringLiteral("PENDING HITL APPROVALS"));
    hdr->setObjectName("aaPanelTitle");
    hdr->setContentsMargins(12, 6, 12, 6);
    root->addWidget(hdr);

    list_ = new QListWidget(this);
    list_->setObjectName("aaHitlList");
    root->addWidget(list_, 1);
}

void HitlPanel::set_competition(const QString& competition_id) {
    competition_id_ = competition_id;
    list_->clear();
}

void HitlPanel::refresh() {
    if (competition_id_.isEmpty()) {
        list_->clear();
        return;
    }
    // Re-surface any pending approvals from prior sessions (the engine signal
    // only fires on fresh requests, so on first show we walk the DB).
    auto r = AlphaArenaRepo::instance().pending_hitl_for(competition_id_);
    if (r.is_err()) return;
    list_->clear();
    for (const auto& row : r.value()) {
        const QString summary = QStringLiteral("%1 — %2").arg(row.requested_at.left(19),
                                                              row.prompt_text.left(80));
        rebuild_buttons_for(row.approval_id, row.agent_id, summary);
    }
}

void HitlPanel::on_hitl_requested(QString approval_id, QString agent_id, QString summary) {
    rebuild_buttons_for(approval_id, agent_id, summary);
}

void HitlPanel::rebuild_buttons_for(const QString& approval_id, const QString& agent_id,
                                     const QString& summary) {
    auto* w = new QWidget;
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(8, 4, 8, 4);

    auto* label = new QLabel(QStringLiteral("⚠ %1 — %2").arg(agent_id.left(8), summary));
    hl->addWidget(label, 1);

    auto* approve = new QPushButton(QStringLiteral("APPROVE"));
    auto* reject = new QPushButton(QStringLiteral("REJECT"));
    hl->addWidget(approve);
    hl->addWidget(reject);

    auto* item = new QListWidgetItem(list_);
    item->setSizeHint(w->sizeHint());
    list_->addItem(item);
    list_->setItemWidget(item, w);

    connect(approve, &QPushButton::clicked, this, [this, approval_id, item]() {
        emit approval_resolved(approval_id, true);
        delete list_->takeItem(list_->row(item));
    });
    connect(reject, &QPushButton::clicked, this, [this, approval_id, item]() {
        emit approval_resolved(approval_id, false);
        delete list_->takeItem(list_->row(item));
    });
}

// ── RiskPanel ───────────────────────────────────────────────────────────────

RiskPanel::RiskPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("aaRiskPanel");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* hdr = new QLabel(QStringLiteral("RISK & TELEMETRY"));
    hdr->setObjectName("aaPanelTitle");
    hdr->setContentsMargins(12, 6, 12, 6);
    root->addWidget(hdr);

    telemetry_label_ = new QLabel(QStringLiteral("Awaiting first tick…"));
    telemetry_label_->setObjectName("aaTelemetryLabel");
    telemetry_label_->setContentsMargins(12, 0, 12, 6);
    telemetry_label_->setStyleSheet("color:#aaa;font-family:monospace;");
    root->addWidget(telemetry_label_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels(
        {"AGENT", "STATE", "PARSE FAILS", "RISK REJECTS", "MAX DD", "AVG LEV"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(table_, 1);
}

void RiskPanel::set_competition(const QString& competition_id) {
    competition_id_ = competition_id;
    table_->setRowCount(0);
}

void RiskPanel::refresh() {
    // Telemetry header — read regardless of agent table availability.
    if (telemetry_label_) {
        const auto t = AlphaArenaEngine::instance().telemetry();
        telemetry_label_->setText(QStringLiteral(
            "tick latency  p50=%1ms  p99=%2ms   |   parse-fail %3%   risk-reject %4%   venue-err %5   (n=%6)")
            .arg(t.tick_latency_p50_ms, 0, 'f', 0)
            .arg(t.tick_latency_p99_ms, 0, 'f', 0)
            .arg(t.parse_failure_rate * 100.0, 0, 'f', 1)
            .arg(t.risk_rejection_rate * 100.0, 0, 'f', 1)
            .arg(t.venue_errors)
            .arg(t.sample_count));
    }
    if (competition_id_.isEmpty()) {
        table_->setRowCount(0);
        return;
    }
    auto& repo = AlphaArenaRepo::instance();
    auto agents = repo.agents_for(competition_id_);
    auto lb = repo.leaderboard(competition_id_);
    if (agents.is_err()) {
        table_->setRowCount(0);
        return;
    }

    table_->setSortingEnabled(false);
    table_->setRowCount(agents.value().size());
    for (int i = 0; i < agents.value().size(); ++i) {
        const auto& a = agents.value()[i];

        auto* it_agent = new QTableWidgetItem(a.display_name);
        it_agent->setForeground(QColor(a.color_hex));
        table_->setItem(i, 0, it_agent);

        auto* it_state = new QTableWidgetItem(a.state);
        if (a.state == QLatin1String("circuit_open") || a.state == QLatin1String("halted"))
            it_state->setForeground(QColor(fincept::ui::colors::NEGATIVE()));
        table_->setItem(i, 1, it_state);

        table_->setItem(i, 2, new QTableWidgetItem(QString::number(a.consecutive_parse_failures)));
        table_->setItem(i, 3, new QTableWidgetItem(QString::number(a.consecutive_risk_rejects)));

        double max_dd = 0.0, avg_lev = 0.0;
        if (lb.is_ok()) {
            for (const auto& row : lb.value()) {
                if (row.agent_id == a.id) {
                    max_dd = row.max_drawdown;
                    avg_lev = row.avg_leverage;
                    break;
                }
            }
        }
        auto* it_dd = new QTableWidgetItem(fmt_signed_pct(-std::fabs(max_dd)));
        it_dd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it_dd->setForeground(QColor(fincept::ui::colors::NEGATIVE()));
        table_->setItem(i, 4, it_dd);

        auto* it_lev = new QTableWidgetItem(QString::number(avg_lev, 'f', 2) + QStringLiteral("x"));
        it_lev->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 5, it_lev);
    }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
}

// ── AuditPanel ──────────────────────────────────────────────────────────────

AuditPanel::AuditPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("aaAuditPanel");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* hdr = new QLabel(QStringLiteral("AUDIT LOG"));
    hdr->setObjectName("aaPanelTitle");
    hdr->setContentsMargins(12, 6, 12, 6);
    root->addWidget(hdr);

    list_ = new QListWidget(this);
    list_->setObjectName("aaAuditList");
    root->addWidget(list_, 1);
}

void AuditPanel::set_competition(const QString& competition_id) {
    competition_id_ = competition_id;
    list_->clear();
    last_seq_ = 0;
}

void AuditPanel::refresh() {
    if (competition_id_.isEmpty()) return;

    auto rows = AlphaArenaRepo::instance().events_since(competition_id_, last_seq_, 200);
    if (rows.is_err()) return;
    for (const auto& e : rows.value()) {
        const QString summary = QStringLiteral("[%1]  #%2  %3  %4")
            .arg(e.ts.left(19))
            .arg(e.seq)
            .arg(e.type, -16)
            .arg(e.payload_json.left(80));
        auto* it = new QListWidgetItem(summary);
        if (e.type.contains(QLatin1String("error")) ||
            e.type.contains(QLatin1String("circuit_open")) ||
            e.type.contains(QLatin1String("liquidation"))) {
            it->setForeground(QColor(fincept::ui::colors::NEGATIVE()));
        } else if (e.type.contains(QLatin1String("decision")) ||
                   e.type.contains(QLatin1String("fill"))) {
            it->setForeground(QColor(fincept::ui::colors::POSITIVE()));
        }
        list_->addItem(it);
        last_seq_ = std::max<qint64>(last_seq_, e.seq);
    }
    list_->scrollToBottom();
}

} // namespace fincept::screens::alpha_arena
