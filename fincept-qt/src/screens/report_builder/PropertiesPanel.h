#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QStackedWidget>

namespace fincept::screens {

struct ReportComponent;

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
