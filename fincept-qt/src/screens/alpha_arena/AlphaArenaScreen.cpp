#include "screens/alpha_arena/AlphaArenaScreen.h"
#include "screens/alpha_arena/ArenaTabs.h"
#include "screens/alpha_arena/EquityCurveWidget.h"
#include "screens/alpha_arena/LeaderboardCards.h"
#include "screens/alpha_arena/NewCompetitionWizard.h"
#include "services/alpha_arena/ArenaEngine.h"
#include "services/alpha_arena/ArenaStore.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace fincept::screens::alpha_arena {
using fincept::arena::ArenaEngine;
using fincept::arena::ArenaStore;

namespace {
QString fmt_mid(double v) {
    const int dp = v >= 1000 ? 1 : v >= 10 ? 2 : 4;
    return QLocale(QLocale::English, QLocale::UnitedStates).toString(v, 'f', dp);
}
} // namespace

AlphaArenaScreen::AlphaArenaScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("aaScreen");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(build_header());
    root->addWidget(build_ticker_strip());

    disclaimer_ = new QLabel(tr("⚠ LIVE MODE — orders are sent to Hyperliquid with real funds. "
                                "Past performance ≠ future results. Not financial advice."));
    disclaimer_->setStyleSheet("background:#5C1F1F;color:#FFD;padding:4px 12px;font-weight:700;"
                               "font-size:11px;");
    disclaimer_->hide();
    root->addWidget(disclaimer_);

    body_ = new QStackedWidget;
    body_->addWidget(build_empty_state());
    body_->addWidget(build_dashboard());
    root->addWidget(body_, 1);

    countdown_ = new QTimer(this);
    countdown_->setInterval(1000);
    connect(countdown_, &QTimer::timeout, this, [this]() {
        const int s = ArenaEngine::instance().next_round_in_seconds();
        countdown_label_->setText(s >= 0 ? tr("next %1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'))
                                         : QStringLiteral("--"));
        round_label_->setText(tr("ROUND %1").arg(ArenaEngine::instance().round_seq()));
    });
}

QWidget* AlphaArenaScreen::build_header() {
    auto* hdr = new QWidget;
    hdr->setFixedHeight(38);
    hdr->setStyleSheet("background:#141414;border-bottom:1px solid #2A2A2A;");
    auto* h = new QHBoxLayout(hdr);
    h->setContentsMargins(10, 0, 10, 0);
    h->setSpacing(8);
    auto* title = new QLabel(tr("ALPHA ARENA"));
    title->setStyleSheet("color:#FF8800;font-weight:700;font-size:12px;border:none;");
    h->addWidget(title);
    venue_badge_ = new QLabel(tr("PAPER"));
    status_badge_ = new QLabel(tr("IDLE"));
    round_label_ = new QLabel(tr("ROUND 0"));
    countdown_label_ = new QLabel(QStringLiteral("--"));
    for (auto* b : {venue_badge_, status_badge_})
        b->setStyleSheet("background:#222;color:#888;padding:1px 7px;font-weight:700;font-size:10px;");
    round_label_->setStyleSheet("color:#BBB;font-size:11px;border:none;"
                                "font-family:'Consolas','Courier New',monospace;");
    countdown_label_->setStyleSheet("color:#888;font-size:11px;border:none;"
                                    "font-family:'Consolas','Courier New',monospace;");
    h->addWidget(venue_badge_); h->addWidget(status_badge_);
    h->addWidget(round_label_); h->addWidget(countdown_label_);
    h->addStretch(1);
    switcher_ = new QComboBox;
    switcher_->setMinimumWidth(180);
    connect(switcher_, &QComboBox::activated, this, [this](int idx) {
        show_competition(switcher_->itemData(idx).toString());
    });
    h->addWidget(switcher_);
    pause_btn_ = new QPushButton(tr("⏸ PAUSE"));
    connect(pause_btn_, &QPushButton::clicked, this, [this]() {
        auto& e = ArenaEngine::instance();
        const bool running = e.is_running();
        auto r = running ? e.pause() : e.resume();
        if (r.is_err()) QMessageBox::warning(this, tr("Alpha Arena"), QString::fromStdString(r.error()));
    });
    force_btn_ = new QPushButton(tr("⚡ FORCE ROUND"));
    connect(force_btn_, &QPushButton::clicked, this, []() { ArenaEngine::instance().force_round(); });
    kill_btn_ = new QPushButton(tr("✖ KILL ALL"));
    kill_btn_->setStyleSheet("background:#C72020;color:#FFF;font-weight:700;");
    connect(kill_btn_, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, tr("Alpha Arena"),
                tr("Close every position and end the competition?")) != QMessageBox::Yes) return;
        if (auto r = ArenaEngine::instance().kill_all(); r.is_err())
            QMessageBox::warning(this, tr("Alpha Arena"), QString::fromStdString(r.error()));
        reload_competitions();
    });
    auto* new_btn = new QPushButton(tr("+ NEW"));
    new_btn->setStyleSheet("background:#FF8800;color:#000;font-weight:700;");
    for (auto* b : {pause_btn_, force_btn_, kill_btn_, new_btn}) b->setFixedHeight(24);
    connect(new_btn, &QPushButton::clicked, this, &AlphaArenaScreen::on_new_competition);
    for (auto* b : {pause_btn_, force_btn_, kill_btn_, new_btn}) h->addWidget(b);
    return hdr;
}

// Second strip: live mids ticker (left, monospace, colored vs previous tick)
// + inline cadence editor (right: CADENCE [spin]s ✓).
QWidget* AlphaArenaScreen::build_ticker_strip() {
    auto* strip = new QWidget;
    strip->setFixedHeight(24);
    strip->setStyleSheet("background:#0D0D0D;border-bottom:1px solid #2A2A2A;");
    auto* h = new QHBoxLayout(strip);
    h->setContentsMargins(10, 0, 10, 0);
    h->setSpacing(8);

    ticker_ = new QLabel(QStringLiteral("--"));
    ticker_->setTextFormat(Qt::RichText);
    ticker_->setStyleSheet("color:#777;font-size:11px;border:none;"
                           "font-family:'Consolas','Courier New',monospace;");
    h->addWidget(ticker_, 1);

    auto* cad = new QLabel(tr("CADENCE"));
    cad->setStyleSheet("color:#777;font-size:10px;font-weight:700;border:none;");
    h->addWidget(cad);
    cadence_spin_ = new QSpinBox;
    cadence_spin_->setRange(60, 3600);
    cadence_spin_->setValue(180);
    cadence_spin_->setSuffix(QStringLiteral("s"));
    cadence_spin_->setFixedHeight(18);
    cadence_spin_->setToolTip(tr("Round cadence (60–3600 seconds)"));
    cadence_spin_->setStyleSheet(
        "QSpinBox{background:#141414;color:#DDD;border:1px solid #2A2A2A;font-size:11px;"
        " padding:0 2px;font-family:'Consolas','Courier New',monospace;}");
    h->addWidget(cadence_spin_);
    auto* apply = new QPushButton(QStringLiteral("✓"));
    apply->setToolTip(tr("Apply cadence to the running competition"));
    apply->setFixedSize(20, 18);
    apply->setCursor(Qt::PointingHandCursor);
    apply->setStyleSheet("QPushButton{background:#222;color:#FF8800;border:1px solid #2A2A2A;"
                         "font-weight:700;padding:0;}QPushButton:hover{background:#2A2A2A;}");
    connect(apply, &QPushButton::clicked, this, [this]() {
        if (auto r = ArenaEngine::instance().set_cadence(cadence_spin_->value()); r.is_err())
            QMessageBox::warning(this, tr("Alpha Arena"), QString::fromStdString(r.error()));
    });
    h->addWidget(apply);
    return strip;
}

QWidget* AlphaArenaScreen::build_empty_state() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->addStretch(2);
    auto* big = new QLabel(tr("ALPHA ARENA"));
    big->setAlignment(Qt::AlignCenter);
    big->setStyleSheet("color:#FF8800;font-size:28px;font-weight:800;");
    auto* sub = new QLabel(tr("Pit LLMs against each other in a live crypto trading competition.\n"
                              "Real Hyperliquid market data · paper money · any provider, or free with Ollama."));
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color:#999;font-size:13px;");
    auto* cta = new QPushButton(tr("+  NEW COMPETITION"));
    cta->setFixedSize(260, 44);
    cta->setStyleSheet("background:#FF8800;color:#000;font-weight:800;font-size:14px;");
    connect(cta, &QPushButton::clicked, this, &AlphaArenaScreen::on_new_competition);
    v->addWidget(big);
    v->addWidget(sub);
    auto* hb = new QHBoxLayout;
    hb->addStretch(1); hb->addWidget(cta); hb->addStretch(1);
    v->addLayout(hb);
    v->addStretch(3);
    return page;
}

QWidget* AlphaArenaScreen::build_dashboard() {
    chart_ = new EquityCurveWidget;
    cards_ = new LeaderboardCards;
    grid_ = new ArenaPanelGrid;

    auto* top = new QSplitter(Qt::Horizontal);
    top->setHandleWidth(1);
    top->setChildrenCollapsible(false);
    top->setStyleSheet("QSplitter::handle{background:#2A2A2A;}");
    top->addWidget(chart_);
    top->addWidget(cards_);
    top->setStretchFactor(0, 58);
    top->setStretchFactor(1, 42);
    top->setSizes({580, 420});

    auto* body = new QSplitter(Qt::Vertical);
    body->setHandleWidth(1);
    body->setChildrenCollapsible(false);
    body->setStyleSheet("QSplitter::handle{background:#2A2A2A;}");
    body->addWidget(top);
    body->addWidget(grid_);
    body->setStretchFactor(0, 45);
    body->setStretchFactor(1, 55);
    body->setSizes({450, 550});

    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(body, 1);

    connect(cards_, &LeaderboardCards::agent_selected, this, [this](const QString& id) {
        grid_->set_agent_filter(id);
    });
    connect(cards_, &LeaderboardCards::halt_requested, this, [this](const QString& id) {
        ArenaEngine::instance().halt_agent(id); refresh_dashboard();
    });
    connect(cards_, &LeaderboardCards::resume_requested, this, [this](const QString& id) {
        ArenaEngine::instance().resume_agent(id); refresh_dashboard();
    });
    connect(cards_, &LeaderboardCards::kill_requested, this, [this](const QString& id) {
        QString name = id.left(8);
        for (const auto& a : agents_cache_)
            if (a.id == id) name = a.display_name;
        if (QMessageBox::question(this, tr("Alpha Arena"),
                tr("Close all positions of %1 and halt it?").arg(name)) != QMessageBox::Yes) return;
        if (auto r = ArenaEngine::instance().kill_agent(id); r.is_err())
            QMessageBox::warning(this, tr("Alpha Arena"), QString::fromStdString(r.error()));
        refresh_dashboard();
    });
    connect(grid_, &ArenaPanelGrid::hitl_resolved, this, [](const QString& id, bool ok) {
        ArenaEngine::instance().resume_after_hitl(id, ok);
    });
    return page;
}

void AlphaArenaScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect_engine();
    reload_competitions();
    const QString active = ArenaEngine::instance().active_competition_id();
    if (!active.isEmpty()) show_competition(active);
    else if (!competition_id_.isEmpty()) show_competition(competition_id_);
    countdown_->start();
    // The engine's crash_recovery_pending signal fires at app startup, before
    // any screen exists — poll the queryable list here so the offer isn't lost.
    // Deferred: a modal dialog inside showEvent can re-enter during dock
    // retiling and stack a second prompt.
    QTimer::singleShot(0, this, &AlphaArenaScreen::maybe_offer_crash_recovery);
}

void AlphaArenaScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    disconnect_engine();
    countdown_->stop();
}

void AlphaArenaScreen::connect_engine() {
    if (!conns_.isEmpty()) return;   // re-show without hide must not stack duplicates
    auto& eng = ArenaEngine::instance();
    conns_ << connect(&eng, &ArenaEngine::round_completed, this, [this](int) { refresh_dashboard(); });
    conns_ << connect(&eng, &ArenaEngine::decision_received, this, [this](QString, QString, QString) {
        if (grid_) grid_->refresh();
    });
    conns_ << connect(&eng, &ArenaEngine::order_executed, this, [this](QString, QString, QString) {
        if (grid_) grid_->refresh();
    });
    conns_ << connect(&eng, &ArenaEngine::agent_status_changed, this, [this](QString, QString, QString) {
        refresh_dashboard();
    });
    conns_ << connect(&eng, &ArenaEngine::competition_status_changed, this,
                      [this](QString id, QString status) {
        if (id == competition_id_) update_badges(status);   // don't restyle a viewed past comp
        reload_competitions();
    });
    conns_ << connect(&eng, &ArenaEngine::hitl_requested, this,
                      [this](QString id, QString agent_id, QString summary) {
        QString label = agent_id.left(8);
        if (auto r = ArenaStore::instance().agents_for(competition_id_); r.is_ok())
            for (const auto& a : r.value()) if (a.id == agent_id) label = a.display_name;
        if (grid_) grid_->add_hitl(id, label, summary);
    });
    conns_ << connect(&eng, &ArenaEngine::crash_recovery_pending, this,
                      [this](QStringList) { maybe_offer_crash_recovery(); });
    // Realtime: between-round live marks. Keep this handler LIGHT — ticker text,
    // board rows from cached agent meta + account_view, positions uPnL column.
    // No equity_series/chart re-read here (chart refreshes per round).
    conns_ << connect(&eng, &ArenaEngine::marks_updated, this, [this]() {
        update_ticker();
        rebuild_board_light();
        if (grid_) grid_->refresh_positions_only();
    });
}

void AlphaArenaScreen::disconnect_engine() {
    for (auto& c : conns_) QObject::disconnect(c);
    conns_.clear();
}

void AlphaArenaScreen::maybe_offer_crash_recovery() {
    static bool prompt_open = false;   // multiple deferred show events must not stack dialogs
    if (prompt_open) return;
    auto& eng = ArenaEngine::instance();
    const QStringList ids = eng.pending_crash_recoveries();
    if (ids.isEmpty() || !eng.active_competition_id().isEmpty()) return;
    prompt_open = true;
    const auto pick = QMessageBox::question(this, tr("Alpha Arena — Crash Recovery"),
        tr("The previous session left %1 competition(s) running.\nResume the most recent one?")
            .arg(ids.size()));
    prompt_open = false;
    if (pick == QMessageBox::Yes) {
        if (auto r = eng.start(ids.first()); r.is_ok()) show_competition(ids.first());
    } else {
        for (const auto& id : ids) eng.dismiss_crash_recovery(id);
    }
    reload_competitions();
}

void AlphaArenaScreen::reload_competitions() {
    switcher_->blockSignals(true);
    switcher_->clear();
    if (auto r = ArenaStore::instance().list_competitions(); r.is_ok())
        for (const auto& c : r.value())
            switcher_->addItem(QStringLiteral("%1 (%2)").arg(c.name, c.status), c.id);
    for (int i = 0; i < switcher_->count(); ++i)
        if (switcher_->itemData(i).toString() == competition_id_) switcher_->setCurrentIndex(i);
    switcher_->blockSignals(false);
}

void AlphaArenaScreen::show_competition(const QString& id) {
    if (id.isEmpty()) { body_->setCurrentIndex(0); return; }
    competition_id_ = id;
    auto comp = ArenaStore::instance().competition(id);
    if (comp.is_err()) { body_->setCurrentIndex(0); return; }
    body_->setCurrentIndex(1);
    grid_->set_competition(id, comp.value().live_mode);
    disclaimer_->setVisible(comp.value().live_mode);
    cadence_spin_->setValue(comp.value().cadence_seconds);
    last_mids_.clear();   // fresh ticker baseline for the new competition
    venue_badge_->setText(comp.value().live_mode ? tr("HYPERLIQUID • LIVE") : tr("PAPER"));
    venue_badge_->setStyleSheet(comp.value().live_mode
        ? "background:#5C1F1F;color:#FFD;padding:1px 7px;font-weight:700;font-size:10px;"
        : "background:#222;color:#0F0;padding:1px 7px;font-weight:700;font-size:10px;");
    update_badges(comp.value().status);
    refresh_dashboard();
    reload_competitions();
}

void AlphaArenaScreen::refresh_dashboard() {
    if (competition_id_.isEmpty()) return;
    auto comp = ArenaStore::instance().competition(competition_id_);
    auto agents = ArenaStore::instance().agents_for(competition_id_);
    if (comp.is_err() || agents.is_err()) return;
    cap_ = comp.value().capital_per_agent;
    agents_cache_ = agents.value();
    const bool is_active = ArenaEngine::instance().active_competition_id() == competition_id_;

    // Chart series from equity snapshots.
    auto eq = ArenaStore::instance().equity_series(competition_id_);
    QHash<QString, EquitySeries> by_agent;
    for (const auto& a : agents.value()) {
        EquitySeries s; s.agent_id = a.id; s.label = a.display_name; s.color = QColor(a.color_hex);
        by_agent[a.id] = s;
    }
    if (eq.is_ok())
        for (const auto& snap : eq.value())
            if (by_agent.contains(snap.agent_id))
                by_agent[snap.agent_id].points.append(QPointF(double(snap.ts), snap.equity));
    chart_->set_data(by_agent.values(), cap_);

    // Agent board.
    auto tokens = ArenaStore::instance().token_totals(competition_id_);
    tokens_cache_ = tokens.is_ok() ? tokens.value() : QHash<QString, qint64>{};
    QVector<AgentCardData> rows;
    for (const auto& a : agents.value()) {
        AgentCardData c;
        c.agent_id = a.id; c.name = a.display_name; c.status = a.status; c.color = QColor(a.color_hex);
        c.tokens = tokens_cache_.value(a.id);
        if (is_active) {
            const auto acct = ArenaEngine::instance().account_view(a.id);
            c.equity = acct.equity; c.open_positions = acct.positions.size();
            for (auto it = acct.upnl.cbegin(); it != acct.upnl.cend(); ++it) c.upnl += it.value();
        } else if (!by_agent[a.id].points.isEmpty()) {
            c.equity = by_agent[a.id].points.last().y();
        } else {
            c.equity = cap_;
        }
        c.pnl_pct = cap_ > 0 ? (c.equity - cap_) / cap_ * 100.0 : 0;
        rows.append(c);
    }
    std::sort(rows.begin(), rows.end(),
              [](const AgentCardData& x, const AgentCardData& y) { return x.equity > y.equity; });
    cards_->set_data(rows);
    grid_->refresh();
    update_ticker();
}

// Mids ticker: `BTC 94,126.0 ▲ · ETH 3,120.5 ▼ · …`, arrow/color vs the
// previously displayed mid. Runs every marks tick — string building only.
void AlphaArenaScreen::update_ticker() {
    const auto mids = ArenaEngine::instance().current_marks();
    if (mids.isEmpty()) { ticker_->setText(QStringLiteral("--")); return; }
    QStringList coins = mids.keys();
    std::sort(coins.begin(), coins.end());
    QStringList parts;
    for (const auto& coin : coins) {
        const double m = mids.value(coin);
        const double prev = last_mids_.value(coin, m);
        const QString color = m > prev ? QStringLiteral("#26A65B")
                            : m < prev ? QStringLiteral("#E0245E") : QStringLiteral("#999");
        const QString arrow = m > prev ? QStringLiteral("▲")
                            : m < prev ? QStringLiteral("▼") : QStringLiteral("–");
        parts << QStringLiteral("<span style='color:#777'>%1</span>"
                                " <span style='color:%2'>%3 %4</span>")
                     .arg(coin.toHtmlEscaped(), color, fmt_mid(m), arrow);
    }
    last_mids_ = mids;
    ticker_->setText(parts.join(QStringLiteral(" <span style='color:#444'>·</span> ")));
}

// Cheap board rebuild on a marks tick: cached agent meta/tokens + live
// account_view (≤8 agents). Skips entirely when viewing a past competition.
void AlphaArenaScreen::rebuild_board_light() {
    if (competition_id_.isEmpty() || agents_cache_.isEmpty()) return;
    if (ArenaEngine::instance().active_competition_id() != competition_id_) return;
    QVector<AgentCardData> rows;
    for (const auto& a : agents_cache_) {
        AgentCardData c;
        c.agent_id = a.id; c.name = a.display_name; c.status = a.status; c.color = QColor(a.color_hex);
        c.tokens = tokens_cache_.value(a.id);
        const auto acct = ArenaEngine::instance().account_view(a.id);
        c.equity = acct.equity; c.open_positions = acct.positions.size();
        for (auto it = acct.upnl.cbegin(); it != acct.upnl.cend(); ++it) c.upnl += it.value();
        c.pnl_pct = cap_ > 0 ? (c.equity - cap_) / cap_ * 100.0 : 0;
        rows.append(c);
    }
    std::sort(rows.begin(), rows.end(),
              [](const AgentCardData& x, const AgentCardData& y) { return x.equity > y.equity; });
    cards_->set_data(rows);
}

void AlphaArenaScreen::on_new_competition() {
    if (ArenaEngine::instance().is_running()) {
        QMessageBox::information(this, tr("Alpha Arena"),
            tr("A competition is already running. Stop it (KILL ALL) before starting a new one."));
        return;
    }
    NewCompetitionWizard wiz(this);
    if (wiz.exec() != QDialog::Accepted) return;
    auto create_r = ArenaEngine::instance().create_competition(wiz.config());
    if (create_r.is_err()) {
        QMessageBox::critical(this, tr("Alpha Arena"), QString::fromStdString(create_r.error()));
        return;
    }
    if (auto r = ArenaEngine::instance().start(create_r.value()); r.is_err()) {
        QMessageBox::critical(this, tr("Alpha Arena"), QString::fromStdString(r.error()));
        return;
    }
    show_competition(create_r.value());
}

void AlphaArenaScreen::update_badges(const QString& status) {
    status_badge_->setText(status.isEmpty() ? tr("IDLE") : status.toUpper());
    if (status == QLatin1String("running")) {
        status_badge_->setStyleSheet("background:#0A4A18;color:#0F0;padding:1px 7px;font-weight:700;font-size:10px;");
        pause_btn_->setText(tr("⏸ PAUSE"));
    } else {
        status_badge_->setStyleSheet("background:#222;color:#888;padding:1px 7px;font-weight:700;font-size:10px;");
        pause_btn_->setText(tr("▶ RESUME"));
    }
}

void AlphaArenaScreen::restore_state(const QVariantMap& state) {
    competition_id_ = state.value(QStringLiteral("competition_id")).toString();
}

QVariantMap AlphaArenaScreen::save_state() const {
    return {{QStringLiteral("competition_id"), competition_id_}};
}

} // namespace fincept::screens::alpha_arena
