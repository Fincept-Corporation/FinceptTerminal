#pragma once
#include "core/report/ReportDocument.h"

#include <QEvent>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

// ReportComponent comes from core/report/ReportDocument.h. PropertiesPanel
// historically used the screens-namespace forward declaration; with the
// model migrated to fincept::report we refer to the concrete type directly.
using ReportComponent = ::fincept::report::ReportComponent;

/// Right panel — context-sensitive property editor for selected component.
class PropertiesPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void show_properties(const ReportComponent* component, int index);
    void show_empty();

  signals:
    void content_changed(int index, const QString& content);
    void config_changed(int index, const QString& key, const QString& value);
    void duplicate_requested(int index);
    void delete_requested(int index);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    QStackedWidget* stack_ = nullptr;
    QWidget* empty_widget_ = nullptr;
    QWidget* editor_widget_ = nullptr;
    QLabel* empty_label_ = nullptr; // cached for retranslateUi
    int current_index_ = -1;

    void build_empty_page();
    void build_editor_page();
    void retranslateUi();

    QVBoxLayout* editor_layout_ = nullptr;

    // Last shown component — cached so retranslateUi can rebuild the editor
    // (which is recreated wholesale on every selection) in the new language.
    ReportComponent last_component_;
    bool has_component_ = false;
};

} // namespace fincept::screens
