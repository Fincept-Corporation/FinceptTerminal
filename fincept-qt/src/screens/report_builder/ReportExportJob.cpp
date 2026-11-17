// ReportExportJob.cpp — see ReportExportJob.h.

#include "screens/report_builder/ReportExportJob.h"

#include "core/logging/Logger.h"
#include "core/report/ReportExportAssets.h"
#include "core/report/ReportExportPlan.h"
#include "core/report/ReportMarkdownWriter.h"
#include "core/report/ReportOdtWriter.h"
#include "core/report/ReportVariables.h"
#include "screens/report_builder/ReportChartObject.h"
#include "services/report_builder/ReportAssets.h"
#include "services/report_builder/ReportChartCache.h"

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

namespace fincept::screens {

namespace {

constexpr int kRxSliceMs = 25;

// The <body> of a QTextDocument::toHtml() page, every <img src="file"> embedded as a
// data: URI so the page is self-contained. Runs on a worker (file reads).
QString rx_body_with_embedded_images(const QString& html) {
    int b0 = html.indexOf(QLatin1String("<body"));
    b0 = b0 < 0 ? 0 : html.indexOf(QLatin1Char('>'), b0) + 1;
    int b1 = html.lastIndexOf(QLatin1String("</body>"));
    if (b1 < 0)
        b1 = html.size();
    const QString body = html.mid(b0, b1 - b0);
    static const QRegularExpression img_re(QStringLiteral("<img src=\"([^\"]+)\""));
    QString out;
    int last = 0;
    auto it = img_re.globalMatch(body);
    while (it.hasNext()) {
        const auto m = it.next();
        out += body.mid(last, m.capturedStart() - last);
        const QString src = m.captured(1);
        const QString local = src.startsWith(QLatin1String("file:")) ? QUrl(src).toLocalFile() : src;
        QFile f(local);
        if (f.open(QIODevice::ReadOnly)) {
            const QString lower = local.toLower();
            const QString mime = lower.endsWith(QLatin1String(".jpg")) || lower.endsWith(QLatin1String(".jpeg"))
                                     ? QStringLiteral("image/jpeg")
                                 : lower.endsWith(QLatin1String(".svg")) ? QStringLiteral("image/svg+xml")
                                 : lower.endsWith(QLatin1String(".gif")) ? QStringLiteral("image/gif")
                                                                         : QStringLiteral("image/png");
            out += QStringLiteral("<img src=\"data:%1;base64,%2\"").arg(mime, QString::fromLatin1(f.readAll().toBase64()));
        } else {
            out += m.captured(0); // the browser shows a broken image rather than nothing
        }
        last = m.capturedEnd();
    }
    out += body.mid(last);
    return out;
}

QPageSize::PageSizeId rx_page_id(const QString& name) {
    if (name.compare(QLatin1String("Letter"), Qt::CaseInsensitive) == 0)
        return QPageSize::Letter;
    if (name.compare(QLatin1String("Legal"), Qt::CaseInsensitive) == 0)
        return QPageSize::Legal;
    return QPageSize::A4;
}

} // namespace

ReportExportJob::ReportExportJob(Format format, const QString& path, const report::ReportDocument& doc, QObject* parent)
    : QObject(parent), format_(format), path_(path), doc_(doc), cancel_flag_(std::make_shared<std::atomic<bool>>(false)) {
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    timer_->setInterval(0);
    connect(timer_, &QTimer::timeout, this, &ReportExportJob::tick);
}

QString ReportExportJob::extension(Format f) {
    switch (f) {
        case Format::Pdf:
            return QStringLiteral("pdf");
        case Format::Html:
            return QStringLiteral("html");
        case Format::Odt:
            return QStringLiteral("odt");
        case Format::Markdown:
            return QStringLiteral("md");
    }
    return QStringLiteral("pdf");
}

QString ReportExportJob::display_name(Format f) {
    switch (f) {
        case Format::Pdf:
            return QStringLiteral("PDF");
        case Format::Html:
            return tr("Web page");
        case Format::Odt:
            return tr("Word (.odt)");
        case Format::Markdown:
            return QStringLiteral("Markdown");
    }
    return QStringLiteral("PDF");
}

bool ReportExportJob::format_from_name(const QString& name, Format* out) {
    const QString n = name.trimmed().toLower();
    Format f;
    if (n == QLatin1String("pdf"))
        f = Format::Pdf;
    else if (n == QLatin1String("html") || n == QLatin1String("htm"))
        f = Format::Html;
    else if (n == QLatin1String("odt") || n == QLatin1String("word") || n == QLatin1String("docx"))
        f = Format::Odt;
    else if (n == QLatin1String("md") || n == QLatin1String("markdown"))
        f = Format::Markdown;
    else
        return false;
    if (out)
        *out = f;
    return true;
}

ReportExportJob* ReportExportJob::run(Format format, const QString& path, const report::ReportDocument& doc,
                                      QObject* parent, Done done, const Options& options) {
    auto* job = new ReportExportJob(format, path, doc, parent);
    job->set_options(options);
    connect(job, &ReportExportJob::finished, job,
            [job, done](bool ok, bool cancelled, const QString& out, const QString& error) {
                job->deleteLater();
                if (done)
                    done(ok, cancelled, out, error);
            });
    job->start();
    return job;
}

ReportExportJob::~ReportExportJob() {
    timer_->stop();
    if (painter_ && painter_->isActive()) {
        painter_->end();
        QFile::remove(path_ + QStringLiteral(".part"));
    }
    for (Section& s : sections_)
        delete s.doc;
}

void ReportExportJob::start() {
    if (running_ || done_)
        return;
    running_ = true;
    ctx_ = rb::make_context(doc_.components, doc_.metadata, doc_.theme, /*for_export=*/true);
    ctx_.number_style = rb::terminal_number_style();
    geom_ = rb::page_geometry(doc_.metadata);
    emit progress(tr("Preparing the report…"), 0);
    LOG_INFO("Research", logf("Report export started",
                                  {{"format", extension(format_)}, {"blocks", doc_.components.size()},
                                   {"path", log_path(path_)}}));
    switch (format_) {
        case Format::Pdf:
            // Charts go into the PDF as vectors, drawn by the same painter code as the page.
            chart_object_ = new ReportChartObject(ReportChartObject::Mode::Vector, this);
            ctx_.chart_object = chart_object_;
            new_section();
            phase_ = Phase::Build;
            timer_->start();
            return;
        case Format::Html:
            start_html_charts(); // pictures first (worker), then the sliced build
            return;
        case Format::Odt:
        case Format::Markdown:
            start_package_write();
            return;
    }
}

void ReportExportJob::cancel() {
    if (!running_)
        return;
    cancel_flag_->store(true);
    timer_->stop();
    if (painter_ && painter_->isActive())
        painter_->end();
    painter_.reset();
    writer_.reset();
    QFile::remove(path_ + QStringLiteral(".part"));
    running_ = false;
    done_ = true;
    emit finished(false, true, path_, tr("Export cancelled."));
}

ReportExportJob::Section& ReportExportJob::new_section() {
    Section s;
    s.doc = rb::new_section_document(ctx_, this);
    if (chart_object_)
        chart_object_->install(s.doc);
    QTextCursor c(s.doc);
    if (sections_.isEmpty())
        rb::write_header(c, ctx_);
    else
        rb::write_section_spacer(c);
    sections_.append(s);
    return sections_.last();
}

bool ReportExportJob::build_one() {
    while (build_index_ < doc_.components.size()) {
        const report::ReportComponent& c = doc_.components[build_index_++];
        if (c.type == QLatin1String("page_break")) {
            new_section();
            return true;
        }
        if (rb::is_unfilled(c))
            continue; // exports leave out blocks with nothing of their own
        Section& s = sections_.last();
        QTextCursor cur(s.doc);
        cur.movePosition(QTextCursor::End);
        const int start = cur.position();
        rb::write_component(cur, c, ctx_);
        s.ranges.append({c.id, start, cur.position()});
        return true;
    }
    return false;
}

bool ReportExportJob::layout_some() {
    // Asking for a block's rectangle makes Qt lay the document out up to that block, so
    // walking the blocks a slice at a time spreads the layout over several turns.
    while (layout_section_ < sections_.size()) {
        QTextDocument* d = sections_[layout_section_].doc;
        const QTextBlock b = d->findBlock(layout_block_pos_);
        if (!b.isValid() || layout_block_pos_ >= d->characterCount()) {
            sections_[layout_section_].pages = std::max(1, d->pageCount());
            ++layout_section_;
            layout_block_pos_ = 0;
            return true;
        }
        (void)d->documentLayout()->blockBoundingRect(b);
        layout_block_pos_ = b.position() + std::max(1, b.length());
        return true;
    }
    return false;
}

void ReportExportJob::fix_toc_pages() {
    bool has_toc = false;
    for (const auto& c : doc_.components)
        has_toc = has_toc || c.type == QLatin1String("toc");
    if (!has_toc)
        return;
    auto locate = [this](int id, int* s, int* ri) {
        for (int i = 0; i < sections_.size(); ++i)
            for (int k = 0; k < sections_[i].ranges.size(); ++k)
                if (sections_[i].ranges[k].id == id) {
                    *s = i;
                    *ri = k;
                    return true;
                }
        return false;
    };
    const double h = geom_.content.height();
    QHash<int, int> pages;
    for (int id : ctx_.heading_ids) {
        int s = 0, ri = 0;
        if (!locate(id, &s, &ri))
            continue;
        const QTextBlock b = sections_[s].doc->findBlock(sections_[s].ranges[ri].start + 1);
        int before = 0;
        for (int i = 0; i < s; ++i)
            before += sections_[i].pages;
        const double y = sections_[s].doc->documentLayout()->blockBoundingRect(b).top();
        pages.insert(id, before + static_cast<int>(std::floor(y / h)) + 1);
    }
    ctx_.heading_pages = pages;
    for (const auto& c : doc_.components) {
        int s = 0, ri = 0;
        if (c.type != QLatin1String("toc") || !locate(c.id, &s, &ri))
            continue;
        Range& r = sections_[s].ranges[ri];
        QTextCursor cur(sections_[s].doc);
        cur.setPosition(r.start);
        cur.setPosition(r.end, QTextCursor::KeepAnchor);
        cur.removeSelectedText();
        cur.setPosition(r.start);
        rb::write_component(cur, c, ctx_);
        const int delta = cur.position() - r.end;
        r.end = cur.position();
        for (int k = ri + 1; k < sections_[s].ranges.size(); ++k) {
            sections_[s].ranges[k].start += delta;
            sections_[s].ranges[k].end += delta;
        }
    }
    for (Section& s : sections_)
        s.pages = std::max(1, s.doc->pageCount());
}

bool ReportExportJob::write_one_page() {
    QVector<int> per;
    for (const Section& s : sections_)
        per << s.pages;
    const QVector<report::exportplan::PageRef> seq = report::exportplan::page_sequence(per);
    if (written_pages_ >= seq.size())
        return false;
    if (!painter_) {
        writer_ = std::make_unique<QPdfWriter>(path_ + QStringLiteral(".part"));
        writer_->setResolution(1200);
        writer_->setTitle(report::vars::display_title(doc_.metadata));
        writer_->setCreator(QStringLiteral("Fincept Terminal"));
        writer_->setPageLayout(QPageLayout(QPageSize(rx_page_id(doc_.metadata.page_size)),
                                           doc_.metadata.landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
                                           QMarginsF(0, 0, 0, 0)));
        painter_ = std::make_unique<QPainter>();
        if (!painter_->begin(writer_.get())) {
            finish(false, tr("Could not write to %1.").arg(path_));
            return false;
        }
    } else {
        writer_->newPage();
    }
    const auto& ref = seq[written_pages_];
    const double scale = writer_->resolution() / 96.0;
    painter_->save();
    painter_->scale(scale, scale);
    painter_->setRenderHint(QPainter::Antialiasing);
    painter_->setRenderHint(QPainter::SmoothPixmapTransform);
    painter_->fillRect(QRectF(QPointF(0, 0), geom_.page), QColor(doc_.theme.page_bg)); // full bleed paper
    rb::paint_page_content(painter_.get(), sections_[ref.section].doc, geom_, ref.page, doc_.theme);
    rb::paint_page_furniture(painter_.get(), ctx_, geom_, ref.number, total_pages_);
    painter_->restore();
    ++written_pages_;
    return written_pages_ < seq.size();
}

void ReportExportJob::tick() {
    if (!running_)
        return;
    QElapsedTimer t;
    t.start();
    const auto clock = [&t]() { return t.elapsed(); };
    namespace xp = report::exportplan;
    switch (phase_) {
    case Phase::Build: {
        if (xp::run_slice([this]() { return build_one(); }, kRxSliceMs, clock)) {
            phase_ = Phase::Layout;
            layout_section_ = 0;
            layout_block_pos_ = 0;
        }
        const int n = std::max(1, static_cast<int>(doc_.components.size()));
        emit progress(tr("Preparing block %1 of %2…").arg(std::min(build_index_, n)).arg(n),
                      xp::overall_percent(0, build_index_, n));
        break;
    }
    case Phase::Layout: {
        if (xp::run_slice([this]() { return layout_some(); }, kRxSliceMs, clock)) {
            fix_toc_pages();
            total_pages_ = 0;
            for (const Section& s : sections_)
                total_pages_ += s.pages;
            if (format_ == Format::Pdf) {
                phase_ = Phase::Write;
            } else {
                phase_ = Phase::Html;
                html_section_ = 0;
            }
        }
        emit progress(tr("Laying out pages…"),
                      xp::overall_percent(1, layout_section_, std::max(1, static_cast<int>(sections_.size()))));
        break;
    }
    case Phase::Write: {
        const bool more = !xp::run_slice([this]() { return write_one_page(); }, kRxSliceMs, clock);
        if (!running_)
            return; // finish() already ran (could not open the file)
        emit progress(tr("Writing page %1 of %2…").arg(written_pages_).arg(total_pages_),
                      xp::overall_percent(2, written_pages_, total_pages_));
        if (!more) {
            painter_->end();
            painter_.reset();
            writer_.reset();
            const QString part = path_ + QStringLiteral(".part");
            if (QFileInfo(part).size() <= 0) {
                finish(false, tr("The PDF could not be written to %1.").arg(path_));
                return;
            }
            QFile::remove(path_);
            if (!QFile::rename(part, path_)) {
                finish(false, tr("Could not replace %1 (is it open in another program?).").arg(path_));
                return;
            }
            finish(true, QString());
            return;
        }
        break;
    }
    case Phase::Html: {
        if (html_section_ < sections_.size()) {
            html_pages_ << sections_[html_section_++].doc->toHtml();
            emit progress(tr("Writing the web page…"),
                          xp::overall_percent(2, html_section_, static_cast<int>(sections_.size()) + 1));
            break;
        }
        start_html_write();
        return; // the worker finishes the job
    }
    case Phase::Charts:
    case Phase::Worker:
        return; // a worker is busy; it posts back
    case Phase::Done:
        return;
    }
    if (running_)
        timer_->start();
}

void ReportExportJob::start_html_write() {
    phase_ = Phase::Done;
    const report::ReportMetadata meta = doc_.metadata;
    const report::ReportTheme theme = doc_.theme;
    const QStringList pages = html_pages_;
    const QString path = path_;
    QString family = ctx_.font_family;
    family.remove(QLatin1Char('\'')).remove(QLatin1Char(';')).remove(QLatin1Char('}')).remove(QLatin1Char('<'));
    const QPointer<ReportExportJob> self(this);
    (void)QtConcurrent::run([self, meta, theme, pages, path, family]() {
        auto esc = [](const QString& s) { return s.toHtmlEscaped(); };
        auto strip = [](QString s) {
            s.remove(QStringLiteral("{page}"));
            s.remove(QStringLiteral("{total}"));
            return s.trimmed();
        };
        auto band = [&](const QString& l, const QString& c, const QString& r, bool footer) {
            if (l.isEmpty() && c.isEmpty() && r.isEmpty())
                return QString();
            return QStringLiteral("<div class=\"band%4\"><span>%1</span><span>%2</span><span>%3</span></div>")
                .arg(esc(l), esc(c), esc(r), footer ? QStringLiteral(" footer") : QString());
        };
        QStringList bodies;
        for (const QString& p : pages)
            bodies << rx_body_with_embedded_images(p);
        // Document content: report colours, a print typeface and point sizes of its own.
        QString html = QStringLiteral("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">");
        html += QStringLiteral("<title>%1</title>").arg(esc(meta.title));
        html += QStringLiteral("<style>body{background:%1;color:%2;max-width:860px;margin:24px auto;padding:0 24px;"
                               "font-family:'%3',Arial,sans-serif;}"
                               ".band{display:flex;justify-content:space-between;color:#888;font-size:9pt;"
                               "border-bottom:1px solid #ccc;margin:8px 0;}"
                               ".band.footer{border-top:1px solid #ccc;border-bottom:none;}"
                               ".page-break{page-break-before:always;break-before:page;height:0;}"
                               "img{max-width:100%;height:auto;}table{border-collapse:collapse;}</style></head><body>")
                    .arg(theme.page_bg, theme.text_color, family);
        html += band(strip(meta.header_left), strip(meta.header_center), strip(meta.header_right), false);
        html += bodies.join(QStringLiteral("<div class=\"page-break\"></div>"));
        html += band(strip(meta.footer_left), strip(meta.footer_center), strip(meta.footer_right), true);
        html += QStringLiteral("</body></html>\n");
        bool ok = false;
        QSaveFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const QByteArray bytes = html.toUtf8();
            ok = f.write(bytes) == bytes.size() && f.commit();
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ok, path]() {
                if (self)
                    self->finish(ok, ok ? QString() : ReportExportJob::tr("Could not write %1.").arg(path));
            },
            Qt::QueuedConnection);
    });
}

// ── HTML: chart pictures first ──────────────────────────────────────────────

namespace {

struct RxChartTask {
    int id = 0;
    report::chart::ChartSpec spec;
    QSize size;
    bool sparkline = false;
    QVector<double> values;
};

struct RxImageTask {
    int id = 0;
    QString path;
};

QVector<double> rx_numbers(const QString& csv) {
    QVector<double> out;
    for (const QString& t : csv.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const double v = t.trimmed().toDouble(&ok);
        if (ok && std::isfinite(v))
            out << v;
    }
    return out;
}

// Worker: a picture file's bytes as PNG / JPEG (other formats re-encoded as PNG).
report::exportfmt::ImageAsset rx_read_picture(const QString& path) {
    report::exportfmt::ImageAsset a;
    const QString suffix = QFileInfo(path).suffix().toLower();
    QImageReader reader(path);
    a.pixels = reader.size();
    if (suffix == QLatin1String("png") || suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            a.bytes = f.readAll();
        a.ext = suffix == QLatin1String("png") ? QStringLiteral("png") : QStringLiteral("jpg");
        return a;
    }
    const QImage img(path);
    if (img.isNull())
        return a;
    QBuffer buf(&a.bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    a.ext = QStringLiteral("png");
    a.pixels = img.size();
    return a;
}

bool rx_write_file(const QString& path, const QByteArray& bytes, QString* error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        *error = f.errorString();
        return false;
    }
    if (f.write(bytes) != bytes.size()) {
        *error = f.errorString();
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        *error = f.errorString();
        return false;
    }
    return true;
}

} // namespace

void ReportExportJob::start_html_charts() {
    phase_ = Phase::Charts;
    QVector<RxChartTask> tasks;
    for (const report::ReportComponent& c : doc_.components) {
        if (c.type != QLatin1String("chart") || rb::is_unfilled(c))
            continue;
        const report::ReportComponent e = report::vars::expanded_component(c, ctx_.vars);
        tasks.append({c.id, report::chart::from_config(e.config), rb::chart_size(c, ctx_), false, {}});
    }
    const report::chart::ChartPalette pal = ctx_.chart_palette;
    const QString family = ctx_.font_family;
    const auto flag = cancel_flag_;
    const QPointer<ReportExportJob> self(this);
    (void)QtConcurrent::run([self, tasks, pal, family, flag]() {
        QHash<int, QString> paths;
        for (int i = 0; i < tasks.size(); ++i) {
            if (flag->load())
                return;
            QString path;
            const QByteArray png = services::ReportChartCache::png_for(tasks[i].spec, pal, family, tasks[i].size, 2.0, &path);
            if (!png.isEmpty())
                paths.insert(tasks[i].id, path);
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [self, i, n = static_cast<int>(tasks.size())]() {
                    if (self && !self->done_)
                        emit self->progress(ReportExportJob::tr("Drawing chart %1 of %2…").arg(i + 1).arg(n),
                                            std::min(29, 30 * (i + 1) / std::max(1, n)));
                },
                Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, paths]() {
                if (!self || self->done_)
                    return;
                self->ctx_.chart_images = paths;
                self->new_section();
                self->phase_ = Phase::Build;
                self->timer_->start();
            },
            Qt::QueuedConnection);
    });
}

// ── .odt / Markdown: on a worker ────────────────────────────────────────────

void ReportExportJob::start_package_write() {
    phase_ = Phase::Worker;
    // GUI thread: the tables in the terminal's number style, and what the worker must draw / read.
    report::exportfmt::ExportAssets base;
    QVector<RxChartTask> charts;
    QVector<RxImageTask> images;
    for (const report::ReportComponent& c : doc_.components) {
        if (c.type == QLatin1String("table")) {
            base.tables.insert(c.id, rb::formatted_table(c, ctx_));
        } else if (c.type == QLatin1String("chart")) {
            if (rb::is_unfilled(c))
                continue;
            const report::ReportComponent e = report::vars::expanded_component(c, ctx_.vars);
            charts.append({c.id, report::chart::from_config(e.config), rb::chart_size(c, ctx_), false, {}});
        } else if (c.type == QLatin1String("sparkline")) {
            const QVector<double> v = rx_numbers(c.config.value(QStringLiteral("data")));
            if (v.size() >= 2)
                charts.append({c.id, {}, QSize(160, 40), true, v});
        } else if (services::report_assets::has_picture(c)) {
            QString path = services::report_assets::picture_path(c);
            if (path.isEmpty())
                path = services::report_assets::materialize_one(c);
            if (!path.isEmpty())
                images.append({c.id, path});
        }
    }
    const report::chart::ChartPalette pal = ctx_.chart_palette;
    const QString family = ctx_.font_family;
    const auto flag = cancel_flag_;
    const QPointer<ReportExportJob> self(this);
    const report::ReportDocument doc = doc_;
    const QString path = path_;
    const Format format = format_;
    const Options opt = options_;
    auto post_progress = [self](const QString& text, int percent) {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, text, percent]() {
                if (self && !self->done_)
                    emit self->progress(text, percent);
            },
            Qt::QueuedConnection);
    };
    (void)QtConcurrent::run([self, base, charts, images, pal, family, flag, doc, path, format, opt, post_progress]() {
        report::exportfmt::ExportAssets assets = base;
        constexpr double kPrintScale = 3.0; // ~288 dpi pictures
        for (int i = 0; i < charts.size(); ++i) {
            if (flag->load())
                return;
            const RxChartTask& t = charts[i];
            const QByteArray png = t.sparkline
                                       ? services::ReportChartCache::sparkline_png(t.values, pal, t.size, kPrintScale)
                                       : services::ReportChartCache::png_for(t.spec, pal, family, t.size, kPrintScale);
            if (!png.isEmpty()) {
                report::exportfmt::ImageAsset a;
                a.bytes = png;
                a.ext = QStringLiteral("png");
                a.pixels = t.size * kPrintScale;
                a.display_px = QSizeF(t.size);
                assets.charts.insert(t.id, a);
            }
            post_progress(ReportExportJob::tr("Drawing chart %1 of %2…").arg(i + 1).arg(charts.size()),
                          10 + 50 * (i + 1) / std::max(1, static_cast<int>(charts.size())));
        }
        for (const RxImageTask& t : images) {
            if (flag->load())
                return;
            report::exportfmt::ImageAsset a = rx_read_picture(t.path);
            if (!a.bytes.isEmpty())
                assets.images.insert(t.id, a);
        }
        post_progress(ReportExportJob::tr("Writing the document…"), 70);
        const std::function<bool()> cancelled = [flag]() { return flag->load(); };
        bool ok = false;
        QString error;
        if (format == Format::Odt) {
            report::odt::OdtOptions o;
            const QByteArray bytes = report::odt::write(doc, assets, o, cancelled, &error);
            if (flag->load())
                return;
            if (bytes.isEmpty()) {
                if (error.isEmpty())
                    error = ReportExportJob::tr("The document could not be built.");
            } else {
                ok = rx_write_file(path, bytes, &error);
            }
        } else {
            report::md::MarkdownOptions o;
            o.front_matter = opt.md_front_matter;
            o.images = opt.md_inline_images ? report::md::MarkdownOptions::Images::Inline
                                            : report::md::MarkdownOptions::Images::Folder;
            o.assets_dir = QFileInfo(path).completeBaseName() + QStringLiteral("_files");
            const report::md::MarkdownOutput out = report::md::write(doc, assets, o, cancelled);
            if (out.cancelled || flag->load())
                return;
            const QString dir = QFileInfo(path).absolutePath();
            ok = true;
            // Pictures first, so the .md never points at files that are not there yet.
            for (const auto& f : out.files) {
                if (!rx_write_file(dir + QLatin1Char('/') + f.first, f.second, &error)) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                ok = rx_write_file(path, out.markdown.toUtf8(), &error);
        }
        if (!ok && !error.isEmpty())
            error = ReportExportJob::tr("Could not write %1: %2").arg(path, error);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, ok, error]() {
                if (self && !self->done_)
                    self->finish(ok, error);
            },
            Qt::QueuedConnection);
    });
}

void ReportExportJob::finish(bool ok, const QString& error) {
    if (done_)
        return;
    if (ok)
        LOG_INFO("Research", logf("Report export finished", {{"format", extension(format_)}, {"path", log_path(path_)}}));
    timer_->stop();
    if (painter_ && painter_->isActive())
        painter_->end();
    painter_.reset();
    writer_.reset();
    if (!ok)
        QFile::remove(path_ + QStringLiteral(".part"));
    running_ = false;
    done_ = true;
    phase_ = Phase::Done;
    if (!ok)
        LOG_WARN("ReportExport", error);
    emit progress(ok ? tr("Done") : error, 100);
    emit finished(ok, false, path_, error);
}

} // namespace fincept::screens
