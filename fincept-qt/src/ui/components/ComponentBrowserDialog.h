#pragma once
#include <QDialog>
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

  private:
    void build_ui();
    void rebuild_cards();
    void on_search_changed(const QString& query);
    void on_category_changed(int row);

    QLineEdit* search_ = nullptr;
    QListWidget* category_list_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* grid_host_ = nullptr;
    QGridLayout* grid_layout_ = nullptr;
    QLabel* count_label_ = nullptr;

    QString active_category_ = "All";
    QString search_query_;
    QString selected_id_;
};

} // namespace fincept::ui
