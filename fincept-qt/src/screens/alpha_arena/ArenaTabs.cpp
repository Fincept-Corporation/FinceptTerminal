#include "screens/alpha_arena/ArenaTabs.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"

#include "services/alpha_arena/ArenaEngine.h"
#include "services/alpha_arena/ArenaStore.h"

#include <QColor>
#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::alpha_arena {
using fincept::arena::ArenaEngine;
using fincept::arena::ArenaStore;

namespace {
using namespace ui::colors;

// Row height for the tables' text size (20 px at the default font size).
int table_row_height() {
    return qMax(20, QFontMetrics(ui::fonts::font(-3)).height() + 4);
}

QTableWidget* make_table(const QStringList& headers) {
    auto* t = new QTableWidget;
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(table_row_height());
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setShowGrid(false);
    t->setStyleSheet(QString("QTableWidget{background:%1;border:none;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                             "QTableWidget::item{padding:0 4px;border-bottom:1px solid %2;}"
                             "QHeaderView::section{background:%3;color:%4;border:none;"
                             " border-bottom:1px solid %5;padding:1px 4px;font-size:" + fincept::ui::fonts::px(-4) +
                             "px;font-weight:700;}")
                         .arg(BG_SURFACE(), BORDER_DIM(), BG_RAISED(), TEXT_SECONDARY(), BORDER_MED()));
    return t;
}

void fill_row(QTableWidget* t, int r, const QStringList& cells, const QColor& fg = QColor(TEXT_PRIMARY())) {
    for (int c = 0; c < cells.size(); ++c) {
        auto* it = new QTableWidgetItem(cells[c]);
        it->setForeground(fg);
        t->setItem(r, c, it);
    }
}

namespace fmtx = fincept::ui::formatting;

// Per-cell writers for the POSITIONS / TRADES tables (the whole row used to take
// one colour): text left in the default colour unless given one, numbers
// right-aligned and sorted by value.
void text_cell(QTableWidget* t, int r, int c, const QString& text, const QColor& fg = QColor(TEXT_PRIMARY())) {
    auto* it = new QTableWidgetItem(text);
    it->setForeground(fg);
    t->setItem(r, c, it);
}

QTableWidgetItem* num_cell(QTableWidget* t, int r, int c, const QString& text, double value,
                           const QColor& fg = QColor(TEXT_PRIMARY())) {
    auto* it = new ui::NumericTableWidgetItem(text, value);
    it->setForeground(fg);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    t->setItem(r, c, it);
    return it;
}

QColor side_color(const QString& side) {
    const QString s = side.toLower();
    if (s == QLatin1String("long") || s == QLatin1String("buy"))
        return QColor(POSITIVE());
    if (s == QLatin1String("short") || s == QLatin1String("sell"))
        return QColor(NEGATIVE());
    return QColor(TEXT_PRIMARY());
}

QColor pnl_color(double v) {
    return v > 0 ? QColor(POSITIVE()) : (v < 0 ? QColor(NEGATIVE()) : QColor(TEXT_SECONDARY()));
}

QLabel* micro_header(const QString& text) {
    auto* l = new QLabel(text);
    l->setMinimumHeight(18);
    l->setStyleSheet(QString("background:%1;color:%2;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;"
                             "padding:0 6px;border-bottom:1px solid %3;")
                         .arg(BG_RAISED(), TEXT_SECONDARY(), BORDER_MED()));
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
    hitl_banner_->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(tint(DANGER, 0.14), tint(DANGER, 0.35)));
    auto* bh = new QHBoxLayout(hitl_banner_);
    bh->setContentsMargins(8, 3, 8, 3);
    bh->setSpacing(8);
    auto* tag = new QLabel(tr("⚠ APPROVALS"));
    tag->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:800;").arg(DANGER()));
    bh->addWidget(tag);
    hitl_row_ = new QHBoxLayout;
    hitl_row_->setSpacing(8);
    bh->addLayout(hitl_row_, 1);
    bh->addStretch(0);
    hitl_banner_->hide();
    root->addWidget(hitl_banner_);

    // MODEL CHAT: the decisions, or a designed state while there are none yet
    // (before the first round) or the store cannot be read.
    chat_stack_ = new QStackedWidget;
    chat_ = new QTextBrowser(chat_stack_);
    chat_->setOpenExternalLinks(false);
    chat_->setStyleSheet(QString("QTextBrowser{background:%1;color:%2;border:none;font-size:" +
                                 fincept::ui::fonts::px(-3) + "px;}")
                             .arg(BG_SURFACE(), TEXT_PRIMARY()));
    chat_stack_->addWidget(chat_);
    chat_empty_ = new ui::EmptyState(chat_stack_);
    chat_empty_->set_compact(true);
    chat_stack_->addWidget(chat_empty_);
    positions_ = make_table({tr("Agent"), tr("Coin"), tr("Side"), tr("Qty"), tr("Entry"), tr("Lev"), tr("uPnL $")});
    trades_ = make_table({tr("Time"), tr("Agent"), tr("Coin"), tr("Action"), tr("Side"), tr("Qty"), tr("Price"),
                          tr("Notional"), tr("Fee"), tr("rPnL"), tr("Status"), tr("Reason")});
    risk_ = make_table({tr("Agent"), tr("Status"), tr("Fails"), tr("Halt reason")});
    // Header-click sorting; no column until the user picks one (rows keep the
    // store's order: positions by agent, trades newest first).
    for (auto* t : {positions_, trades_}) {
        t->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        t->setSortingEnabled(true);
    }
    audit_ = new QListWidget;
    audit_->setStyleSheet(QString("QListWidget{background:%1;color:%2;border:none;font-size:" +
                                  fincept::ui::fonts::px(-4) + "px;}")
                              .arg(BG_SURFACE(), TEXT_SECONDARY()));

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
    split->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(BORDER_MED()));
    split->addWidget(panel(chat_hdr_, chat_stack_));
    split->addWidget(mid);
    split->addWidget(right);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 4);
    split->setStretchFactor(2, 3);
    root->addWidget(split, 1);

    // Table cells carry item brushes and the chat is HTML with its colours
    // written in; rebuild both, at the row height of the new font size.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        for (auto* t : {positions_, trades_, risk_})
            t->verticalHeader()->setDefaultSectionSize(table_row_height());
        refresh();
    });
}

void ArenaPanelGrid::set_competition(const QString& id, bool live_mode) {
    Q_UNUSED(live_mode); // approvals only ever arrive in live mode (engine-gated)
    competition_id_ = id;
    agent_filter_.clear();
    // Drop stale approval entries from a previously viewed competition.
    while (hitl_row_->count() > 0) {
        auto* it = hitl_row_->takeAt(0);
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
    hitl_banner_->hide();
    refresh();
}

void ArenaPanelGrid::set_agent_filter(const QString& agent_id) {
    agent_filter_ = agent_id;
    refresh();
}

void ArenaPanelGrid::refresh() {
    if (competition_id_.isEmpty())
        return;
    refresh_chat();
    refresh_positions();
    refresh_trades();
    refresh_risk();
    refresh_audit();
}

void ArenaPanelGrid::refresh_positions_only() {
    if (competition_id_.isEmpty())
        return;
    refresh_positions();
}

void ArenaPanelGrid::refresh_chat() {
    auto agents_r = ArenaStore::instance().agents_for(competition_id_);
    QHash<QString, QPair<QString, QString>> meta; // id → (name, color)
    if (agents_r.is_ok())
        for (const auto& a : agents_r.value())
            meta[a.id] = {a.display_name, a.color_hex};
    auto r = ArenaStore::instance().latest_decisions(competition_id_, agent_filter_, 50);
    if (r.is_err() || r.value().isEmpty()) {
        ui::EmptyState::Spec s;
        if (r.is_err()) {
            s = ui::EmptyState::error_spec(tr("The model chat could not be read from the arena database: %1")
                                               .arg(QString::fromStdString(r.error())),
                                           tr("Model chat unavailable"), QString());
            s.primary.clear(); // it is re-read after every round
        } else if (ArenaEngine::instance().active_competition_id() == competition_id_) {
            // No time in the text (the header counts down): the state stays the
            // same between refreshes, so it is applied once, not per refresh.
            s = ui::EmptyState::no_data_spec(
                agent_filter_.isEmpty() ? tr("Waiting for the first round") : tr("No decision from this model yet"),
                tr("Each model's reasoning and orders appear here after every round — the header counts down to "
                   "the next one."));
            s.glyph = QStringLiteral("⏱");
        } else {
            s = ui::EmptyState::no_data_spec(tr("No decisions recorded"),
                                             agent_filter_.isEmpty()
                                                 ? tr("This competition ended before any model answered a round.")
                                                 : tr("This model did not answer a round in this competition."));
        }
        // Applied only when it changes (refresh() also runs on theme changes,
        // where nothing may be re-styled or shown / hidden).
        const QString key = s.title + QLatin1Char('\x1f') + s.message;
        if (key != chat_empty_key_) {
            chat_empty_key_ = key;
            chat_empty_->apply(s);
        }
        chat_stack_->setCurrentWidget(chat_empty_);
        return;
    }
    chat_stack_->setCurrentWidget(chat_);
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
                // Size and leverage are what make a decision cautious or reckless.
                QString sizing;
                const double size_usd = o.value("size_usd").toDouble();
                const double lev = o.value("leverage").toDouble();
                if (size_usd > 0)
                    sizing += QStringLiteral(" · ") + fmtx::money(size_usd, QStringLiteral("USD"), fmtx::Compact::None, 0);
                if (o.value("action").toString() == QLatin1String("open") && lev > 0)
                    sizing += QStringLiteral(" · %1x").arg(QString::number(lev, 'f', lev == std::floor(lev) ? 0 : 1));
                body += QStringLiteral("<b>%1 %2</b> %3%4 — <i>%5</i><br>")
                            .arg(o.value("action").toString().toUpper().toHtmlEscaped(),
                                 o.value("coin").toString().toHtmlEscaped(), o.value("side").toString().toHtmlEscaped(),
                                 sizing.toHtmlEscaped(), o.value("reason").toString().toHtmlEscaped());
            }
        } else {
            body = QStringLiteral("<span style='color:%1'>[%2] %3</span><br>")
                       .arg(QString(DANGER()), d.status, d.parse_error.toHtmlEscaped());
        }
        // Agent dot in its provider's brand colour, made legible on the theme.
        html += QStringLiteral("<div style='margin:6px 0'>"
                               "<span style='color:%1'>●</span> <b style='color:%2'>%3</b>"
                               " <span style='color:%4'>round %5 · %6 · %7ms · %8 tok</span><br>%9</div>")
                    .arg(m.second.isEmpty() ? QString(TEXT_SECONDARY()) : legible(QColor(m.second)),
                         QString(TEXT_PRIMARY()), m.first.toHtmlEscaped(), QString(TEXT_TERTIARY()))
                    .arg(d.round_seq)
                    .arg(QDateTime::fromMSecsSinceEpoch(d.ts).toString("HH:mm:ss"))
                    .arg(d.latency_ms)
                    .arg(d.prompt_tokens + d.completion_tokens)
                    .arg(body);
    }
    chat_->setHtml(html);
}

void ArenaPanelGrid::refresh_positions() {
    auto r = ArenaStore::instance().positions_for(competition_id_, agent_filter_);
    auto agents_r = ArenaStore::instance().agents_for(competition_id_);
    QHash<QString, QString> names;
    if (agents_r.is_ok())
        for (const auto& a : agents_r.value())
            names[a.id] = a.display_name;
    positions_->setSortingEnabled(false); // rows are inserted by index
    positions_->setRowCount(0);
    if (r.is_err()) {
        positions_hdr_->setText(tr("POSITIONS"));
        positions_->setSortingEnabled(true);
        return;
    }
    // Live uPnL exists only for the competition the engine has loaded (it marks
    // the venue). For any other one, "0.00" in green read as "flat" — show "—".
    const bool live = ArenaEngine::instance().active_competition_id() == competition_id_;
    for (const auto& p : r.value()) {
        const auto acct = live ? ArenaEngine::instance().account_view(p.agent_id) : fincept::arena::AccountView{};
        const double upnl = acct.upnl.value(p.coin);
        const int row = positions_->rowCount();
        positions_->insertRow(row);
        text_cell(positions_, row, 0, names.value(p.agent_id));
        text_cell(positions_, row, 1, p.coin);
        text_cell(positions_, row, 2, p.side.toUpper(), side_color(p.side));
        num_cell(positions_, row, 3, fmtx::quantity(p.qty, 6), p.qty);
        num_cell(positions_, row, 4, fmtx::price(p.entry_price), p.entry_price);
        num_cell(positions_, row, 5, QStringLiteral("%1x").arg(p.leverage, 0, 'f', 1), p.leverage);
        if (live)
            num_cell(positions_, row, 6, fmtx::signed_money(upnl, QStringLiteral("USD")), upnl, pnl_color(upnl));
        else
            num_cell(positions_, row, 6, QStringLiteral("—"), 0.0, QColor(TEXT_SECONDARY()));
    }
    positions_->setSortingEnabled(true);
    positions_hdr_->setText(r.value().isEmpty() ? tr("POSITIONS · none open")
                                                : tr("POSITIONS · %1").arg(r.value().size()));
}

void ArenaPanelGrid::refresh_trades() {
    auto r = ArenaStore::instance().orders_for(competition_id_, agent_filter_, 200);
    auto agents_r = ArenaStore::instance().agents_for(competition_id_);
    QHash<QString, QString> names;
    if (agents_r.is_ok())
        for (const auto& a : agents_r.value())
            names[a.id] = a.display_name;
    trades_->setSortingEnabled(false); // rows are inserted by index
    trades_->setRowCount(0);
    if (r.is_err()) {
        trades_hdr_->setText(tr("TRADES"));
        trades_->setSortingEnabled(true);
        return;
    }
    const QString usd = QStringLiteral("USD");
    for (const auto& o : r.value()) {
        const int row = trades_->rowCount();
        trades_->insertRow(row);
        // Order state colours the Status cell only: filled plain, liquidated the
        // danger red, anything else a caution.
        const QColor status_fg = o.status == QLatin1String("filled")       ? QColor(TEXT_PRIMARY())
                                 : o.status == QLatin1String("liquidated") ? QColor(DANGER())
                                                                           : QColor(WARNING());
        const QDateTime when = QDateTime::fromMSecsSinceEpoch(o.ts);
        // HH:mm:ss in the row; a competition runs for days, so the full date-time is in the tooltip.
        auto* time_item = num_cell(trades_, row, 0, fmtx::time(when, true), double(o.ts), QColor(TEXT_SECONDARY()));
        time_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        time_item->setToolTip(fmtx::date_time(when, fmtx::DateStyle::Medium, true));
        text_cell(trades_, row, 1, names.value(o.agent_id));
        text_cell(trades_, row, 2, o.coin);
        text_cell(trades_, row, 3, o.action);
        text_cell(trades_, row, 4, o.side, side_color(o.side));
        num_cell(trades_, row, 5, fmtx::quantity(o.qty, 6), o.qty);
        num_cell(trades_, row, 6, fmtx::price(o.price), o.price);
        num_cell(trades_, row, 7, fmtx::money(o.notional_usd, usd, fmtx::Compact::None, 0), o.notional_usd);
        num_cell(trades_, row, 8, fmtx::money(o.fee, usd), o.fee, QColor(TEXT_SECONDARY()));
        // An opening fill realises nothing: "—", not a 0.00.
        num_cell(trades_, row, 9, o.realized_pnl == 0.0 ? QStringLiteral("—") : fmtx::signed_money(o.realized_pnl, usd),
                 o.realized_pnl, pnl_color(o.realized_pnl));
        text_cell(trades_, row, 10, o.status, status_fg);
        text_cell(trades_, row, 11, o.reject_reason, QColor(TEXT_SECONDARY()));
    }
    trades_->setSortingEnabled(true);
    trades_hdr_->setText(r.value().isEmpty() ? tr("TRADES · none yet") : tr("TRADES · %1").arg(r.value().size()));
}

void ArenaPanelGrid::refresh_risk() {
    auto r = ArenaStore::instance().agents_for(competition_id_);
    risk_->setRowCount(0);
    if (r.is_err())
        return;
    for (const auto& a : r.value()) {
        const int row = risk_->rowCount();
        risk_->insertRow(row);
        fill_row(risk_, row, {a.display_name, a.status, QString::number(a.consecutive_failures), a.halt_reason},
                 a.status == QLatin1String("active") ? QColor(SUCCESS()) : QColor(DANGER()));
    }
}

void ArenaPanelGrid::refresh_audit() {
    auto r = ArenaStore::instance().recent_events(competition_id_, 200);
    audit_->clear();
    if (r.is_ok()) {
        for (const auto& line : r.value())
            audit_->addItem(line);
        audit_hdr_->setText(tr("AUDIT · %1").arg(r.value().size()));
    }
}

void ArenaPanelGrid::update_hitl_banner_visibility() {
    bool any = false;
    for (int i = 0; i < hitl_row_->count(); ++i)
        if (auto* w = hitl_row_->itemAt(i)->widget(); w && w->isVisibleTo(hitl_banner_)) {
            any = true;
            break;
        }
    hitl_banner_->setVisible(any);
}

void ArenaPanelGrid::add_hitl(const QString& approval_id, const QString& agent_label, const QString& summary) {
    auto* entry = new QWidget;
    entry->setStyleSheet(QString("background:%1;border:none;").arg(tint(DANGER, 0.28)));
    auto* h = new QHBoxLayout(entry);
    h->setContentsMargins(6, 1, 6, 1);
    h->setSpacing(6);
    auto* label = new QLabel(QStringLiteral("%1: %2").arg(agent_label, summary));
    label->setTextFormat(Qt::PlainText); // summary may carry model-derived text
    label->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(TEXT_PRIMARY()));
    h->addWidget(label, 1);
    // Filled status-green / status-red buttons carry the on-accent text colour.
    const QString btn_css = QString("background:%1;color:%2;font-size:" + fincept::ui::fonts::px(-4) +
                                    "px;font-weight:700;border:none;padding:2px 8px;");
    auto* approve = new QPushButton(tr("✓ APPROVE"));
    approve->setStyleSheet(btn_css.arg(SUCCESS(), TEXT_ON_ACCENT()));
    auto* reject = new QPushButton(tr("✖ REJECT"));
    reject->setStyleSheet(btn_css.arg(DANGER(), TEXT_ON_ACCENT()));
    for (auto* b : {approve, reject})
        b->setCursor(Qt::PointingHandCursor);
    h->addWidget(approve);
    h->addWidget(reject);
    hitl_row_->addWidget(entry);
    hitl_banner_->show();
    auto resolve = [this, entry, approval_id, approve, reject](bool ok) {
        approve->setEnabled(false); // guard against a second click racing the
        reject->setEnabled(false);  // deferred entry deletion
        emit hitl_resolved(approval_id, ok);
        entry->hide();
        entry->deleteLater(); // the clicked button is a child — defer
        update_hitl_banner_visibility();
    };
    connect(approve, &QPushButton::clicked, this, [resolve]() { resolve(true); });
    connect(reject, &QPushButton::clicked, this, [resolve]() { resolve(false); });
}

} // namespace fincept::screens::alpha_arena
