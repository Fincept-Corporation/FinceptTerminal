#pragma once
#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

#include <functional>

namespace fincept::screens {

/// Data source descriptor for AkShare scripts.
struct AkShareSource {
    QString id;
    QString name;
    QString script; // e.g. "akshare_stocks_realtime.py"
    QString icon;   // emoji
};

/// AkShare Data Explorer screen.
/// Provides access to 26+ AkShare data sources with 1000+ endpoints.
/// Users select a source, pick an endpoint, optionally set parameters,
/// and view results in a table or raw JSON.
class AkShareScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AkShareScreen(QWidget* parent = nullptr);

    // IStatefulScreen — remembers active source, last endpoint, search text,
    // and whether the user prefers table vs JSON view.
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return "akshare"; }

  private slots:
    void on_source_clicked(int index);
    void on_endpoint_clicked(QListWidgetItem* item);
    void on_search_changed(const QString& text);
    void on_execute();
    void on_view_toggle();
    void on_refresh();

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_source_grid();
    QWidget* create_endpoint_panel();
    QWidget* create_params_panel();
    QWidget* create_data_panel();
    QWidget* create_status_bar();

    void load_endpoints(const AkShareSource& source);
    void populate_endpoint_list(const QJsonObject& result);
    void execute_query(const QString& script, const QString& endpoint, const QStringList& args);
    void display_table_data(const QJsonArray& data);
    void display_json_data(const QJsonArray& data);
    void display_error(const QString& error);
    void set_loading(bool loading);

    // Data sources
    QList<AkShareSource> sources_;
    int active_source_ = -1;
    QString active_endpoint_;

    // Endpoint cache: script -> { endpoints json }
    QHash<QString, QJsonObject> endpoint_cache_;

    // UI
    QList<QPushButton*> source_btns_;
    QListWidget* endpoint_list_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QLabel* endpoint_count_ = nullptr;

    // Parameters
    QLineEdit* param_symbol_ = nullptr;
    QLineEdit* param_start_ = nullptr;
    QLineEdit* param_end_ = nullptr;
    QComboBox* param_period_ = nullptr;

    // Data display
    QStackedWidget* view_stack_ = nullptr;
    QTableWidget* data_table_ = nullptr;
    QTextEdit* json_view_ = nullptr;
    QPushButton* view_toggle_btn_ = nullptr;
    QPushButton* exec_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* data_status_ = nullptr;
    QLabel* record_count_ = nullptr;

    // Status bar
    QLabel* status_source_ = nullptr;
    QLabel* status_endpoint_ = nullptr;

    bool is_table_view_ = true;
    bool loading_ = false;
};

} // namespace fincept::screens
