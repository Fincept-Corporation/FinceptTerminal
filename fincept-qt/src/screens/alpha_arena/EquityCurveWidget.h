#pragma once
// EquityCurveWidget — multi-line equity chart, custom QPainter (no WebEngine).
// One line per agent in its brand color; dashed baseline at starting capital;
// hover crosshair with per-agent values; range filter applied by caller.
#include <QColor>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <QVector>

namespace fincept::screens::alpha_arena {

struct EquitySeries {
    QString agent_id, label;
    QColor color;
    QVector<QPointF> points;   // x = ts (ms), y = equity
};

class EquityCurveWidget : public QWidget {
    Q_OBJECT
  public:
    explicit EquityCurveWidget(QWidget* parent = nullptr);
    void set_data(QVector<EquitySeries> series, double baseline);
  protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;
  private:
    QRectF plot_rect() const;
    QVector<EquitySeries> series_;
    double baseline_ = 10000, min_y_ = 0, max_y_ = 1;
    qint64 min_x_ = 0, max_x_ = 1;
    QPoint hover_{-1, -1};
};

} // namespace fincept::screens::alpha_arena
