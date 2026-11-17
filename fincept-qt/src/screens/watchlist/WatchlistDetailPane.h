#pragma once
// WatchlistDetailPane — the Watchlist screen's third pane, driven by the
// table's current row: header (name, last, change, session), a price chart
// with range presets (1D 5D 1M 3M 6M YTD 1Y 5Y), key-stat tiles, the entry's
// notes (editable) and the symbol's latest headlines.
//
// The chart reads the shared `market:history:*` topics (the 1M-1Y presets use
// the same daily series as the table's performance columns, so no extra
// fetch); headlines come from EquityResearchService::fetch_news (read-only).
// Nothing is fetched while the pane is hidden.

#include "screens/watchlist/WatchlistModel.h"

#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace fincept::ui {
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

class WatchlistDetailPane : public QWidget {
    Q_OBJECT
  public:
    explicit WatchlistDetailPane(QWidget* parent = nullptr);

    /// Show `row` (nullptr = nothing selected). Cheap to call on every selection change.
    void set_row(const WatchlistRowState* row);
    /// The shown symbol's data changed (quote / info / perf): refresh header and tiles.
    void update_row(const WatchlistRowState& row);
    QString symbol() const { return symbol_; }
    /// The model supplies the session (venue) state for the header.
    void set_model(const WatchlistModel* model) { model_ = model; }
    void refresh_theme();
    void retranslate();

  signals:
    /// The user edited the notes of `symbol` (debounced).
    void notes_edited(const QString& symbol, const QString& notes);
    void open_requested(const QString& screen_id, const QString& symbol);
    void close_requested();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void update_header(const WatchlistRowState& row);
    void update_tiles(const WatchlistRowState& row);
    void load_chart();
    void apply_history(const QString& topic, const QVariant& value);
    void load_news();
    void set_news_status(const QString& text);

    QPointer<const WatchlistModel> model_;
    QString symbol_;
    QString loaded_symbol_; ///< symbol whose chart / news requests are out
    QString chart_topic_;
    QString preset_ = QStringLiteral("3M");
    QString price_ccy_;
    int price_digits_ = -1;
    bool notes_dirty_ = false;
    bool setting_notes_ = false;

    QWidget* header_ = nullptr;
    QLabel* sym_label_ = nullptr;
    QLabel* name_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* session_label_ = nullptr;
    QPushButton* research_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    int series_ = -1;
    ui::KpiStrip* tiles_ = nullptr;
    QLabel* notes_title_ = nullptr;
    QPlainTextEdit* notes_ = nullptr;
    QLabel* news_title_ = nullptr;
    QLabel* news_status_ = nullptr;
    QListWidget* news_ = nullptr;
    QLabel* empty_label_ = nullptr;
    QWidget* scroll_ = nullptr;
    QWidget* body_ = nullptr;
    QTimer* load_timer_ = nullptr;
    QTimer* notes_timer_ = nullptr;
};

} // namespace fincept::screens
