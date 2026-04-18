#pragma once

#include <QTimer>
#include <QWidget>

class QTableWidget;

namespace fincept::screens::devtools {

/// Minimal live view over `DataHub::stats()`. Refreshes once per second
/// while visible. Surfaced in Settings → Developer as an inline diagnostic
/// panel (topics, subscriber counts, policy, last publish time).
class DataHubInspector : public QWidget {
    Q_OBJECT
  public:
    explicit DataHubInspector(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void refresh();

    QTableWidget* table_ = nullptr;
    QTimer refresh_timer_;
};

} // namespace fincept::screens::devtools
