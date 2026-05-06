#pragma once
// DataSourcesSection.h — quick management panel for configured data-source
// connections. Full CRUD lives in the dedicated Data Sources screen.

#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class DataSourcesSection : public QWidget {
    Q_OBJECT
  public:
    explicit DataSourcesSection(QWidget* parent = nullptr);

  private:
    /// Rebuild the inner content widget — used after bulk actions.
    void rebuild();

    /// Build the inner content (stats + connections table + bulk actions).
    QWidget* build_content();

    QVBoxLayout* host_layout_ = nullptr; // wraps the swappable content widget
    QWidget*     content_     = nullptr;
};

} // namespace fincept::screens
