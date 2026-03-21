#pragma once
#include <QWidget>
#include <QSplitter>
#include <QUndoStack>
#include <QTimer>
#include "screens/report_builder/ComponentToolbar.h"
#include "screens/report_builder/DocumentCanvas.h"
#include "screens/report_builder/PropertiesPanel.h"

namespace fincept::screens {

struct ReportComponent {
    int     id = 0;
    QString type;      // "heading", "text", "table", "image", "chart", "code", "divider", "quote", "list"
    QString content;
    QMap<QString, QString> config;
};

struct ReportMetadata {
    QString title  = "Untitled Report";
    QString author = "Analyst";
    QString company= "Fincept Corporation";
    QString date;
};

/// Report Builder screen — 3-panel layout for document creation.
class ReportBuilderScreen : public QWidget {
    Q_OBJECT
public:
    explicit ReportBuilderScreen(QWidget* parent = nullptr);

    void add_component(const QString& type);
    void select_component(int index);
    void remove_component(int index);
    void update_component(int index, const QString& content, const QMap<QString, QString>& config);

signals:
    void back_to_dashboard();

private:
    QWidget* build_toolbar();

    ComponentToolbar* comp_toolbar_ = nullptr;
    DocumentCanvas*   canvas_       = nullptr;
    PropertiesPanel*  properties_   = nullptr;
    QSplitter*        splitter_     = nullptr;
    QUndoStack*       undo_stack_   = nullptr;
    QTimer*           autosave_     = nullptr;

    QVector<ReportComponent> components_;
    ReportMetadata           metadata_;
    int                      selected_    = -1;
    int                      next_id_     = 1;

    void refresh_canvas();
    void refresh_structure();

private slots:
    void on_save();
    void on_export_pdf();
    void on_preview();
};

} // namespace fincept::screens
