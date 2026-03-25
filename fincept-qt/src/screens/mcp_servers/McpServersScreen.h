#pragma once

#include "mcp/McpClient.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class McpServersScreen : public QWidget {
    Q_OBJECT
  public:
    explicit McpServersScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_view_changed(int view);
    void on_install_server(int index);   // from marketplace catalog
    void on_start_server();
    void on_stop_server();
    void on_remove_server();
    void on_toggle_autostart();
    void on_server_selected(QListWidgetItem* item); // kept for compat
    void on_refresh();
    void on_search_changed(const QString& text);
    void on_view_logs();
    void on_add_server();
    void on_tool_enabled_changed(int row, int col);

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_marketplace_view();
    QWidget* create_installed_view();
    QWidget* create_tools_view();
    QWidget* create_status_bar();

    void populate_marketplace();
    void refresh_installed();
    void refresh_tools();
    void update_status_bar();

    /// Builds a self-contained server card for the Installed view.
    QWidget* build_server_card(const fincept::mcp::McpServerConfig& s);

    // State
    int     active_view_       = 0;
    bool    loaded_            = false;
    QString selected_server_id_;

    // Header
    QList<QPushButton*> view_btns_;
    QLineEdit*   search_input_ = nullptr;
    QPushButton* refresh_btn_  = nullptr;

    // View stack
    QStackedWidget* view_stack_ = nullptr;

    // Marketplace
    QListWidget* mkt_cat_list_    = nullptr;  // category sidebar
    QWidget*     mkt_cards_widget_ = nullptr;
    QVBoxLayout* mkt_cards_layout_ = nullptr;

    // Installed
    QWidget*     inst_cards_widget_ = nullptr;
    QVBoxLayout* inst_cards_layout_ = nullptr;
    QPushButton* add_server_btn_    = nullptr;

    // Tools
    QTableWidget* tools_table_ = nullptr;
    QLabel*       tools_count_ = nullptr;

    // Status bar
    QLabel* status_view_    = nullptr;
    QLabel* status_count_   = nullptr;
    QLabel* status_running_ = nullptr;
};

} // namespace fincept::screens
