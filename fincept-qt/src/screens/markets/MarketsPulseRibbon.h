#pragma once
// MarketsPulseRibbon — one row of ten compact StatTiles (S&P 500 ... BITCOIN) with
// price, direction, change and a 5-day sparkline, always live while the screen is
// visible (the store's live set). Never wraps: tiles drop from the right when narrow.

#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace fincept::ui {
class StatTile;
}

namespace fincept::screens {

class MarketsQuoteStore;

class MarketsPulseRibbon : public QWidget {
    Q_OBJECT
  public:
    MarketsPulseRibbon(MarketsQuoteStore* store, QWidget* parent = nullptr);

    QStringList symbols() const;
    void refresh_theme();
    void retranslate();
    /// Never demands ten tiles' width: tiles drop from the right in resizeEvent instead.
    QSize minimumSizeHint() const override;

  signals:
    void symbol_selected(const QString& symbol);
    void symbol_activated(const QString& symbol);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* e) override;

  private:
    void update_tiles(const QSet<QString>& symbols);
    void update_tile(int index);
    void apply_visible_count();

    struct Tile {
        QString symbol;
        const char* label;
        ui::StatTile* tile = nullptr;
    };
    QPointer<MarketsQuoteStore> store_;
    QVector<Tile> tiles_;
    int shown_ = -1;
};

} // namespace fincept::screens
