// DocumentCanvas.cpp — see DocumentCanvas.h.
//
// Editor chrome (the desk around the pages, the selection overlay, the status strip)
// takes terminal tokens; everything ON a page is document content drawn by ReportRenderer.

#include "screens/report_builder/DocumentCanvas.h"

#include "core/report/ReportLayoutPlan.h"
#include "screens/report_builder/ReportChartObject.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractTextDocumentLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {
constexpr int kDcMargin = 24;      // desk margin around the pages (view px)
constexpr int kDcGap = 20;         // gap between pages
constexpr int kDcBuildSliceMs = 12; // layout work per event-loop turn during a full build
} // namespace

// ── The painted page strip ──────────────────────────────────────────────────

class RbPageView : public QWidget {
  public:
    RbPageView(DocumentCanvas* canvas, QWidget* parent) : QWidget(parent), canvas_(canvas) {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setFocusPolicy(Qt::ClickFocus);
    }

    void relayout() {
        const int pages = std::max(1, canvas_->page_count());
        const double pw = canvas_->geom_.page.width() * canvas_->zoom_;
        const double ph = canvas_->geom_.page.height() * canvas_->zoom_;
        const int w = static_cast<int>(std::ceil(pw)) + 2 * kDcMargin;
        const int h = static_cast<int>(std::ceil(pages * (ph + kDcGap) - kDcGap)) + 2 * kDcMargin;
        if (minimumSize() != QSize(w, h))
            setMinimumSize(w, h);
        update();
    }

  protected:
    void paintEvent(QPaintEvent* e) override {
        QPainter p(this);
        p.fillRect(e->rect(), QColor(ui::colors::BORDER_DIM()));
        const auto& c = *canvas_;
        if (c.sections_.isEmpty())
            return;
        const int total = c.page_count();
        int global = 0;
        for (int s = 0; s < c.sections_.size(); ++s) {
            const auto& sec = c.sections_[s];
            for (int pg = 0; pg < sec.pages; ++pg, ++global) {
                const QRectF pr = c.page_rect(global);
                if (!pr.toAlignedRect().intersects(e->rect()))
                    continue;
                p.save();
                p.translate(pr.topLeft());
                p.scale(c.zoom_, c.zoom_);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::TextAntialiasing);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                p.fillRect(QRectF(QPointF(0, 0), c.geom_.page), QColor(c.theme_.page_bg));
                rb::paint_page_content(&p, sec.doc, c.geom_, pg, c.theme_);
                rb::paint_page_furniture(&p, c.ctx_, c.geom_, global + 1, total);
                if (global == 0 && c.drawn_.isEmpty() && !c.preview_ && !c.building_) {
                    // A blank report says what to do next (screen only, never exported).
                    QFont f;
                    f.setPixelSize(15);
                    p.setFont(f);
                    p.setPen(QColor(c.theme_.meta_color));
                    p.drawText(c.geom_.content.adjusted(0, c.geom_.content.height() * 0.3, 0, 0),
                               Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                               DocumentCanvas::tr("This report is empty.\nAdd blocks from the left panel, or open the "
                                                  "Library to start from a template — the live-data templates fill "
                                                  "themselves."));
                }
                p.restore();
                p.setPen(QPen(QColor(ui::colors::BORDER_MED()), 1));
                p.setBrush(Qt::NoBrush);
                p.drawRect(pr.adjusted(-0.5, -0.5, 0.5, 0.5));
            }
        }
        // Selection overlay (editor chrome, never printed).
        int ss = -1, ri = -1;
        if (!c.preview_ && c.selected_id_ > 0 && c.locate(c.selected_id_, &ss, &ri)) {
            const QRectF r = c.component_rect(ss, ri);
            if (r.isValid()) {
                const double h = c.geom_.content.height();
                const int first = std::max(0, static_cast<int>(std::floor(r.top() / h)));
                const int last = std::max(first, static_cast<int>(std::floor((r.bottom() - 0.5) / h)));
                const QColor accent(ui::colors::AMBER());
                QColor wash = accent;
                wash.setAlphaF(0.07f);
                for (int pg = first; pg <= last && pg < c.sections_[ss].pages; ++pg) {
                    const QRectF slice = r.intersected(QRectF(-20, pg * h, c.geom_.content.width() + 40, h));
                    if (slice.isEmpty())
                        continue;
                    const QRectF pr = c.page_rect(c.global_page_of(ss, pg));
                    const QRectF vr((c.geom_.content.left() + slice.left() - 4) * c.zoom_ + pr.left(),
                                    (c.geom_.content.top() + slice.top() - pg * h - 2) * c.zoom_ + pr.top(),
                                    (slice.width() + 8) * c.zoom_, (slice.height() + 4) * c.zoom_);
                    p.fillRect(vr, wash);
                    p.fillRect(QRectF(vr.left() - 4, vr.top(), 3, vr.height()), accent);
                }
            }
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton)
            return QWidget::mousePressEvent(e);
        bool on_page = false;
        const int id = canvas_->hit_component(e->position(), &on_page);
        if (id > 0)
            emit canvas_->component_clicked(id);
        else
            emit canvas_->background_clicked();
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override {
        bool on_page = false;
        const int id = canvas_->hit_component(e->position(), &on_page);
        if (id > 0)
            emit canvas_->component_activated(id);
    }

    void wheelEvent(QWheelEvent* e) override {
        if (e->modifiers() & Qt::ControlModifier) {
            const double step = e->angleDelta().y() > 0 ? 1.1 : 1 / 1.1;
            canvas_->set_zoom(canvas_->zoom_ * step, false);
            e->accept();
            return;
        }
        QWidget::wheelEvent(e);
    }

  private:
    DocumentCanvas* canvas_;
};

// ── Canvas ──────────────────────────────────────────────────────────────────

DocumentCanvas::DocumentCanvas(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    // Stylesheets go on before the children exist (one selector per widget).
    setObjectName(QStringLiteral("rbCanvas"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("rbCanvasScroll"));
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setStyleSheet(QStringLiteral("#rbCanvasScroll { background: %1; border: none; }").arg(ui::colors::BORDER_DIM()));
    view_ = new RbPageView(this, scroll_);
    scroll_->setWidget(view_);
    root->addWidget(scroll_, 1);

    status_bar_ = new QWidget(this);
    status_bar_->setObjectName(QStringLiteral("rbCanvasStatus"));
    status_bar_->setStyleSheet(QStringLiteral("#rbCanvasStatus { background: %1; border-top: 1px solid %2; }"
                                              "#rbCanvasStatus QLabel { color: %3; background: transparent; font-size: %4px; }"
                                              "#rbCanvasStatus QPushButton { background: transparent; color: %3; border: 1px solid %2;"
                                              " padding: 0 6px; font-size: %4px; }"
                                              "#rbCanvasStatus QPushButton:hover { color: %5; border-color: %5; }")
                                   .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                                        ui::fonts::px(-2), ui::colors::AMBER()));
    auto* sl = new QHBoxLayout(status_bar_);
    sl->setContentsMargins(8, 2, 8, 2);
    sl->setSpacing(4);
    status_label_ = new QLabel(status_bar_);
    sl->addWidget(status_label_, 1);
    zoom_out_btn_ = new QPushButton(QStringLiteral("−"), status_bar_);
    zoom_fit_btn_ = new QPushButton(tr("Fit width"), status_bar_);
    zoom_in_btn_ = new QPushButton(QStringLiteral("+"), status_bar_);
    zoom_out_btn_->setToolTip(tr("Zoom out  (Ctrl+wheel)"));
    zoom_in_btn_->setToolTip(tr("Zoom in  (Ctrl+wheel)"));
    zoom_fit_btn_->setToolTip(tr("Fit the page to the width of the view"));
    for (QPushButton* b : {zoom_out_btn_, zoom_fit_btn_, zoom_in_btn_}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        sl->addWidget(b);
    }
    root->addWidget(status_bar_);
    connect(zoom_out_btn_, &QPushButton::clicked, this, [this]() { set_zoom(zoom_ / 1.15, false); });
    connect(zoom_in_btn_, &QPushButton::clicked, this, [this]() { set_zoom(zoom_ * 1.15, false); });
    connect(zoom_fit_btn_, &QPushButton::clicked, this, [this]() {
        fit_ = true;
        refit_zoom();
    });
    connect(scroll_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { update_status(); });

    build_timer_ = new QTimer(this);
    build_timer_->setSingleShot(true);
    build_timer_->setInterval(0);
    connect(build_timer_, &QTimer::timeout, this, &DocumentCanvas::build_step);

    // A chart image drawn off the GUI thread arrived: its object already has its final
    // size, so only a repaint is needed (no relayout, the scroll position stays).
    chart_object_ = new ReportChartObject(ReportChartObject::Mode::Screen, this);
    connect(chart_object_, &ReportChartObject::chart_ready, view_, [this]() { view_->update(); });

    // The desk and the overlay read tokens when painting; repaint after a theme change
    // (no stylesheet / geometry work inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { view_->update(); });
    // Font size changes re-fit the status strip once the appearance has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        const QFontMetrics fm(status_bar_->font());
        const int w = fm.horizontalAdvance(QStringLiteral("+")) + 18;
        zoom_out_btn_->setMinimumWidth(w);
        zoom_in_btn_->setMinimumWidth(w);
        update_status();
    });

    geom_ = rb::page_geometry(meta_);
    ctx_ = rb::make_context({}, meta_, theme_, false);
    update_status();
}

DocumentCanvas::~DocumentCanvas() {
    build_timer_->stop();
    clear_sections();
}

void DocumentCanvas::set_note_provider(std::function<rb::BlockNote(int)> provider) {
    note_provider_ = std::move(provider);
    ctx_.note_of = preview_ ? nullptr : note_provider_;
}

int DocumentCanvas::page_count() const {
    int n = 0;
    for (const Section& s : sections_)
        n += std::max(1, s.pages);
    return std::max(1, n);
}

int DocumentCanvas::global_page_of(int section, int page) const {
    int n = 0;
    for (int s = 0; s < section && s < sections_.size(); ++s)
        n += std::max(1, sections_[s].pages);
    return n + page;
}

QRectF DocumentCanvas::page_rect(int global_page) const {
    const double pw = geom_.page.width() * zoom_;
    const double ph = geom_.page.height() * zoom_;
    const double x = std::max<double>(kDcMargin, (view_->width() - pw) / 2.0);
    return QRectF(x, kDcMargin + global_page * (ph + kDcGap), pw, ph);
}

// ── Building ────────────────────────────────────────────────────────────────

void DocumentCanvas::clear_sections() {
    for (Section& s : sections_)
        delete s.doc;
    sections_.clear();
}

DocumentCanvas::Section* DocumentCanvas::new_section() {
    Section s;
    s.doc = rb::new_section_document(ctx_, this);
    chart_object_->install(s.doc);
    QTextCursor c(s.doc);
    if (sections_.isEmpty())
        rb::write_header(c, ctx_);
    else
        rb::write_section_spacer(c);
    s.lead_end = c.position();
    sections_.append(s);
    return &sections_.last();
}

void DocumentCanvas::rebuild_context() {
    const QHash<int, int> pages = ctx_.heading_pages;
    ctx_ = rb::make_context(drawn_, meta_, theme_, preview_);
    ctx_.heading_pages = pages;
    ctx_.note_of = preview_ ? nullptr : note_provider_;
    ctx_.chart_object = chart_object_;
    ctx_.number_style = rb::terminal_number_style();
    geom_ = rb::page_geometry(meta_);
}

void DocumentCanvas::show_document(const QVector<ReportComponent>& components, const ReportMetadata& metadata,
                                   const ReportTheme& theme) {
    const bool look_changed = !have_document_ || metadata.title != meta_.title || metadata.author != meta_.author ||
                              metadata.company != meta_.company || metadata.date != meta_.date ||
                              metadata.font_family != meta_.font_family || metadata.font_size != meta_.font_size ||
                              metadata.page_size != meta_.page_size || metadata.landscape != meta_.landscape ||
                              metadata.margin_left_mm != meta_.margin_left_mm ||
                              metadata.margin_right_mm != meta_.margin_right_mm ||
                              metadata.margin_top_mm != meta_.margin_top_mm ||
                              metadata.margin_bottom_mm != meta_.margin_bottom_mm || theme.name != theme_.name ||
                              // Variables and heading numbers change printed text everywhere.
                              metadata.variables != meta_.variables ||
                              metadata.number_headings != meta_.number_headings;
    // A cover page in front replaces the title header of the first section.
    bool cover_now = false;
    for (const auto& c : components) {
        if (c.type == QLatin1String("page_break"))
            continue;
        cover_now = c.type == QLatin1String("cover");
        break;
    }
    const bool cover_changed = have_document_ && cover_now != ctx_.has_cover;
    const bool paint_only_changed = metadata.watermark != meta_.watermark ||
                                    metadata.confidentiality != meta_.confidentiality;
    const bool bands_changed = paint_only_changed || metadata.header_left != meta_.header_left ||
                               metadata.header_center != meta_.header_center ||
                               metadata.header_right != meta_.header_right || metadata.footer_left != meta_.footer_left ||
                               metadata.footer_center != meta_.footer_center ||
                               metadata.footer_right != meta_.footer_right ||
                               metadata.show_page_numbers != meta_.show_page_numbers;
    const bool geometry_changed =
        bands_changed && rb::page_geometry(metadata).content != rb::page_geometry(meta_).content;
    meta_ = metadata;
    theme_ = theme;
    have_document_ = true;
    if (look_changed || geometry_changed || cover_changed || building_ || preview_) {
        drawn_ = components;
        start_full_build();
        return;
    }
    if (bands_changed) {
        // Header / footer text only: painted per page, nothing to re-lay.
        rebuild_context();
        view_->update();
    }
    if (!apply_incremental(components)) {
        drawn_ = components;
        start_full_build();
        return;
    }
    update_pages(true);
    update_toc_pages();
    view_->update();
}

void DocumentCanvas::start_full_build() {
    build_timer_->stop();
    rebuild_context();
    clear_sections();
    new_section();
    build_index_ = 0;
    building_ = true;
    update_pages(false);
    update_status();
    build_step(); // the first slice right away: short reports appear in one go
}

void DocumentCanvas::build_step() {
    if (!building_)
        return;
    QElapsedTimer t;
    t.start();
    while (build_index_ < drawn_.size()) {
        const ReportComponent& c = drawn_[build_index_++];
        if (c.type == QLatin1String("page_break")) {
            new_section();
            continue;
        }
        if (preview_ && rb::is_unfilled(c))
            continue;
        Section& s = sections_.last();
        QTextCursor cur(s.doc);
        cur.movePosition(QTextCursor::End);
        const int start = cur.position();
        rb::write_component(cur, c, ctx_);
        s.ranges.append({c.id, start, cur.position()});
        if (t.elapsed() >= kDcBuildSliceMs)
            break;
    }
    if (build_index_ >= drawn_.size()) {
        finish_build();
        return;
    }
    update_pages(false);
    build_timer_->start();
}

void DocumentCanvas::finish_build() {
    building_ = false;
    update_pages(true);
    update_toc_pages();
    view_->update();
    update_status();
}

// ── Incremental updates ─────────────────────────────────────────────────────

bool DocumentCanvas::locate(int id, int* section, int* range_index) const {
    for (int s = 0; s < sections_.size(); ++s) {
        const auto& r = sections_[s].ranges;
        for (int i = 0; i < r.size(); ++i) {
            if (r[i].id == id) {
                if (section)
                    *section = s;
                if (range_index)
                    *range_index = i;
                return true;
            }
        }
    }
    return false;
}

void DocumentCanvas::shift_ranges(int section, int from_range, int delta) {
    auto& r = sections_[section].ranges;
    for (int i = from_range; i < r.size(); ++i) {
        r[i].start += delta;
        r[i].end += delta;
    }
}

void DocumentCanvas::replace_range(int section, int range_index, const ReportComponent& comp) {
    Section& s = sections_[section];
    Range& r = s.ranges[range_index];
    QTextCursor c(s.doc);
    c.setPosition(r.start);
    c.setPosition(r.end, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.setPosition(r.start);
    rb::write_component(c, comp, ctx_);
    const int delta = c.position() - r.end;
    r.end = c.position();
    shift_ranges(section, range_index + 1, delta);
}

void DocumentCanvas::insert_component(int section, int range_index, const ReportComponent& comp) {
    Section& s = sections_[section];
    const int start = range_index > 0 ? s.ranges[range_index - 1].end : s.lead_end;
    QTextCursor c(s.doc);
    c.setPosition(start);
    rb::write_component(c, comp, ctx_);
    const int end = c.position();
    s.ranges.insert(range_index, {comp.id, start, end});
    shift_ranges(section, range_index + 1, end - start);
}

void DocumentCanvas::remove_range(int section, int range_index) {
    Section& s = sections_[section];
    const Range r = s.ranges[range_index];
    QTextCursor c(s.doc);
    c.setPosition(r.start);
    c.setPosition(r.end, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    s.ranges.removeAt(range_index);
    shift_ranges(section, range_index, -(r.end - r.start));
}

bool DocumentCanvas::apply_incremental(const QVector<ReportComponent>& comps) {
    namespace lay = ::fincept::report::layout;
    if (preview_ || sections_.isEmpty())
        return false;
    const lay::UpdatePlan plan = lay::plan_update(drawn_, comps);
    if (plan.kind == lay::UpdatePlan::Kind::None) {
        drawn_ = comps;
        return true;
    }
    if (plan.kind == lay::UpdatePlan::Kind::Full)
        return false;
    drawn_ = comps;
    rebuild_context(); // headings (TOC) may have changed

    auto find = [&comps](int id) -> const ReportComponent* {
        for (const auto& c : comps)
            if (c.id == id)
                return &c;
        return nullptr;
    };
    int s = 0, ri = 0;
    switch (plan.kind) {
    case lay::UpdatePlan::Kind::Replace:
        for (int id : plan.replace_ids) {
            const ReportComponent* c = find(id);
            if (!c || !locate(id, &s, &ri))
                return false;
            replace_range(s, ri, *c);
        }
        break;
    case lay::UpdatePlan::Kind::Insert: {
        const QVector<lay::Section> secs = lay::split_sections(comps);
        const int idx = plan.inserted_index;
        s = lay::section_of(secs, idx);
        if (s >= sections_.size())
            return false;
        ri = idx - secs[s].first;
        if (ri < 0 || ri > sections_[s].ranges.size())
            return false;
        insert_component(s, ri, comps[idx]);
        break;
    }
    case lay::UpdatePlan::Kind::Remove:
        if (!locate(plan.removed_id, &s, &ri))
            return false;
        remove_range(s, ri);
        break;
    default:
        return false;
    }
    if (plan.toc) {
        for (const auto& c : comps)
            if (c.type == QLatin1String("toc") && locate(c.id, &s, &ri))
                replace_range(s, ri, c);
    }
    return true;
}

void DocumentCanvas::refresh_component(int id) {
    int s = 0, ri = 0;
    if (building_ || !locate(id, &s, &ri))
        return;
    for (const auto& c : drawn_) {
        if (c.id == id) {
            replace_range(s, ri, c);
            update_pages(true);
            view_->update();
            return;
        }
    }
}

void DocumentCanvas::update_pages(bool notify) {
    for (Section& s : sections_)
        s.pages = std::max(1, s.doc->pageCount());
    view_->relayout();
    const int total = page_count();
    if (notify && total != last_pages_) {
        last_pages_ = total;
        emit page_count_changed(total);
    }
    update_status();
}

void DocumentCanvas::update_toc_pages() {
    bool has_toc = false;
    for (const auto& c : drawn_)
        has_toc = has_toc || c.type == QLatin1String("toc");
    if (!has_toc || building_)
        return;
    QHash<int, int> pages;
    const double h = geom_.content.height();
    for (int id : ctx_.heading_ids) {
        int s = 0, ri = 0;
        if (!locate(id, &s, &ri))
            continue;
        const QTextBlock b = sections_[s].doc->findBlock(sections_[s].ranges[ri].start + 1);
        if (!b.isValid())
            continue;
        const double y = sections_[s].doc->documentLayout()->blockBoundingRect(b).top();
        pages.insert(id, global_page_of(s, static_cast<int>(std::floor(y / h))) + 1);
    }
    if (pages == ctx_.heading_pages)
        return;
    ctx_.heading_pages = pages;
    // The numbers sit on lines that already exist: re-laying the TOC cannot move a heading.
    for (const auto& c : drawn_) {
        int s = 0, ri = 0;
        if (c.type == QLatin1String("toc") && locate(c.id, &s, &ri))
            replace_range(s, ri, c);
    }
    update_pages(true);
}

// ── Geometry of a component, hit testing ───────────────────────────────────

QRectF DocumentCanvas::component_rect(int section, int range_index) const {
    const Section& s = sections_[section];
    const Range& r = s.ranges[range_index];
    QAbstractTextDocumentLayout* layout = s.doc->documentLayout();
    QRectF out;
    QTextFrame* root = s.doc->rootFrame();
    for (auto it = root->begin(); !it.atEnd(); ++it) {
        if (QTextFrame* f = it.currentFrame()) {
            if (f->firstPosition() > r.start && f->firstPosition() <= r.end)
                out = out.united(layout->frameBoundingRect(f));
        } else {
            const QTextBlock b = it.currentBlock();
            if (b.position() > r.start && b.position() <= r.end)
                out = out.united(layout->blockBoundingRect(b));
            else if (b.position() > r.end)
                break;
        }
    }
    return out;
}

int DocumentCanvas::hit_component(const QPointF& pos, bool* on_page) const {
    if (on_page)
        *on_page = false;
    const int total = page_count();
    for (int g = 0; g < total; ++g) {
        const QRectF pr = page_rect(g);
        if (!pr.contains(pos))
            continue;
        if (on_page)
            *on_page = true;
        // Which section / page is global page g?
        int s = 0, local = g;
        while (s < sections_.size() && local >= std::max(1, sections_[s].pages)) {
            local -= std::max(1, sections_[s].pages);
            ++s;
        }
        if (s >= sections_.size())
            return 0;
        const QPointF in_page = (pos - pr.topLeft()) / zoom_;
        if (!geom_.content.adjusted(-12, -4, 12, 4).contains(in_page))
            return 0;
        const QPointF doc_pt(in_page.x() - geom_.content.left(),
                             in_page.y() - geom_.content.top() + local * geom_.content.height());
        const int at = sections_[s].doc->documentLayout()->hitTest(doc_pt, Qt::FuzzyHit);
        if (at < 0)
            return 0;
        for (const Range& r : sections_[s].ranges)
            if (at > r.start && at <= r.end)
                return r.id;
        return 0;
    }
    return 0;
}

void DocumentCanvas::set_selected_id(int id, bool scroll_into_view) {
    if (selected_id_ != id) {
        selected_id_ = id;
        view_->update();
    }
    if (scroll_into_view)
        scroll_to_component(id);
}

void DocumentCanvas::scroll_to_component(int id) {
    int s = 0, ri = 0;
    if (id <= 0 || !locate(id, &s, &ri))
        return;
    const QRectF r = component_rect(s, ri);
    if (!r.isValid())
        return;
    const double h = geom_.content.height();
    const int pg = std::max(0, static_cast<int>(std::floor(r.top() / h)));
    const QRectF pr = page_rect(global_page_of(s, pg));
    const double y = pr.top() + (geom_.content.top() + r.top() - pg * h) * zoom_;
    const double bottom = y + std::min(r.height() * zoom_, scroll_->viewport()->height() * 0.6);
    scroll_->ensureVisible(static_cast<int>(pr.center().x()), static_cast<int>((y + bottom) / 2), 0,
                           static_cast<int>((bottom - y) / 2 + 40));
}

void DocumentCanvas::set_preview_mode(bool on) {
    if (preview_ == on)
        return;
    preview_ = on;
    if (have_document_)
        start_full_build();
}

// ── Zoom / status ───────────────────────────────────────────────────────────

void DocumentCanvas::set_zoom(double z, bool fit) {
    fit_ = fit;
    z = std::clamp(z, 0.35, 2.5);
    if (std::abs(z - zoom_) < 0.004)
        return;
    // Keep the point at the top of the view where it is.
    const double rel = view_->height() > 0 ? scroll_->verticalScrollBar()->value() / double(view_->height()) : 0;
    zoom_ = z;
    view_->relayout();
    scroll_->verticalScrollBar()->setValue(static_cast<int>(rel * view_->minimumHeight()));
    update_status();
}

void DocumentCanvas::refit_zoom() {
    if (!fit_)
        return;
    const double avail = scroll_->viewport()->width() - 2.0 * kDcMargin;
    if (avail <= 50 || geom_.page.width() <= 0)
        return;
    set_zoom(std::clamp(avail / geom_.page.width(), 0.35, 1.6), true);
}

void DocumentCanvas::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    refit_zoom();
}

void DocumentCanvas::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        zoom_fit_btn_->setText(tr("Fit width"));
        update_status();
    }
    QWidget::changeEvent(e);
}

void DocumentCanvas::update_status() {
    if (!status_label_)
        return;
    const int total = page_count();
    const double ph = geom_.page.height() * zoom_ + kDcGap;
    const int center = scroll_->verticalScrollBar()->value() + scroll_->viewport()->height() / 2 - kDcMargin;
    const int current = std::clamp(ph > 0 ? static_cast<int>(center / ph) + 1 : 1, 1, total);
    QString text = tr("Page %1 of %2").arg(current).arg(total) + QStringLiteral("  ·  ") +
                   QStringLiteral("%1%").arg(qRound(zoom_ * 100)) + QStringLiteral("  ·  ") +
                   QStringLiteral("%1 %2").arg(meta_.page_size, meta_.landscape ? tr("landscape") : tr("portrait"));
    if (building_)
        text += QStringLiteral("  ·  ") + tr("laying out pages…");
    if (preview_)
        text += QStringLiteral("  ·  ") + tr("print view");
    status_label_->setText(text);
}

// ── Drag-and-drop ───────────────────────────────────────────────────────────

namespace {
bool dc_is_image_url(const QUrl& url) {
    if (!url.isLocalFile())
        return false;
    static const QStringList exts = {"png", "jpg", "jpeg", "bmp", "svg", "gif", "webp"};
    return exts.contains(QFileInfo(url.toLocalFile()).suffix().toLower());
}
} // namespace

void DocumentCanvas::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) {
        for (const auto& url : e->mimeData()->urls()) {
            if (dc_is_image_url(url)) {
                e->acceptProposedAction();
                return;
            }
        }
    }
    e->ignore();
}

void DocumentCanvas::dragMoveEvent(QDragMoveEvent* e) {
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
    else
        e->ignore();
}

void DocumentCanvas::dropEvent(QDropEvent* e) {
    if (!e->mimeData()->hasUrls()) {
        e->ignore();
        return;
    }
    for (const auto& url : e->mimeData()->urls()) {
        if (dc_is_image_url(url)) {
            emit image_dropped(url.toLocalFile());
            e->acceptProposedAction();
            return;
        }
    }
    e->ignore();
}

} // namespace fincept::screens
