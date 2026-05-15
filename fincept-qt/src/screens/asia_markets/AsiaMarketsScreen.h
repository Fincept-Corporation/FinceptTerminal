#pragma once

#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// Stock category descriptor for the Asia Markets explorer.
struct StockCategory {
    QString id;
    QString name;
    QString script;
    QString color; // hex color for the tab
    int endpoint_count;
};

/// Asia Markets Terminal — 398+ AKShare stock endpoints for
/// Chinese A/B shares, Hong Kong, and US markets.
/// 8 categories: Realtime, Historical, Financial, Holdings,
/// Fund Flow, Boards, Margin/HSGT, Hot & News.
class AsiaMarketsScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AsiaMarketsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "asia_markets"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* e) override;

  private slots:
    void on_category_changed(int index);
    void on_region_changed(int index);
    void on_endpoint_clicked(QListWidgetItem* item);
    void on_search_changed(const QString& text);
    void on_execute();
    void on_view_toggle();

  private:
    void setup_ui();
    bool first_show_ = true;
    QWidget* create_header();
    QWidget* create_category_bar();
    QWidget* create_left_panel();
    QWidget* create_data_panel();
    QWidget* create_status_bar();

    void load_endpoints(int cat_index);
    void populate_endpoint_list(const QJsonObject& result);
    void execute_query(const QString& endpoint, const QStringList& extra_args);
    void display_table(const QJsonArray& data);
    void display_json(const QJsonArray& data);
    void display_error(const QString& error);
    void set_loading(bool loading);

    // Categories
    QList<StockCategory> categories_;
    int active_category_ = 0;

    // Market regions
    QStringList regions_ = {"CN_A", "CN_B", "HK", "US"};
    int active_region_ = 0;

    // Endpoint cache per script
    QHash<QString, QJsonObject> endpoint_cache_;
    QString active_endpoint_;

    // UI — category bar
    QList<QPushButton*> cat_btns_;
    QList<QPushButton*> region_btns_;

    // UI — left panel
    QListWidget* endpoint_list_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;
    QLabel* endpoint_count_label_ = nullptr;

    // UI — data panel
    QStackedWidget* view_stack_ = nullptr;
    QTableWidget* data_table_ = nullptr;
    QTextEdit* json_view_ = nullptr;
    QPushButton* view_toggle_btn_ = nullptr;
    QPushButton* exec_btn_ = nullptr;
    QLabel* data_status_ = nullptr;
    QLabel* record_count_ = nullptr;

    // UI — status bar
    QLabel* status_category_ = nullptr;
    QLabel* status_region_ = nullptr;

    bool is_table_view_ = true;
    bool loading_ = false;
    QJsonArray last_data_;
};

} // namespace fincept::screens
