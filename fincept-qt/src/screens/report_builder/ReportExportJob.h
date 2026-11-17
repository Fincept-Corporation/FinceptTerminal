#pragma once
// ReportExportJob — PDF / HTML / Word-compatible (.odt) / Markdown export that never
// freezes the window.
//
// Works on a snapshot of the report taken when the export starts (edits made meanwhile
// do not leak into the file), reports progress, and cancel() stops it and removes the
// partial file. Every file is written to a temporary name and moved over <path> only when
// complete, so a failed or cancelled export never leaves a truncated file — nor destroys
// the previous one.
//   PDF  — QTextDocument layout and painting stay on the GUI thread, so the job runs in
//          SLICES across event-loop turns (core/report/ReportExportPlan.h); charts are drawn
//          as vectors straight into the PDF (ReportChartObject, Vector mode). Pagination is
//          the page view's (same renderer, same 96-dpi layout), scaled to the printer.
//   HTML — chart pictures are rendered on a worker first, then sliced like the PDF.
//   .odt / Markdown — tables are formatted on the GUI thread (the terminal's number style);
//          everything else (chart pictures at print resolution, reading images, writing the
//          ODF package / Markdown and its picture folder) runs on a worker
//          (core/report/ReportOdtWriter.h, ReportMarkdownWriter.h).

#include "core/report/ReportDocument.h"
#include "screens/report_builder/ReportRenderer.h"

#include <QObject>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

class QPainter;
class QPdfWriter;
class QTextDocument;
class QTimer;

namespace fincept::screens {

class ReportChartObject;

class ReportExportJob : public QObject {
    Q_OBJECT
  public:
    enum class Format { Pdf, Html, Odt, Markdown };
    struct Options {
        bool md_front_matter = true;   ///< Markdown: YAML front matter (title, author, date)
        bool md_inline_images = false; ///< Markdown: pictures as data: URIs instead of a folder
    };

    ReportExportJob(Format format, const QString& path, const report::ReportDocument& doc, QObject* parent = nullptr);
    ~ReportExportJob() override;

    void set_options(const Options& o) { options_ = o; }
    void start();
    void cancel();
    bool running() const { return running_; }
    Format format() const { return format_; }
    QString path() const { return path_; }

    /// "pdf" | "html" | "odt" | "md"
    static QString extension(Format f);
    static bool format_from_name(const QString& name, Format* out);
    /// "PDF", "Web page", "Word (.odt)", "Markdown"
    static QString display_name(Format f);

    using Done = std::function<void(bool ok, bool cancelled, const QString& path, const QString& error)>;
    /// Create, start and self-delete a job; `done` runs once on the GUI thread.
    static ReportExportJob* run(Format format, const QString& path, const report::ReportDocument& doc,
                                QObject* parent, Done done, const Options& options);
    /// The same with default options. An overload, not `= {}`: GCC and clang reject a default
    /// argument that needs Options' member initializers inside the enclosing class.
    static ReportExportJob* run(Format format, const QString& path, const report::ReportDocument& doc,
                                QObject* parent, Done done) {
        return run(format, path, doc, parent, std::move(done), Options());
    }

  signals:
    /// `text` says what is happening ("Writing page 4 of 22"); `percent` 0..100.
    void progress(const QString& text, int percent);
    /// Exactly once. `cancelled` = stopped by cancel() (no error to show).
    void finished(bool ok, bool cancelled, const QString& path, const QString& error);

  private:
    enum class Phase { Charts, Build, Layout, Write, Html, Worker, Done };
    struct Range {
        int id = 0;
        int start = 0;
        int end = 0;
    };
    struct Section {
        QTextDocument* doc = nullptr;
        QVector<Range> ranges;
        int pages = 1;
    };

    void tick();
    bool build_one();
    bool layout_some();
    bool write_one_page();
    void fix_toc_pages();
    void start_html_write();
    void start_html_charts();
    void start_package_write();
    void finish(bool ok, const QString& error);
    Section& new_section();

    Format format_;
    QString path_;
    report::ReportDocument doc_;
    Options options_;
    rb::RenderContext ctx_;
    rb::PageGeometry geom_;
    QVector<Section> sections_;
    QTimer* timer_ = nullptr;
    ReportChartObject* chart_object_ = nullptr;
    Phase phase_ = Phase::Build;
    bool running_ = false;
    bool done_ = false;
    int build_index_ = 0;
    int layout_section_ = 0;
    int layout_block_pos_ = 0;
    int written_pages_ = 0;
    int total_pages_ = 0;
    int html_section_ = 0;
    QStringList html_pages_;
    std::unique_ptr<QPdfWriter> writer_;
    std::unique_ptr<QPainter> painter_;
    std::shared_ptr<std::atomic<bool>> cancel_flag_;
};

} // namespace fincept::screens
