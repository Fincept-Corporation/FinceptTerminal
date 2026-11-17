// src/screens/ma_analytics/MaDealsWorkspace.h
#pragma once
// Deal Database module: the "Deals" and "Filings" sub-tabs of the M&A Analytics screen.
//
//  * Filings — scan SEC EDGAR (form types, days, how many filings to read), with progress and Cancel; the stored
//    filings in a table; open a filing on sec.gov; extract the deal from one filing.
//  * Deals   — every deal in the local deal database (ma_deals.db, Python-side) in a sortable, filterable table.
//    A deal extracted automatically from a filing carries an UNVERIFIED badge until the user checks it: Edit and
//    Save, or Mark verified. Add / delete deals, export the table as CSV.
//
// All database work runs in Python through MAAnalyticsService (asynchronous, request ids guard against stale
// replies); this class only builds widgets and reacts to replies. Pages are built lazily by MAModulePanel's
// add_lazy_sub_tab and styled before their children exist.
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QComboBox;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableView;
class QTimer;
class QWidget;

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

class MaDealsModel;
class MaDealsProxy;

class MaDealsWorkspace : public QObject {
    Q_OBJECT
  public:
    /// `host` owns this object and parents its dialogs; `set_status` writes the module header's status text.
    MaDealsWorkspace(QColor accent, std::function<void(const QString&)> set_status, QWidget* host);

    void build_deals_page(QWidget* page);
    void build_filings_page(QWidget* page);

    /// Re-apply the pages' stylesheets and repaint the badges (ThemeManager::theme_changed).
    void restyle();

  private:
    // Deals
    void reload_deals();
    void on_deals_loaded(quint64 id, const QJsonArray& deals);
    void update_deal_buttons();
    /// The info line and which of table / loading skeleton / empty state shows.
    void update_deals_info();
    /// The deals empty state: "nothing yet" (ADD DEAL / SCAN SEC FILINGS) or a load failure (RELOAD).
    void set_deals_empty(bool failed, const QString& message);
    /// Bring the Filings sub-tab forward (from the deals empty state).
    void show_filings_tab();
    QJsonObject selected_deal() const;
    void open_deal_dialog(const QJsonObject& deal, const QString& intro);
    void toggle_verified();
    void delete_selected();
    void export_csv();

    // Filings
    void reload_filings();
    void on_filings_loaded(quint64 id, const QJsonArray& filings);
    /// Table, loading skeleton or the "no filings yet" state (SCAN is never started automatically).
    void update_filings_view();
    void start_scan();
    void set_scanning(bool on);
    void update_filing_buttons();
    QJsonObject selected_filing() const;
    void open_selected_filing();
    void extract_selected();

    // Replies
    void on_scan_progress(quint64 id, const QString& message, int done, int total, int filings, int deals_created);
    void on_scan_finished(quint64 id, const QJsonObject& result);
    void on_scan_failed(quint64 id, const QString& message, bool cancelled, int kept_filings);
    void on_deal_saved(quint64 id, const QJsonObject& deal);
    void on_deal_removed(quint64 id, const QString& deal_id);
    void on_deal_extracted(quint64 id, const QJsonObject& reply);
    void on_op_failed(quint64 id, const QString& message, const QStringList& missing_inputs);

    void set_note(QLabel* label, const QString& text, const char* tone);
    QString page_stylesheet(const QString& object_name) const;

    QColor accent_;
    std::function<void(const QString&)> set_status_;
    QPointer<QWidget> host_;

    // Deals page
    QPointer<QWidget> deals_page_;
    MaDealsModel* deals_model_ = nullptr;
    MaDealsProxy* deals_proxy_ = nullptr;
    QTableView* deals_view_ = nullptr;
    QLineEdit* filter_edit_ = nullptr;
    QComboBox* show_combo_ = nullptr;
    QTimer* filter_timer_ = nullptr;
    QLabel* deals_info_ = nullptr;
    QLabel* deals_note_ = nullptr;
    ui::EmptyState* deals_empty_ = nullptr;
    ui::LoadingSkeleton* deals_loading_ = nullptr;
    bool deals_failed_ = false;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* verify_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Filings page
    QPointer<QWidget> filings_page_;
    MaDealsModel* filings_model_ = nullptr;
    QTableView* filings_view_ = nullptr;
    QVector<QPair<QString, QCheckBox*>> form_checks_;
    QSpinBox* days_spin_ = nullptr;
    QSpinBox* max_spin_ = nullptr;
    QPushButton* scan_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* scan_note_ = nullptr;
    ui::EmptyState* filings_empty_ = nullptr;
    ui::LoadingSkeleton* filings_loading_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QPushButton* extract_btn_ = nullptr;

    // Latest request per kind; a reply with another id is stale and dropped.
    quint64 deals_req_ = 0;
    quint64 filings_req_ = 0;
    quint64 scan_req_ = 0;
    quint64 extract_req_ = 0;
    QSet<quint64> op_reqs_; // save / verify / delete in flight
};

} // namespace fincept::screens
