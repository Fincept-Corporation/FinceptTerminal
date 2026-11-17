#pragma once
#include "screens/dashboard/canvas/DashboardTemplates.h"

#include <QDialog>
#include <QString>
#include <QVector>

class QLabel;
class QListWidget;
class QPushButton;

namespace fincept::screens {

class TemplatePreview;

/// Dashboard template chooser (toolbar RESET). A list of every template — name,
/// one-line purpose, a small schematic of its grid, the current one marked —
/// beside the selected template's large labelled schematic, its tiles with their
/// sizes, what it needs and what applying it does to the current layout.
/// Emits template_selected(id) on APPLY / double-click; the screen decides
/// whether the current layout must be saved first (it is never discarded
/// silently). Non-modal-blocking: open it with open().
class TemplatePicker : public QDialog {
    Q_OBJECT
  public:
    /// `current_id`: template the dashboard shows now ("" = none);
    /// `current_is_pristine`: the dashboard is exactly that template / a saved
    /// layout (nothing to lose by replacing it).
    TemplatePicker(const QString& current_id, bool current_is_pristine, QWidget* parent = nullptr);

  signals:
    void template_selected(const QString& template_id);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_list();
    void show_details(int row);
    void apply_styles();
    void retranslateUi();
    void accept_current();

    QString current_id_;
    bool current_pristine_ = false;
    QVector<DashboardTemplate> templates_;

    QLabel* title_lbl_ = nullptr;
    QLabel* sub_lbl_ = nullptr;
    QListWidget* list_ = nullptr;
    TemplatePreview* preview_ = nullptr;
    QLabel* name_lbl_ = nullptr;
    QLabel* desc_lbl_ = nullptr;
    QLabel* meta_lbl_ = nullptr;
    QLabel* tiles_lbl_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
};

} // namespace fincept::screens
