#pragma once
// MarketsDetailPane — the right-hand pane of the Markets screen (spec 2.3 f): the
// selected symbol's header (ticker, name, venue, last, change, session), a price
// chart with range presets, key-figure tiles (quote + 1y daily-history performance)
// and actions (Research / Trade / News / + Watchlist / Add to panel).
//
// Quote data comes from the screen's MarketsQuoteStore (the detail symbol is in its
// live set). The pane owns its history subscriptions (chart preset + the shared 1y/1d
// performance series), debounced 350 ms per selection, dropped while hidden.

#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class QEvent;

namespace fincept::ui {
class EmptyState;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

class MarketsQuoteStore;

class MarketsDetailPane : public QWidget {
    Q_OBJECT
  public:
    MarketsDetailPane(MarketsQuoteStore* store, QWidget* parent = nullptr);

    void set_symbol(const QString& symbol);
    QString symbol() const { return symbol_; }
    /// false: the symbol is in no panel (shows ADD TO PANEL).
    void set_listed(bool listed);
    void set_panel_titles(const QStringList& titles);
    void refresh_theme();
    void retranslate();

  signals:
    void open_requested(const QString& screen_id, const QString& symbol);
    void close_requested();
    /// panel_index -1 = a new panel.
    void add_to_panel_requested(const QString& symbol, int panel_index);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void on_rows_changed(const QSet<QString>& symbols);
    void update_header();
    void update_quote_tiles();
    void update_perf_tiles();
    void load_histories();
    void drop_subscriptions();
    void apply_history(const QString& topic, const QVariant& value);
    void apply_perf(const QVariant& value);
    /// The chart's price-axis formatter for the current symbol's quote (decimals, locale).
    void apply_chart_formatter();
    QString chart_topic_for(const QString& preset) const;
    static void set_tone(QLabel* label, const char* tone);

    QPointer<MarketsQuoteStore> store_;
    QString symbol_;
    QString preset_ = QStringLiteral("3M");
    QString chart_topic_;
    QString perf_topic_;
    int series_ = -1;
    bool listed_ = true;
    QStringList panel_titles_;

    enum class PerfState { None, Loading, Ready, Failed };
    PerfState perf_state_ = PerfState::None;
    QString perf_error_;
    double perf_1m_, perf_ytd_, perf_1y_, perf_vol20_;

    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr; // the shared empty state (nothing selected)
    QLabel* sym_label_ = nullptr;
    QLabel* name_label_ = nullptr;
    QToolButton* close_btn_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* session_label_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    ui::KpiStrip* tiles_ = nullptr;
    QPushButton* research_btn_ = nullptr;
    QPushButton* trade_btn_ = nullptr;
    QPushButton* news_btn_ = nullptr;
    QToolButton* watch_btn_ = nullptr;
    QToolButton* add_btn_ = nullptr;
    QTimer* load_timer_ = nullptr;
};

} // namespace fincept::screens
