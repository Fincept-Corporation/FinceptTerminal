// src/screens/backtesting/BacktestingScreen.cpp
//
// Core lifecycle: ctor, show/hide events, service wiring, top-level layout
// assembly, IStatefulScreen/IGroupLinked overrides, and the on_error slot
// (kept here to keep the screen-level error path next to the lifecycle
// methods that consume it).
//
// This file is the entry point of a partial-class split. The rest of the
// implementation lives in:
//   - BacktestingScreen_Layout.cpp     — top/left/center/right/status bars
//   - BacktestingScreen_Commands.cpp   — provider/command selection, run,
//                                        gather_args, gather_strategy_params
//   - BacktestingScreen_Strategies.cpp — strategy combos + params rebuild
//   - BacktestingScreen_Results.cpp    — display_result/display_error and
//                                        the service-callback handlers
// Shared helpers live in BacktestingScreen_internal.h.

#include "screens/backtesting/BacktestingScreen.h"

#include "core/logging/Logger.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/theme/Theme.h"

#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::backtest;

// ── Constructor ──────────────────────────────────────────────────────────────

BacktestingScreen::BacktestingScreen(QWidget* parent) : QWidget(parent) {
    providers_ = all_providers();
    commands_ = all_commands();
    // strategies_ starts empty — populated dynamically per provider via load_strategies()
    build_ui();
    connect_service();
    LOG_INFO("Backtesting", "Screen constructed");
}

void BacktestingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_provider_changed(0);
        on_command_changed(0);
    }
}

void BacktestingScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

void BacktestingScreen::connect_service() {
    auto& svc = BacktestingService::instance();
    connect(&svc, &BacktestingService::result_ready, this, &BacktestingScreen::on_result);
    connect(&svc, &BacktestingService::error_occurred, this, &BacktestingScreen::on_error);
    connect(&svc, &BacktestingService::command_options_loaded, this, &BacktestingScreen::on_command_options_loaded);
}

void BacktestingScreen::set_status_state(const QString& text, const QString& color, const QString& bg_rgba) {
    status_label_->setText(text);
    status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                     .arg(color)
                                     .arg(ui::fonts::TINY)
                                     .arg(ui::fonts::DATA_FAMILY));
    status_dot_->setText(text);
    status_dot_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                       "padding:3px 8px; background:%4;"
                                       "border:1px solid %5;")
                                   .arg(color)
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(bg_rgba)
                                   .arg(bg_rgba.isEmpty() ? QString("transparent") : color));
}

void BacktestingScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));
    splitter->addWidget(build_left_panel());
    splitter->addWidget(build_center_panel());
    splitter->addWidget(build_right_panel());
    splitter->setSizes({220, 600, 280});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setCollapsible(2, false);

    root->addWidget(splitter, 1);
    root->addWidget(build_status_bar());

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap BacktestingScreen::save_state() const {
    return {
        {"provider", active_provider_},
        {"command", active_command_},
    };
}

void BacktestingScreen::restore_state(const QVariantMap& state) {
    const int prov = state.value("provider", 0).toInt();
    const int cmd = state.value("command", 0).toInt();
    if (prov != active_provider_)
        on_provider_changed(prov);
    if (cmd != active_command_)
        on_command_changed(cmd);
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

SymbolRef BacktestingScreen::current_symbol() const {
    if (!symbols_edit_)
        return {};
    const QString text = symbols_edit_->text().trimmed();
    if (text.isEmpty())
        return {};
    // First comma-separated token is the "active" symbol for group linking.
    const QString first = text.section(',', 0, 0).trimmed();
    if (first.isEmpty())
        return {};
    return SymbolRef::equity(first);
}

void BacktestingScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid() || !symbols_edit_)
        return;
    // Replace the entire field — adding to a comma list silently would
    // surprise users who have a multi-symbol backtest configured.
    if (symbols_edit_->text().trimmed() == ref.symbol)
        return;
    QSignalBlocker block(symbols_edit_); // avoid bouncing the publish back
    symbols_edit_->setText(ref.symbol);
}

void BacktestingScreen::publish_first_symbol_to_group() {
    if (link_group_ == SymbolGroup::None)
        return;
    const SymbolRef ref = current_symbol();
    if (!ref.is_valid())
        return;
    SymbolContext::instance().set_group_symbol(link_group_, ref, this);
}

} // namespace fincept::screens
