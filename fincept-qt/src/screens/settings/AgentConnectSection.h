#pragma once
// AgentConnectSection.h — Settings › AI Agents: the agent harnesses this
// terminal can drive, and the terminal's MCP server that external agents use.
//
//   AGENT HARNESSES       the shared HarnessListWidget (Claude Code, Codex,
//                         Grok, …): detected install, version, sign-in, default
//                         model / effort, API-key policy, path — the SAME widget
//                         as the Agent Workbench's HARNESSES tab, reading and
//                         writing harness::HarnessRegistry only
//   TERMINAL MCP SERVER
//     Server              status, URL, start / stop, start-with-terminal, port, counters
//     Access token        show / copy / regenerate
//     Permissions         look (always) · operate the UI · run Python scripts · result size
//     Connect             per-client setup: every registered harness that documents
//                         a way to add an MCP server (one-click ADD where the
//                         harness can read the token from an environment variable
//                         — the token never goes on a command line), plus VS Code,
//                         stdio clients and other HTTP clients
//     Tools               the ten core tools agents get
//     Activity            live log of external tool calls, and the clients seen
//
// The Agent Workbench's CONNECT tab is this widget with Sections::McpServerOnly
// (its HARNESSES tab shows the harness list). It styles itself with one
// object-name stylesheet on its root, set once before any child exists — the
// theme system rewrites it on an appearance change. Live updates follow
// visibility (P3): subscriptions and the uptime timer run only while shown.

#include "mcp/server/McpHttpServer.h"

#include <QAbstractTableModel>
#include <QList>
#include <QMetaObject>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStandardItemModel;
class QTableView;
class QTimer;

namespace fincept::screens {

class HarnessListWidget;

/// External tool calls, newest first.
class McpActivityModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column { ColTime = 0, ColClient, ColTool, ColOutcome, ColDuration, ColCount };
    static constexpr int kMaxRows = 200;

    explicit McpActivityModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void reset_records(const QList<mcp::ExternalCallRecord>& records);   // newest first
    void prepend_records(const QList<mcp::ExternalCallRecord>& records); // newest first; trims to kMaxRows
    void retranslate_headers();

  private:
    QList<mcp::ExternalCallRecord> rows_;
};

class AgentConnectSection : public QWidget {
    Q_OBJECT
  public:
    enum class Sections { All, McpServerOnly };
    explicit AgentConnectSection(QWidget* parent = nullptr, Sections sections = Sections::All);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    QWidget* build_server_card(QWidget* parent);
    QWidget* build_token_card(QWidget* parent);
    QWidget* build_permissions_card(QWidget* parent);
    QWidget* build_connect_card(QWidget* parent);
    /// CHECK CONNECTIONS: mcp::run_mcp_self_check_async — one row per harness
    /// profile, pass / fail and the plain reason. Talks to the server over
    /// loopback like each client would; calls no model.
    QWidget* build_check_card(QWidget* parent);
    void on_check_connections();
    void show_check_report(const QStringList& clients, const QStringList& results, const QStringList& details);
    void retranslate_check_headers();
    QWidget* build_tools_card(QWidget* parent);
    QWidget* build_activity_card(QWidget* parent);
    QString style_sheet() const;
    void fit_rows();
    void retranslate();

    void subscribe();
    void unsubscribe();
    void refresh_status();
    void refresh_snippet();
    void refresh_clients();
    void rebuild_client_list();
    void flush_activity();
    QString masked(const QString& text) const;
    static void copy_text(const QString& text);
    /// "harness:<id>" | "vscode" | "stdio" | "http"
    QString current_client() const;
    QString current_harness() const; // empty for the non-harness clients
    /// The text of the selected client's snippet / config entry; `reveal` = the real token.
    QString client_command(bool reveal) const;
    QString client_config(bool reveal) const;

    void on_start();
    void on_stop();
    void on_regenerate();
    void on_ui_control_toggled(bool on);
    void on_scripts_toggled(bool on);
    /// Auto mode: asks for confirmation before turning it on.
    void on_auto_toggled(bool on);
    /// The note under the permissions: what still asks first, or the auto-mode warning.
    void update_auto_note();
    void on_register();
    void on_registration_finished(const QString& harness_id, bool ok, bool already_exists, const QString& message);

    Sections sections_ = Sections::All;

    // Header
    QLabel* title_ = nullptr;
    QLabel* harness_title_ = nullptr;
    HarnessListWidget* harness_list_ = nullptr;
    QLabel* mcp_title_ = nullptr;
    QLabel* intro_ = nullptr;

    // Server
    QLabel* server_title_ = nullptr;
    QLabel* state_lbl_ = nullptr;
    QLineEdit* url_edit_ = nullptr;
    QPushButton* copy_url_btn_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QLabel* stats_lbl_ = nullptr;
    QLabel* error_lbl_ = nullptr;
    QCheckBox* autostart_check_ = nullptr;
    QLabel* port_lbl_ = nullptr;
    QSpinBox* port_spin_ = nullptr;
    QLabel* port_hint_ = nullptr;

    // Token
    QLabel* token_title_ = nullptr;
    QLineEdit* token_edit_ = nullptr;
    QPushButton* show_token_btn_ = nullptr;
    QPushButton* copy_token_btn_ = nullptr;
    QPushButton* regen_btn_ = nullptr;
    QLabel* token_hint_ = nullptr;

    // Permissions
    QLabel* perms_title_ = nullptr;
    QLabel* look_lbl_ = nullptr;
    QCheckBox* ui_control_check_ = nullptr;
    QCheckBox* scripts_check_ = nullptr;
    QCheckBox* auto_check_ = nullptr;
    QLabel* always_ask_lbl_ = nullptr;
    QLabel* budget_lbl_ = nullptr;
    QComboBox* budget_combo_ = nullptr;

    // Connect
    QLabel* connect_title_ = nullptr;
    QLabel* client_lbl_ = nullptr;
    QComboBox* client_combo_ = nullptr;
    QStringList client_keys_; // combo index -> current_client() key
    QLabel* client_desc_ = nullptr;
    QPlainTextEdit* snippet_ = nullptr;
    QPushButton* copy_snippet_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QLabel* add_result_ = nullptr;
    QLabel* second_lbl_ = nullptr;
    QPlainTextEdit* second_snippet_ = nullptr;
    QPushButton* copy_second_btn_ = nullptr;

    // Connection check
    QLabel* check_title_ = nullptr;
    QPushButton* check_btn_ = nullptr;
    QLabel* check_hint_ = nullptr;
    QTableView* check_table_ = nullptr;
    QStandardItemModel* check_model_ = nullptr;
    bool check_running_ = false;
    bool check_done_ = false; // a report is on screen (the hint then says so)

    // Tools
    QLabel* tools_title_ = nullptr;
    QLabel* tools_list_ = nullptr;

    // Activity
    QLabel* activity_title_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QTableView* activity_table_ = nullptr;
    McpActivityModel* activity_model_ = nullptr;
    QLabel* clients_title_ = nullptr;
    QLabel* clients_list_ = nullptr;

    QTimer* activity_timer_ = nullptr; // coalesces bursts of activity rows
    QTimer* uptime_timer_ = nullptr;   // refreshes the counters line while shown
    QList<mcp::ExternalCallRecord> pending_activity_;
    QList<QMetaObject::Connection> conns_;
    bool show_token_ = false;
    QString awaiting_harness_; // a one-click ADD in flight (empty = none)
};

} // namespace fincept::screens
