#pragma once
// ComponentToolbar — the Report Builder's left panel: report actions, add-block buttons,
// the report typeface, and the structure list (model/view, addressed by stable id).

#include <QEvent>
#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QFontComboBox;
class QLabel;
class QListView;
class QPushButton;
class QSpinBox;
class QStandardItemModel;
class QVBoxLayout;

namespace fincept::screens {

class ComponentToolbar : public QWidget {
    Q_OBJECT
  public:
    explicit ComponentToolbar(QWidget* parent = nullptr);

    struct StructureItem {
        int id = 0;
        QString label;
        bool bound = false;  ///< refreshable data block
        bool stale = false;
        bool unseen = false; ///< added by another tab, not looked at yet
    };
    void update_structure(const QVector<StructureItem>& items, int selected_id);

    /// Show the report's typeface / size (ReportMetadata) without emitting font_changed.
    void set_document_font(const QString& family, int size);
    /// Show the selected component's bold / italic without emitting style_changed;
    /// `enabled` = false when nothing is selected or the component has no text.
    void set_component_style(bool enabled, bool bold, bool italic);
    /// Width that fits the panel at the current UI font.
    int preferred_width() const;

  signals:
    void add_component(const QString& type);
    void structure_selected(int id);
    void move_up(int id);
    void move_down(int id);
    void duplicate(int id);
    void delete_item(int id);
    /// The report's typeface / body size changed (a document setting).
    void font_changed(const QString& family, int size);
    /// B / I toggled — applies to the selected component.
    void style_changed(bool bold, bool italic);
    void new_report_requested();
    void open_report_requested();
    void library_requested();
    void recent_reports_requested();
    void save_as_template_requested();
    void templates_requested();
    void theme_requested();
    void metadata_requested();
    void page_setup_requested();
    void variables_requested();
    void schedule_requested();
    /// "summary" (executive summary block) | "title" (suggest a title) — opens the AI panel.
    void ai_document_requested(const QString& action);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    QPushButton* make_action(QVBoxLayout* vl, const QString& text);
    QLabel* make_section(QVBoxLayout* vl, const QString& text);
    void add_type_button(QVBoxLayout* layout, const QString& type);
    int selected_structure_id() const;
    void retranslateUi();

    QListView* structure_view_ = nullptr;
    QStandardItemModel* structure_model_ = nullptr;
    QFontComboBox* font_combo_ = nullptr;
    QSpinBox* font_size_ = nullptr;
    QPushButton* bold_btn_ = nullptr;
    QPushButton* italic_btn_ = nullptr;

    QLabel* sec_file_ = nullptr;
    QLabel* sec_document_ = nullptr;
    QLabel* sec_add_ = nullptr;
    QLabel* sec_font_ = nullptr;
    QLabel* sec_structure_ = nullptr;

    QPushButton* new_btn_ = nullptr;
    QPushButton* library_btn_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QPushButton* recent_btn_ = nullptr;
    QPushButton* meta_btn_ = nullptr;
    QPushButton* page_btn_ = nullptr;
    QPushButton* template_btn_ = nullptr;
    QPushButton* save_tpl_btn_ = nullptr;
    QPushButton* theme_btn_ = nullptr;
    QPushButton* vars_btn_ = nullptr;
    QPushButton* schedule_btn_ = nullptr;
    QPushButton* ai_summary_btn_ = nullptr;
    QPushButton* ai_title_btn_ = nullptr;
    QLabel* sec_ai_ = nullptr;

    QPushButton* up_btn_ = nullptr;
    QPushButton* dn_btn_ = nullptr;
    QPushButton* dup_btn_ = nullptr;
    QPushButton* del_btn_ = nullptr;

    QHash<QString, QPushButton*> type_buttons_;
};

} // namespace fincept::screens
