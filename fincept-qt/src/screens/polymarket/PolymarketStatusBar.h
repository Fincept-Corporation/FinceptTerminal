#pragma once

#include <QLabel>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Bottom status strip with view info, market count, selection, WS status.
class PolymarketStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketStatusBar(QWidget* parent = nullptr);

    void set_view(const QString& view);
    void set_count(int count, const QString& label);
    void set_selected(const QString& question);
    void set_ws_status(bool connected);

  private:
    QLabel* view_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* selected_label_ = nullptr;
    QLabel* ws_label_ = nullptr;
};

} // namespace fincept::screens::polymarket
