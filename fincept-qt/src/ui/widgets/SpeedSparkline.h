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

    /// Override the colours; an invalid colour (the default) means the
    /// theme accent, and the fill the accent at low alpha, read at paint time.
    void set_line_color(const QColor& c);
    void set_fill_color(const QColor& c);

  protected:
    void paintEvent(QPaintEvent* e) override;

  private:
    QVector<qint64> samples_;
    int capacity_ = 60;
    QColor line_color_;
    QColor fill_color_;
};

} // namespace fincept::ui
