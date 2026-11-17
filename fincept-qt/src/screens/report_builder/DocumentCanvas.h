#pragma once
// DocumentCanvas — the Report Builder's page view.
//
// Shows the report as real pages of the chosen paper (A4 / Letter / Legal, portrait or
// landscape, the report's margins, running header / footer with page numbers), laid out
// exactly like the PDF export (ReportRenderer). One QTextDocument per section (between
// page_break blocks); every component's character range is remembered, so:
//   * an edit re-renders only that component in place (Qt re-lays its section from there);
//   * a click on the page selects the component under the pointer (double-click focuses
//     its editor);
//   * the selection is an overlay, never baked into the document (nothing re-lays on select);
//   * the scroll position stays where it is — the view widget is never rebuilt.
// A full build (open, template, page setup) runs in slices across event-loop turns, so a
// long report never freezes the window while it is laid out.

#include "core/report/ReportDocument.h"
#include "screens/report_builder/ReportRenderer.h"

#include <QHash>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QScrollArea;
class QTextDocument;
class QTimer;

namespace fincept::screens {

using ReportComponent = ::fincept::report::ReportComponent;
using ReportMetadata = ::fincept::report::ReportMetadata;
using ReportTheme = ::fincept::report::ReportTheme;

class RbPageView;
class ReportChartObject;

class DocumentCanvas : public QWidget {
    Q_OBJECT
  public:
    explicit DocumentCanvas(QWidget* parent = nullptr);
    ~DocumentCanvas() override;

    /// Show this document. Compares with what is drawn and re-lays only what changed
    /// (metadata / theme / page breaks moved = full rebuild, in slices).
    void show_document(const QVector<ReportComponent>& components, const ReportMetadata& metadata,
                       const ReportTheme& theme);
    /// Re-render one component in place (its refresh state changed).
    void refresh_component(int id);
    /// Screen-only notes under data blocks (refreshing / failed).
    void set_note_provider(std::function<rb::BlockNote(int)> provider);

    void set_selected_id(int id, bool scroll_into_view);
    int selected_id() const { return selected_id_; }
    void scroll_to_component(int id);

    /// Print view: what an export prints (no editor hints, no selection, no notes).
    void set_preview_mode(bool on);
    bool preview_mode() const { return preview_; }

    int page_count() const;
    bool is_building() const { return building_; }

    /// True when `c` has no content yet (exports leave it out).
    static bool is_unfilled(const ReportComponent& c) { return rb::is_unfilled(c); }

  signals:
    void component_clicked(int id);
    void component_activated(int id);
    void background_clicked();
    void image_dropped(const QString& file_path);
    void page_count_changed(int pages);

  protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    friend class RbPageView;

    struct Range {
        int id = 0;
        int start = 0; ///< position before the component's first block separator
        int end = 0;   ///< end of the component's last block
    };
    struct Section {
        QTextDocument* doc = nullptr;
        QVector<Range> ranges;
        int pages = 1;
        int lead_end = 0; ///< end of the cover header (section 0) / spacer (others)
    };

    void start_full_build();
    void build_step();
    void finish_build();
    void clear_sections();
    Section* new_section();
    bool apply_incremental(const QVector<ReportComponent>& comps);
    void replace_range(int section, int range_index, const ReportComponent& c);
    void insert_component(int section, int range_index, const ReportComponent& c);
    void remove_range(int section, int range_index);
    void shift_ranges(int section, int from_range, int delta);
    bool locate(int id, int* section, int* range_index) const;
    void update_pages(bool notify);
    void update_toc_pages();
    void rebuild_context();
    QRectF component_rect(int section, int range_index) const; ///< document coordinates
    void refit_zoom();
    void set_zoom(double z, bool fit);
    void update_status();
    int hit_component(const QPointF& view_pos, bool* on_page) const;
    QRectF page_rect(int global_page) const; ///< view coordinates
    int global_page_of(int section, int page) const;

    QScrollArea* scroll_ = nullptr;
    RbPageView* view_ = nullptr;
    QWidget* status_bar_ = nullptr;
    QLabel* status_label_ = nullptr;
    QPushButton* zoom_out_btn_ = nullptr;
    QPushButton* zoom_fit_btn_ = nullptr;
    QPushButton* zoom_in_btn_ = nullptr;
    QTimer* build_timer_ = nullptr;
    /// Charts on the pages: fixed-size objects whose images arrive off the GUI thread.
    ReportChartObject* chart_object_ = nullptr;

    QVector<Section> sections_;
    QVector<ReportComponent> drawn_;   ///< what the documents show
    ReportMetadata meta_;
    ReportTheme theme_;
    rb::RenderContext ctx_;
    rb::PageGeometry geom_;
    std::function<rb::BlockNote(int)> note_provider_;
    int build_index_ = 0;
    bool building_ = false;
    bool have_document_ = false;
    bool preview_ = false;
    int selected_id_ = 0;
    double zoom_ = 1.0;
    bool fit_ = true;
    int last_pages_ = -1;
};

} // namespace fincept::screens
