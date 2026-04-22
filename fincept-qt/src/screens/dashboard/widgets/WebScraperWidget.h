#pragma once
// WebScraperWidget — fully generic data scraper for the dashboard.
//
// Fetches a URL and auto-detects the payload format (HTML tables, JSON
// arrays, CSV/TSV, XML/RSS/Atom), then renders the result in a sortable
// table. When the source has multiple tables the user picks which one via
// a dropdown in the widget body. All work is pure C++ over
// QNetworkAccessManager — no Python, no QtWebEngine dependency.
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
//     "force_format": ""              // "", "html", "json", "csv", "xml"
//   }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace fincept::ui {
class DataTable;
}

namespace fincept::screens::widgets {

/// A table of extracted data — one header row plus N data rows.
struct ScrapedTable {
    QString label;           // "Table 1", "items[]", "RSS feed" etc.
    QStringList headers;
    QVector<QStringList> rows;
};

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
    QDialog* make_config_dialog(QWidget* parent) override;

  private slots:
    void on_refresh_clicked();
    void on_table_selected(int index);
    void on_auto_refresh_tick();

  private:
    void build_ui();
    void apply_styles();
    void start_fetch();
    void handle_reply(QNetworkReply* reply);
    void parse_payload(const QByteArray& body, const QString& content_type);
    void render_selected_table();
    void set_status(const QString& msg, bool error = false);
    void apply_timer_state();

    // Per-format parsers (return 0..N tables).
    QVector<ScrapedTable> parse_html_tables(const QString& html) const;
    QVector<ScrapedTable> parse_json_tables(const QByteArray& body) const;
    QVector<ScrapedTable> parse_csv_tables(const QString& text, QChar delim) const;
    QVector<ScrapedTable> parse_xml_tables(const QByteArray& body) const;

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
    QNetworkAccessManager* net_ = nullptr;
    QPointer<QNetworkReply> pending_reply_;
    QTimer* auto_timer_ = nullptr;
    QVector<ScrapedTable> tables_;
    QString last_format_;
};

} // namespace fincept::screens::widgets
