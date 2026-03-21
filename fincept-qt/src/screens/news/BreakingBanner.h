#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include "services/news/NewsClusterService.h"

namespace fincept::screens {

/// Red/amber animated breaking news banner with auto-dismiss.
class BreakingBanner : public QWidget {
    Q_OBJECT
public:
    explicit BreakingBanner(QWidget* parent = nullptr);

    void show_cluster(const fincept::services::NewsCluster& cluster);

signals:
    void banner_clicked();

private:
    QLabel* tag_label_      = nullptr;
    QLabel* headline_label_ = nullptr;
    QLabel* source_label_   = nullptr;
    QTimer* dismiss_timer_  = nullptr;
};

} // namespace fincept::screens
