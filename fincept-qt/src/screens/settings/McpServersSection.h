#pragma once
// McpServersSection.h — MCP external server management panel

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class McpServersSection : public QWidget {
    Q_OBJECT
  public:
    explicit McpServersSection(QWidget* parent = nullptr);

    void reload();

  private slots:
    void on_server_selected(int row);
    void on_add_server();
    void on_remove_server();
    void on_start_server();
    void on_stop_server();
    void on_tab_changed(int idx);

  private:
    QStackedWidget* tab_stack_ = nullptr;

    // Servers tab
    QListWidget* server_list_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QLabel* detail_lbl_ = nullptr;
    QLabel* status_lbl_ = nullptr;

    // Tools tab
    QTableWidget* tools_table_ = nullptr;

    void build_ui();
    QWidget* build_servers_tab();
    QWidget* build_tools_tab();
    QWidget* build_add_server_form();

    void load_servers();
    void load_tools();
    void refresh_server_detail(const QString& server_id);
    void show_status(const QString& msg, bool error = false);

    QString selected_server_id_;
};

} // namespace fincept::screens
