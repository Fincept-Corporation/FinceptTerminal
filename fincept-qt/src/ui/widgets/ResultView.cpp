// src/ui/widgets/ResultView.cpp
//
// Widget side of the result renderer — see ResultView.h. The shape decisions
// are made by result::build() (ResultLayout.cpp); this file only maps each
// block to a kit component. Colours and font sizes come from the theme tokens;
// a theme / font change rebuilds the view from the kept payload.

#include "ui/widgets/ResultView.h"

#include "ui/charts/BarChart.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/FanChart.h"
#include "ui/charts/Gauge.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/tables/DataTable.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/StatTile.h"

#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPair>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimeZone>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::ui {

namespace {

using result::Block;
using result::Kind;
using result::Unit;

constexpr int kRvMessagePage = 0;
constexpr int kRvContentPage = 1;
constexpr int kRvRawPage = 2;
constexpr int kRvRawCharCap = 2'000'000;

QString rv_tr(const char* text) {
    return ResultView::tr(text);
}

QString rv_title_style() {
    return QStringLiteral("color:%1; background:transparent; font-size:%2px; font-weight:700; letter-spacing:0.5px;")
        .arg(colors::TEXT_SECONDARY())
        .arg(fonts::px(-2));
}

QLabel* rv_title(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text.toUpper(), parent);
    l->setStyleSheet(rv_title_style());
    return l;
}

// A titled frame: header row (title, optional extras on the right) + body.
QFrame* rv_card(const QString& title, const QString& subtitle, QHBoxLayout** header, QVBoxLayout** body) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("rvCard"));
    card->setStyleSheet(QStringLiteral("QFrame#rvCard { background:%1; border:1px solid %2; }")
                            .arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 10);
    vl->setSpacing(6);
    auto* hl = new QHBoxLayout;
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    if (!title.isEmpty())
        hl->addWidget(rv_title(title, card));
    if (!subtitle.isEmpty()) {
        auto* sub = new QLabel(subtitle, card);
        sub->setStyleSheet(QStringLiteral("color:%1; background:transparent; font-size:%2px;")
                               .arg(colors::TEXT_TERTIARY())
                               .arg(fonts::px(-3)));
        hl->addWidget(sub);
    }
    hl->addStretch(1);
    vl->addLayout(hl);
    *header = hl;
    *body = vl;
    return card;
}

QString rv_toggle_style() {
    return QStringLiteral("QPushButton { color:%1; background:transparent; border:1px solid %2;"
                          " padding:1px 8px; font-size:%3px; font-weight:600; }"
                          "QPushButton:hover { color:%4; border-color:%5; }"
                          "QPushButton:checked { color:%6; border-color:%6; }")
        .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM())
        .arg(fonts::px(-3))
        .arg(colors::TEXT_PRIMARY(), colors::BORDER_MED(), colors::AMBER());
}

QPushButton* rv_toggle_button(const QString& text, QWidget* parent) {
    auto* b = new QPushButton(text, parent);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    b->setStyleSheet(rv_toggle_style());
    return b;
}

QTableWidgetItem* rv_cell(const QJsonValue& v, Unit unit, const result::Options& o, bool signed_col) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        auto* item = new NumericTableWidgetItem(result::format_number(d, unit, o, signed_col), d);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QColor fg(colors::TEXT_PRIMARY());
        if (signed_col && d > 0)
            fg = QColor(colors::POSITIVE());
        else if (signed_col && d < 0)
            fg = QColor(colors::NEGATIVE());
        item->setForeground(fg);
        item->setToolTip(QString::number(d, 'g', 15));
        return item;
    }
    if (v.isNull()) {
        auto* item = new NumericTableWidgetItem(result::format_value(v, unit, o), std::numeric_limits<double>::lowest());
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setForeground(QColor(colors::TEXT_TERTIARY()));
        item->setToolTip(rv_tr("Not a finite number (±∞ or NaN) for these inputs."));
        return item;
    }
    const QString text = v.isUndefined() ? QString() : result::format_value(v, Unit::Text, o);
    auto* item = new QTableWidgetItem(text);
    item->setForeground(QColor(colors::TEXT_PRIMARY()));
    if (text.size() > 40)
        item->setToolTip(text);
    return item;
}

// Height that shows up to `max_rows` rows without an inner scroll bar.
void rv_fit_height(QTableWidget* t, int max_rows) {
    const int rows = std::min(t->rowCount(), max_rows);
    int h = t->horizontalHeader()->isHidden() ? 0 : t->horizontalHeader()->sizeHint().height();
    h += rows * t->verticalHeader()->defaultSectionSize();
    h += 2 * t->frameWidth() + 2;
    if (t->columnCount() > 5)
        h += t->horizontalScrollBar()->sizeHint().height();
    t->setFixedHeight(std::max(h, 56));
}

DataTable* rv_table(const QStringList& headers) {
    auto* t = new DataTable;
    t->set_headers(headers);
    t->setSelectionMode(QAbstractItemView::ExtendedSelection);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    return t;
}

void rv_finish_table(DataTable* t, int max_rows) {
    t->resizeColumnsToContents();
    // Keep the payload's row order until the user clicks a header.
    t->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    t->setSortingEnabled(true);
    rv_fit_height(t, max_rows);
}

QString rv_x_text(double x, const Block& b, const result::Options& o) {
    if (b.x_dates)
        return QDateTime::fromMSecsSinceEpoch(qint64(x), QTimeZone::UTC).date().toString(Qt::ISODate);
    return result::format_number(x, b.x_unit, o);
}

double rv_quantile(QVector<double> v, double q) {
    v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return !std::isfinite(x); }), v.end());
    if (v.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    std::sort(v.begin(), v.end());
    const double pos = q * double(v.size() - 1);
    const int i = int(std::floor(pos));
    const double f = pos - i;
    return i + 1 < v.size() ? v[i] * (1 - f) + v[i + 1] * f : v[i];
}

KpiStrip* rv_stats_strip(const QVector<QPair<QString, QString>>& items) {
    auto* s = new KpiStrip;
    for (const auto& it : items) {
        StatTile* t = s->add_tile(it.first);
        t->set_value(it.second);
        t->set_compact(true);
    }
    return s;
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

ResultView::ResultView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("rvRoot"));
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    bar_ = new QWidget(this);
    auto* bl = new QHBoxLayout(bar_);
    bl->setContentsMargins(12, 6, 12, 0);
    bl->setSpacing(6);
    bl->addStretch(1);
    raw_btn_ = rv_toggle_button(tr("RAW"), bar_);
    raw_btn_->setToolTip(tr("Show the raw JSON response"));
    connect(raw_btn_, &QPushButton::toggled, this, [this](bool on) {
        if (has_result_)
            set_page(on ? kRvRawPage : kRvContentPage);
    });
    bl->addWidget(raw_btn_);
    vl->addWidget(bar_);
    bar_->hide();

    stack_ = new QStackedWidget(this);

    message_ = new QLabel(this);
    message_->setAlignment(Qt::AlignCenter);
    message_->setWordWrap(true);
    message_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    message_->setMinimumHeight(120);
    stack_->addWidget(message_); // kRvMessagePage

    page_ = new QWidget(this);
    auto* pl = new QVBoxLayout(page_);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);
    scroll_ = new QScrollArea(page_);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    content_ = new QWidget;
    content_->setObjectName(QStringLiteral("rvContent"));
    content_layout_ = new QVBoxLayout(content_);
    content_layout_->setContentsMargins(12, 10, 12, 12);
    content_layout_->setSpacing(10);
    scroll_->setWidget(content_);
    pl->addWidget(scroll_);
    stack_->addWidget(page_); // kRvContentPage

    raw_text_ = new QPlainTextEdit(this);
    raw_text_->setReadOnly(true);
    raw_text_->setLineWrapMode(QPlainTextEdit::NoWrap);
    stack_->addWidget(raw_text_); // kRvRawPage

    vl->addWidget(stack_, 1);

    overlay_ = new LoadingOverlay(this);

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        restyle();
        if (has_result_)
            rebuild();
    });
    restyle();
    show_idle(QString());
}

void ResultView::restyle() {
    content_->setStyleSheet(QStringLiteral("QWidget#rvContent { background:transparent; }"));
    scroll_->setStyleSheet(QStringLiteral("QScrollArea { background:transparent; border:none; }"));
    raw_text_->setStyleSheet(QStringLiteral("QPlainTextEdit { background:%1; color:%2; border:none; font-size:%3px; }")
                                 .arg(colors::BG_BASE(), colors::TEXT_PRIMARY())
                                 .arg(fonts::px(-1)));
    const bool error = message_->property("rvError").toBool();
    message_->setStyleSheet(QStringLiteral("color:%1; background:transparent; font-size:%2px; padding:16px;")
                                .arg(error ? colors::DANGER() : colors::TEXT_SECONDARY())
                                .arg(fonts::px(0)));
    raw_btn_->setStyleSheet(rv_toggle_style());
}

void ResultView::set_options(const result::Options& options) {
    options_ = options;
}

void ResultView::set_scrollable(bool on) {
    if (on == scrollable_)
        return;
    scrollable_ = on;
    auto* pl = static_cast<QVBoxLayout*>(page_->layout());
    if (on) {
        pl->removeWidget(content_);
        scroll_->setWidget(content_);
        scroll_->show();
    } else {
        scroll_->takeWidget();
        content_->setParent(page_);
        pl->addWidget(content_);
        content_->show();
        scroll_->hide();
    }
}

void ResultView::set_raw_toggle_visible(bool on) {
    raw_btn_->setVisible(on);
    bar_->setVisible(on && has_result_);
}

QWidget* ResultView::content_widget() const {
    return content_;
}

QWidget* ResultView::make_card(const QString& title, QWidget* content, const QString& subtitle) {
    QHBoxLayout* header = nullptr;
    QVBoxLayout* body = nullptr;
    QFrame* card = rv_card(title, subtitle, &header, &body);
    if (content) {
        content->setParent(card);
        body->addWidget(content);
    }
    return card;
}

// ── States ───────────────────────────────────────────────────────────────────

void ResultView::set_page(int page) {
    stack_->setCurrentIndex(page);
}

void ResultView::stop_loading() {
    loading_ = false;
    overlay_->hide_loading();
}

void ResultView::show_idle(const QString& message) {
    stop_loading();
    has_result_ = false;
    drop_custom_widgets();
    clear_content();
    idle_text_ = message;
    message_->setProperty("rvError", false);
    message_->setText(message);
    restyle();
    bar_->hide();
    set_page(kRvMessagePage);
}

void ResultView::clear() {
    show_idle(idle_text_);
}

void ResultView::show_loading(const QString& message) {
    loading_ = true;
    if (!has_result_) {
        message_->setProperty("rvError", false);
        message_->setText(QString());
        restyle();
        set_page(kRvMessagePage);
    }
    overlay_->show_loading(message.isEmpty() ? tr("COMPUTING…") : message);
}

void ResultView::show_error(const QString& message) {
    stop_loading();
    has_result_ = false;
    drop_custom_widgets();
    clear_content();
    message_->setProperty("rvError", true);
    message_->setText(message);
    restyle();
    bar_->hide();
    set_page(kRvMessagePage);
}

void ResultView::show_result(const QJsonValue& payload, const QJsonValue& raw) {
    stop_loading();
    drop_custom_widgets();
    payload_ = payload;
    raw_ = raw.isUndefined() ? payload : raw;
    has_result_ = true;

    QByteArray text;
    if (raw_.isObject())
        text = QJsonDocument(raw_.toObject()).toJson(QJsonDocument::Indented);
    else if (raw_.isArray())
        text = QJsonDocument(raw_.toArray()).toJson(QJsonDocument::Indented);
    else
        text = QJsonDocument(QJsonArray{raw_}).toJson(QJsonDocument::Compact);
    if (text.size() > kRvRawCharCap) {
        const qsizetype total = text.size();
        text.truncate(kRvRawCharCap);
        text += "\n… " + tr("truncated: %1 of %2 bytes shown").arg(kRvRawCharCap).arg(total).toUtf8();
    }
    raw_text_->setPlainText(QString::fromUtf8(text));

    rebuild();
    bar_->setVisible(raw_btn_->isVisibleTo(bar_));
    set_page(raw_btn_->isChecked() ? kRvRawPage : kRvContentPage);
}

void ResultView::add_custom_widget(QWidget* widget) {
    if (!widget)
        return;
    // The caller's widget is the content: drop the "no data" note of an all-consumed payload.
    if (auto* empty = content_->findChild<QLabel*>(QStringLiteral("rvEmpty"), Qt::FindDirectChildrenOnly)) {
        content_layout_->removeWidget(empty);
        empty->hide();
        empty->deleteLater();
    }
    custom_top_.append(widget);
    widget->setParent(content_);
    const int at = std::min(custom_index_ + int(custom_top_.size()) - 1, content_layout_->count());
    content_layout_->insertWidget(at, widget);
    widget->show();
}

void ResultView::append_custom_widget(QWidget* widget) {
    if (!widget)
        return;
    if (auto* empty = content_->findChild<QLabel*>(QStringLiteral("rvEmpty"), Qt::FindDirectChildrenOnly)) {
        content_layout_->removeWidget(empty);
        empty->hide();
        empty->deleteLater();
    }
    custom_end_.append(widget);
    widget->setParent(content_);
    // Before the trailing stretch.
    content_layout_->insertWidget(std::max(0, content_layout_->count() - 1), widget);
    widget->show();
}

void ResultView::drop_custom_widgets() {
    for (const auto& w : custom_top_)
        if (w)
            w->deleteLater();
    for (const auto& w : custom_end_)
        if (w)
            w->deleteLater();
    custom_top_.clear();
    custom_end_.clear();
}

void ResultView::clear_content() {
    while (content_layout_->count() > 0) {
        QLayoutItem* item = content_layout_->takeAt(0);
        if (QWidget* w = item->widget()) {
            const bool custom = std::any_of(custom_top_.begin(), custom_top_.end(), [w](const QPointer<QWidget>& p) {
                                    return p == w;
                                }) ||
                                std::any_of(custom_end_.begin(), custom_end_.end(),
                                            [w](const QPointer<QWidget>& p) { return p == w; });
            w->hide();
            if (!custom)
                w->deleteLater();
        }
        delete item;
    }
    custom_index_ = 0;
}

void ResultView::rebuild() {
    clear_content();
    if (!has_result_)
        return;
    const Block root = result::build(payload_, options_);
    render_children(root, content_layout_, 0);
    if (content_layout_->count() == 0 && custom_top_.isEmpty() && custom_end_.isEmpty()) {
        auto* empty = new QLabel(tr("The calculation returned no data."), content_);
        empty->setObjectName(QStringLiteral("rvEmpty"));
        empty->setStyleSheet(QStringLiteral("color:%1; background:transparent; font-size:%2px; padding:16px;")
                                 .arg(colors::TEXT_SECONDARY())
                                 .arg(fonts::px(0)));
        empty->setAlignment(Qt::AlignCenter);
        content_layout_->addWidget(empty);
    }
    int at = custom_index_;
    for (const auto& w : custom_top_) {
        if (!w)
            continue;
        content_layout_->insertWidget(at++, w);
        w->show();
    }
    for (const auto& w : custom_end_) {
        if (!w)
            continue;
        content_layout_->addWidget(w);
        w->show();
    }
    content_layout_->addStretch(1);
}

void ResultView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        raw_btn_->setText(tr("RAW"));
        raw_btn_->setToolTip(tr("Show the raw JSON response"));
        if (has_result_)
            rebuild();
    }
    QWidget::changeEvent(e);
}

// ── Blocks ───────────────────────────────────────────────────────────────────

void ResultView::render_children(const Block& section, QVBoxLayout* into, int depth) {
    for (const Block& c : section.children) {
        QWidget* w = render_block(c, depth);
        if (!w)
            continue;
        into->addWidget(w);
        if (depth == 0 && c.kind == Kind::Kpis && into == content_layout_ && custom_index_ == 0)
            custom_index_ = into->count();
    }
}

QWidget* ResultView::render_block(const Block& b, int depth) {
    switch (b.kind) {
        case Kind::Kpis:
            return render_kpis(b, depth);
        case Kind::Fields:
            return render_fields(b);
        case Kind::Caption:
            return render_caption(b);
        case Kind::List:
            return render_list(b);
        case Kind::Table:
            return render_table(b);
        case Kind::Section:
            return render_section(b, depth);
        case Kind::Line:
        case Kind::Bars:
        case Kind::Donut:
        case Kind::Distribution:
        case Kind::Fan:
        case Kind::Heatmap:
        case Kind::Gauge:
            return render_chart_card(b);
    }
    return nullptr;
}

QWidget* ResultView::render_kpis(const Block& b, int depth) {
    if (b.fields.isEmpty())
        return nullptr;
    auto* strip = new KpiStrip;
    for (const result::Field& f : b.fields) {
        StatTile* t = strip->add_tile(f.label.toUpper());
        t->set_value(result::format_value(f.value, f.unit, options_, f.signed_result));
        if (depth > 0)
            t->set_compact(true);
        if (f.signed_result && f.value.isDouble()) {
            const double v = f.value.toDouble();
            if (v > 0)
                t->set_tone(StatTile::Tone::Positive);
            else if (v < 0)
                t->set_tone(StatTile::Tone::Negative);
        }
        QString tip = f.path;
        if (f.value.isDouble())
            tip += QStringLiteral(" = ") + QString::number(f.value.toDouble(), 'g', 15);
        else if (f.value.isNull())
            tip += QStringLiteral(": ") + tr("not a finite number (±∞ or NaN) for these inputs");
        t->set_tooltip_text(tip);
    }
    return strip;
}

QWidget* ResultView::render_fields(const Block& b) {
    if (b.fields.isEmpty())
        return nullptr;
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    if (!b.title.isEmpty())
        vl->addWidget(rv_title(b.title, box));
    DataTable* t = rv_table({tr("Metric"), tr("Value")});
    t->setRowCount(int(b.fields.size()));
    for (int r = 0; r < b.fields.size(); ++r) {
        const result::Field& f = b.fields.at(r);
        auto* k = new QTableWidgetItem(f.label);
        k->setForeground(QColor(colors::TEXT_SECONDARY()));
        k->setToolTip(f.path);
        t->setItem(r, 0, k);
        t->setItem(r, 1, rv_cell(f.value, f.unit, options_, f.signed_result));
    }
    rv_finish_table(t, 18);
    vl->addWidget(t);
    return box;
}

QWidget* ResultView::render_table(const Block& b) {
    if (b.columns.isEmpty())
        return nullptr;
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    auto* hl = new QHBoxLayout;
    hl->setContentsMargins(0, 0, 0, 0);
    if (!b.title.isEmpty())
        hl->addWidget(rv_title(b.title, box));
    const QString count = b.total_rows > b.rows.size()
                              ? tr("showing %1 of %2 rows").arg(b.rows.size()).arg(b.total_rows)
                              : tr("%n row(s)", nullptr, b.total_rows);
    auto* n = new QLabel(count, box);
    n->setStyleSheet(QStringLiteral("color:%1; background:transparent; font-size:%2px;")
                         .arg(colors::TEXT_TERTIARY())
                         .arg(fonts::px(-3)));
    hl->addWidget(n);
    hl->addStretch(1);
    vl->addLayout(hl);

    DataTable* t = rv_table(b.column_labels);
    t->setRowCount(int(b.rows.size()));
    QVector<bool> signed_col;
    for (const QString& c : b.columns)
        signed_col << result::is_signed_result_key(c);
    const bool per_row = !b.row_units.isEmpty();
    for (int r = 0; r < b.rows.size(); ++r) {
        const auto& row = b.rows.at(r);
        for (int c = 0; c < row.size() && c < b.columns.size(); ++c) {
            // A comparison table carries one unit (and sign meaning) per row.
            const Unit u = (per_row && c > 0) ? b.row_units.value(r, Unit::Plain) : b.column_units.value(c, Unit::Plain);
            const bool sgn = (per_row && c > 0) ? result::is_signed_result_key(b.row_keys.value(r)) : signed_col.value(c);
            QTableWidgetItem* item = rv_cell(row.at(c), u, options_, sgn);
            if (per_row && c == 0)
                item->setForeground(QColor(colors::TEXT_SECONDARY()));
            t->setItem(r, c, item);
        }
    }
    rv_finish_table(t, 14);
    vl->addWidget(t);
    return box;
}

QWidget* ResultView::render_caption(const Block& b) {
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);
    if (!b.title.isEmpty())
        vl->addWidget(rv_title(b.title, box));
    auto* text = new QLabel(b.text, box);
    text->setWordWrap(true);
    text->setTextFormat(Qt::PlainText);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setStyleSheet(QStringLiteral("color:%1; background:transparent; font-size:%2px;")
                            .arg(colors::TEXT_PRIMARY())
                            .arg(fonts::px(-1)));
    vl->addWidget(text);
    return box;
}

QWidget* ResultView::render_list(const Block& b) {
    if (b.items.isEmpty())
        return nullptr;
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);
    if (!b.title.isEmpty())
        vl->addWidget(rv_title(b.title, box));
    // Many short codes read best as one wrapped line; sentences as bullets.
    const bool short_items = std::all_of(b.items.begin(), b.items.end(), [](const QString& s) { return s.size() <= 14; });
    QString body;
    if (short_items && b.items.size() > 6) {
        body = b.items.join(QStringLiteral(" · "));
    } else {
        QStringList lines;
        for (const QString& s : b.items)
            lines << QStringLiteral("•  ") + s;
        body = lines.join(QLatin1Char('\n'));
    }
    auto* text = new QLabel(body, box);
    text->setWordWrap(true);
    text->setTextFormat(Qt::PlainText);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setStyleSheet(QStringLiteral("color:%1; background:transparent; font-size:%2px;")
                            .arg(colors::TEXT_PRIMARY())
                            .arg(fonts::px(-1)));
    vl->addWidget(text);
    return box;
}

QWidget* ResultView::render_section(const Block& b, int depth) {
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, depth == 0 ? 4 : 0, 0, 0);
    vl->setSpacing(4);

    auto* head = new QToolButton(box);
    head->setText(b.title.toUpper());
    head->setCheckable(true);
    head->setChecked(!b.collapsed);
    head->setArrowType(b.collapsed ? Qt::RightArrow : Qt::DownArrow);
    head->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    head->setCursor(Qt::PointingHandCursor);
    head->setAutoRaise(true);
    head->setStyleSheet(QStringLiteral("QToolButton { color:%1; background:transparent; border:none; padding:2px 0;"
                                       " font-size:%2px; font-weight:700; letter-spacing:0.5px; }"
                                       "QToolButton:hover { color:%3; }")
                            .arg(colors::AMBER())
                            .arg(fonts::px(-2))
                            .arg(colors::TEXT_PRIMARY()));
    vl->addWidget(head);

    auto* body = new QWidget(box);
    body->setObjectName(QStringLiteral("rvSectionBody"));
    body->setStyleSheet(QStringLiteral("QWidget#rvSectionBody { border-left:1px solid %1; }").arg(colors::BORDER_DIM()));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 2, 0, 4);
    bl->setSpacing(8);
    render_children(b, bl, depth + 1);
    if (bl->count() == 0) {
        delete box;
        return nullptr;
    }
    body->setVisible(!b.collapsed);
    vl->addWidget(body);
    connect(head, &QToolButton::toggled, body, [head, body](bool on) {
        body->setVisible(on);
        head->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });
    return box;
}

// ── Charts ───────────────────────────────────────────────────────────────────

QWidget* ResultView::render_chart_card(const Block& b) {
    QWidget* stats = nullptr;
    QWidget* chart = nullptr;
    switch (b.kind) {
        case Kind::Line:
            chart = make_line(b);
            break;
        case Kind::Bars:
            chart = make_bars(b);
            break;
        case Kind::Donut:
            chart = make_donut(b);
            break;
        case Kind::Distribution:
            chart = make_distribution(b, &stats);
            break;
        case Kind::Fan:
            chart = make_fan(b, &stats);
            break;
        case Kind::Heatmap:
            chart = make_heatmap(b);
            break;
        case Kind::Gauge:
            chart = make_gauge(b);
            break;
        default:
            break;
    }
    if (!chart)
        return nullptr;

    QString subtitle;
    if (b.kind == Kind::Heatmap && (!b.row_title.isEmpty() || !b.col_title.isEmpty()))
        subtitle = tr("rows: %1 · columns: %2")
                       .arg(b.row_title.isEmpty() ? QStringLiteral("#") : b.row_title,
                            b.col_title.isEmpty() ? QStringLiteral("#") : b.col_title);
    else if (b.kind == Kind::Line && !b.x_title.isEmpty())
        subtitle = tr("vs %1").arg(b.x_title);

    QHBoxLayout* header = nullptr;
    QVBoxLayout* body = nullptr;
    QFrame* card = rv_card(b.title, subtitle, &header, &body);
    if (stats) {
        stats->setParent(card);
        body->addWidget(stats);
    }
    chart->setParent(card);
    body->addWidget(chart);

    if (b.kind == Kind::Gauge) {
        card->setMaximumWidth(360);
        return card;
    }
    if (b.kind != Kind::Fan) {
        // The data behind the chart, one click away (built on first use).
        QPushButton* toggle = rv_toggle_button(tr("TABLE"), card);
        toggle->setToolTip(tr("Show the numbers behind this chart"));
        header->addWidget(toggle);
        const Block copy = b;
        connect(toggle, &QPushButton::toggled, card, [this, copy, card, body](bool on) {
            auto* table = card->findChild<QWidget*>(QStringLiteral("rvChartTable"));
            if (!table && on) {
                table = make_data_table(copy);
                if (!table)
                    return;
                table->setObjectName(QStringLiteral("rvChartTable"));
                table->setParent(card);
                body->addWidget(table);
            }
            if (table)
                table->setVisible(on);
        });
    }
    return card;
}

QWidget* ResultView::make_line(const Block& b) {
    if (b.series.isEmpty() || b.xs.size() < 2)
        return nullptr;
    auto* c = new TimeSeriesChart;
    c->set_x_is_time(b.x_dates);
    c->set_show_mode_buttons(false);
    c->set_transparent_background(true);
    c->set_show_legend(b.series.size() > 1 || b.diagonal);
    c->set_export_name(b.title);
    const result::Options o = options_;
    const Unit yu = b.series.first().unit;
    c->set_value_formatter(TimeSeriesChart::Axis::Left, [yu, o](double v) { return result::format_number(v, yu, o); });
    if (!b.x_dates) {
        const Unit xu = b.x_unit;
        auto fx = [xu, o](double x) { return result::format_number(x, xu, o); };
        c->set_tick_time_formatter(fx);
        c->set_time_formatter(fx);
    }
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    for (double x : b.xs)
        if (std::isfinite(x)) {
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
    for (int i = 0; i < b.series.size(); ++i) {
        const result::Series& s = b.series.at(i);
        TimeSeriesChart::SeriesSpec spec;
        spec.name = s.name;
        spec.xs = b.xs;
        spec.ys = s.values;
        while (spec.ys.size() < spec.xs.size())
            spec.ys.append(std::numeric_limits<double>::quiet_NaN());
        spec.ys.resize(spec.xs.size());
        const Unit u = s.unit;
        spec.formatter = [u, o](double v) { return result::format_number(v, u, o); };
        c->add_series(spec);
    }
    if (b.diagonal && std::isfinite(lo) && hi > lo) {
        TimeSeriesChart::SeriesSpec d;
        d.name = tr("Random");
        d.xs = {lo, hi};
        d.ys = {lo, hi};
        d.dashed = true;
        d.color = TimeSeriesChart::ColorRole::Muted;
        d.show_in_tooltip = false;
        c->add_series(d);
    }
    c->setMinimumHeight(260);
    return c;
}

QWidget* ResultView::make_bars(const Block& b) {
    if (b.series.isEmpty() || b.categories.isEmpty())
        return nullptr;
    auto* c = new BarChart;
    QVector<BarSeries> ss;
    bool negative = false;
    for (const result::Series& s : b.series) {
        BarSeries bs;
        bs.name = s.name;
        bs.values = s.values;
        ss.append(bs);
        for (double v : s.values)
            negative = negative || (std::isfinite(v) && v < 0);
    }
    c->set_categories(b.categories);
    c->set_series(ss);
    const result::Options o = options_;
    const Unit u = b.series.first().unit;
    c->set_value_formatter([u, o](double v) { return result::format_number(v, u, o); });
    c->set_signed_colors(ss.size() == 1 && negative);
    c->set_show_values(b.categories.size() <= 12 && ss.size() <= 2);
    int longest = 0;
    for (const QString& cat : b.categories)
        longest = std::max(longest, int(cat.size()));
    if (longest > 10 || b.categories.size() > 16) {
        c->set_orientation(Qt::Horizontal);
        c->setMinimumHeight(std::clamp(int(b.categories.size()) * 24 + 48, 160, 640));
    } else {
        c->setMinimumHeight(240);
    }
    return c;
}

QWidget* ResultView::make_donut(const Block& b) {
    if (b.series.isEmpty())
        return nullptr;
    auto* c = new DonutChart;
    QVector<DonutSlice> slices;
    const result::Series& s = b.series.first();
    for (int i = 0; i < s.values.size(); ++i) {
        DonutSlice d;
        d.id = b.categories.value(i, QString::number(i + 1));
        d.label = d.id;
        d.value = s.values.at(i);
        slices.append(d);
    }
    c->set_slices(slices);
    const result::Options o = options_;
    const Unit u = s.unit;
    c->set_value_formatter([u, o](double v) { return result::format_number(v, u, o); });
    c->setMinimumHeight(220);
    return c;
}

QWidget* ResultView::make_distribution(const Block& b, QWidget** stats) {
    if (b.series.isEmpty())
        return nullptr;
    const result::Series& s = b.series.first();
    auto* c = new DistributionChart;
    c->set_samples(s.values);
    if (c->sample_count() < 2) {
        delete c;
        return nullptr;
    }
    const result::Options o = options_;
    const Unit u = s.unit;
    auto f = [u, o](double v) { return result::format_number(v, u, o); };
    c->set_value_formatter(f);
    c->set_normal_overlay(c->sample_count() >= 30);
    bool pos = false, neg = false;
    for (double v : s.values) {
        pos = pos || (std::isfinite(v) && v > 0);
        neg = neg || (std::isfinite(v) && v < 0);
    }
    c->set_signed_colors(pos && neg && result::is_signed_result_key(s.key));
    const double p5 = c->quantile(0.05), p95 = c->quantile(0.95);
    c->add_marker(c->mean(), tr("Mean"), DistributionChart::MarkerRole::Mean);
    c->add_marker(p5, QStringLiteral("P5"), DistributionChart::MarkerRole::Neutral);
    c->add_marker(p95, QStringLiteral("P95"), DistributionChart::MarkerRole::Neutral);
    c->setMinimumHeight(240);
    *stats = rv_stats_strip({{tr("SAMPLES"), result::format_number(c->sample_count(), Unit::Integer, o)},
                             {tr("MEAN"), f(c->mean())},
                             {tr("STD DEV"), f(c->stdev())},
                             {QStringLiteral("P5"), f(p5)},
                             {QStringLiteral("P95"), f(p95)}});
    return c;
}

QWidget* ResultView::make_fan(const Block& b, QWidget** stats) {
    if (b.paths.isEmpty() || b.xs.size() < 2)
        return nullptr;
    auto* c = new FanChart;
    const int samples = std::min<int>(int(b.paths.size()), 20);
    c->set_paths(b.xs, b.paths, {{5, 95}, {25, 75}}, samples);
    const result::Options o = options_;
    const Unit u = b.series.isEmpty() ? Unit::Plain : b.series.first().unit;
    auto f = [u, o](double v) { return result::format_number(v, u, o); };
    c->set_value_formatter(f);
    const Unit xu = b.x_unit;
    c->set_x_formatter([xu, o](double x) { return result::format_number(x, xu, o); });
    c->setMinimumHeight(260);
    QVector<double> terminal;
    for (const auto& p : b.paths) {
        for (int i = int(p.size()) - 1; i >= 0; --i)
            if (std::isfinite(p.at(i))) {
                terminal << p.at(i);
                break;
            }
    }
    double mean = 0;
    for (double v : terminal)
        mean += v;
    mean = terminal.isEmpty() ? std::numeric_limits<double>::quiet_NaN() : mean / terminal.size();
    double var = 0;
    for (double v : terminal)
        var += (v - mean) * (v - mean);
    const double sd = terminal.size() > 1 ? std::sqrt(var / (terminal.size() - 1)) : std::numeric_limits<double>::quiet_NaN();
    *stats = rv_stats_strip({{tr("PATHS"), result::format_number(b.paths.size(), Unit::Integer, o)},
                             {tr("TERMINAL MEAN"), f(mean)},
                             {tr("TERMINAL STD"), f(sd)},
                             {tr("TERMINAL P5"), f(rv_quantile(terminal, 0.05))},
                             {tr("TERMINAL P95"), f(rv_quantile(terminal, 0.95))}});
    return c;
}

QWidget* ResultView::make_heatmap(const Block& b) {
    if (b.matrix.isEmpty())
        return nullptr;
    auto* h = new MatrixHeatmap;
    h->set_matrix(b.row_labels, b.col_labels, b.matrix);
    if (b.scale == 1)
        h->set_diverging_scale(-1.0, 1.0, 0.0);
    else if (b.scale == 2)
        h->set_sequential_scale();
    else
        h->set_diverging_scale();
    const result::Options o = options_;
    const Unit u = b.value_unit;
    h->set_value_formatter([u, o](double v) { return result::format_number(v, u, o); });
    h->setMinimumHeight(std::clamp(int(b.matrix.size()) * 26 + 80, 180, 560));
    return h;
}

QWidget* ResultView::make_gauge(const Block& b) {
    if (!std::isfinite(b.value) || b.gauge_max <= b.gauge_min)
        return nullptr;
    auto* g = new Gauge;
    g->set_style(Gauge::Style::SemiCircle);
    g->set_range(b.gauge_min, b.gauge_max);
    g->set_value(std::clamp(b.value, b.gauge_min, b.gauge_max));
    QVector<GaugeZone> zones;
    QString zone_label;
    for (const result::GaugeBand& band : b.bands) {
        GaugeZone z;
        z.from = band.from;
        z.to = band.to;
        z.tone = band.tone < 0 ? GaugeZone::Tone::Bad : (band.tone == 0 ? GaugeZone::Tone::Caution : GaugeZone::Tone::Good);
        z.label = band.label;
        zones.append(z);
        if (b.value >= band.from && b.value <= band.to && zone_label.isEmpty())
            zone_label = band.label;
    }
    g->set_zones(zones);
    const result::Options o = options_;
    const QString value_text = result::format_number(b.value, Unit::Plain, o);
    g->set_caption(zone_label.isEmpty() ? value_text : value_text + QStringLiteral(" · ") + zone_label);
    g->setMinimumHeight(130);
    return g;
}

QWidget* ResultView::make_data_table(const Block& b) {
    const result::Options& o = options_;
    DataTable* t = nullptr;
    switch (b.kind) {
        case Kind::Line: {
            QStringList h{b.x_title.isEmpty() ? QStringLiteral("x") : b.x_title};
            for (const auto& s : b.series)
                h << s.name;
            t = rv_table(h);
            const int n = std::min<int>(int(b.xs.size()), result::kMaxTableRows);
            t->setRowCount(n);
            for (int r = 0; r < n; ++r) {
                const double x = b.xs.at(r);
                auto* xi = new NumericTableWidgetItem(rv_x_text(x, b, o), x);
                xi->setForeground(QColor(colors::TEXT_SECONDARY()));
                t->setItem(r, 0, xi);
                for (int c = 0; c < b.series.size(); ++c) {
                    const double v = b.series.at(c).values.value(r, std::numeric_limits<double>::quiet_NaN());
                    t->setItem(r, c + 1,
                               rv_cell(std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null),
                                       b.series.at(c).unit, o, result::is_signed_result_key(b.series.at(c).key)));
                }
            }
            break;
        }
        case Kind::Bars:
        case Kind::Donut: {
            QStringList h{QString()};
            for (const auto& s : b.series)
                h << s.name;
            t = rv_table(h);
            t->setRowCount(int(b.categories.size()));
            for (int r = 0; r < b.categories.size(); ++r) {
                auto* k = new QTableWidgetItem(b.categories.at(r));
                k->setForeground(QColor(colors::TEXT_SECONDARY()));
                t->setItem(r, 0, k);
                for (int c = 0; c < b.series.size(); ++c) {
                    const double v = b.series.at(c).values.value(r, std::numeric_limits<double>::quiet_NaN());
                    t->setItem(r, c + 1,
                               rv_cell(std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null),
                                       b.series.at(c).unit, o, result::is_signed_result_key(b.series.at(c).key)));
                }
            }
            break;
        }
        case Kind::Distribution: {
            if (b.series.isEmpty())
                return nullptr;
            const result::Series& s = b.series.first();
            QVector<double> v = s.values;
            v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return !std::isfinite(x); }), v.end());
            if (v.isEmpty())
                return nullptr;
            double sum = 0;
            for (double x : v)
                sum += x;
            const double mean = sum / v.size();
            double var = 0;
            for (double x : v)
                var += (x - mean) * (x - mean);
            const QVector<QPair<QString, double>> rows = {
                {tr("Samples"), double(v.size())},
                {tr("Mean"), mean},
                {tr("Std dev"), v.size() > 1 ? std::sqrt(var / (v.size() - 1)) : std::numeric_limits<double>::quiet_NaN()},
                {tr("Min"), *std::min_element(v.begin(), v.end())},
                {QStringLiteral("P5"), rv_quantile(v, 0.05)},
                {QStringLiteral("P25"), rv_quantile(v, 0.25)},
                {tr("Median"), rv_quantile(v, 0.5)},
                {QStringLiteral("P75"), rv_quantile(v, 0.75)},
                {QStringLiteral("P95"), rv_quantile(v, 0.95)},
                {tr("Max"), *std::max_element(v.begin(), v.end())}};
            t = rv_table({tr("Statistic"), s.name});
            t->setRowCount(int(rows.size()));
            for (int r = 0; r < rows.size(); ++r) {
                auto* k = new QTableWidgetItem(rows.at(r).first);
                k->setForeground(QColor(colors::TEXT_SECONDARY()));
                t->setItem(r, 0, k);
                t->setItem(r, 1, rv_cell(QJsonValue(rows.at(r).second), r == 0 ? Unit::Integer : s.unit, o, false));
            }
            break;
        }
        case Kind::Heatmap: {
            QStringList h{b.row_title.isEmpty() ? QStringLiteral("#") : b.row_title};
            h << b.col_labels;
            t = rv_table(h);
            t->setRowCount(int(b.matrix.size()));
            for (int r = 0; r < b.matrix.size(); ++r) {
                auto* k = new QTableWidgetItem(b.row_labels.value(r, QString::number(r + 1)));
                k->setForeground(QColor(colors::TEXT_SECONDARY()));
                t->setItem(r, 0, k);
                for (int c = 0; c < b.matrix.at(r).size(); ++c) {
                    const double v = b.matrix.at(r).at(c);
                    t->setItem(r, c + 1, rv_cell(std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null),
                                                 b.value_unit, o, false));
                }
            }
            break;
        }
        default:
            return nullptr;
    }
    rv_finish_table(t, 14);
    return t;
}

} // namespace fincept::ui
