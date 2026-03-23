#pragma once

#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept {
class HttpClient;
}

namespace fincept::screens {

/// Economic data source descriptor.
struct EconSource {
    QString id;
    QString name;
    QString script;        // Python script name
    QString color;         // source accent color
    bool needs_api_key;
    QString default_indicator;
};

/// Economics Data Explorer — 15 global data sources with 1000+ indicators.
/// Sources: World Bank, BIS, IMF, FRED, OECD, WTO, CFTC, EIA, ADB,
/// Federal Reserve, BLS, UNESCO, FiscalData, BEA, Fincept Macro API.
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
    void setup_ui();
    QWidget* create_header();
    QWidget* create_left_panel();
    QWidget* create_right_panel();
    QWidget* create_status_bar();

    void populate_countries(int source_index);
    void populate_indicators(int source_index);
    void execute_fetch(const QString& script, const QStringList& args);
    void display_data(const QJsonArray& data, const QString& title);
    void display_stats(const QJsonArray& data);
    void display_error(const QString& error);
    void set_loading(bool loading);

    // Sources
    QList<EconSource> sources_;
    int active_source_ = 0;
    QString active_country_ = "USA";
    QString active_indicator_;

    // Header
    QComboBox* source_combo_ = nullptr;
    QComboBox* date_preset_ = nullptr;
    QLineEdit* date_start_ = nullptr;
    QLineEdit* date_end_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;

    // Left panel
    QListWidget* country_list_ = nullptr;
    QListWidget* indicator_list_ = nullptr;
    QLineEdit* country_search_ = nullptr;
    QLineEdit* indicator_search_ = nullptr;
    QLabel* country_count_ = nullptr;
    QLabel* indicator_count_ = nullptr;

    // Right panel
    QLabel* stat_latest_ = nullptr;
    QLabel* stat_change_ = nullptr;
    QLabel* stat_min_ = nullptr;
    QLabel* stat_max_ = nullptr;
    QLabel* stat_avg_ = nullptr;
    QLabel* stat_count_ = nullptr;
    QTableWidget* data_table_ = nullptr;
    QLabel* data_title_ = nullptr;
    QLabel* data_status_ = nullptr;

    // Status bar
    QLabel* status_source_ = nullptr;
    QLabel* status_country_ = nullptr;
    QLabel* status_indicator_ = nullptr;

    bool loading_ = false;
};

} // namespace fincept::screens
