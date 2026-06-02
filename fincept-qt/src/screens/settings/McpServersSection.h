#pragma once
// McpServersSection.h — MCP external server management panel

#include <QEvent>
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

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_server_selected(int row);
    void on_add_server();
    void on_remove_server();
    void on_start_server();
    void on_stop_server();
    void on_tab_changed(int idx);

  private:
    QStackedWidget* tab_stack_ = nullptr;

    // Title bar
    QLabel* title_lbl_ = nullptr;
    QLabel* tools_badge_ = nullptr;

    // Tab buttons
    QPushButton* servers_tab_btn_ = nullptr;
    QPushButton* tools_tab_btn_ = nullptr;

    // Servers tab
    QLabel* server_list_lbl_ = nullptr;
    QListWidget* server_list_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QLabel* detail_lbl_ = nullptr;
    QLabel* status_lbl_ = nullptr;

    // Tools tab
    QLabel* tools_info_lbl_ = nullptr;
    QTableWidget* tools_table_ = nullptr;

    void build_ui();
    QWidget* build_servers_tab();
    QWidget* build_tools_tab();
    QWidget* build_add_server_form();

    void load_servers();
    void load_tools();
    void refresh_server_detail(const QString& server_id);
    void show_status(const QString& msg, bool error = false);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QString selected_server_id_;
};

} // namespace fincept::screens
