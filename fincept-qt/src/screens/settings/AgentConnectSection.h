#pragma once
// AgentConnectSection.h — Settings › AI Agents (MCP): everything about letting
// external AI agents (Claude Code, Codex CLI, Gemini CLI, Cursor, VS Code,
// Claude Desktop …) operate this terminal through its MCP server.
//
//   Server       status, URL, start / stop, start-with-terminal, port, counters
//   Access token show / copy / regenerate
//   Permissions  look (always) · operate the UI · run Python scripts · result size
//   Connect      per-client setup snippet (+ one-click add for Claude Code / Codex)
//   Tools        the ten core tools agents get
//   Activity     live log of external tool calls, and the clients seen
//
// The same widget is the Agent Workbench's CONNECT tab. It styles itself (one
// object-name stylesheet on its root, re-applied on theme change), so it looks
// right in either host. Live updates follow visibility (P3): subscriptions and
// the uptime timer run only while it is shown.

#include "mcp/server/McpHttpServer.h"

#include <QAbstractTableModel>
#include <QList>
#include <QMetaObject>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableView;
class QTimer;

namespace fincept::screens {

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
    explicit AgentConnectSection(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    enum Client { ClaudeCode = 0, Codex, GeminiCli, Cursor, VsCode, ClaudeDesktop, ClientCount };

    void build_ui();
    QWidget* build_server_card(QWidget* parent);
    QWidget* build_token_card(QWidget* parent);
    QWidget* build_permissions_card(QWidget* parent);
    QWidget* build_connect_card(QWidget* parent);
    QWidget* build_tools_card(QWidget* parent);
    QWidget* build_activity_card(QWidget* parent);
    void apply_theme();
    void retranslate();

    void subscribe();
    void unsubscribe();
    void refresh_status();
    void refresh_snippet();
    void refresh_clients();
    void flush_activity();
    QString masked(const QString& text) const;
    static void copy_text(const QString& text);

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
    void on_registration_finished(const QString& agent, bool ok, bool already_exists, const QString& message);

    // Header
    QLabel* title_ = nullptr;
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
    QLabel* client_desc_ = nullptr;
    QPlainTextEdit* snippet_ = nullptr;
    QPushButton* copy_snippet_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QLabel* add_result_ = nullptr;
    QLabel* second_lbl_ = nullptr;
    QPlainTextEdit* second_snippet_ = nullptr;
    QPushButton* copy_second_btn_ = nullptr;

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
    bool awaiting_registration_ = false;
};

} // namespace fincept::screens
