#pragma once
#include "SurfaceTypes.h"
#include <QWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::surface {

// SurfaceTableWidget — renders the current surface data as a color-coded table
class SurfaceTableWidget : public QWidget {
    Q_OBJECT
public:
    explicit SurfaceTableWidget(QWidget* parent = nullptr);

    void show_surface(ChartType type,
                      const void* data_ptr); // cast based on ChartType

    // Typed overloads for the most common ones
    void show_vol(const VolatilitySurfaceData& d);
    void show_greeks(const GreeksSurfaceData& d);
    void show_correlation(const CorrelationMatrixData& d);
    void show_yield(const YieldCurveData& d);
    void show_pca(const PCAData& d);
    void show_generic_matrix(
        const std::vector<std::string>& row_labels,
        const std::vector<std::string>& col_labels,
        const std::vector<std::vector<float>>& z,
        float min_val, float max_val, bool diverging = false);

private:
    QTableWidget* table_;
    void apply_heatmap(float value, float min_val, float max_val, bool diverging, int row, int col);
    static QColor heat_color(float t, bool diverging);
};

} // namespace fincept::surface
