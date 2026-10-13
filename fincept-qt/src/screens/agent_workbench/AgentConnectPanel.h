#pragma once
// AgentConnectPanel.h — CONNECT tab of the Agent Workbench: the terminal's
// external MCP server (status, start/stop, toolset, actions policy, port,
// token) and copy-paste / one-click setup for Claude Code, Codex, Cursor and
// Claude Desktop, plus a live log of external tool calls.

#include "mcp/server/McpHttpServer.h"

#include <QAbstractTableModel>
#include <QList>
#include <QMetaObject>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableView;
class QTimer;

namespace fincept::screens {

class AgentActivityModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column { ColTime = 0, ColClient, ColTool, ColOutcome, ColDuration, ColCount };
    static constexpr int kMaxRows = 200;

    explicit AgentActivityModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /// Newest first.
    void reset_records(const QList<mcp::ExternalCallRecord>& records);
    /// Newest first; trims to kMaxRows.
    void prepend_records(const QList<mcp::ExternalCallRecord>& records);
    void retranslate_headers();

  private:
    QList<mcp::ExternalCallRecord> rows_;
};

class AgentConnectPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentConnectPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    QWidget* build_server_section(QWidget* parent);
    QWidget* build_client_card(QWidget* parent, QLabel** title, QLabel** desc, QWidget* snippet, QPushButton** copy,
                               QPushButton** action, QLabel** result);
    void retranslate();
    void subscribe();
    void unsubscribe();
    void refresh_status();
    void refresh_snippets();
    QString masked(const QString& text) const;
    void on_start();
    void on_stop();
    void on_regenerate();
    void on_actions_toggled(bool on);
    void on_register(bool codex);
    void on_registration_finished(const QString& agent, bool ok, bool already_exists, const QString& message);
    void flush_activity();
    static void copy_text(const QString& text);

    // Server
    QLabel* server_title_ = nullptr;
    QLabel* state_lbl_ = nullptr;
    QLineEdit* url_edit_ = nullptr;
    QPushButton* copy_url_btn_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QLabel* clients_lbl_ = nullptr;
    QLabel* error_lbl_ = nullptr;
    QCheckBox* autostart_check_ = nullptr;
    QLabel* toolset_lbl_ = nullptr;
    QRadioButton* essentials_radio_ = nullptr;
    QRadioButton* full_radio_ = nullptr;
    QCheckBox* actions_check_ = nullptr;
    QLabel* actions_hint_ = nullptr;
    QLabel* port_lbl_ = nullptr;
    QSpinBox* port_spin_ = nullptr;
    QLabel* port_hint_ = nullptr;
    QLabel* token_lbl_ = nullptr;
    QLineEdit* token_edit_ = nullptr;
    QPushButton* show_token_btn_ = nullptr;
    QPushButton* copy_token_btn_ = nullptr;
    QPushButton* regen_btn_ = nullptr;

    // Clients
    QLabel* clients_title_ = nullptr;
    QLabel* claude_title_ = nullptr;
    QLabel* claude_desc_ = nullptr;
    QLineEdit* claude_snippet_ = nullptr;
    QPushButton* claude_copy_btn_ = nullptr;
    QPushButton* claude_add_btn_ = nullptr;
    QLabel* claude_result_ = nullptr;
    QLabel* codex_title_ = nullptr;
    QLabel* codex_desc_ = nullptr;
    QLineEdit* codex_snippet_ = nullptr;
    QPushButton* codex_copy_btn_ = nullptr;
    QPushButton* codex_add_btn_ = nullptr;
    QLabel* codex_result_ = nullptr;
    QLabel* codex_toml_lbl_ = nullptr;
    QPlainTextEdit* codex_toml_ = nullptr;
    QPushButton* codex_toml_copy_btn_ = nullptr;
    QLabel* http_title_ = nullptr;
    QLabel* http_desc_ = nullptr;
    QPlainTextEdit* http_snippet_ = nullptr;
    QPushButton* http_copy_btn_ = nullptr;
    QLabel* desktop_title_ = nullptr;
    QLabel* desktop_desc_ = nullptr;
    QPlainTextEdit* desktop_snippet_ = nullptr;
    QPushButton* desktop_copy_btn_ = nullptr;

    // Activity
    QLabel* activity_title_ = nullptr;
    QTableView* activity_table_ = nullptr;
    AgentActivityModel* activity_model_ = nullptr;
    QTimer* activity_timer_ = nullptr;
    QList<mcp::ExternalCallRecord> pending_activity_; // newest first

    bool show_token_ = false;
    bool awaiting_claude_ = false; // this panel started a registration and waits for its result
    bool awaiting_codex_ = false;
    QList<QMetaObject::Connection> conns_;
};

} // namespace fincept::screens
