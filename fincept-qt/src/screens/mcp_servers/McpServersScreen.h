#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// Standalone MCP Servers management screen.
/// 3 views: Marketplace (browse & install), Installed (manage lifecycle),
/// Tools (enable/disable per category).
/// Uses McpManager for server lifecycle and McpServerRepository for persistence.
class McpServersScreen : public QWidget {
    Q_OBJECT
  public:
    explicit McpServersScreen(QWidget* parent = nullptr);

  private slots:
    void on_view_changed(int view);   // 0=marketplace, 1=installed, 2=tools
    void on_install_server(int index);
    void on_start_server();
    void on_stop_server();
    void on_remove_server();
    void on_toggle_autostart();
    void on_server_selected(QListWidgetItem* item);
    void on_refresh();
    void on_search_changed(const QString& text);
    void on_view_logs();

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_marketplace_view();
    QWidget* create_installed_view();
    QWidget* create_tools_view();
    QWidget* create_status_bar();

    void refresh_installed();
    void refresh_tools();
    void populate_marketplace();

    // State
    int active_view_ = 0;
    QString selected_server_id_;

    // View buttons
    QList<QPushButton*> view_btns_;

    // Header
    QLineEdit* search_input_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // View stack
    QStackedWidget* view_stack_ = nullptr;

    // Marketplace
    QListWidget* marketplace_list_ = nullptr;

    // Installed
    QListWidget* installed_list_ = nullptr;
    QLabel* server_name_ = nullptr;
    QLabel* server_status_ = nullptr;
    QLabel* server_command_ = nullptr;
    QLabel* server_category_ = nullptr;
    QLabel* server_autostart_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QPushButton* autostart_btn_ = nullptr;
    QPushButton* logs_btn_ = nullptr;
    QWidget* detail_panel_ = nullptr;

    // Tools
    QTableWidget* tools_table_ = nullptr;
    QLabel* tools_count_ = nullptr;

    // Logs
    QTextEdit* logs_view_ = nullptr;

    // Status bar
    QLabel* status_view_ = nullptr;
    QLabel* status_count_ = nullptr;
    QLabel* status_running_ = nullptr;
};

} // namespace fincept::screens
