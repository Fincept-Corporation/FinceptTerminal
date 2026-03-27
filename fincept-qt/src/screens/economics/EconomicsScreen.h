#pragma once

#include <QComboBox>
#include <QFrame>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QWidget>

namespace fincept::screens {

/// Economic data source descriptor.
struct EconSource {
    QString id;
    QString name;
    QString script;         // Python script name
    QString color;          // source accent color
    bool    needs_api_key;
    QString default_indicator;
    QString description;    // short blurb shown in header
};

/// Economics Data Explorer — 15 global data sources with 1000+ indicators.
class EconomicsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit EconomicsScreen(QWidget* parent = nullptr);

  private slots:
    void on_source_changed(int index);
    void on_country_clicked(QListWidgetItem* item);
    void on_indicator_clicked(QListWidgetItem* item);
    void on_fetch();
    void on_date_preset(int index);
    void on_country_search(const QString& text);
    void on_indicator_search(const QString& text);

  private:
    void     setup_ui();
    QWidget* create_toolbar();
    QWidget* create_source_bar();
    QWidget* create_left_panel();
    QWidget* create_right_panel();
    QWidget* create_status_bar();
    QWidget* create_stat_card(const QString& label, QLabel*& value_out, QLabel*& sub_out);

    void populate_countries(int source_index);
    void populate_indicators(int source_index);
    void execute_fetch(const QString& script, const QStringList& args);
    void display_data(const QJsonArray& data, const QString& title);
    void display_stats(const QJsonArray& data);
    void display_error(const QString& error);
    void set_loading(bool loading);
    void update_source_badge(int index);

    // Sources
    QList<EconSource> sources_;
    int     active_source_ = 0;
    QString active_country_ = "USA";
    QString active_indicator_;

    // Toolbar
    QComboBox*   source_combo_  = nullptr;
    QComboBox*   date_preset_   = nullptr;
    QLineEdit*   date_start_    = nullptr;
    QLineEdit*   date_end_      = nullptr;
    QPushButton* fetch_btn_     = nullptr;

    // Source badge strip
    QWidget* source_badge_bar_  = nullptr;

    // Left panel
    QListWidget* country_list_    = nullptr;
    QListWidget* indicator_list_  = nullptr;
    QLineEdit*   country_search_  = nullptr;
    QLineEdit*   indicator_search_= nullptr;
    QLabel*      country_count_   = nullptr;
    QLabel*      indicator_count_ = nullptr;

    // Stat cards (value + sub-label)
    QLabel* stat_latest_val_  = nullptr;
    QLabel* stat_latest_sub_  = nullptr;
    QLabel* stat_change_val_  = nullptr;
    QLabel* stat_change_sub_  = nullptr;
    QLabel* stat_min_val_     = nullptr;
    QLabel* stat_min_sub_     = nullptr;
    QLabel* stat_max_val_     = nullptr;
    QLabel* stat_max_sub_     = nullptr;
    QLabel* stat_avg_val_     = nullptr;
    QLabel* stat_avg_sub_     = nullptr;
    QLabel* stat_count_val_   = nullptr;
    QLabel* stat_count_sub_   = nullptr;

    // Right panel
    QTableWidget* data_table_  = nullptr;
    QLabel*       data_title_  = nullptr;
    QLabel*       data_status_ = nullptr;
    QWidget*      empty_state_ = nullptr;
    QStackedWidget* table_stack_ = nullptr;

    // Status bar
    QLabel* status_source_    = nullptr;
    QLabel* status_country_   = nullptr;
    QLabel* status_indicator_ = nullptr;
    QLabel* status_points_    = nullptr;

    bool loading_ = false;
};

} // namespace fincept::screens
