#pragma once

#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QVBoxLayout>
#include <QWidget>

class QShowEvent;

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
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    void rebuild_categories(const QString& filter = {});
    void start_drag(const QString& type_id);
    void retranslateUi();

    QLabel* header_title_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QVBoxLayout* categories_layout_ = nullptr;
    QWidget* categories_container_ = nullptr;
    /// The appearance changed while the palette was hidden: rebuild the rows on the next show.
    bool rebuild_pending_ = false;
};

} // namespace fincept::workflow
