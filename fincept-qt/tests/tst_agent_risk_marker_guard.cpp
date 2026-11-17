// Source guard: the order, money and account controls stay marked for AI agents.
//
// Screens tell the MCP UI engine what a control commits with the dynamic property
// `fincept_agent_risk` (src/core/agent/AgentRiskMarkers.h, kinds in src/mcp/core/UiRisk.h).
// The words on a button are only a fallback, so losing the marker on an order /
// kill / sign-out / key control silently lets an agent press it with less
// scrutiny. This test pins every `order`, `money` and `account` marker in
// src/screens, src/ui/widgets/algo and src/ui/navigation by file + the expression
// it is set on + kind (+ how many times), tolerant of moved lines. A failure means
// a marker was removed or its control renamed: restore the marker (or, when the
// control really went away, remove its pin below in the same change).
//
// A second table (kViaPins, since round 7) pins the markers that are NOT a
// `setProperty("fincept_agent_risk", …)` line: a cell's kind through the item-model
// role kAgentRiskRole (`item->setData(kAgentRiskRole, "order")`, or a model's data()
// returning the kind for that role), and keys / columns of a view
// (fincept_agent_risk_keys / fincept_agent_risk_columns = "Ctrl+Return=order;…").
//
// (The test class is declared before any raw string literal: moc can miss a
// Q_OBJECT class that follows one. No raw string is passed to a macro.)

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTest>

#ifndef FINCEPT_SOURCE_DIR
#    error "FINCEPT_SOURCE_DIR (fincept-qt/) must be defined by tests/CMakeLists.txt"
#endif

class TstAgentRiskMarkerGuard : public QObject {
    Q_OBJECT

  private slots:
    void pinned_markers_are_present();
    void matcher_catches_a_removed_marker();
    // Round 7: role / keys / columns markers.
    void pinned_via_markers_are_present();
    void via_matcher_catches_a_removed_marker();
};

namespace {

struct Pin {
    const char* file;   // under src/
    const char* target; // the expression the property is set on, as written
    const char* kind;
    int count;          // at least this many such lines
};

const Pin kPins[] = {
    {"screens/action_center/PendingOrdersPanel.cpp", "approve", "order", 1},
    {"screens/action_center/PendingOrdersPanel.cpp", "approve_all_btn_", "order", 1},
    {"screens/action_center/PendingOrdersPanel.cpp", "kill_btn_", "order", 1},
    {"screens/action_center/PendingOrdersPanel.cpp", "reject", "order", 1},
    {"screens/action_center/PendingOrdersPanel.cpp", "reject_all_btn_", "order", 1},
    // The Agent Workbench was merged into AGENT CONFIG (2026-11-17): its harness widgets moved there.
    {"screens/agent_config/CreateAgentPanel_Layout.cpp", "terminal_destructive_check_", "account", 1},
    {"screens/agent_config/HarnessListWidget.cpp", "apply_path_btn_", "account", 1},
    {"screens/agent_config/HarnessListWidget.cpp", "full_access_check_", "account", 1},
    {"screens/agent_config/HarnessRunOptionsWidget.cpp", "mode_combo_", "account", 1},
    {"screens/algo_trading/AlgoDeployDialog.cpp", "buttons_->button(QDialogButtonBox::Ok)", "order", 1},
    {"screens/algo_trading/AlgoDeployDialog.cpp", "mode_combo_", "account", 1},
    // PAUSE / STOP carry no marker (stopping commits nothing); RESUME does (dd_mark_pause_toggle's `b`),
    // and so does the stop confirmation when it can close an open position (`flatten`, `yes`).
    {"screens/algo_trading/DeploymentDashboard.cpp", "b", "order", 1},
    {"screens/algo_trading/DeploymentDashboard.cpp", "flatten", "order", 1},
    {"screens/algo_trading/DeploymentDashboard.cpp", "stop_all_btn_", "order", 1},
    {"screens/algo_trading/DeploymentDashboard.cpp", "yes", "order", 1},
    // The Builder's DEPLOY (PAPER) command-bar action opens the deploy dialog (whose OK is the order step).
    {"screens/algo_trading/StrategyBuilderPanel.cpp", "deploy_action", "order", 1},
    {"screens/alpha_arena/AlphaArenaScreen.cpp", "kill_btn_", "order", 1},
    {"screens/alpha_arena/AlphaArenaScreen.cpp", "pause_btn_", "order", 1},
    {"screens/alpha_arena/LeaderboardCards.cpp", "kill", "order", 1},
    {"screens/alpha_arena/LeaderboardCards.cpp", "toggle", "order", 1},
    {"screens/alpha_arena/panels/LiveModeGateDialog.cpp", "confirm_btn_", "order", 1},
    {"screens/chat_mode/ChatAgentPanel.cpp", "mcp_del", "account", 1},
    {"screens/chat_mode/ChatAgentPanel.cpp", "mcp_save", "account", 1},
    {"screens/common/PaperBlotterPanel.cpp", "exit_btn", "order", 1},
    {"screens/common/PaperBlotterPanel.cpp", "square_all_btn_", "order", 1},
    {"screens/crypto_center/CryptoCenterScreen.cpp", "strip_connect_button_", "account", 1},
    {"screens/crypto_center/WalletActionConfirmDialog.cpp", "primary_button_", "money", 1},
    {"screens/crypto_center/panels/LockPanel.cpp", "lock_button_", "money", 1},
    {"screens/crypto_center/panels/SwapPanel.cpp", "swap_button_", "money", 1},
    {"screens/crypto_center/tabs/HomeTab.cpp", "disconnect_button_", "account", 1},
    {"screens/crypto_center/tabs/SettingsTab.cpp", "helius_clear_button_", "account", 1},
    {"screens/crypto_center/tabs/SettingsTab.cpp", "helius_save_button_", "account", 1},
    {"screens/crypto_trading/CryptoBottomPanel.cpp", "cancel_all_btn_", "order", 1},
    {"screens/crypto_trading/CryptoBottomPanel.cpp", "cancel_btn", "order", 2},
    {"screens/crypto_trading/CryptoBottomPanel.cpp", "close_act", "order", 1},
    {"screens/crypto_trading/CryptoBottomPanel.cpp", "close_all_btn_", "order", 1},
    {"screens/crypto_trading/CryptoCredentials.cpp", "clear_btn_", "account", 1},
    {"screens/crypto_trading/CryptoCredentials.cpp", "remove_btn", "account", 1},
    {"screens/crypto_trading/CryptoCredentials.cpp", "save_btn_", "account", 1},
    {"screens/crypto_trading/CryptoOrderEntry.cpp", "apply_lev_btn_", "money", 1},
    {"screens/crypto_trading/CryptoOrderEntry.cpp", "submit_btn_", "order", 1},
    {"screens/crypto_trading/CryptoTradingScreen.cpp", "mode_btn_", "account", 1},
    {"screens/crypto_trading/CryptoTradingScreen_Handlers.cpp", "b", "order", 2},
    {"screens/crypto_trading/CryptoTradingScreen_Handlers.cpp", "yes", "order", 1},
    {"screens/dashboard/widgets/BrokerHoldingsWidget.cpp", "exit_btn", "order", 1},
    {"screens/dashboard/widgets/BrokerHoldingsWidget.cpp", "square_off_all_btn_", "order", 1},
    {"screens/dashboard/widgets/OrderBookMiniWidget.cpp", "cancel_btn", "order", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "add_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "approval_mode_combo_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "buttons->button(QDialogButtonBox::Save)", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "connect_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "f_browser_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "f_manual_connect_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "mt4_connect_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "one_click_cb_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "remove_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "z_browser_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "z_connect_btn_", "account", 1},
    {"screens/equity_trading/AccountManagementDialog.cpp", "z_manual_connect_btn_", "account", 1},
    {"screens/equity_trading/BasketOrdersDialog.cpp", "execute_btn_", "order", 1},
    {"screens/equity_trading/BroadcastOrderDialog.cpp", "place_btn_", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "btn", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "cancel_all_btn_", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "cancel_btn", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "cancel_row_btn", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "close_all_btn_", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "cnc", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "edit_btn", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "holdings_square_off_btn_", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "loss_squareoff_btn_", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "ok_btn", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "sell", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "sell_btn", "order", 1},
    {"screens/equity_trading/EquityBottomPanel.cpp", "win_squareoff_btn_", "order", 1},
    {"screens/equity_trading/EquityChart.cpp", "buy_act", "order", 1},
    {"screens/equity_trading/EquityChart.cpp", "sell_act", "order", 1},
    {"screens/equity_trading/EquityChartPanel.cpp", "pos_exit_btn_", "order", 1},
    {"screens/equity_trading/EquityOrderEntry.cpp", "place_strategy_btn_", "order", 1},
    {"screens/equity_trading/EquityOrderEntry.cpp", "submit_btn_", "order", 1},
    {"screens/equity_trading/EquityTradingScreen.cpp", "mode_btn_", "account", 1},
    {"screens/equity_trading/EquityTradingScreen_Handlers.cpp", "ok", "order", 2},
    {"screens/equity_trading/EquityTradingScreen_Handlers.cpp", "send", "order", 1},
    {"screens/equity_trading/EquityTradingScreen_Handlers.cpp", "unwind", "order", 1},
    {"screens/equity_trading/EquityTradingScreen_Paper.cpp", "reset_btn", "money", 1},
    {"screens/equity_trading/EquityTradingScreen_Paper.cpp", "topup_btn", "money", 1},
    {"screens/equity_trading/OrderConfirmDialog.cpp", "send_btn_", "order", 1},
    {"screens/equity_trading/OrderConfirmDialog.cpp", "skip_box_", "account", 1},
    {"screens/equity_trading/PortfolioReplicationDialog.cpp", "replicate_btn_", "order", 1},
    {"screens/equity_trading/PortfolioReplicationDialog.cpp", "topup_btn_", "money", 1},
    {"screens/fno/BuilderSubTab.cpp", "trade_btn_", "order", 1},
    {"screens/fno/ChainSubTab.cpp", "ok", "order", 1},
    {"screens/fno/OptionChainTable.cpp", "buy", "order", 1},
    {"screens/fno/OptionChainTable.cpp", "sell", "order", 1},
    {"screens/fno/OrderConfirmDialog.cpp", "place_btn_", "order", 1},
    {"screens/node_editor/NodeEditorScreen.cpp", "proceed", "order", 1},
    {"screens/node_editor/canvas/NodeItem.cpp", "exec_from_action", "order", 1},
    {"screens/node_editor/toolbar/DeployDialog.cpp", "deploy_btn_", "order", 1},
    {"screens/node_editor/toolbar/DeployDialog.cpp", "live_radio_", "account", 1},
    {"screens/node_editor/toolbar/NodeEditorToolbar.cpp", "execute_btn_", "order", 1},
    {"screens/polymarket/PolymarketDetailPanel.cpp", "ok_btn", "order", 1},
    {"screens/polymarket/PolymarketDetailPanel.cpp", "ticket_submit_btn_", "order", 1},
    {"screens/polymarket/PolymarketOrderBlotter.cpp", "cancel_all_btn", "order", 1},
    {"screens/polymarket/PolymarketOrderBlotter.cpp", "cancel_btn", "order", 1},
    {"screens/polymarket/PredictionAccountDialog.cpp", "ks_clear_btn_", "account", 1},
    {"screens/polymarket/PredictionAccountDialog.cpp", "ks_save_btn_", "account", 1},
    {"screens/polymarket/PredictionAccountDialog.cpp", "ks_use_demo_", "account", 1},
    {"screens/polymarket/PredictionAccountDialog.cpp", "pm_clear_btn_", "account", 1},
    {"screens/polymarket/PredictionAccountDialog.cpp", "pm_save_btn_", "account", 1},
    {"screens/portfolio/views/EconomicsView.cpp", "macro_key_btn_", "account", 1},
    {"screens/portfolio_monitor/PortfolioMonitorScreen.cpp", "buttons->button(QDialogButtonBox::Ok)", "order", 1},
    {"screens/portfolio_monitor/PortfolioMonitorScreen.cpp", "exit_all", "order", 1},
    {"screens/portfolio_monitor/PortfolioMonitorScreen.cpp", "exit_btn", "order", 1},
    {"screens/portfolio_monitor/PortfolioMonitorScreen.cpp", "mode_btn_", "account", 1},
    {"screens/portfolio_monitor/PortfolioMonitorScreen.cpp", "square_all", "order", 1},
    {"screens/profile/ProfileScreen.cpp", "confirm", "account", 1},
    {"screens/profile/ProfileScreen.cpp", "lb", "account", 1},
    {"screens/profile/ProfileScreen.cpp", "rg", "account", 1},
    {"screens/profile/ProfileScreen.cpp", "set_pw_btn", "account", 1},
    {"screens/settings/AgentConnectSection.cpp", "auto_check_", "account", 1},
    {"screens/settings/AgentConnectSection.cpp", "autostart_check_", "account", 1},
    {"screens/settings/AgentConnectSection.cpp", "copy_token_btn_", "account", 1},
    {"screens/settings/AgentConnectSection.cpp", "regen_btn_", "account", 1},
    {"screens/settings/AgentConnectSection.cpp", "show_token_btn_", "account", 1},
    {"screens/settings/AgentConnectSection.cpp", "start_btn_", "account", 1},
    {"screens/settings/CredentialsSection.cpp", "save_btn", "account", 1},
    {"screens/settings/LlmConfigSection_Harness.cpp", "allow", "account", 1},
    {"screens/settings/LlmConfigSection_Harness.cpp", "use", "account", 1},
    {"screens/settings/LlmConfigSection_Providers.cpp", "delete_btn_", "account", 1},
    {"screens/settings/LlmConfigSection_Providers.cpp", "save_btn_", "account", 1},
    {"screens/settings/NotificationsSection.cpp", "save_btn_", "account", 1},
    {"screens/settings/SecuritySection.cpp", "save_btn_", "account", 1},
    {"screens/settings/SecuritySection.cpp", "save_pin_btn_", "account", 1},
    {"screens/settings/SecuritySection.cpp", "sec_autolock_toggle_", "account", 1},
    {"screens/settings/SecuritySection.cpp", "sec_change_pin_btn_", "account", 1},
    {"screens/settings/VoiceConfigSection.cpp", "save_btn_", "account", 1},
    {"ui/navigation/ToolBar.cpp", "logout_btn_", "account", 1},
    {"ui/navigation/ToolBar.cpp", "m->actions().constLast()", "account", 1},
    {"ui/navigation/ToolBar.cpp", "menu->actions().constLast()", "account", 1},
};

/// Lines setting `kind` on `target` in `text`.
int count_markers(const QString& text, const QString& target, const QString& kind) {
    static const QString kSetProperty =
        QString::fromLatin1(R"re(\s*->\s*setProperty\(\s*"fincept_agent_risk"\s*,\s*(?:QStringLiteral\(\s*)?")re");
    const QRegularExpression rx(QRegularExpression::escape(target) + kSetProperty + QRegularExpression::escape(kind) +
                                QLatin1Char('"'));
    int n = 0;
    for (auto it = rx.globalMatch(text); it.hasNext(); it.next())
        ++n;
    return n;
}

// ── Round 7: markers that are not a setProperty("fincept_agent_risk", …) line ──

/// How a pinned marker is set.
enum class Via {
    Role,    ///< item->setData(kAgentRiskRole, "kind"), or (target "") a model's data() returning it
    Keys,    ///< target->setProperty(fincept_agent_risk_keys, "…=kind;…")
    Columns, ///< target->setProperty(fincept_agent_risk_columns, "…=kind;…")
};

struct ViaPin {
    Via via;
    const char* file;   // under src/
    const char* target; // the item / widget expression, as written; "" = the object itself
    const char* kind;
    int count;          // at least this many such statements
};

const ViaPin kViaPins[] = {
    // The Price cell amends a live order on double-click / Enter — marked only when the venue
    // supports amend (the item is built inside `if (supports_amend_)`).
    {Via::Role, "screens/polymarket/PolymarketOrderBlotter.cpp", "price_item", "order", 1},
    // Delete on the MCP server list removes a server from the agents' tools.
    {Via::Keys, "screens/chat_mode/ChatAgentPanel.cpp", "mcp_list", "account", 1},
    // Ctrl+Enter runs the workflow (EXECUTE's kind): set at construction and again after each run.
    {Via::Keys, "screens/node_editor/NodeEditorScreen.cpp", "", "order", 2},
    // Enter in the Equity ticket's quantity / limit-price box sends the order (as the submit button).
    {Via::Keys, "screens/equity_trading/EquityOrderEntry.cpp", "qty_edit_", "order", 1},
    {Via::Keys, "screens/equity_trading/EquityOrderEntry.cpp", "price_edit_", "order", 1},
};

/// A string literal (optionally wrapped) whose text is exactly `kind`.
QString kind_literal(const QString& kind) {
    return QString::fromLatin1(
               R"re((?:QVariant\s*\(\s*)?(?:(?:QStringLiteral|QLatin1String|QLatin1StringView|QString::fromLatin1|QString::fromUtf8|QString)\s*\(\s*)?")re") +
           QRegularExpression::escape(kind) + QLatin1Char('"');
}

/// `target->` / `target.` before a call; "" = an unqualified call or `this->` (the object itself).
QString call_on(const QString& target) {
    if (target.isEmpty())
        return QString::fromLatin1(R"re((?:\bthis\s*->\s*|(?<![\w.>])))re");
    return QRegularExpression::escape(target) + QString::fromLatin1(R"re(\s*(?:->|\.)\s*)re");
}

/// Statements setting `kind` on `target` in `text` the `via` way.
int count_via_markers(const QString& text, Via via, const QString& target, const QString& kind) {
    int n = 0;
    if (via == Via::Role) {
        const QString role = QString::fromLatin1(R"re((?:[\w:]+::)?kAgentRiskRole)re");
        // item->setData(kAgentRiskRole, "kind")
        const QRegularExpression set_data(call_on(target) + QString::fromLatin1(R"re(setData\(\s*)re") + role +
                                          QString::fromLatin1(R"re(\s*,\s*)re") + kind_literal(kind));
        for (auto it = set_data.globalMatch(text); it.hasNext(); it.next())
            ++n;
        if (target.isEmpty()) {
            // A model's data(): the role tested, then the kind returned in the same statement
            // ("if (role == kAgentRiskRole …) return "order";", "case kAgentRiskRole: return …").
            const QRegularExpression returned(role + QString::fromLatin1(R"re([^;]{0,200}?\breturn\s+)re") +
                                              kind_literal(kind));
            for (auto it = returned.globalMatch(text); it.hasNext(); it.next())
                ++n;
        }
        return n;
    }
    const QString prop = via == Via::Keys
                             ? QString::fromLatin1(R"re((?:"fincept_agent_risk_keys"|(?:[\w:]+::)?kAgentRiskKeysProperty))re")
                             : QString::fromLatin1(
                                   R"re((?:"fincept_agent_risk_columns"|(?:[\w:]+::)?kAgentRiskColumnsProperty))re");
    // The whole statement (it may span lines; string literals may hold ';'), value captured.
    const QRegularExpression statement(call_on(target) + QString::fromLatin1(R"re(setProperty\(\s*)re") + prop +
                                       QString::fromLatin1(R"re(\s*,((?:[^;"]|"(?:[^"\\]|\\.)*")*)\)\s*;)re"));
    // "…=kind" ends a pair: followed by ';' or the end of the literal.
    const QRegularExpression pair(QLatin1Char('=') + QRegularExpression::escape(kind) +
                                  QString::fromLatin1(R"re((?=[;"]))re"));
    for (auto it = statement.globalMatch(text); it.hasNext();) {
        if (it.next().captured(1).contains(pair))
            ++n;
    }
    return n;
}

} // namespace

void TstAgentRiskMarkerGuard::pinned_markers_are_present() {
    QStringList missing;
    for (const Pin& p : kPins) {
        const QString path = QStringLiteral(FINCEPT_SOURCE_DIR) + QStringLiteral("/src/") + QString::fromUtf8(p.file);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            missing << QStringLiteral("%1: file is gone").arg(QString::fromUtf8(p.file));
            continue;
        }
        const QString text = QString::fromUtf8(f.readAll());
        const int found = count_markers(text, QString::fromUtf8(p.target), QString::fromUtf8(p.kind));
        if (found < p.count)
            missing << QStringLiteral("%1: %2 -> \"%3\" (found %4 of %5)")
                           .arg(QString::fromUtf8(p.file), QString::fromUtf8(p.target), QString::fromUtf8(p.kind))
                           .arg(found)
                           .arg(p.count);
    }
    QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("agent-risk markers lost:\n  ") + missing.join(QStringLiteral("\n  "))));
}

void TstAgentRiskMarkerGuard::matcher_catches_a_removed_marker() {
    const QString marked = QStringLiteral("    kill_btn_->setProperty(\"fincept_agent_risk\", \"order\");\n"
                                          "    x->setProperty(\"fincept_agent_risk\", QStringLiteral(\"account\"));\n");
    QCOMPARE(count_markers(marked, QStringLiteral("kill_btn_"), QStringLiteral("order")), 1);
    QCOMPARE(count_markers(marked, QStringLiteral("x"), QStringLiteral("account")), 1);
    QCOMPARE(count_markers(marked, QStringLiteral("kill_btn_"), QStringLiteral("spend")), 0); // kind changed
    QCOMPARE(count_markers(QStringLiteral("    kill_btn_->setText(\"KILL\");"), QStringLiteral("kill_btn_"),
                           QStringLiteral("order")),
             0); // removed
}

void TstAgentRiskMarkerGuard::pinned_via_markers_are_present() {
    QStringList missing;
    for (const ViaPin& p : kViaPins) {
        const QString path = QStringLiteral(FINCEPT_SOURCE_DIR) + QStringLiteral("/src/") + QString::fromUtf8(p.file);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            missing << QStringLiteral("%1: file is gone").arg(QString::fromUtf8(p.file));
            continue;
        }
        const QString text = QString::fromUtf8(f.readAll());
        const int found = count_via_markers(text, p.via, QString::fromUtf8(p.target), QString::fromUtf8(p.kind));
        if (found < p.count) {
            const QString how = p.via == Via::Role   ? QStringLiteral("kAgentRiskRole")
                                : p.via == Via::Keys ? QStringLiteral("fincept_agent_risk_keys")
                                                     : QStringLiteral("fincept_agent_risk_columns");
            const QString target = *p.target ? QString::fromUtf8(p.target) : QStringLiteral("(itself)");
            missing << QStringLiteral("%1: %2 %3 -> \"%4\" (found %5 of %6)")
                           .arg(QString::fromUtf8(p.file), target, how, QString::fromUtf8(p.kind))
                           .arg(found)
                           .arg(p.count);
        }
    }
    QVERIFY2(missing.isEmpty(),
             qPrintable(QStringLiteral("agent-risk role / keys / columns markers lost:\n  ") +
                        missing.join(QStringLiteral("\n  "))));
}

void TstAgentRiskMarkerGuard::via_matcher_catches_a_removed_marker() {
    const QString o = QStringLiteral("order");
    // Role: an item's setData.
    const QString item = QStringLiteral(
        "        price_item->setData(fincept::core::agent::kAgentRiskRole, QStringLiteral(\"order\"));\n");
    QCOMPARE(count_via_markers(item, Via::Role, QStringLiteral("price_item"), o), 1);
    QCOMPARE(count_via_markers(item, Via::Role, QStringLiteral("size_item"), o), 0);                   // other item
    QCOMPARE(count_via_markers(item, Via::Role, QStringLiteral("price_item"), QStringLiteral("money")), 0); // kind changed
    QCOMPARE(count_via_markers(item, Via::Keys, QStringLiteral("price_item"), o), 0);                  // not a keys marker
    QCOMPARE(count_via_markers(QStringLiteral("        price_item->setData(Qt::UserRole, QStringLiteral(\"order\"));\n"),
                               Via::Role, QStringLiteral("price_item"), o),
             0); // another role
    QCOMPARE(count_via_markers(QStringLiteral("        price_item->setToolTip(tr(\"Double-click to amend price\"));\n"),
                               Via::Role, QStringLiteral("price_item"), o),
             0); // removed
    // Role: a model's data() returning the kind (pinned with an empty target).
    const QString model = QStringLiteral("    if (role == fincept::core::agent::kAgentRiskRole && index.column() == 3)\n"
                                         "        return QStringLiteral(\"order\");\n"
                                         "    switch (role) {\n"
                                         "    case kAgentRiskRole:\n"
                                         "        return QVariant(QStringLiteral(\"money\"));\n");
    QCOMPARE(count_via_markers(model, Via::Role, QString(), o), 1);
    QCOMPARE(count_via_markers(model, Via::Role, QString(), QStringLiteral("money")), 1);
    QCOMPARE(count_via_markers(model, Via::Role, QString(), QStringLiteral("account")), 0);
    QCOMPARE(count_via_markers(QStringLiteral("    if (role == kAgentRiskRole)\n        return {};\n"
                                              "    return QStringLiteral(\"order\");\n"),
                               Via::Role, QString(), o),
             0); // the role's statement returns nothing; a later return is another statement
    // Keys: the object itself, a named widget, a value built over several lines.
    const QString keys = QStringLiteral(
        "    setProperty(fincept::core::agent::kAgentRiskKeysProperty, QStringLiteral(\"Ctrl+Return=order;Ctrl+Enter=order\"));\n"
        "        setProperty(fincept::core::agent::kAgentRiskKeysProperty, QVariant()); // Ctrl+Enter now stops\n"
        "    mcp_list->setProperty(\"fincept_agent_risk_keys\", \"Delete=account\"); // agents: Delete removes a server\n"
        "    table_->setProperty(fincept::core::agent::kAgentRiskKeysProperty,\n"
        "                        QKeySequence(QKeySequence::Delete).toString(QKeySequence::PortableText) +\n"
        "                            QStringLiteral(\"=money\"));\n");
    QCOMPARE(count_via_markers(keys, Via::Keys, QString(), o), 1); // the cleared one does not count
    QCOMPARE(count_via_markers(keys, Via::Keys, QString(), QStringLiteral("account")), 0); // mcp_list's, not its own
    QCOMPARE(count_via_markers(keys, Via::Keys, QStringLiteral("mcp_list"), QStringLiteral("account")), 1);
    QCOMPARE(count_via_markers(keys, Via::Keys, QStringLiteral("mcp_list"), QStringLiteral("delete")), 0); // key, not kind
    QCOMPARE(count_via_markers(keys, Via::Keys, QStringLiteral("table_"), QStringLiteral("money")), 1);
    QCOMPARE(count_via_markers(keys, Via::Columns, QStringLiteral("mcp_list"), QStringLiteral("account")), 0);
    QCOMPARE(count_via_markers(QStringLiteral("    this->setProperty(\"fincept_agent_risk_keys\", \"F9=order\");\n"),
                               Via::Keys, QString(), o),
             1);
    QCOMPARE(count_via_markers(QStringLiteral("    mcp_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);\n"),
                               Via::Keys, QStringLiteral("mcp_list"), QStringLiteral("account")),
             0); // removed
    QCOMPARE(count_via_markers(QStringLiteral("    mcp_list->setProperty(\"fincept_agent_risk_keys\", \"Delete=delete\");\n"),
                               Via::Keys, QStringLiteral("mcp_list"), QStringLiteral("account")),
             0); // kind changed
    // Columns.
    const QString cols = QStringLiteral(
        "    view->setProperty(fincept::core::agent::kAgentRiskColumnsProperty, QStringLiteral(\"Action=order;5=delete\"));\n");
    QCOMPARE(count_via_markers(cols, Via::Columns, QStringLiteral("view"), o), 1);
    QCOMPARE(count_via_markers(cols, Via::Columns, QStringLiteral("view"), QStringLiteral("delete")), 1);
    QCOMPARE(count_via_markers(cols, Via::Columns, QStringLiteral("view"), QStringLiteral("money")), 0);
    QCOMPARE(count_via_markers(cols, Via::Keys, QStringLiteral("view"), o), 0);
}

QTEST_MAIN(TstAgentRiskMarkerGuard)
#include "tst_agent_risk_marker_guard.moc"
