#include "screens/derivatives/DerivCommon.h"

#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/FormGridMetrics.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>
#include <initializer_list>
#include <utility>

namespace fincept::screens::deriv {

namespace fmt = fincept::ui::formatting;
using namespace fincept::ui;

namespace {

/// Fills `$NAME$` placeholders by name. The sheet used to be one positional .arg() chain over
/// %1..%19 whose template never used %6, %14 or %15: QString::arg() always fills the lowest
/// number still present, so from POSITIVE on every colour landed one or more slots late —
/// CYAN in the TEXT_ON_ACCENT slot (cyan text on every accent fill), POSITIVE as the hover
/// wash, BG_SURFACE as every border, NEGATIVE as disabled text, TEXT_ON_ACCENT as the zebra
/// row. Named placeholders cannot drift.
QString dc_fill(QString css, std::initializer_list<std::pair<const char*, QString>> values) {
    for (const auto& [key, value] : values)
        css.replace(QLatin1Char('$') + QLatin1String(key) + QLatin1Char('$'), value);
    return css;
}

} // namespace

QString screen_style() {
    static const char* const kSheet =
        "#derivScreen, QDialog#derivDialog { background: $BASE$; }"
        // Screen header: one line — title, the page segments, then chip / refresh / F&O link.
        "#derivHeader { background: $RAISED$; border-bottom: 1px solid $ACCENT$; }"
        "#derivHeaderTitle { color: $ACCENT$; font-size: $F3$px; font-weight: 700; letter-spacing: 1px; "
        "  background: transparent; padding-right: 8px; }"
        "#derivHeaderSub { color: $TEXT2$; font-size: $F5$px; letter-spacing: 0.5px; background: transparent; }"
        "#derivHeaderBadge { color: $TEXT2$; font-size: $F6$px; font-weight: 700; border: 1px solid $BORDER$; "
        "  padding: 1px 6px; background: transparent; }"
        "#derivHeaderIcon { background: transparent; border: 1px solid transparent; padding: 2px; }"
        "#derivHeaderIcon:hover, #derivHeaderIcon:focus { background: $HOVER$; border-color: $BORDER$; }"
        "#derivPageBar { background: transparent; }"
        "#derivInstrBar { background: $BASE$; border-bottom: 1px solid $BORDER$; }"
        // Segments: the active one is an accent fill with TEXT_ON_ACCENT, whatever else applies.
        "#derivInstrBtn, #derivPageBtn, #derivSubBtn { background: transparent; color: $TEXT2$; border: none; "
        "  font-size: $F5$px; font-weight: 700; letter-spacing: 0.5px; padding: 4px 10px; }"
        "#derivInstrBtn:hover, #derivPageBtn:hover, #derivSubBtn:hover { color: $TEXT$; background: $HOVER$; }"
        "#derivInstrBtn:focus, #derivPageBtn:focus, #derivSubBtn:focus { color: $TEXT$; text-decoration: underline; }"
        "#derivInstrBtn[active=\"true\"], #derivPageBtn[active=\"true\"] { background: $ACCENT$; color: $ON_ACCENT$; }"
        "#derivInstrBtn[active=\"true\"]:hover, #derivPageBtn[active=\"true\"]:hover, "
        "#derivInstrBtn[active=\"true\"]:focus, #derivPageBtn[active=\"true\"]:focus "
        "  { background: $ACCENT_DIM$; color: $ON_ACCENT$; }"
        "#derivSubBtn[active=\"true\"] { color: $ACCENT$; border-bottom: 2px solid $ACCENT$; }"
        "#derivLinkBtn { background: transparent; color: $CYAN$; border: 1px solid $BORDER$; padding: 2px 10px; "
        "  font-size: $F5$px; font-weight: 700; }"
        "#derivLinkBtn:hover, #derivLinkBtn:focus { color: $TEXT$; border-color: $BORDER_HI$; }"
        "#derivLinkBtn:disabled { color: $TEXT3$; border-color: $BORDER$; }"
        // Panels, strips and their text.
        "#derivPanel { background: $SURFACE$; border: 1px solid $BORDER$; }"
        "#derivPanelHeader { background: $RAISED$; border-bottom: 1px solid $BORDER$; }"
        "#derivPanelTitle { color: $TEXT$; font-size: $F3$px; font-weight: 700; background: transparent; }"
        "#derivPanelTag { color: $ACCENT$; font-size: $F6$px; font-weight: 700; border: 1px solid $ACCENT$; "
        "  padding: 0px 4px; background: transparent; }"
        "#derivBar { background: $SURFACE$; border: none; border-bottom: 1px solid $BORDER$; }"
        "#derivLabel { color: $TEXT2$; font-size: $F5$px; font-weight: 700; letter-spacing: 0.5px; "
        "  background: transparent; }"
        "#derivSection { color: $ACCENT$; font-size: $F5$px; font-weight: 700; letter-spacing: 0.5px; "
        "  background: transparent; }"
        "#derivNote, #derivResultNote { color: $TEXT3$; font-size: $F5$px; background: transparent; }"
        "#derivStatusLine { color: $TEXT2$; font-size: $F5$px; background: transparent; }"
        "#derivChip { color: $TEXT2$; font-size: $F6$px; font-weight: 700; border: 1px solid $BORDER$; "
        "  padding: 1px 6px; background: transparent; }"
        "#derivChip[tone=\"warn\"] { color: $WARN$; border-color: $WARN$; }"
        "#derivChip[tone=\"info\"] { color: $CYAN$; border-color: $CYAN$; }"
        // Inputs.
        "QDoubleSpinBox, QSpinBox, QDateEdit, QComboBox { background: $BASE$; color: $TEXT$; "
        "  border: 1px solid $BORDER$; padding: 3px 6px; font-size: $F2$px; }"
        "QDoubleSpinBox:hover, QSpinBox:hover, QDateEdit:hover, QComboBox:hover { border-color: $BORDER_HI$; }"
        "QDoubleSpinBox:focus, QSpinBox:focus, QDateEdit:focus, QComboBox:focus { border-color: $ACCENT$; }"
        "QDoubleSpinBox:disabled, QSpinBox:disabled, QDateEdit:disabled, QComboBox:disabled "
        "  { color: $TEXT3$; background: $SURFACE$; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button, QSpinBox::up-button, QSpinBox::down-button "
        "  { width: 0; }"
        "QDateEdit::drop-down, QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background: $RAISED$; color: $TEXT$; border: 1px solid $BORDER$; "
        "  selection-background-color: $ACCENT$; selection-color: $ON_ACCENT$; }"
        "QCheckBox { color: $TEXT$; font-size: $F4$px; background: transparent; spacing: 6px; }"
        // Buttons: the primary action is an accent fill with TEXT_ON_ACCENT.
        "#derivCalcBtn { background: $ACCENT$; color: $ON_ACCENT$; border: 1px solid $ACCENT$; padding: 4px 14px; "
        "  font-size: $F5$px; font-weight: 700; letter-spacing: 0.5px; }"
        "#derivCalcBtn:hover { background: $ACCENT_DIM$; border-color: $ACCENT_DIM$; color: $ON_ACCENT$; }"
        "#derivCalcBtn:focus { border-color: $TEXT$; color: $ON_ACCENT$; }"
        "#derivCalcBtn:disabled { background: $RAISED$; color: $TEXT3$; border-color: $BORDER$; }"
        "#derivToolBtn { background: transparent; color: $TEXT2$; border: 1px solid $BORDER$; padding: 3px 9px; "
        "  font-size: $F5$px; font-weight: 700; }"
        "#derivToolBtn:hover, #derivToolBtn:focus { color: $TEXT$; border-color: $BORDER_HI$; }"
        "#derivToolBtn:disabled { color: $TEXT3$; }"
        // Results.
        "#derivResultsPanel { background: $SURFACE$; border: 1px solid $BORDER$; }"
        "#derivResultsHeader { background: $RAISED$; border-bottom: 1px solid $BORDER$; }"
        "#derivResultsTitle { color: $TEXT$; font-size: $F3$px; font-weight: 700; background: transparent; }"
        "#derivResultsMeta { color: $TEXT3$; font-size: $F5$px; letter-spacing: 0.5px; background: transparent; }"
        // Status strip.
        "#derivStatusBar { background: $RAISED$; border-top: 1px solid $BORDER$; }"
        "#derivStatusText { color: $TEXT2$; font-size: $F5$px; background: transparent; }"
        "#derivStatusEngine { color: $CYAN$; font-size: $F5$px; background: transparent; }"
        // Tables and lists: dense, zebra per the token, a readable selection.
        "#derivTable { background: $BASE$; color: $TEXT$; border: 1px solid $BORDER$; gridline-color: $BORDER$; "
        "  font-size: $F4$px; selection-background-color: $ACCENT_BG$; selection-color: $TEXT$; "
        "  alternate-background-color: $ROW_ALT$; }"
        "#derivTable QHeaderView::section { background: $RAISED$; color: $TEXT2$; border: none; "
        "  border-right: 1px solid $BORDER$; border-bottom: 1px solid $BORDER$; padding: 2px 6px; "
        "  font-size: $F5$px; font-weight: 700; }"
        "#derivTable QTableCornerButton::section { background: $RAISED$; border: none; }"
        "#derivList { background: $BASE$; color: $TEXT$; border: 1px solid $BORDER$; font-size: $F4$px; }"
        "#derivList::item { padding: 2px 6px; }"
        "#derivList::item:hover { background: $HOVER$; }"
        "#derivList::item:selected { background: $ACCENT$; color: $ON_ACCENT$; }"
        "QScrollArea#derivScroll { background: $BASE$; border: none; }";
    return dc_fill(QString::fromLatin1(kSheet),
                   {{"BASE", colors::BG_BASE()},
                    {"SURFACE", colors::BG_SURFACE()},
                    {"RAISED", colors::BG_RAISED()},
                    {"HOVER", colors::BG_HOVER()},
                    {"BORDER_HI", colors::BORDER_BRIGHT()},
                    {"BORDER", colors::BORDER_DIM()},
                    {"TEXT2", colors::TEXT_SECONDARY()},
                    {"TEXT3", colors::TEXT_TERTIARY()},
                    {"TEXT", colors::TEXT_PRIMARY()},
                    {"ACCENT_DIM", colors::AMBER_DIM()},
                    {"ACCENT_BG", colors::ACCENT_BG()},
                    {"ACCENT", colors::AMBER()},
                    {"ON_ACCENT", colors::TEXT_ON_ACCENT()},
                    {"CYAN", colors::CYAN()},
                    {"WARN", colors::WARNING()},
                    {"ROW_ALT", colors::ROW_ALT()},
                    {"F2", fonts::px(-2)},
                    {"F3", fonts::px(-3)},
                    {"F4", fonts::px(-4)},
                    {"F5", fonts::px(-5)},
                    {"F6", fonts::px(-6)}});
}

QDoubleSpinBox* make_spin(QWidget* parent, double value, double lo, double hi, int decimals, const QString& suffix) {
    auto* s = new QDoubleSpinBox(parent);
    s->setRange(lo, hi);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s->setKeyboardTracking(false);
    if (!suffix.isEmpty())
        s->setSuffix(QLatin1Char(' ') + suffix);
    return s;
}

QSpinBox* make_int_spin(QWidget* parent, int value, int lo, int hi, const QString& suffix) {
    auto* s = new QSpinBox(parent);
    s->setRange(lo, hi);
    s->setValue(value);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s->setKeyboardTracking(false);
    if (!suffix.isEmpty())
        s->setSuffix(QLatin1Char(' ') + suffix);
    return s;
}

QComboBox* make_combo(QWidget* parent, const QStringList& items) {
    auto* c = new QComboBox(parent);
    c->addItems(items);
    return c;
}

QDateEdit* make_date(QWidget* parent, const QDate& date) {
    auto* d = new QDateEdit(date, parent);
    d->setCalendarPopup(true);
    d->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    return d;
}

QPushButton* make_button(QWidget* parent, const QString& text, bool primary) {
    auto* b = new QPushButton(literal_ampersands(text), parent);
    b->setObjectName(primary ? QStringLiteral("derivCalcBtn") : QStringLiteral("derivToolBtn"));
    b->setCursor(Qt::PointingHandCursor);
    // Natural width and height wherever it is put: never a full-width slab in a column.
    b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return b;
}

QLabel* add_field(QGridLayout* grid, int row, int col, const QString& caption, QWidget* input, int col_span) {
    // Caption on grid row 2·row, input on 2·row + 1: both stay children of the grid's widget
    // (the input was created there by the caller), nothing is re-parented.
    auto* label = new QLabel(caption, grid->parentWidget());
    label->setObjectName(QStringLiteral("derivLabel"));
    label->setBuddy(input);
    // A long caption wraps in a narrow column instead of setting the row's minimum width (no
    // horizontal clipping at 1280 px with a large font); it sits on its input either way.
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    grid->addWidget(label, 2 * row, col, 1, col_span);
    grid->addWidget(input, 2 * row + 1, col, 1, col_span);
    return label;
}

Card make_card(QWidget* parent, const QString& tag, const QString& title) {
    // The header used to be a plain QWidget with a Preferred vertical policy next to a body
    // with the same policy and no stretch: a card given more height than its content (a
    // QStackedWidget sizes every page to its tallest one) handed half the spare height to the
    // header, and the tag label (Preferred too) grew into a box the height of the band. Now the
    // header is the shared one-line ui::PanelHeader (Fixed, height from the font), the tag is a
    // Fixed chip, and the body takes every spare pixel with its content at the top.
    Card c;
    c.frame = new QWidget(parent);
    c.frame->setObjectName(QStringLiteral("derivPanel"));
    auto* vl = new QVBoxLayout(c.frame);
    vl->setContentsMargins(1, 1, 1, 1); // the panel's hairline border stays visible around the header
    vl->setSpacing(0);
    c.header = new PanelHeader(title, c.frame);
    c.title = c.header->title_label();
    c.tag = new QLabel(tag, c.header);
    c.tag->setObjectName(QStringLiteral("derivPanelTag"));
    c.tag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    c.tag->setAlignment(Qt::AlignCenter);
    if (auto* row = qobject_cast<QHBoxLayout*>(c.header->layout()))
        row->insertWidget(1, c.tag, 0, Qt::AlignVCenter); // after the accent tick, before the title
    vl->addWidget(c.header);
    auto* body = new QWidget(c.frame);
    c.body = new QVBoxLayout(body);
    c.body->setContentsMargins(12, 10, 12, 12);
    c.body->setSpacing(8);
    c.body->setAlignment(Qt::AlignTop); // content at the top unless something in it expands
    vl->addWidget(body, 1);
    return c;
}

QWidget* make_strip(QWidget* parent, QHBoxLayout** layout) {
    auto* bar = new QWidget(parent);
    bar->setObjectName(QStringLiteral("derivBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 3, 8, 3);
    hl->setSpacing(6);
    if (layout)
        *layout = hl;
    return bar;
}

void compact_grid(QGridLayout* grid) {
    if (!grid)
        return;
    grid->setColumnStretch(grid->columnCount(), 1);
}

void dense_table(QTableView* table) {
    if (!table)
        return;
    table->setObjectName(QStringLiteral("derivTable"));
    table->setAlternatingRowColors(true);
    table->setWordWrap(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setHighlightSections(false);
    // The stylesheet gives the table base-4 px text; the row is that line plus a little air.
    auto fit = [table] {
        const int h = QFontMetrics(fonts::font(-4)).height() + 6;
        if (table->verticalHeader()->defaultSectionSize() != h)
            table->verticalHeader()->setDefaultSectionSize(h);
    };
    fit();
    QObject::connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, table, fit);
}

// ── FormFit ──────────────────────────────────────────────────────────────────

void FormFit::add(QWidget* input, QLabel* caption, int span) {
    if (caption) {
        caption->setWordWrap(true);
        caption->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
        entries_.append({QPointer<QWidget>(caption), span});
    }
    if (input)
        entries_.append({QPointer<QWidget>(input), span});
    refit();
}

void FormFit::add_wide(QWidget* w, int span) {
    if (!w)
        return;
    entries_.append({QPointer<QWidget>(w), span});
    refit();
}

int FormFit::field_px() const {
    // The inputs' font (screen_style: base-2 px) and box: 2 × 6 px padding, 2 × 1 px border and
    // the 18 px drop-down of a date or combo box.
    const QFontMetrics fm(fonts::font(-2));
    return form_grid::field_px(fm.horizontalAdvance(QLatin1Char('0')), chars_, 32);
}

int FormFit::span_px(int span) const {
    return form_grid::span_px(field_px(), span, kGap);
}

void FormFit::refit() const {
    const int field = field_px();
    for (const Entry& e : entries_) {
        if (!e.w)
            continue;
        const int w = form_grid::span_px(field, e.span, kGap);
        if (e.w->minimumWidth() != w || e.w->maximumWidth() != w)
            e.w->setFixedWidth(w);
    }
}

QLabel* make_note(QWidget* parent, const QString& text) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("derivNote"));
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QLabel* make_section(QWidget* parent, const QString& text) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("derivSection"));
    return l;
}

QString num(double v, int decimals) {
    return fmt::number(v, decimals);
}

QString signed_num(double v, int decimals) {
    return fmt::signed_number(v, decimals);
}

QString money(double v, const QString& ccy) {
    return fmt::money(v, ccy);
}

QString signed_money(double v, const QString& ccy) {
    return fmt::signed_money(v, ccy);
}

QString vol_pct(double sigma, int decimals) {
    return fmt::fraction_pct(sigma, decimals, false);
}

QString pct_points(double percent, int decimals) {
    return fmt::pct(percent, decimals, false);
}

TimeSeriesChart* make_xy_chart(QWidget* parent, const QString& title, const QString& x_title,
                               const QVector<LineSpec>& lines, const QVector<std::tuple<double, double, QString>>& markers,
                               const std::function<QString(double)>& x_fmt, const std::function<QString(double)>& y_fmt) {
    auto* c = new TimeSeriesChart(parent);
    c->set_x_is_time(false);
    c->set_title(title);
    c->set_show_mode_buttons(false);
    c->set_export_name(title);
    const auto xf = x_fmt ? x_fmt : std::function<QString(double)>([](double x) { return fmt::number(x, 2); });
    const QString xt = x_title;
    c->set_time_formatter([xf, xt](double x) { return xt.isEmpty() ? xf(x) : xt + QLatin1Char(' ') + xf(x); });
    c->set_tick_time_formatter(xf);
    if (y_fmt)
        c->set_value_formatter(TimeSeriesChart::Axis::Left, y_fmt);
    int first = -1;
    for (const LineSpec& l : lines) {
        if (l.points.size() < 2)
            continue;
        TimeSeriesChart::SeriesSpec s;
        s.name = l.name;
        s.points = l.points;
        s.dashed = l.dashed;
        s.rebase = false;
        if (l.muted) {
            s.color = TimeSeriesChart::ColorRole::Muted;
        } else if (l.palette >= 0) {
            s.color = TimeSeriesChart::ColorRole::Palette;
            s.palette_index = l.palette;
        }
        const int h = c->add_series(s);
        if (first < 0)
            first = h;
    }
    for (const auto& [x, y, label] : markers)
        if (std::isfinite(x) && std::isfinite(y))
            c->add_price_marker(x, y, TimeSeriesChart::MarkerKind::Custom, label, label);
    // A floor, not a size: the chart takes the spare height of its panel (stretch at the call
    // site); 12 lines keeps a large font from forcing a scroll at 1280 x 720.
    c->setMinimumHeight(QFontMetrics(c->font()).height() * 12);
    return c;
}

// ── DerivResultBar ───────────────────────────────────────────────────────────

DerivResultBar::DerivResultBar(const QString& export_name, QWidget* parent) : QWidget(parent), export_name_(export_name) {
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    copy_btn_ = make_button(this, QString(), false);
    csv_btn_ = make_button(this, QString(), false);
    png_btn_ = make_button(this, QString(), false);
    hl->addWidget(copy_btn_);
    hl->addWidget(csv_btn_);
    hl->addWidget(png_btn_);
    report_btn_ = new AddToReportButton(QStringLiteral("Derivatives"), this);
    report_btn_->set_show_result(false);
    hl->addWidget(report_btn_);
    connect(copy_btn_, &QPushButton::clicked, this, &DerivResultBar::copy);
    connect(csv_btn_, &QPushButton::clicked, this, &DerivResultBar::save_csv);
    connect(png_btn_, &QPushButton::clicked, this, &DerivResultBar::save_png);
    png_btn_->setEnabled(false);
    retranslate();
}

void DerivResultBar::set_table(TableFn fn) {
    table_ = std::move(fn);
}

void DerivResultBar::set_image(ImageFn fn) {
    image_ = std::move(fn);
    png_btn_->setEnabled(static_cast<bool>(image_));
}

void DerivResultBar::set_payload(PayloadFn fn) {
    report_btn_->set_payload_provider(std::move(fn));
}

void DerivResultBar::retranslate() {
    copy_btn_->setText(tr("COPY"));
    copy_btn_->setToolTip(tr("Copy the result as tab-separated text (pastes into a spreadsheet)"));
    csv_btn_->setText(tr("CSV"));
    csv_btn_->setToolTip(tr("Save the result table as a CSV file"));
    png_btn_->setText(tr("PNG"));
    png_btn_->setToolTip(tr("Save the chart as an image"));
}

void DerivResultBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivResultBar::copy() {
    if (!table_)
        return;
    QApplication::clipboard()->setText(table_tsv(table_()));
}

void DerivResultBar::save_csv() {
    if (!table_)
        return;
    const ChartExport::Table t = table_();
    ChartExport::save_csv(t.headers, t.rows, export_name_, this);
}

void DerivResultBar::save_png() {
    if (!image_)
        return;
    const QPixmap pm = image_();
    if (!pm.isNull())
        ChartExport::save_png(pm, export_name_, this);
}

QString TrBook::text(const char* src) const {
    return QCoreApplication::translate(context_, src);
}

void TrBook::label(QLabel* w, const char* src) {
    entries_.append({QPointer<QWidget>(w), 0, {src}});
}

void TrBook::button(QPushButton* w, const char* src) {
    entries_.append({QPointer<QWidget>(w), 1, {src}});
}

void TrBook::tooltip(QWidget* w, const char* src) {
    entries_.append({QPointer<QWidget>(w), 2, {src}});
}

void TrBook::combo(QComboBox* w, const QVector<const char*>& items) {
    entries_.append({QPointer<QWidget>(w), 3, items});
}

void TrBook::apply() const {
    for (const Entry& e : entries_) {
        if (!e.w || e.src.isEmpty())
            continue;
        switch (e.kind) {
            case 0:
                static_cast<QLabel*>(e.w.data())->setText(text(e.src.first()));
                break;
            case 1:
                static_cast<QPushButton*>(e.w.data())->setText(literal_ampersands(text(e.src.first())));
                break;
            case 2:
                e.w->setToolTip(text(e.src.first()));
                break;
            case 3: {
                auto* c = static_cast<QComboBox*>(e.w.data());
                for (int i = 0; i < c->count() && i < e.src.size(); ++i)
                    c->setItemText(i, text(e.src[i]));
                break;
            }
        }
    }
}

QString table_tsv(const ChartExport::Table& t) {
    QStringList lines;
    auto clean = [](QString s) { return s.replace(QLatin1Char('\t'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' ')); };
    QStringList h;
    for (const QString& s : t.headers)
        h << clean(s);
    lines << h.join(QLatin1Char('\t'));
    for (const QStringList& r : t.rows) {
        QStringList row;
        for (const QString& s : r)
            row << clean(s);
        lines << row.join(QLatin1Char('\t'));
    }
    return lines.join(QLatin1Char('\n'));
}

QByteArray png_bytes(const QPixmap& pm) {
    QByteArray out;
    if (pm.isNull())
        return out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return out;
}

} // namespace fincept::screens::deriv
