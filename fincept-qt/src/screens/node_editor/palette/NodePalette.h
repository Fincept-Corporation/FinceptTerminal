#pragma once

#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::workflow {

/// Left panel showing categorized node types that can be dragged onto the canvas.
class NodePalette : public QWidget {
    Q_OBJECT
  public:
    explicit NodePalette(QWidget* parent = nullptr);

  signals:
    void node_type_selected(const QString& type_id);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void rebuild_categories(const QString& filter = {});
    void start_drag(const QString& type_id);
    void retranslateUi();

    QLabel* header_title_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QVBoxLayout* categories_layout_ = nullptr;
    QWidget* categories_container_ = nullptr;
};

} // namespace fincept::workflow
