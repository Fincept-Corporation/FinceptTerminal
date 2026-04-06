#pragma once
#include "screens/report_builder/ComponentToolbar.h"
#include "screens/report_builder/DocumentCanvas.h"
#include "screens/report_builder/PropertiesPanel.h"

#include <QSplitter>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>
#include <QWidget>

namespace fincept::screens {

struct ReportComponent {
    int id = 0;
    // Types: heading, text, table, image, chart, sparkline, stats_block,
    //        callout, code, divider, quote, list, market_data, page_break, toc
    QString type;
    QString content;
    QMap<QString, QString> config;
};

struct ReportMetadata {
    QString title   = "Untitled Report";
    QString author  = "Analyst";
    QString company = "Fincept Corporation";
    QString date;
    QString header_left;
    QString header_center;
    QString header_right;
    QString footer_left;
    QString footer_center  = "Page {page}";
    QString footer_right;
    bool show_page_numbers = true;
};

// Report color theme
struct ReportTheme {
    QString name;
    QString heading_color;
    QString accent_color;
    QString page_bg;
    QString text_color;
    QString table_header_bg;
    QString table_header_fg;
    QString code_bg;
    QString quote_bg;
    QString meta_color;
    QString divider_color;
};

namespace report_themes {
inline ReportTheme light_professional() {
    return {"Light Professional", "#1a1a1a", "#d97706", "#ffffff",
            "#333333",            "#f0f0f0", "#1a1a1a", "#f5f5f5",
            "#f9f9f9",            "#666666", "#cccccc"};
}
inline ReportTheme dark_corporate() {
    return {"Dark Corporate",     "#e5e5e5", "#d97706", "#1e1e1e",
            "#cccccc",            "#2a2a2a", "#e5e5e5", "#111111",
            "#1a1a1a",            "#808080", "#444444"};
}
inline ReportTheme bloomberg() {
    return {"Bloomberg Terminal", "#ff8c00", "#ff8c00", "#0a0a0a",
            "#d4d4d4",            "#1a0a00", "#ff8c00", "#050505",
            "#0f0f0f",            "#888888", "#333300"};
}
inline ReportTheme midnight_blue() {
    return {"Midnight Blue",      "#e2e8f0", "#3b82f6", "#0f172a",
            "#cbd5e1",            "#1e3a5f", "#e2e8f0", "#0d1b2e",
            "#132033",            "#64748b", "#1e3a5f"};
}
} // namespace report_themes

// ── Undo Commands ──────────────────────────────────────────────────────────────

class ReportBuilderScreen;

class AddComponentCommand : public QUndoCommand {
  public:
    AddComponentCommand(ReportBuilderScreen* screen, const ReportComponent& comp, int insert_at);
    void undo() override;
    void redo() override;

  private:
    ReportBuilderScreen* screen_;
    ReportComponent comp_;
    int insert_at_;
};

class RemoveComponentCommand : public QUndoCommand {
  public:
    RemoveComponentCommand(ReportBuilderScreen* screen, int index);
    void undo() override;
    void redo() override;

  private:
    ReportBuilderScreen* screen_;
    ReportComponent saved_;
    int index_;
};

class UpdateComponentCommand : public QUndoCommand {
  public:
    UpdateComponentCommand(ReportBuilderScreen* screen, int index,
                           const QString& old_content, const QMap<QString, QString>& old_config,
                           const QString& new_content, const QMap<QString, QString>& new_config);
    void undo() override;
    void redo() override;
    int id() const override { return 1001; }
    bool mergeWith(const QUndoCommand* other) override;

  private:
    ReportBuilderScreen* screen_;
    int index_;
    QString old_content_, new_content_;
    QMap<QString, QString> old_config_, new_config_;
};

class MoveComponentCommand : public QUndoCommand {
  public:
    MoveComponentCommand(ReportBuilderScreen* screen, int from, int to);
    void undo() override;
    void redo() override;

  private:
    ReportBuilderScreen* screen_;
    int from_, to_;
};

// ── Main Screen ────────────────────────────────────────────────────────────────

class ReportBuilderScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ReportBuilderScreen(QWidget* parent = nullptr);

    // Called by undo commands — do NOT push to undo stack inside these
    void add_component_direct(const ReportComponent& comp, int at);
    void remove_component_direct(int index);
    void update_component_direct(int index, const QString& content, const QMap<QString, QString>& config);
    void swap_components(int a, int b);

    QVector<ReportComponent>& components() { return components_; }
    const QVector<ReportComponent>& components() const { return components_; }
    int& selected() { return selected_; }

    void refresh_canvas();
    void refresh_structure();

  private:
    QWidget* build_toolbar();
    void apply_template(const QString& name);
    void load_report(const QString& path);
    QString serialize_to_json() const;
    bool deserialize_from_json(const QString& json);
    void update_recent(const QString& path);
    QStringList load_recent() const;
    void show_recent_dialog();
    void show_template_dialog();
    void show_theme_dialog();
    void show_metadata_dialog();

    // Public-facing operations that push to undo stack
    void add_component(const QString& type);
    void select_component(int index);
    void remove_component(int index);
    void update_component(int index, const QString& content, const QMap<QString, QString>& config);

    ComponentToolbar* comp_toolbar_ = nullptr;
    DocumentCanvas*   canvas_       = nullptr;
    PropertiesPanel*  properties_   = nullptr;
    QSplitter*        splitter_     = nullptr;
    QUndoStack*       undo_stack_   = nullptr;
    QTimer*           autosave_     = nullptr;

    QVector<ReportComponent> components_;
    ReportMetadata           metadata_;
    ReportTheme              theme_     = report_themes::light_professional();
    int                      selected_  = -1;
    int                      next_id_   = 1;
    QString                  current_file_;
    QString                  autosave_path_;

    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_save();
    void on_open();
    void on_new();
    void on_auto_save();
    void on_export_pdf();
    void on_preview();
};

} // namespace fincept::screens
