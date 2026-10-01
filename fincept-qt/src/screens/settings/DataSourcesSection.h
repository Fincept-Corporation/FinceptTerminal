#pragma once
// DataSourcesSection.h — quick management panel for configured data-source
// connections. Full CRUD lives in the dedicated Data Sources screen.

#include <QEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class DataSourcesSection : public QWidget {
    Q_OBJECT
  public:
    explicit DataSourcesSection(QWidget* parent = nullptr);

  protected:
    /// Re-reads the connection list every time the section is shown — sources are
    /// added / removed / toggled in the full Data Sources screen, and the list
    /// used to stay frozen at whatever existed when Settings was constructed.
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    /// Rebuild the inner content widget — used after bulk actions and on
    /// QEvent::LanguageChange (the rebuild re-runs every tr() lookup, which is
    /// simpler than caching the many dynamically-generated per-connection rows).
    void rebuild();

    /// Build the inner content (stats + connections table + bulk actions).
    QWidget* build_content();

    QVBoxLayout* host_layout_ = nullptr; // wraps the swappable content widget
    QWidget* content_ = nullptr;
};

} // namespace fincept::screens
