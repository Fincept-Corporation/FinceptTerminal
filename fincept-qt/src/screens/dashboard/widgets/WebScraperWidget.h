#pragma once
// WebScraperWidget — generic table scraper tile for the dashboard.
//
// Fetching and format detection/parsing (HTML tables, JSON arrays, CSV/TSV,
// XML/RSS/Atom) live in services::WebScrapeService — one shared, bounded
// network stack with the parse off the GUI thread. This tile only holds the
// configuration and renders the chosen table.
//
// Config (persisted):
//   {
//     "url": "https://...",
//     "table_index": 0,
//     "refresh_sec": 0,               // 0 = manual refresh only
//     "user_agent": "...",            // optional override
//     "headers": { "K": "V", ... },   // optional extra HTTP headers
//     "encoding": "utf-8",            // optional; falls back to Content-Type
//     "json_path": "data.items",      // optional: dot path into a JSON root
//     "force_format": ""              // "", "html", "json", "csv", "tsv", "xml"
//   }

#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/web_scrape/WebScrapeService.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QTimer;

namespace fincept::ui {
class DataTable;
}

namespace fincept::screens::widgets {

class WebScraperWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit WebScraperWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);
    ~WebScraperWidget() override;

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private slots:
    void on_refresh_clicked();
    void on_table_selected(int index);
    void on_auto_refresh_tick();

  private:
    void build_ui();
    void apply_styles();
    void start_fetch();
    void cancel_fetch();
    void on_result(const services::WebScrapeResult& result);
    void render_selected_table();
    void set_status(const QString& msg, bool error = false);
    void apply_timer_state();

    // Config
    QString url_;
    int table_index_ = 0;
    int refresh_sec_ = 0;
    QString user_agent_;
    QHash<QString, QString> headers_;
    QString encoding_;
    QString json_path_;
    QString force_format_;

    // UI
    QComboBox* table_combo_ = nullptr;
    QLabel* format_badge_ = nullptr;
    QLabel* status_label_ = nullptr;
    ui::DataTable* table_ = nullptr;

    // Runtime
    quint64 pending_id_ = 0; ///< WebScrapeService request id, 0 = idle
    QTimer* auto_timer_ = nullptr;
    QVector<services::ScrapedTable> tables_;
    QString last_format_;
    bool has_result_ = false; ///< a fetch completed (successfully) since the URL was set
};

} // namespace fincept::screens::widgets
