// src/screens/gov_data/GovDataTreasuryPanel.h
// Specialized panel for US Treasury data: Prices, Auctions, Summary
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QJsonObject>
#include <QStackedWidget>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <atomic>
#include <memory>

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class InputMemory;
class KpiStrip;
class StatTile;
} // namespace fincept::ui

namespace fincept::screens {

class GovTableModel;

class GovDataTreasuryPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataTreasuryPanel(QWidget* parent = nullptr);

  public slots:
    /// Called by the screen when the panel is first opened; the pane loads the
    /// selected tab on show by itself.
    void load_initial_data();

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_endpoint_changed(int index);
    void on_fetch();
    void on_export_csv();

  private:
    enum Endpoint { Prices = 0, Auctions, Summary };

    void build_ui();
    QWidget* build_toolbar();
    QWidget* build_filter_bar();
    void build_pane(QWidget* parent);
    void retranslateUi();

    /// View of `ep` for the inputs on screen: "prices|<end>|<type>", "auctions|<start>|<end>|<type>",
    /// "summary|<end>". The default dates are written "latest" / "default", so the next day's
    /// default view is the same view (its remembered result shows, then refreshes).
    QString view_key(Endpoint ep) const;
    static Endpoint endpoint_of(const QString& key);
    /// Switch to `ep` with the inputs on screen. `user`: run now when nothing is remembered;
    /// `force`: run even when something is (FETCH).
    void go(Endpoint ep, bool user, bool force = false);
    void start_run(quint64 run);
    void request(quint64 run, const QString& key);
    QDate resolve_end(const QString& token) const;
    QDate resolve_start(const QString& token) const;
    static QDate previous_business_day(const QDate& d);
    /// The payload of endpoint `ep` from a script answer (dates as ISO text, resolved on the GUI
    /// thread). Pure: runs on the shaping thread.
    static QJsonObject shape(Endpoint ep, const QJsonObject& data, const QString& start_iso, const QString& end_iso);
    void render(const QJsonValue& payload);
    void show_no_data(const QJsonObject& payload);
    void update_toolbar_state();
    void recolor_items();

    void populate_prices(const QJsonArray& rows);
    void populate_auctions(const QJsonArray& rows);
    void populate_summary(const QJsonObject& data);

    Endpoint active_endpoint_ = Prices;
    QDate default_end_;   // latest business day with prices, as far as known
    QDate default_start_; // a month before today
    int fallback_left_ = 0; // steps back still allowed for the default date of the run in flight

    // Toolbar
    QPushButton* prices_btn_ = nullptr;
    QPushButton* auctions_btn_ = nullptr;
    QPushButton* summary_btn_ = nullptr;
    QDateEdit* start_date_ = nullptr;
    QDateEdit* end_date_ = nullptr;
    QComboBox* security_type_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* as_of_label_ = nullptr;
    QLabel* info_label_ = nullptr;

    // Content
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTableView* prices_table_ = nullptr;   // ~460 securities a day: a model, cells formatted when shown
    QTableView* auctions_table_ = nullptr;
    GovTableModel* prices_model_ = nullptr;
    GovTableModel* auctions_model_ = nullptr;
    QWidget* summary_page_ = nullptr;
    ui::EmptyState* no_data_page_ = nullptr;
    QDate no_data_retry_date_; // what the no-data page's button opens

    // Summary
    ui::KpiStrip* summary_strip_ = nullptr;
    ui::StatTile* tile_total_ = nullptr;
    ui::StatTile* tile_min_rate_ = nullptr;
    ui::StatTile* tile_max_rate_ = nullptr;
    ui::StatTile* tile_avg_rate_ = nullptr;
    ui::StatTile* tile_min_price_ = nullptr;
    ui::StatTile* tile_max_price_ = nullptr;
    ui::StatTile* tile_avg_price_ = nullptr;
    QTableWidget* type_breakdown_table_ = nullptr;
    QLabel* breakdown_hdr_lbl_ = nullptr;

    // Filter bar labels (cached for retranslateUi)
    QLabel* filter_from_lbl_ = nullptr;
    QLabel* filter_to_lbl_ = nullptr;
    QLabel* filter_type_lbl_ = nullptr;

    ui::InputMemory* memory_ = nullptr;
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> tokens_;
};

} // namespace fincept::screens
