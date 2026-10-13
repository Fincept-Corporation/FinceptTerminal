#include "screens/news/NewsTickerStrip.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>

#include <limits>

namespace fincept::screens {

namespace {
// Shapes `text` once, exactly as QPainter::drawText(QPointF, QString) would on
// `device` (plain text, one unwrapped line, `dir` as the base direction), with
// glyph positions re-based on the baseline: drawGlyphRun(QPointF(x, baseline))
// then puts every glyph where drawText(QPointF(x, baseline), text) did.
QList<QGlyphRun> news_ticker_layout_runs(const QString& text, const QFont& font, Qt::LayoutDirection dir,
                                         QPaintDevice* device) {
    QTextLayout layout(text, font, device);
    QTextOption opt;
    opt.setWrapMode(QTextOption::NoWrap);
    opt.setTextDirection(dir);
    layout.setTextOption(opt);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    qreal ascent = 0;
    if (line.isValid()) {
        line.setLineWidth(std::numeric_limits<int>::max() / 128);
        line.setPosition(QPointF(0, 0));
        ascent = line.ascent();
    }
    layout.endLayout();
    QList<QGlyphRun> runs = layout.glyphRuns();
    for (auto& run : runs) {
        QList<QPointF> positions = run.positions();
        for (auto& pos : positions)
            pos.ry() -= ascent;
        run.setPositions(positions);
    }
    return runs;
}
} // namespace

NewsTickerStrip::NewsTickerStrip(QWidget* parent)
    : QWidget(parent), ticker_font_(ui::fonts::DATA_FAMILY, 10), ticker_fm_(ticker_font_) {
    setObjectName("newsTickerStrip");
    setFixedHeight(22);
    refresh_colors();

    connect(&scroll_timer_, &QTimer::timeout, this, [this]() {
        offset_ += 1.0;
        if (total_width_ > 0 && offset_ >= total_width_)
            offset_ = 0;
        update();
    });
    // Timer interval set but NOT started — parent manages via pause/resume (P3)
    scroll_timer_.setInterval(50); // 20fps

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        refresh_colors();
        update();
    });
}

void NewsTickerStrip::refresh_colors() {
    source_color_ = QColor(ui::colors::CYAN());
    headline_color_ = QColor(ui::colors::TEXT_SECONDARY());
    for (auto& entry : entries_)
        entry.tag_color = QColor(services::priority_color(entry.article.priority));
}

void NewsTickerStrip::set_articles(const QVector<services::NewsArticle>& breaking_articles) {
    // The screen re-pushes this list on every filter pass and every progressive
    // partial. When it hasn't actually changed, keep the scroll position instead
    // of snapping the strip back to the start each time.
    {
        QStringList incoming;
        for (const auto& article : breaking_articles) {
            if (article.priority == services::Priority::FLASH || article.priority == services::Priority::URGENT ||
                article.priority == services::Priority::BREAKING)
                incoming << article.id;
        }
        if (!incoming.isEmpty() && incoming.size() == entries_.size()) {
            bool same = true;
            for (int i = 0; i < incoming.size() && same; ++i)
                same = entries_[i].article.id == incoming[i];
            if (same)
                return;
        }
    }

    entries_.clear();
    total_width_ = 0;

    for (const auto& article : breaking_articles) {
        if (article.priority != services::Priority::FLASH && article.priority != services::Priority::URGENT &&
            article.priority != services::Priority::BREAKING)
            continue;

        TickerEntry entry;
        entry.article = article;
        entry.tag_text = services::priority_string(article.priority);
        entry.source_text = article.source.toUpper();
        entry.headline_text = article.headline;

        entry.tag_width = ticker_fm_.horizontalAdvance(entry.tag_text);
        entry.source_width = ticker_fm_.horizontalAdvance(entry.source_text);
        entry.headline_width = ticker_fm_.horizontalAdvance(entry.headline_text);
        entry.total_width =
            entry.tag_width + SEGMENT_GAP + entry.source_width + SEGMENT_GAP + entry.headline_width + ITEM_SPACING;

        entry.tag_runs = news_ticker_layout_runs(entry.tag_text, ticker_font_, layoutDirection(), this);
        entry.source_runs = news_ticker_layout_runs(entry.source_text, ticker_font_, layoutDirection(), this);
        entry.headline_runs = news_ticker_layout_runs(entry.headline_text, ticker_font_, layoutDirection(), this);
        entry.tag_color = QColor(services::priority_color(article.priority));

        total_width_ += entry.total_width;
        entries_.append(entry);
    }

    offset_ = 0;
    // resume() is called from the screen's showEvent, before any articles have
    // arrived — with an empty entries_ it left the timer stopped, so the strip
    // never scrolled for the whole session. Re-evaluate the timer whenever the
    // content changes.
    if (!paused_ && !entries_.isEmpty())
        scroll_timer_.start();
    else if (entries_.isEmpty())
        scroll_timer_.stop();
    setAccessibleName(tr("Breaking news ticker"));
    setAccessibleDescription(
        entries_.isEmpty()
            ? tr("No breaking headlines")
            : tr("%n breaking headline(s); click to open one", "", static_cast<int>(entries_.size())));
    update();
}

void NewsTickerStrip::pause() {
    paused_ = true;
    scroll_timer_.stop();
}

void NewsTickerStrip::resume() {
    paused_ = false;
    if (!entries_.isEmpty())
        scroll_timer_.start();
}

void NewsTickerStrip::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Respect an explicit pause() (hover-to-read / edit) — visibility must not
    // silently override a deliberate pause the user is relying on.
    if (!paused_ && !entries_.isEmpty())
        scroll_timer_.start();
}

void NewsTickerStrip::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    scroll_timer_.stop();
}

void NewsTickerStrip::paintEvent(QPaintEvent*) {
    if (entries_.isEmpty())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setFont(ticker_font_);

    int text_y = (height() + ticker_fm_.ascent() - ticker_fm_.descent()) / 2;
    const int viewport_w = width();
    double x = -offset_;
    int passes = total_width_ > 0 ? (width() / total_width_) + 2 : 1;
    // Pixel-identical to drawText(QPointF(x, text_y), text) — the runs carry
    // drawText's own shaping with positions relative to the baseline.
    auto draw_runs = [&p, text_y](double at_x, const QList<QGlyphRun>& runs) {
        for (const auto& run : runs)
            p.drawGlyphRun(QPointF(at_x, text_y), run);
    };

    // Strings, widths, colours and the laid-out text are all prepared outside
    // paintEvent (P9); this runs 20x a second.
    for (int pass = 0; pass < passes; ++pass) {
        for (const auto& entry : entries_) {
            // Skip entries scrolled off either edge — every headline of every
            // pass used to be shaped and drawn each frame.
            if (x + entry.total_width < 0 || x > viewport_w) {
                x += entry.total_width;
                continue;
            }

            // Tag in priority color
            p.setPen(entry.tag_color);
            draw_runs(x, entry.tag_runs);
            x += entry.tag_width + SEGMENT_GAP;

            // Source in cyan
            p.setPen(source_color_);
            draw_runs(x, entry.source_runs);
            x += entry.source_width + SEGMENT_GAP;

            // Headline in secondary text
            p.setPen(headline_color_);
            draw_runs(x, entry.headline_runs);
            x += entry.headline_width + ITEM_SPACING;
        }
    }
}

void NewsTickerStrip::mousePressEvent(QMouseEvent* event) {
    if (entries_.isEmpty())
        return;

    double click_x = event->position().x() + offset_;
    // Normalize to within one cycle
    if (total_width_ > 0)
        click_x = std::fmod(click_x, static_cast<double>(total_width_));

    double cumulative = 0;
    for (const auto& entry : entries_) {
        cumulative += entry.total_width;
        if (click_x < cumulative) {
            emit article_clicked(entry.article);
            return;
        }
    }
}

void NewsTickerStrip::enterEvent(QEnterEvent*) {
    scroll_timer_.stop();
}

void NewsTickerStrip::leaveEvent(QEvent*) {
    if (!paused_ && !entries_.isEmpty())
        scroll_timer_.start();
}

} // namespace fincept::screens
