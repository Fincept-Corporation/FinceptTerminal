// src/screens/equity_research/EquityFilingsTab.h
#pragma once

#include "services/equity/SecFilingsService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

#include <array>

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QSplitter;
class QTableWidget;
class QVBoxLayout;

namespace fincept::screens {

/// Equity Research → FILINGS. SEC EDGAR views for the active (US-registered) symbol:
/// recent filings with document links, 10-K / 10-Q key XBRL metrics with YoY, 8-K events,
/// Form 4 insider activity and 13F institutional holders. Loads only when shown; each
/// panel has its own loading / error + RETRY state and shows a cached copy (labelled)
/// while it refreshes. Data comes from SecFilingsService (mcp/edgar/main.py).
class EquityFilingsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityFilingsTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    struct Section {
        const char* title_key = nullptr;
        QFrame* frame = nullptr;
        QLabel* title = nullptr;
        QLabel* meta = nullptr;   ///< "fetched 14:02" / "cached copy … refreshing"
        QLabel* status = nullptr; ///< loading / empty / error text
        QPushButton* retry = nullptr;
        QWidget* body = nullptr;
        bool loaded = false;
        bool loading = false;
        bool stale = false;
        qint64 fetched_ms = 0;
    };

    void build_ui();
    QWidget* build_header();
    QWidget* build_filings_body();
    QWidget* build_financials_body();
    QWidget* build_events_body();
    QWidget* build_insiders_body();
    QWidget* build_holders_body();
    /// Panel frame for sections_[slot]; its RETRY re-requests `target` (the 8-K panel is
    /// fed by the Filings call).
    void make_section(int slot, services::equity::SecSection target, const char* title_key, QWidget* body);
    Section& section(services::equity::SecSection which);

    void request_all(bool force);
    void request(services::equity::SecSection which, bool force);
    void on_section_loaded(const services::equity::SecSectionData& d);
    void on_section_failed(const QString& symbol, services::equity::SecSection which, const QString& error);
    void show_loading(Section& sec);
    void show_error(Section& sec, const QString& message);
    void show_body(Section& sec);
    void update_meta(Section& sec);

    void render_filings(const QJsonObject& data);
    void render_financials(const QJsonObject& data);
    void render_insiders(const QJsonObject& data);
    void render_holders(const QJsonObject& data);
    void apply_filings_filter();
    void set_table_headers();

    void open_url(const QString& url);
    void show_link_menu(QTableWidget* table, const QPoint& pos);
    void retranslateUi();

    QString current_symbol_;
    bool sec_eligible_ = false;

    // Header
    QLabel* title_lbl_ = nullptr;
    QLabel* company_lbl_ = nullptr;
    QLabel* latest_lbl_ = nullptr; ///< rich-text links to the latest 10-K / 10-Q / proxy
    QPushButton* edgar_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* notice_lbl_ = nullptr; ///< non-US / no-symbol explanation
    QSplitter* body_splitter_ = nullptr;
    QString edgar_url_;

    // Filings
    QComboBox* filter_combo_ = nullptr;
    QLabel* filings_count_lbl_ = nullptr;
    QTableWidget* filings_table_ = nullptr;
    // Financials
    QTableWidget* fin_table_ = nullptr;
    QLabel* fin_note_ = nullptr;
    // 8-K events
    QTableWidget* events_table_ = nullptr;
    // Insiders
    QLabel* ins_buys_lbl_ = nullptr;
    QLabel* ins_sells_lbl_ = nullptr;
    QLabel* ins_note_ = nullptr;
    QTableWidget* ins_table_ = nullptr;
    // Holders
    QLabel* hold_summary_lbl_ = nullptr;
    QTableWidget* hold_table_ = nullptr;
    QLabel* own13f_lbl_ = nullptr;
    QTableWidget* own13f_table_ = nullptr;
    QLabel* hold_note_ = nullptr;

    // 8-K events share the filings section's state (one script call feeds both panels).
    std::array<Section, 5> sections_{};
    QJsonObject filings_data_;
    QJsonObject financials_data_;
    QJsonObject insiders_data_;
    QJsonObject holders_data_;
};

} // namespace fincept::screens
