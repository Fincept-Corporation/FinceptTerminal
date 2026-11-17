// AiChatTranscript_Blocks.cpp — the widget of one block of an answer (AiChatTranscript.h).
//
//   Paragraph / Heading / Quote / List  a rich-text QLabel: the parser's sanitised HTML themed by
//                                       style::inline_html(); links and ticker chips go through
//                                       the view (open_link / show_text_menu)
//   List with a numeric series          the label + [CHART] → a local BarChart, made on click
//   Code                                header strip (LANG · COPY · OPEN IN CODE TAB) + a read-only
//                                       editor (at most 24 lines high, scrolls beyond)
//   Table                               a painted dense grid + [COPY CSV] [CHART]
//   Rule / Math                         a hairline / a monospace label
//
// Charts are built from the block's own figures and only on click: nothing is fetched. Every
// block widget carries its BlockKind in the "aic_block" property; labels keep their neutral
// HTML so a theme switch re-renders them without re-parsing.

#include "screens/ai_chat/AiChatTranscript.h"

#include "screens/ai_chat/AiChatStyle.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"

#include <QApplication>
#include <QClipboard>
#include <QDate>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::aichat {

namespace {

constexpr const char* tb_kKindProp = "aic_block";      // int(BlockKind) on every block widget
constexpr const char* tb_kNeutralProp = "aic_neutral"; // label: the parser's HTML (neutral markers)
constexpr const char* tb_kCaretProp = "aic_caret";     // label: the streaming caret is appended
constexpr const char* tb_kGenProp = "aic_gen";         // label: style::inline_generation() rendered with
constexpr const char* tb_kHoverProp = "aic_hover";     // label: the link under the mouse
constexpr int tb_kCodeMaxLines = 24;
constexpr int tb_kMaxChartSeries = 6;
constexpr int tb_kMaxBarCategories = 60;
constexpr int tb_kWidthSampleRows = 300;
constexpr int tb_kMaxColChars = 40;

QString tb_caret_html() {
    return QStringLiteral("<span style='color:%1'>%2</span>").arg(style::accent().name(), QString(QChar(0x258D)));
}

/// Nested <ul> / <ol> from the parser's items (each item is sanitised inline HTML).
QString tb_list_html(const Block& b) {
    const QString list_style = QStringLiteral(" style='margin-top:0px;margin-bottom:0px;'");
    QString out;
    QVector<bool> open; // per open list: ordered
    for (qsizetype i = 0; i < b.items.size(); ++i) {
        const int level = std::clamp(b.item_levels.value(i, 0), 0, 8);
        while (int(open.size()) < level + 1) {
            const bool ordered = open.isEmpty() && b.ordered;
            if (ordered && b.start != 1)
                out += QStringLiteral("<ol start='%1'%2>").arg(QString::number(b.start), list_style);
            else if (ordered)
                out += QStringLiteral("<ol%1>").arg(list_style);
            else
                out += QStringLiteral("<ul%1>").arg(list_style);
            open.append(ordered);
        }
        while (int(open.size()) > level + 1)
            out += open.takeLast() ? QStringLiteral("</ol>") : QStringLiteral("</ul>");
        out += QStringLiteral("<li>") + b.items.at(i) + QStringLiteral("</li>");
    }
    while (!open.isEmpty())
        out += open.takeLast() ? QStringLiteral("</ol>") : QStringLiteral("</ul>");
    return out;
}

QLabel* tb_new_text_label(BlockKind kind, QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QLatin1String(kind == BlockKind::Heading ? style::kHeading
                                   : kind == BlockKind::Quote ? style::kQuote
                                                              : style::kText));
    l->setTextFormat(Qt::RichText);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextBrowserInteraction);
    l->setOpenExternalLinks(false);
    l->setFocusPolicy(Qt::ClickFocus); // the view stays the keyboard host
    l->setContextMenuPolicy(Qt::CustomContextMenu);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QSizePolicy sp(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    l->setSizePolicy(sp);
    return l;
}

/// Neutral HTML → themed HTML (+ the caret) on the label.
void tb_render(QLabel* l) {
    QString html = style::inline_html(l->property(tb_kNeutralProp).toString());
    if (l->property(tb_kCaretProp).toBool()) {
        const qsizetype li = html.lastIndexOf(QLatin1String("</li>"));
        if (li >= 0)
            html.insert(li, tb_caret_html());
        else
            html += tb_caret_html();
    }
    l->setText(html);
    l->setProperty(tb_kGenProp, QVariant::fromValue<quint64>(style::inline_generation()));
}

void tb_set_label(QLabel* l, const QString& neutral, bool caret) {
    if (!l->text().isEmpty() && l->property(tb_kNeutralProp).toString() == neutral &&
        l->property(tb_kCaretProp).toBool() == caret &&
        l->property(tb_kGenProp).toULongLong() == style::inline_generation())
        return;
    l->setProperty(tb_kNeutralProp, neutral);
    l->setProperty(tb_kCaretProp, caret);
    tb_render(l);
}

void tb_set_heading_level(QLabel* l, int level) {
    const QString v = QString::number(std::clamp(level, 1, 3));
    if (l->property("level").toString() == v)
        return;
    l->setProperty("level", v);
    l->style()->unpolish(l);
    l->style()->polish(l);
}

QPushButton* tb_link_button(QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QLatin1String(style::kLinkBtn));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedHeight(style::button_h());
    return b;
}

void tb_label_button(QPushButton* b, const QString& text, const QString& accessible, const QString& tip) {
    b->setText(text);
    b->setAccessibleName(accessible);
    b->setToolTip(tip);
}

/// A block changed size on its own (a chart toggled): drop cached hints up to the turn, whose
/// layout request then re-measures it in the view.
void tb_relayout_up(QWidget* w) {
    for (QWidget* p = w; p; p = p->parentWidget()) {
        if (QLayout* l = p->layout())
            l->invalidate();
        if (p->objectName() == QLatin1String(style::kTurn))
            break;
        p->updateGeometry();
    }
}

QString tb_copied(qsizetype n) {
    return AicTranscriptView::tr("Copied %1 characters").arg(QLocale().toString(qlonglong(n)));
}

QFont tb_head_font() {
    return style::ui_font(-3, QFont::DemiBold);
}

int tb_chart_h() {
    return std::max(style::u(40), style::text_h() * 13);
}

// ── Period labels → dates (a table's time column as a time axis) ─────────────

int tb_year(const QString& digits) {
    int y = digits.toInt();
    if (digits.size() <= 2)
        y += y < 70 ? 2000 : 1900;
    return y;
}

int tb_month(const QString& name) {
    static const char* const months[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                         "jul", "aug", "sep", "oct", "nov", "dec"};
    const QString n = name.left(3).toLower();
    for (int i = 0; i < 12; ++i)
        if (n == QLatin1String(months[i]))
            return i + 1;
    return 0;
}

/// "2024", "FY23", "2024E", "2024-03-31", "2024-03", "Q1 2024", "2024 Q1", "1Q24", "H1 2025",
/// "Mar 2024" → the first day of the period.
bool tb_period_date(const QString& label, QDate* out) {
    const QString s = label.trimmed();
    if (s.isEmpty() || s.size() > 24)
        return false;
    constexpr auto ci = QRegularExpression::CaseInsensitiveOption;
    static const QRegularExpression ymd(QStringLiteral("^(\\d{4})[-/.](\\d{1,2})(?:[-/.](\\d{1,2}))?$"));
    static const QRegularExpression year(QStringLiteral("^(?:(?:FY|CY)\\s*'?)?(\\d{4})[AEFP]?$"), ci);
    static const QRegularExpression fy2(QStringLiteral("^(?:FY|CY)\\s*'?(\\d{2})[AEFP]?$"), ci);
    static const QRegularExpression q_first(
        QStringLiteral("^Q([1-4])\\s*[-'/ ]?\\s*(?:FY|CY)?\\s*'?(\\d{4}|\\d{2})[AEFP]?$"), ci);
    static const QRegularExpression q_last(QStringLiteral("^(?:FY|CY)?\\s*(\\d{4})\\s*[-/ ]?\\s*Q([1-4])$"), ci);
    static const QRegularExpression q_num(QStringLiteral("^([1-4])Q\\s*'?(\\d{4}|\\d{2})$"), ci);
    static const QRegularExpression h_first(QStringLiteral("^H([12])\\s*[-'/ ]?\\s*(?:FY)?\\s*'?(\\d{4}|\\d{2})$"), ci);
    static const QRegularExpression h_last(QStringLiteral("^(\\d{4})\\s*[-/ ]?\\s*H([12])$"), ci);
    static const QRegularExpression mon(QStringLiteral("^([A-Za-z]{3,9})\\.?\\s*[-' ]?\\s*'?(\\d{4}|\\d{2})$"));
    QRegularExpressionMatch m;
    auto hit = [&](const QRegularExpression& rx) {
        m = rx.match(s);
        return m.hasMatch();
    };
    QDate d;
    if (hit(ymd))
        d = QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).isEmpty() ? 1 : m.captured(3).toInt());
    else if (hit(year))
        d = QDate(m.captured(1).toInt(), 1, 1);
    else if (hit(fy2))
        d = QDate(tb_year(m.captured(1)), 1, 1);
    else if (hit(q_first))
        d = QDate(tb_year(m.captured(2)), (m.captured(1).toInt() - 1) * 3 + 1, 1);
    else if (hit(q_last))
        d = QDate(m.captured(1).toInt(), (m.captured(2).toInt() - 1) * 3 + 1, 1);
    else if (hit(q_num))
        d = QDate(tb_year(m.captured(2)), (m.captured(1).toInt() - 1) * 3 + 1, 1);
    else if (hit(h_first))
        d = QDate(tb_year(m.captured(2)), m.captured(1).toInt() == 1 ? 1 : 7, 1);
    else if (hit(h_last))
        d = QDate(m.captured(1).toInt(), m.captured(2).toInt() == 1 ? 1 : 7, 1);
    else if (hit(mon)) {
        const int mo = tb_month(m.captured(1));
        if (mo > 0)
            d = QDate(tb_year(m.captured(2)), mo, 1);
    }
    if (!d.isValid())
        d = QDate::fromString(s, Qt::ISODate);
    if (!d.isValid())
        return false;
    *out = d;
    return true;
}

// ── Local charts ─────────────────────────────────────────────────────────────

/// A table's numeric columns over its time column (TimeSeriesChart) or its label column
/// (BarChart). nullptr when there is nothing to plot.
QWidget* tb_table_chart(const TableData& t, QWidget* parent) {
    const int cols = int(t.header.size());
    const int x_col = t.time_col >= 0 ? t.time_col : t.label_col;
    QVector<int> ycols;
    for (int c = 0; c < cols && int(ycols.size()) < tb_kMaxChartSeries; ++c)
        if (c != x_col && t.numeric.value(c))
            ycols.append(c);
    if (ycols.isEmpty() || t.rows.isEmpty())
        return nullptr;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QVector<QVector<double>> ys(ycols.size());
    for (const QStringList& row : t.rows) {
        for (int k = 0; k < int(ycols.size()); ++k) {
            double v = 0.0;
            ys[k].append(parse_figure(row.value(ycols.at(k)), &v) ? v : nan);
        }
    }
    if (t.time_col >= 0) {
        QVector<double> xs;
        xs.reserve(t.rows.size());
        bool all = true;
        for (const QStringList& row : t.rows) {
            QDate d;
            if (!tb_period_date(row.value(t.time_col), &d)) {
                all = false;
                break;
            }
            xs.append(double(d.startOfDay().toMSecsSinceEpoch()));
        }
        if (all && xs.size() >= 2) {
            auto* chart = new fincept::ui::TimeSeriesChart(parent);
            chart->set_transparent_background(true);
            chart->set_zoom_enabled(false); // the wheel keeps scrolling the transcript
            chart->set_show_mode_buttons(false);
            chart->set_show_legend(ycols.size() > 1);
            for (int k = 0; k < int(ycols.size()); ++k) {
                fincept::ui::TimeSeriesChart::SeriesSpec spec;
                spec.name = t.header.value(ycols.at(k));
                spec.xs = xs;
                spec.ys = ys.at(k);
                chart->add_series(spec);
            }
            chart->setFixedHeight(tb_chart_h());
            return chart;
        }
    }
    const int label_col = x_col >= 0 ? x_col : 0;
    const int n = std::min(int(t.rows.size()), tb_kMaxBarCategories);
    QStringList cats;
    for (int i = 0; i < n; ++i)
        cats << t.rows.at(i).value(label_col);
    QVector<fincept::ui::BarSeries> series;
    bool negative = false;
    for (int k = 0; k < int(ycols.size()); ++k) {
        fincept::ui::BarSeries s;
        s.name = t.header.value(ycols.at(k));
        s.values = ys.at(k).mid(0, n);
        for (const double v : std::as_const(s.values))
            negative = negative || v < 0.0;
        series.append(s);
    }
    auto* bars = new fincept::ui::BarChart(parent);
    bars->set_categories(cats);
    bars->set_series(series);
    bars->set_signed_colors(series.size() == 1 && negative);
    bars->set_show_values(series.size() == 1 && n <= 12);
    bars->setFixedHeight(tb_chart_h());
    return bars;
}

/// A list's numeric series ("2021: 12.3", "Q1 — 4.5%") as bars.
QWidget* tb_series_chart(const NumericSeries& s, QWidget* parent) {
    if (!s.valid())
        return nullptr;
    const int n = std::min(int(s.values.size()), tb_kMaxBarCategories);
    bool negative = false;
    for (int i = 0; i < n; ++i)
        negative = negative || s.values.at(i) < 0.0;
    auto* bars = new fincept::ui::BarChart(parent);
    const QString unit = s.unit.trimmed();
    bars->set_data(s.labels.mid(0, n), s.values.mid(0, n), unit.isEmpty() ? AicTranscriptView::tr("Value") : unit);
    bars->set_signed_colors(negative);
    bars->set_show_values(n <= 12);
    if (!unit.isEmpty()) {
        const bool prefix = unit.size() == 1 && !unit.at(0).isLetterOrNumber() && unit != QLatin1String("%");
        const bool spaced = unit.at(0).isLetter();
        bars->set_value_formatter([unit, prefix, spaced](double v) -> QString {
            const QString num = QLocale().toString(v, 'f', std::fabs(v) >= 100.0 ? 0 : 2);
            if (prefix)
                return unit + num;
            if (spaced)
                return num + QStringLiteral(" ") + unit;
            return num + unit;
        });
    }
    bars->setFixedHeight(tb_chart_h());
    return bars;
}

// ── Code ─────────────────────────────────────────────────────────────────────

/// Read-only code, no wrapping; as tall as its lines up to 24, then it scrolls.
class AicTbCodeEdit : public QPlainTextEdit {
  public:
    explicit AicTbCodeEdit(QWidget* parent) : QPlainTextEdit(parent) {
        setObjectName(QLatin1String(style::kCodeBody));
        setReadOnly(true);
        setLineWrapMode(QPlainTextEdit::NoWrap);
        setFrameShape(QFrame::NoFrame);
        setFocusPolicy(Qt::ClickFocus);
        setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setAccessibleName(AicTranscriptView::tr("Code"));
        refont();
    }
    const QString& code() const { return code_; }
    void set_code(const QString& code) {
        if (code == code_ && !document()->isEmpty())
            return;
        if (!code_.isEmpty() && code.startsWith(code_)) {
            // The streaming tail grew: append instead of re-laying out the whole block.
            QTextCursor c(document());
            c.movePosition(QTextCursor::End);
            c.insertText(code.mid(code_.size()));
        } else {
            setPlainText(code);
        }
        code_ = code;
        lines_ = int(code_.count(QLatin1Char('\n'))) + 1;
        longest_ = 0;
        qsizetype start = 0;
        while (start <= code_.size()) {
            qsizetype nl = code_.indexOf(QLatin1Char('\n'), start);
            if (nl < 0)
                nl = code_.size();
            longest_ = std::max(longest_, int(nl - start));
            start = nl + 1;
        }
        refit();
    }
    void refont() {
        setFont(style::mono_font(-1));
        setTabStopDistance(4.0 * QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));
        refit();
    }
    void refit() {
        const QFontMetrics fm(font());
        const int margin = int(std::ceil(document()->documentMargin()));
        int h = std::clamp(lines_, 1, tb_kCodeMaxLines) * fm.lineSpacing() + 2 * margin + 2 * frameWidth() + 2;
        const int text_w = longest_ * fm.horizontalAdvance(QLatin1Char('0')) + 2 * margin;
        if (isVisible() && text_w > viewport()->width())
            h += horizontalScrollBar()->sizeHint().height();
        if (h != height())
            setFixedHeight(h);
    }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        refit();
    }

  private:
    QString code_;
    int lines_ = 1;
    int longest_ = 0;
};

class AicTbCode : public QFrame {
  public:
    AicTbCode(AicTranscriptView* view, QWidget* parent) : QFrame(parent), view_(view) {
        setObjectName(QLatin1String(style::kCode));
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        head_ = new QWidget(this);
        head_->setObjectName(QLatin1String(style::kCodeHead));
        head_->setAttribute(Qt::WA_StyledBackground);
        auto* hl = new QHBoxLayout(head_);
        hl->setContentsMargins(style::u(2), 0, style::u(1), 0);
        hl->setSpacing(style::u(2));
        lang_ = new QLabel(head_);
        lang_->setObjectName(QLatin1String(style::kDim));
        lang_->setTextFormat(Qt::PlainText);
        copy_ = tb_link_button(head_);
        open_ = tb_link_button(head_);
        open_->hide();
        hl->addWidget(lang_);
        hl->addStretch(1);
        hl->addWidget(copy_);
        hl->addWidget(open_);
        edit_ = new AicTbCodeEdit(this);
        vl->addWidget(head_);
        vl->addWidget(edit_);
        QObject::connect(copy_, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setText(edit_->code());
            if (view_)
                emit view_->status_message(tb_copied(edit_->code().size()));
        });
        QObject::connect(open_, &QPushButton::clicked, this, [this]() {
            if (view_)
                emit view_->code_open_requested(edit_->code(), lang_raw_);
        });
        restyle();
    }
    void set_block(const Block& b) {
        lang_raw_ = b.lang.trimmed();
        lang_->setText(lang_raw_.isEmpty() ? AicTranscriptView::tr("CODE") : lang_raw_.toUpper());
        const QString l = lang_raw_.toLower();
        const bool python = l == QLatin1String("python") || l == QLatin1String("py") ||
                            l == QLatin1String("ipython") || l == QLatin1String("python3");
        open_->setVisible(python);
        open_->setEnabled(b.closed);
        edit_->set_code(b.code);
    }
    void restyle() {
        using V = AicTranscriptView;
        head_->setFixedHeight(style::button_h() + style::u(1));
        copy_->setFixedHeight(style::button_h());
        open_->setFixedHeight(style::button_h());
        tb_label_button(copy_, V::tr("COPY"), V::tr("Copy code"), V::tr("Copy this code block"));
        tb_label_button(open_, V::tr("OPEN IN CODE TAB"), V::tr("Open in the Code tab"),
                        V::tr("Open this Python code in the Code tab (nothing runs by itself)"));
        if (lang_raw_.isEmpty())
            lang_->setText(V::tr("CODE"));
        edit_->refont();
    }

  private:
    QPointer<AicTranscriptView> view_;
    QWidget* head_ = nullptr;
    QLabel* lang_ = nullptr;
    QPushButton* copy_ = nullptr;
    QPushButton* open_ = nullptr;
    AicTbCodeEdit* edit_ = nullptr;
    QString lang_raw_;
};

// ── Table ────────────────────────────────────────────────────────────────────

/// A dense painted grid: upper-case header, 1 px hairlines, numeric columns right-aligned in
/// tabular figures, elided cells with tooltips, a horizontal scroll bar when it is wider than
/// the column. Only the rows in the exposed rectangle are painted.
class AicTbGrid : public QWidget {
  public:
    explicit AicTbGrid(QWidget* parent) : QWidget(parent), hbar_(new QScrollBar(Qt::Horizontal, this)) {
        setMouseTracking(true);
        setFocusPolicy(Qt::NoFocus);
        setContextMenuPolicy(Qt::CustomContextMenu);
        QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sp.setHeightForWidth(true);
        setSizePolicy(sp);
        hbar_->hide();
        hbar_->setAccessibleName(AicTranscriptView::tr("Scroll the table sideways"));
        QObject::connect(hbar_, &QScrollBar::valueChanged, this, [this]() { update(); });
        setAccessibleName(AicTranscriptView::tr("Table"));
    }
    const TableData& table() const { return t_; }
    void set_table(const TableData& t) {
        t_ = t;
        hover_ = -1;
        refit();
    }
    void refit() {
        const QFontMetrics fh(tb_head_font()), fc(style::ui_font(-1)), fn(style::mono_font(-1));
        pad_ = style::u(3);
        row_h_ = std::max(fc.height(), fn.height()) + style::u(1) + style::u(1) / 2;
        head_h_ = fh.height() + style::u(2);
        const int cols = int(t_.header.size());
        const int ac = std::max(1, fc.averageCharWidth());
        const int max_w = tb_kMaxColChars * ac;
        const int min_w = 3 * ac;
        const int sample = std::min(int(t_.rows.size()), tb_kWidthSampleRows);
        col_x_.resize(cols + 1);
        col_x_[0] = 0;
        for (int c = 0; c < cols; ++c) {
            int w = fh.horizontalAdvance(t_.header.at(c).toUpper());
            const QFontMetrics& m = t_.numeric.value(c) ? fn : fc;
            for (int r = 0; r < sample; ++r)
                w = std::max(w, m.horizontalAdvance(t_.rows.at(r).value(c)));
            col_x_[c + 1] = col_x_[c] + std::clamp(w, min_w, std::max(min_w, max_w)) + pad_;
        }
        place_bar();
        updateGeometry();
        update();
    }
    QString cell_at(const QPoint& pos) const {
        const Hit h = hit(pos);
        if (h.col < 0)
            return {};
        if (h.row == -1)
            return t_.header.value(h.col);
        if (h.row >= 0)
            return t_.rows.at(h.row).value(h.col);
        return {};
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return body_h() + (total_w() > w ? hbar_->sizeHint().height() : 0); }
    QSize sizeHint() const override { return {total_w(), body_h()}; }
    QSize minimumSizeHint() const override { return {style::u(10), body_h()}; }

  protected:
    void paintEvent(QPaintEvent* e) override {
        QPainter p(this);
        const QFont fh_font = tb_head_font();
        const QFont fc_font = style::ui_font(-1);
        const QFont fn_font = style::mono_font(-1);
        const QFontMetrics fh(fh_font), fc(fc_font), fn(fn_font);
        const int right = std::max(0, std::min(width(), total_w() - offset()));
        const int cols = int(t_.header.size());
        const int rows = int(t_.rows.size());
        p.setClipRect(QRect(0, 0, width(), body_h()));

        p.setFont(fh_font);
        p.setPen(style::text_secondary());
        for (int c = 0; c < cols; ++c) {
            const QRect r = cell_rect(-1, c);
            if (r.right() < 0 || r.left() > width())
                continue;
            p.drawText(r, align(c) | Qt::AlignVCenter, fh.elidedText(t_.header.at(c).toUpper(), Qt::ElideRight, r.width()));
        }
        p.fillRect(QRect(0, head_h_ - 1, right, 1), style::border_med());
        if (rows == 0)
            return;

        const QRect clip = e->rect();
        const int first = std::clamp((clip.top() - head_h_) / std::max(1, row_h_), 0, rows - 1);
        const int last = std::clamp((clip.bottom() - head_h_) / std::max(1, row_h_), 0, rows - 1);
        const QColor text = style::text_primary();
        const QColor up = style::positive();
        const QColor down = style::negative();
        const QColor line = style::border_dim();
        for (int r = first; r <= last; ++r) {
            const int y = head_h_ + r * row_h_;
            if (r == hover_)
                p.fillRect(QRect(0, y, right, row_h_ - 1), style::bg_hover());
            const QStringList& row = t_.rows.at(r);
            for (int c = 0; c < cols; ++c) {
                const QRect cr = cell_rect(r, c);
                if (cr.right() < 0 || cr.left() > width())
                    continue;
                const QString cell = row.value(c);
                if (cell.isEmpty())
                    continue;
                const bool num = t_.numeric.value(c);
                QColor col = text;
                // A signed percentage is a move: direction colour, the sign stays visible.
                if (num && cell.contains(QLatin1Char('%'))) {
                    if (cell.startsWith(QLatin1Char('+')))
                        col = up;
                    else if (cell.startsWith(QLatin1Char('-')) || cell.startsWith(QChar(0x2212)))
                        col = down;
                }
                p.setFont(num ? fn_font : fc_font);
                p.setPen(col);
                p.drawText(cr, align(c) | Qt::AlignVCenter, (num ? fn : fc).elidedText(cell, Qt::ElideRight, cr.width()));
            }
            p.fillRect(QRect(0, y + row_h_ - 1, right, 1), line);
        }
    }
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        place_bar();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        const Hit h = hit(e->position().toPoint());
        const int row = h.row >= 0 ? h.row : -1;
        if (row != hover_) {
            hover_ = row;
            update();
        }
        QWidget::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent* e) override {
        if (hover_ != -1) {
            hover_ = -1;
            update();
        }
        QWidget::leaveEvent(e);
    }
    void wheelEvent(QWheelEvent* e) override {
        const QPoint a = e->angleDelta();
        const bool sideways = std::abs(a.x()) > std::abs(a.y()) || e->modifiers().testFlag(Qt::ShiftModifier);
        if (!hbar_->isHidden() && sideways) {
            QCoreApplication::sendEvent(hbar_, e);
            return;
        }
        e->ignore(); // vertical: the transcript scrolls
    }
    bool event(QEvent* e) override {
        if (e->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(e);
            const Hit h = hit(he->pos());
            QString text;
            QRect r;
            if (h.col >= 0 && h.row >= -1) {
                r = cell_rect(h.row, h.col);
                const bool head = h.row == -1;
                text = head ? t_.header.value(h.col).toUpper() : t_.rows.at(h.row).value(h.col);
                const QFontMetrics fm(head ? tb_head_font()
                                           : (t_.numeric.value(h.col) ? style::mono_font(-1) : style::ui_font(-1)));
                if (fm.horizontalAdvance(text) <= r.width())
                    text.clear();
            }
            if (text.isEmpty()) {
                QToolTip::hideText();
                e->ignore();
            } else {
                QToolTip::showText(he->globalPos(), text, this, r);
            }
            return true;
        }
        return QWidget::event(e);
    }

  private:
    struct Hit {
        int row = -2; ///< -1 header, -2 none
        int col = -1;
    };
    int total_w() const { return col_x_.isEmpty() ? 0 : col_x_.constLast(); }
    int body_h() const { return head_h_ + int(t_.rows.size()) * row_h_ + 1; }
    int offset() const { return hbar_->isHidden() ? 0 : hbar_->value(); }
    Qt::Alignment align(int c) const {
        if (c == 0)
            return Qt::AlignLeft;
        if (t_.numeric.value(c))
            return Qt::AlignRight;
        const int a = t_.align.value(c);
        return a == 1 ? Qt::AlignHCenter : a == 2 ? Qt::AlignRight : Qt::AlignLeft;
    }
    QRect cell_rect(int row, int col) const {
        const int x = col_x_.value(col) - offset();
        const int w = col_x_.value(col + 1) - col_x_.value(col);
        return {x + pad_ / 2, row < 0 ? 0 : head_h_ + row * row_h_, std::max(0, w - pad_), row < 0 ? head_h_ : row_h_};
    }
    Hit hit(const QPoint& pos) const {
        Hit h;
        if (pos.y() < 0 || pos.y() >= body_h() || col_x_.size() < 2)
            return h;
        const int x = pos.x() + offset();
        const auto it = std::upper_bound(col_x_.constBegin(), col_x_.constEnd(), x);
        const int col = int(it - col_x_.constBegin()) - 1;
        if (col < 0 || col >= int(t_.header.size()))
            return h;
        h.col = col;
        if (pos.y() < head_h_) {
            h.row = -1;
        } else {
            const int row = (pos.y() - head_h_) / std::max(1, row_h_);
            h.row = row < int(t_.rows.size()) ? row : -2;
        }
        return h;
    }
    void place_bar() {
        const bool need = total_w() > width();
        if (need) {
            const int sbh = hbar_->sizeHint().height();
            hbar_->setRange(0, total_w() - width());
            hbar_->setPageStep(std::max(1, width()));
            hbar_->setSingleStep(style::u(6));
            hbar_->setGeometry(0, height() - sbh, width(), sbh);
        }
        if (need == hbar_->isHidden()) {
            hbar_->setVisible(need);
            updateGeometry();
        }
    }

    TableData t_;
    QVector<int> col_x_; ///< column start offsets (size cols + 1)
    int head_h_ = 20;
    int row_h_ = 20;
    int pad_ = 12;
    int hover_ = -1;
    QScrollBar* hbar_;
};

/// A table (grid + COPY CSV / CHART) or a chartable list (label + CHART); the chart is made on
/// the first click and dropped on the second.
class AicTbBox : public QWidget {
  public:
    AicTbBox(AicTranscriptView* view, BlockKind kind, QWidget* parent) : QWidget(parent), view_(view), kind_(kind) {
        vl_ = new QVBoxLayout(this);
        vl_->setContentsMargins(0, 0, 0, 0);
        vl_->setSpacing(style::u(1));
        if (kind_ == BlockKind::Table) {
            grid_ = new AicTbGrid(this);
            vl_->addWidget(grid_);
        } else {
            label_ = tb_new_text_label(BlockKind::List, this);
            vl_->addWidget(label_);
        }
        actions_ = new QWidget(this);
        auto* al = new QHBoxLayout(actions_);
        al->setContentsMargins(0, 0, 0, 0);
        al->setSpacing(style::u(2));
        if (kind_ == BlockKind::Table) {
            csv_ = tb_link_button(actions_);
            al->addWidget(csv_);
            QObject::connect(csv_, &QPushButton::clicked, this, [this]() { copy_csv(); });
        }
        chart_btn_ = tb_link_button(actions_);
        al->addWidget(chart_btn_);
        al->addStretch(1);
        QObject::connect(chart_btn_, &QPushButton::clicked, this, [this]() { toggle_chart(); });
        vl_->addWidget(actions_);
        retranslate();
    }
    QLabel* label() const { return label_; }
    AicTbGrid* grid() const { return grid_; }
    void set_block(const Block& b, bool caret) {
        const bool changed = first_ || b.source != block_.source;
        first_ = false;
        block_ = b;
        if (label_)
            tb_set_label(label_, tb_list_html(b), caret);
        if (grid_ && changed)
            grid_->set_table(b.table);
        const bool chartable = kind_ == BlockKind::Table ? b.table.chartable : b.series.valid();
        chart_btn_->setVisible(chartable);
        actions_->setVisible(chartable || kind_ == BlockKind::Table);
        if (chart_ && (changed || !chartable))
            drop_chart();
        retranslate();
    }
    void restyle(bool rerender_html) {
        if (label_ && (rerender_html || label_->property(tb_kGenProp).toULongLong() != style::inline_generation()))
            tb_render(label_);
        if (grid_)
            grid_->refit();
        if (chart_)
            chart_->setFixedHeight(tb_chart_h());
        if (csv_)
            csv_->setFixedHeight(style::button_h());
        chart_btn_->setFixedHeight(style::button_h());
        retranslate();
    }

  private:
    void retranslate() {
        using V = AicTranscriptView;
        if (csv_)
            tb_label_button(csv_, V::tr("COPY CSV"), V::tr("Copy the table as CSV"),
                            V::tr("Copy this table to the clipboard as CSV"));
        if (chart_)
            tb_label_button(chart_btn_, V::tr("HIDE CHART"), V::tr("Hide the chart"), V::tr("Hide the chart"));
        else
            tb_label_button(chart_btn_, V::tr("CHART"), V::tr("Chart these figures"),
                            V::tr("Chart the figures of this block (local data only)"));
    }
    void copy_csv() {
        QApplication::clipboard()->setText(table_to_csv(block_.table));
        if (view_)
            emit view_->status_message(
                AicTranscriptView::tr("Copied the table as CSV (%n row(s))", "", int(block_.table.rows.size())));
    }
    void toggle_chart() {
        if (chart_) {
            drop_chart();
        } else {
            chart_ = kind_ == BlockKind::Table ? tb_table_chart(block_.table, this) : tb_series_chart(block_.series, this);
            if (!chart_) {
                if (view_)
                    emit view_->status_message(AicTranscriptView::tr("Nothing in this block to chart."));
                return;
            }
            if (chart_->accessibleName().isEmpty())
                chart_->setAccessibleName(AicTranscriptView::tr("Chart of this block"));
            vl_->addWidget(chart_);
            chart_->show();
            tb_relayout_up(this);
        }
        retranslate();
    }
    void drop_chart() {
        if (!chart_)
            return;
        vl_->removeWidget(chart_);
        chart_->hide();
        chart_->deleteLater();
        chart_ = nullptr;
        tb_relayout_up(this);
    }

    QPointer<AicTranscriptView> view_;
    BlockKind kind_;
    QVBoxLayout* vl_ = nullptr;
    QLabel* label_ = nullptr;
    AicTbGrid* grid_ = nullptr;
    QWidget* actions_ = nullptr;
    QPushButton* csv_ = nullptr;
    QPushButton* chart_btn_ = nullptr;
    QWidget* chart_ = nullptr;
    Block block_;
    bool first_ = true;
};

} // namespace

// ── AicTranscriptView: block widgets ─────────────────────────────────────────

QWidget* AicTranscriptView::create_block_widget(AicTranscriptView* view, quint64 turn_id, const Block& block,
                                                bool caret, QWidget* parent) {
    QPointer<AicTranscriptView> pv(view);
    // Links (ticker chips, web links) and the right-click menu go through the view.
    auto wire = [pv, turn_id](QLabel* l) {
        QObject::connect(l, &QLabel::linkActivated, l, [pv, turn_id](const QString& link) {
            if (pv)
                pv->open_link(turn_id, link);
        });
        QObject::connect(l, &QLabel::linkHovered, l, [l](const QString& link) { l->setProperty(tb_kHoverProp, link); });
        QObject::connect(l, &QWidget::customContextMenuRequested, l, [pv, turn_id, l](const QPoint& pos) {
            if (pv)
                pv->show_text_menu(turn_id, l->mapToGlobal(pos), l->property(tb_kHoverProp).toString(),
                                   l->selectedText());
        });
    };
    QWidget* w = nullptr;
    switch (block.kind) {
        case BlockKind::Paragraph:
        case BlockKind::Heading:
        case BlockKind::Quote: {
            QLabel* l = tb_new_text_label(block.kind, parent);
            wire(l);
            w = l;
            break;
        }
        case BlockKind::List:
            if (block.series.valid()) {
                auto* box = new AicTbBox(view, BlockKind::List, parent);
                wire(box->label());
                w = box;
            } else {
                QLabel* l = tb_new_text_label(BlockKind::List, parent);
                wire(l);
                w = l;
            }
            break;
        case BlockKind::Code:
            w = new AicTbCode(view, parent);
            break;
        case BlockKind::Table: {
            auto* box = new AicTbBox(view, BlockKind::Table, parent);
            AicTbGrid* grid = box->grid();
            QObject::connect(grid, &QWidget::customContextMenuRequested, grid, [pv, turn_id, grid](const QPoint& pos) {
                if (pv)
                    pv->show_text_menu(turn_id, grid->mapToGlobal(pos), QString(), grid->cell_at(pos));
            });
            w = box;
            break;
        }
        case BlockKind::Rule: {
            auto* f = new QFrame(parent);
            f->setObjectName(QLatin1String(style::kHairline));
            f->setFixedHeight(1);
            w = f;
            break;
        }
        case BlockKind::Math: {
            auto* l = new QLabel(parent);
            l->setObjectName(QLatin1String(style::kMath));
            l->setTextFormat(Qt::PlainText);
            l->setWordWrap(true);
            l->setTextInteractionFlags(Qt::TextSelectableByMouse);
            l->setFocusPolicy(Qt::ClickFocus);
            l->setContextMenuPolicy(Qt::CustomContextMenu);
            QSizePolicy sp(QSizePolicy::Ignored, QSizePolicy::Preferred);
            sp.setHeightForWidth(true);
            l->setSizePolicy(sp);
            wire(l);
            w = l;
            break;
        }
    }
    if (!w)
        w = new QWidget(parent);
    w->setProperty(tb_kKindProp, int(block.kind));
    update_block_widget(w, block, caret);
    w->ensurePolished();
    return w;
}

bool AicTranscriptView::update_block_widget(QWidget* widget, const Block& block, bool caret) {
    if (!widget || widget->property(tb_kKindProp).toInt() != int(block.kind))
        return false;
    switch (block.kind) {
        case BlockKind::Paragraph:
        case BlockKind::Heading:
        case BlockKind::Quote: {
            auto* l = qobject_cast<QLabel*>(widget);
            if (!l)
                return false;
            if (block.kind == BlockKind::Heading)
                tb_set_heading_level(l, block.level);
            tb_set_label(l, block.html, caret);
            return true;
        }
        case BlockKind::List: {
            auto* box = dynamic_cast<AicTbBox*>(widget);
            if (block.series.valid() != (box != nullptr))
                return false; // a list became (or stopped being) chartable: recreate
            if (box) {
                box->set_block(block, caret);
                return true;
            }
            auto* l = qobject_cast<QLabel*>(widget);
            if (!l)
                return false;
            tb_set_label(l, tb_list_html(block), caret);
            return true;
        }
        case BlockKind::Code: {
            auto* code = dynamic_cast<AicTbCode*>(widget);
            if (!code)
                return false;
            code->set_block(block);
            return true;
        }
        case BlockKind::Table: {
            auto* box = dynamic_cast<AicTbBox*>(widget);
            if (!box)
                return false;
            box->set_block(block, caret);
            return true;
        }
        case BlockKind::Rule:
            return true;
        case BlockKind::Math: {
            auto* l = qobject_cast<QLabel*>(widget);
            if (!l)
                return false;
            if (l->text() != block.code)
                l->setText(block.code);
            return true;
        }
    }
    return false;
}

void AicTranscriptView::restyle_block_widget(QWidget* widget, bool rerender_html) {
    if (!widget)
        return;
    if (auto* box = dynamic_cast<AicTbBox*>(widget)) {
        box->restyle(rerender_html);
        return;
    }
    if (auto* code = dynamic_cast<AicTbCode*>(widget)) {
        code->restyle();
        return;
    }
    const auto kind = static_cast<BlockKind>(widget->property(tb_kKindProp).toInt());
    if (kind == BlockKind::Rule || kind == BlockKind::Math)
        return;
    if (auto* l = qobject_cast<QLabel*>(widget)) {
        if (rerender_html || l->property(tb_kGenProp).toULongLong() != style::inline_generation())
            tb_render(l);
    }
}

} // namespace fincept::screens::aichat
