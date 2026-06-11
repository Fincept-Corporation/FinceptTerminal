#include "screens/alpha_arena/ArenaTabs.h"
#include "services/alpha_arena/ArenaEngine.h"
#include "services/alpha_arena/ArenaStore.h"
#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace fincept::screens::alpha_arena {
using fincept::arena::ArenaEngine;
using fincept::arena::ArenaStore;

namespace {

QTableWidget* make_table(const QStringList& headers) {
    auto* t = new QTableWidget;
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(20);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setShowGrid(false);
    t->setStyleSheet(
        "QTableWidget{background:#0D0D0D;border:none;font-size:11px;"
        " font-family:'Consolas','Courier New',monospace;}"
        "QTableWidget::item{padding:0 4px;border-bottom:1px solid #1C1C1C;}"
        "QHeaderView::section{background:#141414;color:#777;border:none;"
        " border-bottom:1px solid #2A2A2A;padding:1px 4px;font-size:10px;font-weight:700;}");
    return t;
}

void fill_row(QTableWidget* t, int r, const QStringList& cells, const QColor& fg = QColor("#DDD")) {
    for (int c = 0; c < cells.size(); ++c) {
        auto* it = new QTableWidgetItem(cells[c]);
        it->setForeground(fg);
        t->setItem(r, c, it);
    }
}

QLabel* micro_header(const QString& text) {
    auto* l = new QLabel(text);
    l->setFixedHeight(18);
    l->setStyleSheet("background:#141414;color:#777;font-size:10px;font-weight:700;"
                     "padding:0 6px;border-bottom:1px solid #2A2A2A;");
    return l;
}

QWidget* panel(QLabel* header, QWidget* content) {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(header);
    v->addWidget(content, 1);
    return w;
}

} // namespace

ArenaPanelGrid::ArenaPanelGrid(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // HITL approval banner — high-visibility row above the grid, hidden when
    // empty. Entries are appended by add_hitl (live mode only).
    hitl_banner_ = new QWidget;
    hitl_banner_->setStyleSheet("background:#3A1414;border-bottom:1px solid #5C1F1F;");
    auto* bh = new QHBoxLayout(hitl_banner_);
    bh->setContentsMargins(8, 3, 8, 3);
    bh->setSpacing(8);
    auto* tag = new QLabel(tr("⚠ APPROVALS"));
    tag->setStyleSheet("color:#FFB0B0;font-size:10px;font-weight:800;");
    bh->addWidget(tag);
    hitl_row_ = new QHBoxLayout;
    hitl_row_->setSpacing(8);
    bh->addLayout(hitl_row_, 1);
    bh->addStretch(0);
    hitl_banner_->hide();
    root->addWidget(hitl_banner_);

    chat_ = new QTextBrowser;
    chat_->setOpenExternalLinks(false);
    chat_->setStyleSheet("QTextBrowser{background:#0D0D0D;color:#DDD;border:none;font-size:11px;}");
    positions_ = make_table({tr("Agent"), tr("Coin"), tr("Side"), tr("Qty"), tr("Entry"),
                             tr("Lev"), tr("uPnL $")});
    trades_ = make_table({tr("Time"), tr("Agent"), tr("Coin"), tr("Action"), tr("Side"),
                          tr("Qty"), tr("Price"), tr("Notional"), tr("Fee"), tr("rPnL"),
                          tr("Status"), tr("Reason")});
    risk_ = make_table({tr("Agent"), tr("Status"), tr("Fails"), tr("Halt reason")});
    audit_ = new QListWidget;
    audit_->setStyleSheet("QListWidget{background:#0D0D0D;color:#999;border:none;font-size:10px;"
                          " font-family:'Consolas','Courier New',monospace;}");

    chat_hdr_ = micro_header(tr("MODEL CHAT"));
    positions_hdr_ = micro_header(tr("POSITIONS"));
    trades_hdr_ = micro_header(tr("TRADES"));
    risk_hdr_ = micro_header(tr("RISK"));
    audit_hdr_ = micro_header(tr("AUDIT"));

    auto* mid = new QSplitter(Qt::Vertical);
    mid->setHandleWidth(1);
    mid->setChildrenCollapsible(false);
    mid->addWidget(panel(positions_hdr_, positions_));
    mid->addWidget(panel(trades_hdr_, trades_));
    mid->setStretchFactor(0, 1);
    mid->setStretchFactor(1, 1);

    auto* right = new QSplitter(Qt::Vertical);
    right->setHandleWidth(1);
    right->setChildrenCollapsible(false);
    right->addWidget(panel(risk_hdr_, risk_));
    right->addWidget(panel(audit_hdr_, audit_));
    right->setStretchFactor(0, 1);
    right->setStretchFactor(1, 2);

    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);
    split->setStyleSheet("QSplitter::handle{background:#2A2A2A;}");
    split->addWidget(panel(chat_hdr_, chat_));
    split->addWidget(mid);
    split->addWidget(right);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 4);
    split->setStretchFactor(2, 3);
    root->addWidget(split, 1);
}

void ArenaPanelGrid::set_competition(const QString& id, bool live_mode) {
    Q_UNUSED(live_mode);   // approvals only ever arrive in live mode (engine-gated)
    competition_id_ = id;
    agent_filter_.clear();
    // Drop stale approval entries from a previously viewed competition.
    while (hitl_row_->count() > 0) {
        auto* it = hitl_row_->takeAt(0);
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    hitl_banner_->hide();
    refresh();
}

void ArenaPanelGrid::set_agent_filter(const QString& agent_id) { agent_filter_ = agent_id; refresh(); }

void ArenaPanelGrid::refresh() {
    if (competition_id_.isEmpty()) return;
    refresh_chat();
    refresh_positions();
    refresh_trades();
    refresh_risk();
    refresh_audit();
}

void ArenaPanelGrid::refresh_positions_only() {
    if (competition_id_.isEmpty()) return;
    refresh_positions();
}

void ArenaPanelGrid::refresh_chat() {
    auto agents_r = ArenaStore::instance().agents_for(competition_id_);
    QHash<QString, QPair<QString, QString>> meta;   // id → (name, color)
    if (agents_r.is_ok())
        for (const auto& a : agents_r.value()) meta[a.id] = {a.display_name, a.color_hex};
    auto r = ArenaStore::instance().latest_decisions(competition_id_, agent_filter_, 50);
    if (r.is_err()) return;
    QString html;
    for (const auto& d : r.value()) {
        const auto m = meta.value(d.agent_id);
        QString body;
        if (d.status == QLatin1String("ok")) {
            const auto acts = QJsonDocument::fromJson(d.actions_json.toUtf8()).array();
            for (const auto& v : acts) {
                const auto o = v.toObject();
                // Every field is model-controlled at some layer — escape all of
                // them (hold actions can carry an unvalidated coin string).
                body += QStringLiteral("<b>%1 %2</b> %3 — <i>%4</i><br>")
                            .arg(o.value("action").toString().toUpper().toHtmlEscaped(),
                                 o.value("coin").toString().toHtmlEscaped(),
                                 o.value("side").toString().toHtmlEscaped(),
                                 o.value("reason").toString().toHtmlEscaped());
            }
        } else {
            body = QStringLiteral("<span style='color:#E0245E'>[%1] %2</span><br>")
                       .arg(d.status, d.parse_error.toHtmlEscaped());
        }
        html += QStringLiteral(
            "<div style='margin:6px 0'>"
            "<span style='color:%1'>●</span> <b style='color:#EEE'>%2</b>"
            " <span style='color:#777'>round %3 · %4 · %5ms · %6 tok</span><br>%7</div>")
            .arg(m.second, m.first.toHtmlEscaped()).arg(d.round_seq)
            .arg(QDateTime::fromMSecsSinceEpoch(d.ts).toString("HH:mm:ss"))
            .arg(d.latency_ms).arg(d.prompt_tokens + d.completion_tokens)
            .arg(body);
    }
    chat_->setHtml(html);
}

void ArenaPanelGrid::refresh_positions() {
    auto r = ArenaStore::instance().positions_for(competition_id_, agent_filter_);
    auto agents_r = ArenaStore::instance().agents_for(competition_id_);
    QHash<QString, QString> names;
    if (agents_r.is_ok()) for (const auto& a : agents_r.value()) names[a.id] = a.display_name;
    positions_->setRowCount(0);
    if (r.is_err()) { positions_hdr_->setText(tr("POSITIONS")); return; }
    for (const auto& p : r.value()) {
        const auto acct = ArenaEngine::instance().account_view(p.agent_id);
        const int row = positions_->rowCount();
        positions_->insertRow(row);
        fill_row(positions_, row,
                 {names.value(p.agent_id), p.coin, p.side.toUpper(),
                  QString::number(p.qty, 'g', 8), QString::number(p.entry_price, 'g', 8),
                  QString::number(p.leverage, 'f', 1),
                  QString::number(acct.upnl.value(p.coin), 'f', 2)},
                 acct.upnl.value(p.coin) >= 0 ? QColor("#26A65B") : QColor("#E0245E"));
    }
    positions_hdr_->setText(tr("POSITIONS · %1").arg(r.value().size()));
}

void ArenaPanelGrid::refresh_trades() {
    auto r = ArenaStore::instance().orders_for(competition_id_, agent_filter_, 200);
    auto agents_r = ArenaStore::instance().agents_for(competition_id_);
    QHash<QString, QString> names;
    if (agents_r.is_ok()) for (const auto& a : agents_r.value()) names[a.id] = a.display_name;
    trades_->setRowCount(0);
    if (r.is_err()) { trades_hdr_->setText(tr("TRADES")); return; }
    for (const auto& o : r.value()) {
        const int row = trades_->rowCount();
        trades_->insertRow(row);
        const QColor fg = o.status == QLatin1String("filled") ? QColor("#DDD")
                        : o.status == QLatin1String("liquidated") ? QColor("#FF6B6B") : QColor("#E0A800");
        fill_row(trades_, row,
                 {QDateTime::fromMSecsSinceEpoch(o.ts).toString("HH:mm:ss"), names.value(o.agent_id),
                  o.coin, o.action, o.side, QString::number(o.qty, 'g', 6),
                  QString::number(o.price, 'g', 8), QString::number(o.notional_usd, 'f', 0),
                  QString::number(o.fee, 'f', 2), QString::number(o.realized_pnl, 'f', 2),
                  o.status, o.reject_reason}, fg);
    }
    trades_hdr_->setText(tr("TRADES · %1").arg(r.value().size()));
}

void ArenaPanelGrid::refresh_risk() {
    auto r = ArenaStore::instance().agents_for(competition_id_);
    risk_->setRowCount(0);
    if (r.is_err()) return;
    for (const auto& a : r.value()) {
        const int row = risk_->rowCount();
        risk_->insertRow(row);
        fill_row(risk_, row, {a.display_name, a.status, QString::number(a.consecutive_failures),
                              a.halt_reason},
                 a.status == QLatin1String("active") ? QColor("#26A65B") : QColor("#E0245E"));
    }
}

void ArenaPanelGrid::refresh_audit() {
    auto r = ArenaStore::instance().recent_events(competition_id_, 200);
    audit_->clear();
    if (r.is_ok()) {
        for (const auto& line : r.value()) audit_->addItem(line);
        audit_hdr_->setText(tr("AUDIT · %1").arg(r.value().size()));
    }
}

void ArenaPanelGrid::update_hitl_banner_visibility() {
    bool any = false;
    for (int i = 0; i < hitl_row_->count(); ++i)
        if (auto* w = hitl_row_->itemAt(i)->widget(); w && w->isVisibleTo(hitl_banner_)) { any = true; break; }
    hitl_banner_->setVisible(any);
}

void ArenaPanelGrid::add_hitl(const QString& approval_id, const QString& agent_label,
                              const QString& summary) {
    auto* entry = new QWidget;
    entry->setStyleSheet("background:#5C1F1F;border:none;");
    auto* h = new QHBoxLayout(entry);
    h->setContentsMargins(6, 1, 6, 1);
    h->setSpacing(6);
    auto* label = new QLabel(QStringLiteral("%1: %2").arg(agent_label, summary));
    label->setTextFormat(Qt::PlainText);   // summary may carry model-derived text
    label->setStyleSheet("color:#FFD;font-size:11px;");
    h->addWidget(label, 1);
    auto* approve = new QPushButton(tr("✓ APPROVE"));
    approve->setStyleSheet("background:#0A4A18;color:#9F9;font-size:10px;font-weight:700;"
                           "border:none;padding:2px 8px;");
    auto* reject = new QPushButton(tr("✖ REJECT"));
    reject->setStyleSheet("background:#4A0A0A;color:#F99;font-size:10px;font-weight:700;"
                          "border:none;padding:2px 8px;");
    for (auto* b : {approve, reject}) b->setCursor(Qt::PointingHandCursor);
    h->addWidget(approve); h->addWidget(reject);
    hitl_row_->addWidget(entry);
    hitl_banner_->show();
    auto resolve = [this, entry, approval_id, approve, reject](bool ok) {
        approve->setEnabled(false);   // guard against a second click racing the
        reject->setEnabled(false);    // deferred entry deletion
        emit hitl_resolved(approval_id, ok);
        entry->hide();
        entry->deleteLater();         // the clicked button is a child — defer
        update_hitl_banner_visibility();
    };
    connect(approve, &QPushButton::clicked, this, [resolve]() { resolve(true); });
    connect(reject, &QPushButton::clicked, this, [resolve]() { resolve(false); });
}

} // namespace fincept::screens::alpha_arena
