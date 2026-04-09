// src/screens/portfolio/PortfolioSparkline.h
#pragma once
#include "ui/theme/Theme.h"

#include <QColor>
#include <QPixmap>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Minimal sparkline widget — draws a mini line chart from data points.
/// Caches render as QPixmap (P9) and repaints only on data or size change.
class PortfolioSparkline : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioSparkline(int width = 65, int height = 20, QWidget* parent = nullptr);

    void set_data(const QVector<double>& data);
    void set_color(const QColor& color);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    void rebuild_pixmap();

    QVector<double> data_;
    QColor color_ = QColor(ui::colors::POSITIVE());
    QPixmap cached_;
    bool dirty_ = true;
};

} // namespace fincept::screens
