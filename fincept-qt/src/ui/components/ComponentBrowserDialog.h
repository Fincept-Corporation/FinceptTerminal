#pragma once
#include <QDialog>
#include <QEvent>
#include <QString>

class QLineEdit;
class QListWidget;
class QScrollArea;
class QGridLayout;
class QLabel;

namespace fincept::ui {

class ComponentCard;

/// Modal browser of registered screens. Cards sorted by popularity desc; search matches title+tags.
class ComponentBrowserDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ComponentBrowserDialog(QWidget* parent = nullptr);

  signals:
    /// Dialog closes itself after emitting; caller navigates.
    void component_chosen(const QString& screen_id);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void rebuild_cards();
    void on_search_changed(const QString& query);
    void on_category_changed(int row);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLineEdit* search_ = nullptr;
    QListWidget* category_list_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* grid_host_ = nullptr;
    QGridLayout* grid_layout_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* title_label_ = nullptr;

    // Empty string is the "All categories" sentinel. The category list's
    // first row carries an empty Qt::UserRole to mark it; all other rows carry
    // the real (non-translated) category id used for filtering.
    QString active_category_;
    QString search_query_;
    QString selected_id_;
};

} // namespace fincept::ui
