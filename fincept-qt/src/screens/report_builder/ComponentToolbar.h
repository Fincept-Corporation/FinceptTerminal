#pragma once
#include <QFontComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Left panel — component type buttons, font settings, document structure.
class ComponentToolbar : public QWidget {
    Q_OBJECT
  public:
    explicit ComponentToolbar(QWidget* parent = nullptr);

    void update_structure(const QStringList& items, int selected_index);

  signals:
    void add_component(const QString& type);
    void structure_selected(int index);
    void move_up(int index);
    void move_down(int index);
    void duplicate(int index);
    void delete_item(int index);
    void font_changed(const QString& family, int size, bool bold, bool italic);

  private:
    QListWidget* structure_list_ = nullptr;
    QFontComboBox* font_combo_ = nullptr;
    QSpinBox* font_size_ = nullptr;
    QPushButton* bold_btn_ = nullptr;
    QPushButton* italic_btn_ = nullptr;

    void add_type_button(QVBoxLayout* layout, const QString& label, const QString& type);
};

} // namespace fincept::screens
