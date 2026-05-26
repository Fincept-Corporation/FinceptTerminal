#pragma once
#include "services/options/OptionChainTypes.h"
#include <QPixmap>
#include <QWidget>

namespace fincept::screens::fno {

class PayoffStripWidget : public QWidget {
    Q_OBJECT
public:
    explicit PayoffStripWidget(QWidget* parent = nullptr);
    void set_payoff(const QVector<fincept::services::options::PayoffPoint>& curve,
                    double spot, const QVector<double>& breakevens);
    void clear_payoff();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void rebuild_pixmap();
    QVector<fincept::services::options::PayoffPoint> curve_;
    double spot_ = 0;
    QVector<double> breakevens_;
    QPixmap cached_;
    bool dirty_ = true;
};

} // namespace fincept::screens::fno
