#pragma once
#include "core/report/ReportDocument.h"

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

  private:
    QStackedWidget* stack_ = nullptr;
    QWidget* empty_widget_ = nullptr;
    QWidget* editor_widget_ = nullptr;
    int current_index_ = -1;

    void build_empty_page();
    void build_editor_page();

    QVBoxLayout* editor_layout_ = nullptr;
};

} // namespace fincept::screens
