#pragma once

#include <QColor>
#include <QLabel>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Bottom status strip with brand, view info, market count, selection,
/// exchange-level status (Kalshi only), and WS status.
class PolymarketStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketStatusBar(QWidget* parent = nullptr);

    void set_view(const QString& view);
    void set_count(int count, const QString& label);
    void set_selected(const QString& question);
    void set_ws_status(bool connected);

    /// Brand label (left) follows the active exchange. The colour is used
    /// for the brand text and the selected-market highlight.
    void set_brand(const QString& name, const QColor& accent);

    /// Exchange lifecycle (OPEN / PAUSED / CLOSED / MAINT). Empty string
    /// hides the pill — use this for exchanges that don't expose status.
    void set_exchange_status(const QString& status);

    /// Human-readable "Opens at …" / "Closed" hint pulled from the
    /// exchange schedule. Empty hides the label.
    void set_next_session(const QString& text);

  private:
    QLabel* brand_label_ = nullptr;
    QLabel* view_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* selected_label_ = nullptr;
    QLabel* exchange_status_ = nullptr;
    QLabel* next_session_ = nullptr;
    QLabel* ws_label_ = nullptr;

    QColor accent_{0xD97706};  // default amber — Polymarket
};

} // namespace fincept::screens::polymarket
