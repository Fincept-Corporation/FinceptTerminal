#pragma once
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QGlyphRun>
#include <QHash>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Scrolling ticker bar with live market data.
/// Right-click → "Edit Symbols" to customise the symbol list.
/// Symbol list is persisted via SettingsRepository under "ticker_bar_symbols".
class TickerBar : public QWidget {
    Q_OBJECT
  public:
    explicit TickerBar(QWidget* parent = nullptr);

    /// Callers fill only symbol/price/change; everything below is derived once
    /// in set_data(). paintEvent runs 20×/s over every entry × every pass, so
    /// it must not format strings or measure text — see rebuild_entry_cache().
    struct Entry {
        QString symbol{};
        double price = 0;
        double change = 0;

        // ── Derived (do not set from outside) ──
        // The `{}` are load-bearing: callers brace-init only the first three
        // fields, and -Wmissing-field-initializers (fatal on Linux/macOS under
        // -Wextra -Werror) flags every omitted member that lacks an NSDMI.
        QString price_text{};
        QString change_text{};
        QColor change_col{};
        int symbol_width = 0;
        int price_width = 0;
        int change_width = 0;
        int total_width = 0;
        // Pre-shaped glyphs, positioned relative to the baseline: paintEvent
        // draws these without re-shaping (see layout_runs()).
        QList<QGlyphRun> symbol_runs{};
        QList<QGlyphRun> price_runs{};
        QList<QGlyphRun> change_runs{};
    };

    void set_data(const QVector<Entry>& entries);
    void pause() { scroll_timer_.stop(); }
    void resume() {
        if (total_width_ > 0)
            scroll_timer_.start();
    }

    /// Returns the current symbol list (persisted user preference).
    QStringList symbols() const { return symbols_; }

    /// Replace the symbol list programmatically (MCP tool, other screens) and
    /// emit symbols_changed. An empty list is allowed and clears the strip.
    /// `persist` = false when the caller already wrote the setting.
    void set_symbols(const QStringList& symbols, bool persist = true);

  signals:
    /// Emitted when the user saves a new symbol list — caller should re-fetch.
    void symbols_changed(const QStringList& symbols);
    /// Emitted when the user double-clicks a symbol in the scrolling strip.
    void symbol_activated(const QString& symbol);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void load_symbols();
    void save_symbols();
    void show_edit_bar();
    void hide_edit_bar();
    void commit_edit();
    /// Re-derives font, metrics, per-entry strings/widths/colours and
    /// total_width_ from entries_. Called on new data and on theme change
    /// (both font family/size and colours are theme tokens).
    void rebuild_entry_cache();

    // ── Scrolling ──
    QVector<Entry> entries_;
    QTimer scroll_timer_;
    double offset_ = 0;
    int total_width_ = 0;

    // ── Cached paint state (never built inside paintEvent — P9) ──
    QFont ticker_font_;
    QFontMetrics ticker_fm_;
    QColor symbol_color_;
    QColor price_color_;
    /// Text is shaped once into glyph runs and paintEvent only draws glyphs —
    /// QPainter::drawText() re-shaped every visible segment 20x a second.
    /// Runs by string, carried across set_data() calls so a price update only
    /// shapes the segments whose text changed. Valid for run_cache_font_ /
    /// run_cache_dir_ only.
    QHash<QString, QList<QGlyphRun>> run_cache_;
    QFont run_cache_font_;
    Qt::LayoutDirection run_cache_dir_ = Qt::LeftToRight;
    QList<QGlyphRun> runs_for(const QString& text, QHash<QString, QList<QGlyphRun>>& next_cache);

    // ── Symbol list ──
    QStringList symbols_;

    // ── Inline edit overlay ──
    QWidget* edit_bar_ = nullptr;
    QLabel* edit_label_ = nullptr;
    QLineEdit* edit_input_ = nullptr;
    QPushButton* edit_ok_ = nullptr;
    QPushButton* edit_cancel_ = nullptr;

    void retranslateUi();
};

} // namespace fincept::screens
