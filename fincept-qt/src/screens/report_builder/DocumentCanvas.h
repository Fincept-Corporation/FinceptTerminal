#pragma once
#include <QScrollArea>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

struct ReportComponent;
struct ReportMetadata;
struct ReportTheme;

/// Center panel — A4 document canvas rendering components as rich text.
/// Supports drag-and-drop of image files directly onto the canvas.
class DocumentCanvas : public QWidget {
    Q_OBJECT
  public:
    explicit DocumentCanvas(QWidget* parent = nullptr);

    void render(const QVector<ReportComponent>& components, const ReportMetadata& metadata,
                const ReportTheme& theme, int selected_index);

    QTextEdit* text_edit() const { return editor_; }

  signals:
    /// Emitted when an image file is dropped onto the canvas.
    void image_dropped(const QString& file_path);

  protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

  private:
    QTextEdit* editor_ = nullptr;
};

} // namespace fincept::screens
