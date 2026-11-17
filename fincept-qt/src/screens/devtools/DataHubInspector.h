#pragma once

#include <QEvent>
#include <QTimer>
#include <QWidget>

class QLineEdit;
class QStackedWidget;
class QTableWidget;

namespace fincept::ui {
class EmptyState;
}

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
    void changeEvent(QEvent* event) override;

  private:
    void refresh();
    void retranslateUi();
    /// Table, or a compact state when no row is visible: the hub has no topic yet
    /// (`total` 0) or the filter matches none. Re-applied only when that changes.
    void sync_empty_state(int total, int visible, const QString& needle);

    QLineEdit* filter_edit_ = nullptr; // substring filter over the topic column
    QStackedWidget* stack_ = nullptr;  // table | empty state
    QTableWidget* table_ = nullptr;
    fincept::ui::EmptyState* empty_state_ = nullptr; // built on first need
    int empty_kind_ = -1;                            // -1 table shown, 0 no topics, 1 no match
    QString empty_needle_;
    QTimer refresh_timer_;
    bool initial_sized_ = false;
};

} // namespace fincept::screens::devtools
