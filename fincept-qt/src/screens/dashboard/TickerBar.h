#pragma once
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Scrolling ticker bar with market data.
class TickerBar : public QWidget {
    Q_OBJECT
  public:
    explicit TickerBar(QWidget* parent = nullptr);

    struct Entry {
        QString symbol;
        double price;
        double change;
    };

    void set_data(const QVector<Entry>& entries);
    void pause() { scroll_timer_.stop(); }
    void resume() { scroll_timer_.start(); }

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QVector<Entry> entries_;
    QTimer scroll_timer_;
    double offset_ = 0;
};

} // namespace fincept::screens
