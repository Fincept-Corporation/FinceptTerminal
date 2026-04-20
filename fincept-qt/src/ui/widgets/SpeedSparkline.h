// src/ui/widgets/SpeedSparkline.h
//
// Minimal rolling sparkline — paints the last N sample values as a filled
// polyline. Used by SetupScreen to visualise the download throughput during
// the long first-run Python install.
#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

namespace fincept::ui {

class SpeedSparkline : public QWidget {
    Q_OBJECT
  public:
    explicit SpeedSparkline(QWidget* parent = nullptr);

    /// Push a new sample; oldest is dropped when capacity is exceeded.
    void push(qint64 bytes_per_sec);

    void set_line_color(const QColor& c);
    void set_fill_color(const QColor& c);
    void set_capacity(int samples); // default 60 (=1 minute at 1 Hz)

  protected:
    void paintEvent(QPaintEvent* e) override;

  private:
    QVector<qint64> samples_;
    int capacity_ = 60;
    QColor line_color_ = QColor("#d97706");
    QColor fill_color_ = QColor(217, 119, 6, 40);
};

} // namespace fincept::ui
