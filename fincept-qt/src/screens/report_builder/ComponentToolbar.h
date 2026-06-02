#pragma once
#include <QEvent>
#include <QFontComboBox>
#include <QHash>
#include <QLabel>
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
    void new_report_requested();
    void open_report_requested();
    void recent_reports_requested();
    void templates_requested();
    void theme_requested();
    void metadata_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    QListWidget* structure_list_ = nullptr;
    QFontComboBox* font_combo_ = nullptr;
    QSpinBox* font_size_ = nullptr;
    QPushButton* bold_btn_ = nullptr;
    QPushButton* italic_btn_ = nullptr;

    void add_type_button(QVBoxLayout* layout, const QString& type);
    void retranslateUi();

    // Section headers (cached for retranslateUi)
    QLabel* sec_file_ = nullptr;
    QLabel* sec_document_ = nullptr;
    QLabel* sec_add_ = nullptr;
    QLabel* sec_font_ = nullptr;
    QLabel* sec_structure_ = nullptr;

    // File / document action buttons
    QPushButton* new_btn_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QPushButton* recent_btn_ = nullptr;
    QPushButton* meta_btn_ = nullptr;
    QPushButton* template_btn_ = nullptr;
    QPushButton* theme_btn_ = nullptr;

    // Structure action buttons
    QPushButton* up_btn_ = nullptr;
    QPushButton* dn_btn_ = nullptr;
    QPushButton* dup_btn_ = nullptr;
    QPushButton* del_btn_ = nullptr;

    // Component "add" buttons, keyed by type id so retranslateUi can re-label.
    QHash<QString, QPushButton*> type_buttons_;
};

} // namespace fincept::screens
