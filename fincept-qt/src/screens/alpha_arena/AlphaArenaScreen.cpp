#include "screens/alpha_arena/AlphaArenaScreen.h"

#include "core/logging/Logger.h"
#include "screens/alpha_arena/panels/LeaderboardPanel.h"
#include "screens/alpha_arena/panels/LiveModeGateDialog.h"
#include "screens/alpha_arena/panels/ModelChatPanel.h"
#include "screens/alpha_arena/panels/StatePanels.h"
#include "services/alpha_arena/AlphaArenaEngine.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens::alpha_arena {

using fincept::services::alpha_arena::AgentSpec;
using fincept::services::alpha_arena::AlphaArenaEngine;
using fincept::services::alpha_arena::CompetitionMode;
using fincept::services::alpha_arena::NewCompetitionConfig;
using fincept::services::alpha_arena::kPerpUniverse;

// ── Construction ────────────────────────────────────────────────────────────

AlphaArenaScreen::AlphaArenaScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("aaScreen");
    build_ui();

    countdown_timer_ = new QTimer(this);
    countdown_timer_->setInterval(1000);
    connect(countdown_timer_, &QTimer::timeout, this, &AlphaArenaScreen::on_countdown_tick);

    populate_model_list();
}

void AlphaArenaScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_header());

    disclaimer_ = new QLabel(QStringLiteral(
        "⚠ LIVE MODE — orders are sent to Hyperliquid with real funds. "
        "Past performance ≠ future results. Not financial advice."));
    disclaimer_->setObjectName("aaDisclaimer");
    disclaimer_->setStyleSheet("background:#5C1F1F;color:#FFD; padding:6px 12px; font-weight:700;");
    disclaimer_->hide();
    root->addWidget(disclaimer_);

    root->addWidget(build_create_panel());
    root->addWidget(build_main(), 1);
}

QWidget* AlphaArenaScreen::build_header() {
    auto* hdr = new QWidget;
    hdr->setObjectName("aaHeader");
    hdr->setFixedHeight(44);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("ALPHA ARENA"));
    title->setStyleSheet("color:#FF8800;font-weight:700;font-size:13px;");
    hl->addWidget(title);

    venue_badge_ = new QLabel(QStringLiteral("PAPER"));
    venue_badge_->setStyleSheet("background:#222;color:#0F0;padding:2px 8px;font-weight:700;");
    hl->addWidget(venue_badge_);

    status_badge_ = new QLabel(QStringLiteral("IDLE"));
    status_badge_->setStyleSheet("background:#222;color:#888;padding:2px 8px;font-weight:700;");
    hl->addWidget(status_badge_);

    tick_label_ = new QLabel(QStringLiteral("TICK 0"));
    tick_label_->setStyleSheet("color:#bbb;");
    hl->addWidget(tick_label_);

    countdown_label_ = new QLabel(QStringLiteral("--"));
    countdown_label_->setStyleSheet("color:#888;");
    hl->addWidget(countdown_label_);

    hl->addStretch(1);

    force_tick_btn_ = new QPushButton(QStringLiteral("FORCE TICK"));
    connect(force_tick_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_force_tick_clicked);
    hl->addWidget(force_tick_btn_);

    live_mode_btn_ = new QPushButton(QStringLiteral("LIVE MODE…"));
    connect(live_mode_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_live_mode_toggle_clicked);
    hl->addWidget(live_mode_btn_);

    kill_all_btn_ = new QPushButton(QStringLiteral("KILL ALL"));
    kill_all_btn_->setStyleSheet("background:#C72020;color:#fff;font-weight:700;");
    connect(kill_all_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_kill_all_clicked);
    hl->addWidget(kill_all_btn_);

    auto* cadence_label = new QLabel(QStringLiteral("CADENCE"));
    cadence_label->setStyleSheet("color:#888;");
    hl->addWidget(cadence_label);
    hot_cadence_spin_ = new QSpinBox;
    hot_cadence_spin_->setRange(10, 3600);
    hot_cadence_spin_->setValue(180);
    hot_cadence_spin_->setSuffix(QStringLiteral("s"));
    hl->addWidget(hot_cadence_spin_);
    auto* apply_btn = new QPushButton(QStringLiteral("APPLY"));
    connect(apply_btn, &QPushButton::clicked, this, [this]() {
        auto r = AlphaArenaEngine::instance().set_cadence(hot_cadence_spin_->value());
        if (r.is_err()) {
            QMessageBox::warning(this, tr("Alpha Arena"), QString::fromStdString(r.error()));
        }
    });
    hl->addWidget(apply_btn);

    return hdr;
}

QWidget* AlphaArenaScreen::build_create_panel() {
    create_panel_ = new QGroupBox(QStringLiteral("New competition"));
    auto* grid = new QGridLayout(create_panel_);
    int row = 0;

    grid->addWidget(new QLabel(QStringLiteral("Name")), row, 0);
    comp_name_ = new QLineEdit(QStringLiteral("AlphaArena-1"));
    grid->addWidget(comp_name_, row++, 1);

    grid->addWidget(new QLabel(QStringLiteral("Mode")), row, 0);
    comp_mode_ = new QComboBox;
    comp_mode_->addItem(QStringLiteral("Baseline"), QVariant::fromValue(int(CompetitionMode::Baseline)));
    comp_mode_->addItem(QStringLiteral("Monk"), QVariant::fromValue(int(CompetitionMode::Monk)));
    comp_mode_->addItem(QStringLiteral("Situational"), QVariant::fromValue(int(CompetitionMode::Situational)));
    grid->addWidget(comp_mode_, row++, 1);

    grid->addWidget(new QLabel(QStringLiteral("Venue")), row, 0);
    auto* venue_box = new QWidget;
    auto* vh = new QHBoxLayout(venue_box);
    vh->setContentsMargins(0, 0, 0, 0);
    venue_paper_ = new QRadioButton(QStringLiteral("Paper"));
    venue_hl_ = new QRadioButton(QStringLiteral("Hyperliquid (live)"));
    venue_us_eq_ = new QRadioButton(QStringLiteral("US Equities (Season 2 — Coming)"));
    venue_us_eq_->setEnabled(false);
    venue_paper_->setChecked(true);
    auto* grp = new QButtonGroup(venue_box);
    grp->addButton(venue_paper_);
    grp->addButton(venue_hl_);
    grp->addButton(venue_us_eq_);
    vh->addWidget(venue_paper_);
    vh->addWidget(venue_hl_);
    vh->addWidget(venue_us_eq_);
    vh->addStretch(1);
    grid->addWidget(venue_box, row++, 1);

    grid->addWidget(new QLabel(QStringLiteral("Initial capital / agent")), row, 0);
    comp_capital_ = new QDoubleSpinBox;
    comp_capital_->setRange(100, 1'000'000);
    comp_capital_->setValue(10000);
    grid->addWidget(comp_capital_, row++, 1);

    grid->addWidget(new QLabel(QStringLiteral("Cadence (s)")), row, 0);
    comp_cadence_ = new QSpinBox;
    comp_cadence_->setRange(60, 600);
    comp_cadence_->setValue(180);
    grid->addWidget(comp_cadence_, row++, 1);

    grid->addWidget(new QLabel(QStringLiteral("Instruments")), row, 0);
    comp_instruments_ = new QListWidget;
    comp_instruments_->setSelectionMode(QAbstractItemView::MultiSelection);
    comp_instruments_->setMaximumHeight(80);
    for (const auto& sym : kPerpUniverse()) {
        auto* it = new QListWidgetItem(sym, comp_instruments_);
        it->setSelected(true);
    }
    grid->addWidget(comp_instruments_, row++, 1);

    grid->addWidget(new QLabel(QStringLiteral("Models")), row, 0);
    model_list_ = new QListWidget;
    model_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    model_list_->setMaximumHeight(120);
    grid->addWidget(model_list_, row++, 1);

    auto* btn_row = new QWidget;
    auto* bh = new QHBoxLayout(btn_row);
    bh->setContentsMargins(0, 0, 0, 0);
    create_btn_ = new QPushButton(QStringLiteral("CREATE"));
    start_btn_ = new QPushButton(QStringLiteral("START"));
    start_btn_->setEnabled(false);
    bh->addStretch(1);
    bh->addWidget(create_btn_);
    bh->addWidget(start_btn_);
    grid->addWidget(btn_row, row++, 1);

    connect(create_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_create_clicked);
    connect(start_btn_, &QPushButton::clicked, this, &AlphaArenaScreen::on_start_clicked);

    return create_panel_;
}

QWidget* AlphaArenaScreen::build_right_stack() {
    auto* container = new QWidget;
    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* tabs = new QWidget;
    auto* tabs_h = new QHBoxLayout(tabs);
    tabs_h->setContentsMargins(0, 0, 0, 0);
    const QStringList labels = {QStringLiteral("MODEL CHAT"), QStringLiteral("POSITIONS"),
                                QStringLiteral("HITL"), QStringLiteral("RISK"), QStringLiteral("AUDIT")};
    for (int i = 0; i < labels.size(); ++i) {
        auto* b = new QPushButton(labels[i]);
        b->setProperty("active", i == 0);
        connect(b, &QPushButton::clicked, this, [this, i]() { on_right_tab_changed(i); });
        right_tab_btns_.append(b);
        tabs_h->addWidget(b);
    }
    tabs_h->addStretch(1);
    root->addWidget(tabs);

    right_stack_ = new QStackedWidget;
    modelchat_ = new ModelChatPanel;
    positions_ = new PositionsPanel;
    hitl_ = new HitlPanel;
    risk_ = new RiskPanel;
    audit_ = new AuditPanel;
    right_stack_->addWidget(modelchat_);
    right_stack_->addWidget(positions_);
    right_stack_->addWidget(hitl_);
    right_stack_->addWidget(risk_);
    right_stack_->addWidget(audit_);
    root->addWidget(right_stack_, 1);

    connect(hitl_, &HitlPanel::approval_resolved,
            this, &AlphaArenaScreen::on_hitl_resolved);

    return container;
}

QWidget* AlphaArenaScreen::build_main() {
    auto* split = new QSplitter(Qt::Horizontal);
    leaderboard_ = new LeaderboardPanel;
    connect(leaderboard_, &LeaderboardPanel::agent_double_clicked, this,
            [this](const QString& agent_id) {
                if (agent_id.isEmpty()) return;
                QStringList opts{tr("Halt"), tr("Resume")};
                bool ok = false;
                const QString choice = QInputDialog::getItem(
                    this, tr("Agent action"),
                    tr("Action for agent %1?").arg(agent_id.left(8)),
                    opts, 0, false, &ok);
                if (!ok) return;
                auto& engine = AlphaArenaEngine::instance();
                auto r = (choice == opts[0]) ? engine.halt_agent(agent_id)
                                              : engine.resume_agent(agent_id);
                if (r.is_err()) {
                    QMessageBox::warning(this, tr("Alpha Arena"),
                                         QString::fromStdString(r.error()));
                }
                refresh_all_panels();
            });
    split->addWidget(leaderboard_);
    split->addWidget(build_right_stack());
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    return split;
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void AlphaArenaScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    populate_model_list();

    // Reattach to the engine's currently-active competition (if any).
    auto& engine = AlphaArenaEngine::instance();
    if (!engine.active_competition_id().isEmpty()) {
        apply_competition_id(engine.active_competition_id());
    }

    connect_engine_signals();
    refresh_all_panels();
    countdown_timer_->start();
}

void AlphaArenaScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    disconnect_engine_signals();
    countdown_timer_->stop();
}

void AlphaArenaScreen::connect_engine_signals() {
    auto& engine = AlphaArenaEngine::instance();
    engine_conns_.append(connect(&engine, &AlphaArenaEngine::tick_fired,
                                  this, &AlphaArenaScreen::on_engine_tick));
    engine_conns_.append(connect(&engine, &AlphaArenaEngine::decision_received,
                                  this, &AlphaArenaScreen::on_engine_decision_received));
    engine_conns_.append(connect(&engine, &AlphaArenaEngine::hitl_requested,
                                  this, &AlphaArenaScreen::on_engine_hitl_requested));
    engine_conns_.append(connect(&engine, &AlphaArenaEngine::agent_circuit_open,
                                  this, &AlphaArenaScreen::on_engine_circuit_open));
    engine_conns_.append(connect(&engine, &AlphaArenaEngine::competition_status_changed,
                                  this, &AlphaArenaScreen::on_engine_status_changed));
    engine_conns_.append(connect(&engine, &AlphaArenaEngine::crash_recovery_pending,
                                  this, &AlphaArenaScreen::on_engine_crash_recovery));
}

void AlphaArenaScreen::disconnect_engine_signals() {
    for (auto& c : engine_conns_) QObject::disconnect(c);
    engine_conns_.clear();
}

// ── User intents ───────────────────────────────────────────────────────────

void AlphaArenaScreen::on_create_clicked() {
    NewCompetitionConfig cfg;
    cfg.name = comp_name_->text();
    cfg.mode = static_cast<CompetitionMode>(comp_mode_->currentData().toInt());
    cfg.initial_capital_per_agent = comp_capital_->value();
    cfg.cadence_seconds = comp_cadence_->value();
    cfg.venue_kind = venue_hl_->isChecked() ? QStringLiteral("hyperliquid") : QStringLiteral("paper");
    cfg.live_mode = live_mode_engaged_;

    for (int i = 0; i < comp_instruments_->count(); ++i) {
        auto* it = comp_instruments_->item(i);
        if (it->isSelected()) cfg.instruments << it->text();
    }
    if (cfg.instruments.isEmpty()) {
        QMessageBox::warning(this, tr("Alpha Arena"), tr("Select at least one instrument."));
        return;
    }

    QList<QListWidgetItem*> selected = model_list_->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, tr("Alpha Arena"), tr("Select at least one model."));
        return;
    }
    for (auto* it : selected) {
        const int idx = model_list_->row(it);
        if (idx < 0 || idx >= model_entries_.size()) continue;
        const auto& m = model_entries_[idx];
        AgentSpec a;
        a.provider = m.provider;
        a.model_id = m.model_id;
        a.display_name = m.display_name;
        a.api_key = m.api_key;
        a.base_url = m.base_url;
        cfg.agents.append(a);
    }

    auto r = AlphaArenaEngine::instance().create_competition(cfg);
    if (r.is_err()) {
        QMessageBox::critical(this, tr("Alpha Arena"),
                              QString::fromStdString(r.error()));
        return;
    }
    apply_competition_id(r.value());
    start_btn_->setEnabled(true);
    LOG_INFO("AlphaArena", QStringLiteral("Competition created: %1").arg(r.value()));
}

void AlphaArenaScreen::on_start_clicked() {
    if (competition_id_.isEmpty()) return;
    auto r = AlphaArenaEngine::instance().start(competition_id_);
    if (r.is_err()) {
        QMessageBox::critical(this, tr("Alpha Arena"),
                              QString::fromStdString(r.error()));
        return;
    }
    create_panel_->hide();
    update_status_badge(QStringLiteral("running"));
}

void AlphaArenaScreen::on_force_tick_clicked() {
    AlphaArenaEngine::instance();
    // TickClock::force_tick is the public engine surface for this; if absent
    // (engine not running) we just refresh panels as a no-op.
    refresh_all_panels();
}

void AlphaArenaScreen::on_kill_all_clicked() {
    if (QMessageBox::question(this, tr("Alpha Arena"),
                              tr("Close every agent's positions and stop the competition?\n"
                                 "This cannot be undone."),
                              QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    auto r = AlphaArenaEngine::instance().kill_all();
    if (r.is_err()) {
        QMessageBox::critical(this, tr("Alpha Arena"),
                              QString::fromStdString(r.error()));
    }
    update_status_badge(QStringLiteral("halted_by_user"));
}

void AlphaArenaScreen::on_live_mode_toggle_clicked() {
    if (competition_id_.isEmpty()) {
        QMessageBox::information(this, tr("Alpha Arena"),
                                 tr("Create a competition first, then engage live mode."));
        return;
    }
    // Geofence (best-effort).
#ifndef FINCEPT_UNSAFE_DISABLE_GEOFENCE
    const auto territory = QLocale::system().territory();
    if (territory == QLocale::UnitedStates ||
        territory == QLocale::NorthKorea ||
        territory == QLocale::Iran) {
        QMessageBox::critical(this, tr("Alpha Arena"),
                              tr("Live mode is unavailable in your jurisdiction."));
        return;
    }
#endif
    LiveModeGateDialog dlg(competition_id_, this);
    if (dlg.exec() == QDialog::Accepted) {
        live_mode_engaged_ = true;
        show_disclaimer_if_live(true);
        update_venue_badge();
    }
}

void AlphaArenaScreen::on_right_tab_changed(int idx) {
    if (idx < 0 || idx >= right_stack_->count()) return;
    right_stack_->setCurrentIndex(idx);
    for (int i = 0; i < right_tab_btns_.size(); ++i) {
        right_tab_btns_[i]->setProperty("active", i == idx);
        right_tab_btns_[i]->style()->unpolish(right_tab_btns_[i]);
        right_tab_btns_[i]->style()->polish(right_tab_btns_[i]);
    }
    refresh_all_panels();
}

// ── Engine signal handlers ─────────────────────────────────────────────────

void AlphaArenaScreen::on_engine_tick(int seq) {
    tick_label_->setText(QStringLiteral("TICK %1").arg(seq));
    refresh_all_panels();
}

void AlphaArenaScreen::on_engine_decision_received(QString decision_id, QString agent_id) {
    if (modelchat_) modelchat_->on_decision_received(decision_id, agent_id);
}

void AlphaArenaScreen::on_engine_hitl_requested(QString approval_id, QString agent_id, QString summary) {
    if (hitl_) hitl_->on_hitl_requested(approval_id, agent_id, summary);
    on_right_tab_changed(2); // surface HITL tab
}

void AlphaArenaScreen::on_engine_circuit_open(QString agent_id, QString reason) {
    LOG_WARN("AlphaArena", QStringLiteral("Agent %1 circuit open: %2").arg(agent_id, reason));
    if (risk_) risk_->refresh();
}

void AlphaArenaScreen::on_engine_status_changed(QString /*competition_id*/, QString status) {
    update_status_badge(status);
}

void AlphaArenaScreen::on_engine_crash_recovery(QStringList competition_ids) {
    if (competition_ids.isEmpty()) return;
    QMessageBox::warning(this, tr("Alpha Arena — Crash Recovery"),
        tr("The previous session left %1 competition(s) running. Open the "
           "competition picker to Resume / Halt / Reconcile.")
            .arg(competition_ids.size()));
}

void AlphaArenaScreen::on_hitl_resolved(QString approval_id, bool approved) {
    AlphaArenaEngine::instance().resume_after_hitl(approval_id, approved);
}

void AlphaArenaScreen::on_countdown_tick() {
    auto& engine = AlphaArenaEngine::instance();
    if (!engine.is_running()) {
        countdown_label_->setText(QStringLiteral("--"));
        return;
    }
    countdown_label_->setText(QStringLiteral("%1s")
                                  .arg(engine.next_tick_in_seconds()));
}

// ── Helpers ────────────────────────────────────────────────────────────────

void AlphaArenaScreen::populate_model_list() {
    model_list_->clear();
    model_entries_.clear();

    auto profiles_r = LlmProfileRepository::instance().list_profiles();
    if (profiles_r.is_ok()) {
        for (const auto& p : profiles_r.value()) {
            ArenaModelEntry e;
            e.display_name = p.name + QStringLiteral(" (") + p.model_id + QStringLiteral(")");
            e.provider = p.provider;
            e.model_id = p.model_id;
            e.api_key = p.api_key;
            e.base_url = p.base_url;
            e.profile_id = p.id;
            model_entries_.append(e);
            model_list_->addItem(e.display_name);
        }
    }
    auto cfg_r = LlmConfigRepository::instance().list_models();
    if (cfg_r.is_ok()) {
        for (const auto& m : cfg_r.value()) {
            if (!m.is_enabled) continue;
            ArenaModelEntry e;
            e.display_name = m.display_name + QStringLiteral(" (") + m.model_id + QStringLiteral(")");
            e.provider = m.provider;
            e.model_id = m.model_id;
            e.api_key = m.api_key;
            e.base_url = m.base_url;
            model_entries_.append(e);
            model_list_->addItem(e.display_name);
        }
    }
}

void AlphaArenaScreen::apply_competition_id(const QString& id) {
    competition_id_ = id;
    if (leaderboard_) leaderboard_->set_competition(id);
    if (modelchat_) modelchat_->set_competition(id);
    if (positions_) positions_->set_competition(id);
    if (hitl_) hitl_->set_competition(id);
    if (risk_) risk_->set_competition(id);
    if (audit_) audit_->set_competition(id);
}

void AlphaArenaScreen::update_status_badge(const QString& status) {
    competition_status_ = status;
    status_badge_->setText(status.toUpper());
    if (status == QLatin1String("running"))
        status_badge_->setStyleSheet("background:#0a4a18;color:#0F0;padding:2px 8px;font-weight:700;");
    else if (status.startsWith(QLatin1String("halted")) || status == QLatin1String("failed"))
        status_badge_->setStyleSheet("background:#5a1010;color:#FFB;padding:2px 8px;font-weight:700;");
    else
        status_badge_->setStyleSheet("background:#222;color:#888;padding:2px 8px;font-weight:700;");
}

void AlphaArenaScreen::update_venue_badge() {
    auto& engine = AlphaArenaEngine::instance();
    const QString k = engine.venue_kind();
    if (k == QLatin1String("hyperliquid")) {
        venue_badge_->setText(QStringLiteral("HYPERLIQUID • LIVE"));
        venue_badge_->setStyleSheet("background:#5C1F1F;color:#FFD;padding:2px 8px;font-weight:700;");
    } else {
        venue_badge_->setText(QStringLiteral("PAPER"));
        venue_badge_->setStyleSheet("background:#222;color:#0F0;padding:2px 8px;font-weight:700;");
    }
}

void AlphaArenaScreen::show_disclaimer_if_live(bool live) {
    disclaimer_->setVisible(live);
}

void AlphaArenaScreen::refresh_all_panels() {
    if (leaderboard_) leaderboard_->refresh();
    if (modelchat_) modelchat_->refresh();
    if (positions_) positions_->refresh();
    if (hitl_) hitl_->refresh();
    if (risk_) risk_->refresh();
    if (audit_) audit_->refresh();
}

// ── State persistence ──────────────────────────────────────────────────────

void AlphaArenaScreen::restore_state(const QVariantMap& state) {
    if (state.contains(QStringLiteral("active_tab")))
        on_right_tab_changed(state.value(QStringLiteral("active_tab")).toInt());
}

QVariantMap AlphaArenaScreen::save_state() const {
    QVariantMap m;
    if (right_stack_) m[QStringLiteral("active_tab")] = right_stack_->currentIndex();
    return m;
}

} // namespace fincept::screens::alpha_arena
