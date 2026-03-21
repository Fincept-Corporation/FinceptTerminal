#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QScrollArea>

namespace fincept::screens {

struct ReportComponent;
struct ReportMetadata;

/// Center panel — A4 document canvas rendering components as rich text.
class DocumentCanvas : public QWidget {
    Q_OBJECT
public:
    explicit DocumentCanvas(QWidget* parent = nullptr);

    void render(const QVector<ReportComponent>& components,
                const ReportMetadata& metadata,
                int selected_index);

    QTextEdit* text_edit() const { return editor_; }

private:
    QTextEdit* editor_ = nullptr;
};

} // namespace fincept::screens
