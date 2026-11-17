#include "screens/report_builder/PropertiesPanel.h"

#include "core/config/AppPaths.h"
#include "core/config/AppSettings.h"
#include "core/report/ReportChartSpec.h"
#include "core/report/ReportTableCodec.h"
#include "core/report/ReportTableFormat.h"
#include "services/file_manager/FileManagerService.h"
#include "services/portfolio/PortfolioService.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportLibrary.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QApplication>
#include <QCheckBox>
#include <QMenu>
#include <QToolButton>
#include <QtConcurrent/QtConcurrent>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace rep = ::fincept::report;
using Binder = ::fincept::services::ReportDataBinder;

namespace {

constexpr int kPpDebounceMs = 180;

QSettings pp_settings() {
    return app_settings::open();
}

QString pp_last_symbol() {
    return pp_settings().value(QStringLiteral("report_builder/last_symbol")).toString();
}

void pp_remember_symbol(const QString& s) {
    if (s.isEmpty())
        return;
    QSettings st = pp_settings();
    st.setValue(QStringLiteral("report_builder/last_symbol"), s);
}

void pp_set_role(QWidget* w, const char* role) {
    if (w->property("rbRole").toString() == QLatin1String(role))
        return;
    w->setProperty("rbRole", QLatin1String(role));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

struct PpSource {
    QString key;   ///< source kind, or "portfolio:<what>"
    QString label;
};

QVector<PpSource> pp_sources_for(const QString& type) {
    auto T = [](const char* s) { return PropertiesPanel::tr(s); };
    const PpSource none{QString(), T("None — typed by hand")};
    if (type == QLatin1String("market_data"))
        return {{QStringLiteral("quote"), T("Quote")}};
    if (type == QLatin1String("stats_block"))
        return {none,
                {QStringLiteral("info"), T("Key statistics")},
                {QStringLiteral("quote"), T("Quote")},
                {QStringLiteral("quotes"), T("Quotes list")},
                {QStringLiteral("watchlist"), T("Watchlist quotes")},
                {QStringLiteral("portfolio:summary"), T("Portfolio performance")}};
    if (type == QLatin1String("table"))
        return {none,
                {QStringLiteral("financials"), T("Financial statement")},
                {QStringLiteral("peers"), T("Peer comparison")},
                {QStringLiteral("news"), T("News for a symbol")},
                {QStringLiteral("quotes"), T("Quotes list")},
                {QStringLiteral("watchlist"), T("Watchlist quotes")},
                {QStringLiteral("econ_calendar"), T("Economic calendar")},
                {QStringLiteral("info"), T("Key statistics")},
                {QStringLiteral("portfolio:holdings"), T("Portfolio holdings")}};
    if (type == QLatin1String("chart"))
        return {none,
                {QStringLiteral("history"), T("Price history")},
                {QStringLiteral("financials"), T("Financial statement items (bars)")},
                {QStringLiteral("portfolio:allocation"), T("Portfolio allocation")}};
    if (type == QLatin1String("sparkline"))
        return {none, {QStringLiteral("history"), T("Price history")}, {QStringLiteral("quote"), T("Quote")}};
    if (type == QLatin1String("list"))
        return {none,
                {QStringLiteral("headlines"), T("Market headlines")},
                {QStringLiteral("news"), T("News for a symbol")},
                {QStringLiteral("quotes"), T("Quotes list")},
                {QStringLiteral("watchlist"), T("Watchlist quotes")},
                {QStringLiteral("econ_calendar"), T("Economic calendar")}};
    return {};
}

QString pp_source_key(const rep::Binding& b) {
    if (b.source == QLatin1String(rep::source::kPortfolio))
        return QStringLiteral("portfolio:") + b.param("what", QStringLiteral("holdings"));
    return b.source;
}

/// A recipe for `key`, carrying over the symbol(s) of `prev` (or the last one used).
rep::Binding pp_default_binding(const QString& key, const rep::Binding& prev, const QString& type) {
    QString sym = prev.symbols().value(0);
    if (sym.isEmpty())
        sym = pp_last_symbol();
    if (key == QLatin1String("quote")) {
        rep::Binding b = rep::quote_binding(sym);
        b.view = type == QLatin1String("market_data") ? QString::fromLatin1(rep::view::kQuote)
                 : type == QLatin1String("sparkline") ? QString::fromLatin1(rep::view::kSparkline)
                                                      : QString::fromLatin1(rep::view::kKpi);
        return b;
    }
    if (key == QLatin1String("info"))
        return rep::info_binding(sym);
    if (key == QLatin1String("history")) {
        QStringList syms = prev.symbols();
        if (syms.isEmpty() && !sym.isEmpty())
            syms << sym;
        return rep::history_binding(syms, QStringLiteral("1y"), QStringLiteral("1d"));
    }
    if (key == QLatin1String("financials")) {
        rep::Binding b = rep::financials_binding(sym);
        if (type == QLatin1String("chart")) { // line items per period as grouped bars
            b.view = QString::fromLatin1(rep::view::kChart);
            b.params.insert(QStringLiteral("period"), QStringLiteral("quarterly"));
            b.params.insert(QStringLiteral("n"), 8);
            b.params.insert(QStringLiteral("items"),
                            rep::to_json_array({QStringLiteral("Total Revenue"), QStringLiteral("Net Income")}));
        }
        return b;
    }
    if (key == QLatin1String("watchlist")) {
        rep::Binding b = rep::watchlist_binding(prev.param("watchlist_id"), QStringLiteral("change_desc"));
        b.view = type == QLatin1String("stats_block") ? QString::fromLatin1(rep::view::kKpi)
                 : type == QLatin1String("list")      ? QString::fromLatin1(rep::view::kList)
                                                      : QString::fromLatin1(rep::view::kTable);
        return b;
    }
    if (key == QLatin1String("econ_calendar")) // the week of the report's date; medium and high impact
        return rep::econ_calendar_binding(QStringLiteral("{{date}}"), QStringLiteral("week"), QStringLiteral("medium"),
                                          type == QLatin1String("list") ? QString::fromLatin1(rep::view::kList)
                                                                        : QString::fromLatin1(rep::view::kTable));
    if (key == QLatin1String("peers"))
        return rep::peers_binding(sym);
    if (key == QLatin1String("news"))
        return rep::news_binding(sym);
    if (key == QLatin1String("headlines"))
        return rep::headlines_binding(10);
    if (key == QLatin1String("quotes")) {
        rep::Binding b = rep::quotes_binding({});
        b.params.insert(QStringLiteral("universe"), QStringLiteral("indices"));
        return b;
    }
    if (key.startsWith(QLatin1String("portfolio:")))
        return rep::portfolio_binding(prev.param("portfolio_id"), key.mid(10));
    return {};
}

QComboBox* pp_combo(QWidget* parent, const QVector<QPair<QString, QString>>& items, const QString& current) {
    auto* c = new QComboBox(parent);
    for (const auto& it : items)
        c->addItem(it.second, it.first);
    const int i = c->findData(current);
    c->setCurrentIndex(i >= 0 ? i : 0);
    return c;
}

} // namespace

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("rbProps"));
    setMinimumWidth(0);
    setMaximumWidth(preferred_width());
    // One selector-scoped sheet, set before any child exists: the border belongs to the
    // panel only (a selector-less "border-left" used to frame every label and row).
    setStyleSheet(
        QStringLiteral(
            "#rbProps { background: %1; border-left: 1px solid %2; }"
            "#rbProps QLabel { color: %3; background: transparent; font-size: %8px; }"
            "#rbProps QLabel[rbRole=\"title\"] { color: %4; font-weight: bold; background: %5; padding: 6px;"
            " border-bottom: 1px solid %2; }"
            "#rbProps QLabel[rbRole=\"section\"] { color: %6; font-weight: bold; padding-top: 6px; }"
            "#rbProps QLabel[rbRole=\"hint\"] { color: %6; }"
            "#rbProps QLabel[rbRole=\"ok\"] { color: %10; }"
            "#rbProps QLabel[rbRole=\"bad\"] { color: %11; }"
            "#rbProps QLabel[rbRole=\"warn\"] { color: %12; }"
            "#rbProps QLabel[rbRole=\"busy\"] { color: %4; }"
            "#rbProps QLineEdit, #rbProps QTextEdit, #rbProps QComboBox, #rbProps QSpinBox, #rbProps QTableWidget,"
            " #rbProps QTimeEdit"
            " { background: %7; color: %13; border: 1px solid %14; font-size: %9px; }"
            "#rbProps QPushButton, #rbProps QToolButton { background: %5; color: %13; border: 1px solid %14;"
            " padding: 3px 8px; font-size: %8px; }"
            "#rbProps QPushButton:hover, #rbProps QToolButton:hover { background: %15; }"
            "#rbProps QToolButton::menu-indicator { image: none; width: 0; }"
            "#rbProps QCheckBox { color: %13; background: transparent; font-size: %8px; }"
            "#rbProps QPushButton[rbRole=\"primary\"] { background: %4; color: %16; border-color: %4; font-weight: bold; }"
            "#rbProps QPushButton[rbRole=\"danger\"] { color: %11; border-color: %11; }"
            "#rbProps QPushButton[rbRole=\"danger\"]:hover { background: %11; color: %16; }"
            "#rbProps QFrame[rbRole=\"rule\"] { background: %2; border: none; }"
            "#rbProps QScrollArea { background: transparent; border: none; }")
            .arg(ui::colors::PANEL(), ui::colors::BORDER(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER(),
                 ui::colors::BG_RAISED(), ui::colors::MUTED(), ui::colors::BG_BASE(), ui::fonts::px(-2),
                 ui::fonts::px(-1))
            .arg(ui::colors::SUCCESS(), ui::colors::DANGER(), ui::colors::WARNING(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BORDER_MED(), ui::colors::BG_HOVER(), ui::colors::TEXT_ON_ACCENT()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(1, 0, 0, 0); // keep the border line visible
    vl->setSpacing(0);
    stack_ = new QStackedWidget(this);
    vl->addWidget(stack_);

    // Page 0: the document's properties.
    auto* doc_page = new QWidget(stack_);
    auto* dl = new QVBoxLayout(doc_page);
    dl->setContentsMargins(6, 10, 6, 10);
    dl->setSpacing(6);
    doc_state_ = new ui::EmptyState(doc_page);
    doc_state_->set_compact(true);
    doc_state_->set_alignment(Qt::AlignTop);
    dl->addWidget(doc_state_, 1);
    theme_btn_ = new QPushButton(tr("Report theme…"), doc_page);
    dl->addWidget(theme_btn_);
    vars_btn_ = new QPushButton(tr("Variables…"), doc_page);
    vars_btn_->setToolTip(tr("{{symbol}}, {{period}} and your own: values that every text of the report can use"));
    dl->addWidget(vars_btn_);
    connect(vars_btn_, &QPushButton::clicked, this, &PropertiesPanel::variables_requested);
    connect(doc_state_, &ui::EmptyState::primary_clicked, this, &PropertiesPanel::document_info_requested);
    connect(doc_state_, &ui::EmptyState::secondary_clicked, this, &PropertiesPanel::page_setup_requested);
    connect(theme_btn_, &QPushButton::clicked, this, &PropertiesPanel::theme_requested);
    stack_->addWidget(doc_page);

    // Page 1: the selected block's editor (scrolls when tall).
    auto* scroll = new QScrollArea(stack_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor_widget_ = new QWidget(scroll);
    editor_layout_ = new QVBoxLayout(editor_widget_);
    editor_layout_->setContentsMargins(8, 8, 8, 8);
    editor_layout_->setSpacing(5);
    scroll->setWidget(editor_widget_);
    stack_->addWidget(scroll);
    stack_->setCurrentIndex(0);

    edit_timer_ = new QTimer(this);
    edit_timer_->setSingleShot(true);
    edit_timer_->setInterval(kPpDebounceMs);
    connect(edit_timer_, &QTimer::timeout, this, &PropertiesPanel::flush_pending);

    connect(&Binder::instance(), &Binder::block_state_changed, this, [this](int id) {
        if (id == current_id_ && has_component_)
            update_data_status();
    });
    // (The screen re-fits the panel width on ThemeManager::appearance_settled: it also
    // owns the collapse state, which drives the maximum width.)
}

int PropertiesPanel::preferred_width() const {
    const QFontMetrics fm(ui::fonts::font(-1));
    return std::max(280, fm.averageCharWidth() * 36);
}

QLabel* PropertiesPanel::add_label(const QString& text, const char* role) {
    auto* l = new QLabel(text, editor_widget_);
    l->setWordWrap(true);
    if (role)
        l->setProperty("rbRole", QLatin1String(role));
    editor_layout_->addWidget(l);
    return l;
}

QPushButton* PropertiesPanel::add_button(const QString& text, const char* role) {
    auto* b = new QPushButton(text, editor_widget_);
    b->setCursor(Qt::PointingHandCursor);
    if (role)
        b->setProperty("rbRole", QLatin1String(role));
    editor_layout_->addWidget(b);
    return b;
}

void PropertiesPanel::queue_edit(const QString& key, const QString& value) {
    if (pending_id_ != current_id_) {
        flush_pending();
        pending_id_ = current_id_;
    }
    pending_.insert(key, value);
    edit_timer_->start();
}

void PropertiesPanel::flush_pending() {
    edit_timer_->stop();
    if (pending_.isEmpty() || pending_id_ <= 0)
        return;
    const int id = pending_id_;
    const QHash<QString, QString> edits = pending_;
    pending_.clear();
    for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
        if (it.key() == QLatin1String("@content"))
            emit content_changed(id, it.value());
        else
            emit config_changed(id, it.key(), it.value());
    }
}

void PropertiesPanel::focus_first_editor() {
    if (first_editor_) {
        first_editor_->setFocus(Qt::OtherFocusReason);
        if (auto* le = qobject_cast<QLineEdit*>(first_editor_))
            le->selectAll();
    }
}

// ── Document page ───────────────────────────────────────────────────────────

void PropertiesPanel::show_document(const report::ReportMetadata& meta, const report::ReportTheme& theme, int blocks,
                                    int pages, qint64 library_id, const QString& file_path) {
    flush_pending();
    current_id_ = 0;
    has_component_ = false;
    doc_meta_ = meta;
    doc_theme_ = theme;
    doc_blocks_ = blocks;
    doc_pages_ = pages;
    doc_library_id_ = library_id;
    doc_file_ = file_path;
    ui::EmptyState::Spec spec;
    spec.glyph = QStringLiteral("☰");
    spec.glyph_tone = ui::EmptyState::Tone::Accent;
    spec.title = tr("Document");
    spec.message = blocks > 0 ? tr("Click a block on the page to edit it, or change the report's settings here.")
                              : tr("Add blocks from the left panel, or start from a template.");
    spec.details = {
        {tr("Title"), meta.title},
        {tr("Author"), meta.author.isEmpty() ? QStringLiteral("—") : meta.author},
        {tr("Company"), meta.company.isEmpty() ? QStringLiteral("—") : meta.company},
        {tr("Date"), meta.date.isEmpty() ? QStringLiteral("—") : meta.date},
        {tr("Paper"), QStringLiteral("%1 %2").arg(meta.page_size, meta.landscape ? tr("landscape") : tr("portrait"))},
        {tr("Theme"), theme.name},
        {tr("Blocks / pages"), QStringLiteral("%1 / %2").arg(blocks).arg(pages)},
        {tr("Variables"), [&meta]() {
             QStringList parts;
             for (auto it = meta.variables.cbegin(); it != meta.variables.cend(); ++it)
                 parts << QStringLiteral("%1 = %2").arg(it.key(), it.value().isEmpty() ? QStringLiteral("—") : it.value());
             return parts.isEmpty() ? QStringLiteral("—") : parts.join(QStringLiteral(", "));
         }()},
        {tr("Stored"), library_id > 0 ? (file_path.isEmpty() ? tr("Library") : tr("Library + %1").arg(QFileInfo(file_path).fileName()))
                                      : tr("not saved yet")},
    };
    spec.details_heading = tr("THIS REPORT");
    spec.primary = tr("EDIT DOCUMENT INFO");
    spec.secondary = tr("PAGE SETUP");
    doc_state_->apply(spec);
    stack_->setCurrentIndex(0);
}

// ── Block editor ────────────────────────────────────────────────────────────

void PropertiesPanel::show_component(const ReportComponent& component) {
    flush_pending();
    current_id_ = component.id;
    last_component_ = component;
    has_component_ = true;
    first_editor_ = nullptr;
    data_status_ = nullptr;
    refresh_btn_ = nullptr;
    while (editor_layout_->count() > 0) {
        QLayoutItem* item = editor_layout_->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater(); // may be the widget whose signal got us here
        }
        delete item;
    }
    build_editor(component);
    stack_->setCurrentIndex(1);
}

void PropertiesPanel::build_editor(const ReportComponent& c) {
    const int id = c.id;
    QString type_label = c.type;
    type_label.replace(QLatin1Char('_'), QLatin1Char(' '));
    auto* title = add_label(QStringLiteral("— %1 —").arg(type_label.toUpper()), "title");
    title->setAlignment(Qt::AlignCenter);
    const bool bound = rep::is_bound(c);

    auto text_editor = [this, id](const QString& text, int min_h, int max_h, const QString& placeholder = {}) {
        auto* e = new QTextEdit(editor_widget_);
        e->setAcceptRichText(false);
        e->setPlainText(text);
        e->setMinimumHeight(min_h);
        e->setMaximumHeight(max_h);
        if (!placeholder.isEmpty())
            e->setPlaceholderText(placeholder);
        connect(e, &QTextEdit::textChanged, this, [this, e]() { queue_edit(QStringLiteral("@content"), e->toPlainText()); });
        editor_layout_->addWidget(e);
        if (!first_editor_)
            first_editor_ = e;
        Q_UNUSED(id);
        return e;
    };
    auto line_editor = [this](const QString& key, const QString& value, const QString& placeholder = {}) {
        auto* e = new QLineEdit(value, editor_widget_);
        if (!placeholder.isEmpty())
            e->setPlaceholderText(placeholder);
        connect(e, &QLineEdit::textEdited, this, [this, key](const QString& v) { queue_edit(key, v); });
        editor_layout_->addWidget(e);
        if (!first_editor_)
            first_editor_ = e;
        return e;
    };
    auto config_text = [this](const QString& key, const QString& value, int max_h, const QString& placeholder = {}) {
        auto* e = new QTextEdit(editor_widget_);
        e->setAcceptRichText(false);
        e->setPlainText(value);
        e->setMaximumHeight(max_h);
        if (!placeholder.isEmpty())
            e->setPlaceholderText(placeholder);
        connect(e, &QTextEdit::textChanged, this, [this, e, key]() { queue_edit(key, e->toPlainText()); });
        editor_layout_->addWidget(e);
        if (!first_editor_)
            first_editor_ = e;
        return e;
    };
    auto combo_row = [this, id](const QString& key, const QVector<QPair<QString, QString>>& items, const QString& current) {
        QComboBox* cb = pp_combo(editor_widget_, items, current);
        connect(cb, &QComboBox::currentIndexChanged, this, [this, cb, id, key](int) {
            flush_pending();
            emit config_changed(id, key, cb->currentData().toString());
        });
        editor_layout_->addWidget(cb);
        return cb;
    };
    auto spin_row = [this, id](const QString& key, int lo, int hi, int value, int step = 1) {
        auto* sp = new QSpinBox(editor_widget_);
        sp->setRange(lo, hi);
        sp->setSingleStep(step);
        sp->setValue(value);
        connect(sp, &QSpinBox::valueChanged, this, [this, id, key](int v) {
            flush_pending();
            emit config_changed(id, key, QString::number(v));
        });
        editor_layout_->addWidget(sp);
        return sp;
    };

    const QString& type = c.type;
    if (type == QLatin1String("heading")) {
        add_label(tr("Heading text"));
        line_editor(QStringLiteral("@content"), c.content, tr("Section title"));
        add_label(tr("Level"));
        combo_row(QStringLiteral("level"), {{"1", tr("1 — section")}, {"2", tr("2 — subsection")}, {"3", tr("3 — minor")}},
                  c.config.value(QStringLiteral("level"), QStringLiteral("1")));
    } else if (type == QLatin1String("text")) {
        add_label(tr("Text (Markdown: **bold**, *italic*, - lists, # headings)"));
        QTextEdit* ed = text_editor(c.content, 120, 320, c.config.value(QStringLiteral("placeholder")));
        add_ai_actions(ed, id);
        add_label(tr("Style"));
        combo_row(QStringLiteral("style"), {{"", tr("Body text")}, {"caption", tr("Small print / caption")}},
                  c.config.value(QStringLiteral("style")));
    } else if (type == QLatin1String("code") || type == QLatin1String("quote")) {
        add_label(tr("Content"));
        QTextEdit* ed = text_editor(c.content, 80, 240);
        if (type == QLatin1String("quote"))
            add_ai_actions(ed, id);
    } else if (type == QLatin1String("list")) {
        add_label(bound ? tr("Items (filled by the data source)") : tr("Items (one per line)"));
        auto* e = text_editor(c.content, 80, 200);
        e->setReadOnly(bound);
    } else if (type == QLatin1String("callout")) {
        add_label(tr("Style"));
        combo_row(QStringLiteral("style"),
                  {{"info", tr("Info")}, {"success", tr("Success")}, {"warning", tr("Warning")}, {"danger", tr("Danger")}},
                  c.config.value(QStringLiteral("style"), QStringLiteral("info")));
        add_label(tr("Heading (optional)"));
        line_editor(QStringLiteral("heading"), c.config.value(QStringLiteral("heading")), tr("e.g. Key risk"));
        add_label(tr("Content"));
        QTextEdit* ed = text_editor(c.content, 60, 160);
        add_ai_actions(ed, id);
    } else if (type == QLatin1String("table")) {
        add_label(tr("Table title (optional)"));
        line_editor(QStringLiteral("title"), c.config.value(QStringLiteral("title")));
        if (!bound) {
            QVector<QStringList> grid = rep::table::parse(c.config.value(QStringLiteral("csv")));
            if (grid.isEmpty() && c.config.contains(QStringLiteral("data"))) {
                const QJsonObject cell_data = QJsonDocument::fromJson(c.config.value(QStringLiteral("data")).toUtf8()).object();
                const int lr = c.config.value(QStringLiteral("rows"), QStringLiteral("3")).toInt();
                const int lc = c.config.value(QStringLiteral("cols"), QStringLiteral("3")).toInt();
                for (int r = 0; r < lr; ++r) {
                    QStringList row;
                    for (int k = 0; k < lc; ++k)
                        row << cell_data.value(QStringLiteral("%1_%2").arg(r).arg(k)).toString();
                    grid.append(row);
                }
            }
            int rows = grid.isEmpty() ? c.config.value(QStringLiteral("rows"), QStringLiteral("3")).toInt()
                                      : static_cast<int>(grid.size());
            int cols = c.config.value(QStringLiteral("cols"), QStringLiteral("3")).toInt();
            if (!grid.isEmpty()) {
                cols = 0;
                for (const auto& r : grid)
                    cols = std::max(cols, static_cast<int>(r.size()));
            }
            rows = std::clamp(rows, 1, 200);
            cols = std::clamp(cols, 1, 20);
            auto* dims = new QWidget(editor_widget_);
            auto* dl = new QHBoxLayout(dims);
            dl->setContentsMargins(0, 0, 0, 0);
            dl->setSpacing(6);
            auto* rl = new QLabel(tr("Rows"), dims);
            auto* rs = new QSpinBox(dims);
            rs->setRange(1, 200);
            rs->setValue(rows);
            auto* cl = new QLabel(tr("Cols"), dims);
            auto* cs = new QSpinBox(dims);
            cs->setRange(1, 20);
            cs->setValue(cols);
            dl->addWidget(rl);
            dl->addWidget(rs);
            dl->addWidget(cl);
            dl->addWidget(cs);
            editor_layout_->addWidget(dims);
            add_label(tr("Cells (first row = header)"));
            auto* grid_w = new QTableWidget(rows, cols, editor_widget_);
            grid_w->setMinimumHeight(150);
            grid_w->setMaximumHeight(260);
            grid_w->horizontalHeader()->setVisible(false);
            grid_w->verticalHeader()->setVisible(false);
            const bool blank = grid.isEmpty();
            for (int r = 0; r < rows; ++r)
                for (int k = 0; k < cols; ++k) {
                    QString v = (r < grid.size() && k < grid[r].size()) ? grid[r][k] : QString();
                    if (blank && r == 0)
                        v = tr("Header %1").arg(k + 1);
                    grid_w->setItem(r, k, new QTableWidgetItem(v));
                }
            editor_layout_->addWidget(grid_w);
            auto save_cells = [this, grid_w]() {
                QVector<QStringList> out;
                for (int r = 0; r < grid_w->rowCount(); ++r) {
                    QStringList row;
                    for (int k = 0; k < grid_w->columnCount(); ++k) {
                        auto* it = grid_w->item(r, k);
                        row << (it ? it->text() : QString());
                    }
                    out.append(row);
                }
                queue_edit(QStringLiteral("csv"), rep::table::encode(out));
            };
            connect(grid_w, &QTableWidget::itemChanged, this, save_cells);
            connect(rs, &QSpinBox::valueChanged, this, [this, grid_w, save_cells](int v) {
                grid_w->setRowCount(v);
                queue_edit(QStringLiteral("rows"), QString::number(v));
                save_cells();
            });
            connect(cs, &QSpinBox::valueChanged, this, [this, grid_w, save_cells](int v) {
                grid_w->setColumnCount(v);
                queue_edit(QStringLiteral("cols"), QString::number(v));
                save_cells();
            });
            auto* paste = add_button(tr("Paste table from clipboard"));
            paste->setToolTip(tr("Paste cells copied from Excel or a web page (tab or comma separated)"));
            connect(paste, &QPushButton::clicked, this, [this, id]() {
                const QString text = QApplication::clipboard()->text();
                const QVector<QStringList> rows_in = rep::table::parse(text);
                if (rows_in.isEmpty())
                    return;
                QVector<QStringList> rows_cut = rows_in.mid(0, 200);
                int ncols = 0;
                for (const auto& r : rows_cut)
                    ncols = std::max(ncols, static_cast<int>(r.size()));
                flush_pending();
                emit config_changed(id, QStringLiteral("csv"), rep::table::encode(rows_cut));
                emit config_changed(id, QStringLiteral("rows"), QString::number(rows_cut.size()));
                emit config_changed(id, QStringLiteral("cols"), QString::number(std::min(20, ncols)));
            });
            auto* import = add_button(tr("Import CSV file…"));
            import->setToolTip(tr("Read a .csv or .tsv file into this table (first 200 rows)"));
            connect(import, &QPushButton::clicked, this, [this, id]() {
                const QString path = QFileDialog::getOpenFileName(this, tr("Import CSV"), QString(),
                                                                  tr("CSV / TSV (*.csv *.tsv *.txt)"));
                if (path.isEmpty())
                    return;
                // Read and parse on a worker; only the result comes back to this thread.
                QPointer<PropertiesPanel> self(this);
                (void)QtConcurrent::run([self, id, path]() {
                    QFile f(path);
                    QVector<QStringList> rows;
                    int total = 0;
                    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        rows = rep::table::parse(QString::fromUtf8(f.read(8 * 1024 * 1024)));
                        total = static_cast<int>(rows.size());
                        rows = rows.mid(0, 200);
                    }
                    QMetaObject::invokeMethod(
                        qApp,
                        [self, id, rows, total]() {
                            if (!self || rows.isEmpty())
                                return;
                            int ncols = 0;
                            for (const auto& r : rows)
                                ncols = std::max(ncols, static_cast<int>(r.size()));
                            self->flush_pending();
                            emit self->config_changed(id, QStringLiteral("csv"), rep::table::encode(rows));
                            emit self->config_changed(id, QStringLiteral("rows"), QString::number(rows.size()));
                            emit self->config_changed(id, QStringLiteral("cols"), QString::number(std::min(20, ncols)));
                            if (total > rows.size())
                                emit self->config_changed(id, QStringLiteral("note"),
                                                          PropertiesPanel::tr("Showing the first %1 of %2 rows.")
                                                              .arg(rows.size())
                                                              .arg(total));
                        },
                        Qt::QueuedConnection);
                });
            });
        } else {
            add_label(tr("The cells are filled by the data source below. Unbind to edit them by hand."), "hint");
        }
        add_label(tr("Note under the table (optional)"));
        line_editor(QStringLiteral("note"), c.config.value(QStringLiteral("note")), tr("e.g. USD millions"));
        build_table_formats(c);
    } else if (type == QLatin1String("chart")) {
        add_label(tr("Chart type"));
        QVector<QPair<QString, QString>> kinds;
        for (const auto& k : rep::chart::kind_choices())
            kinds.append({k.first, tr(k.second.toUtf8().constData())});
        QString kind = c.config.value(QStringLiteral("chart_type"), QStringLiteral("line"));
        if (kind == QLatin1String("pie") || kind == QLatin1String("donut") || kind == QLatin1String("candle") ||
            rep::chart::kind_name(rep::chart::kind_from_name(kind)) == kind) {
            // a known kind
        } else {
            kind = QStringLiteral("line");
        }
        combo_row(QStringLiteral("chart_type"), kinds, kind);
        add_label(tr("Title"));
        line_editor(QStringLiteral("title"), c.config.value(QStringLiteral("title"), QStringLiteral("Chart")));
        if (!bound) {
            add_label(tr("Series — one per line: Name: 1, 2, 3   (add [right] for the right axis, [bar] / [line] / "
                         "[area] to mix kinds)"));
            QString series_text;
            const QJsonArray arr = QJsonDocument::fromJson(c.config.value(QStringLiteral("series")).toUtf8()).array();
            if (!arr.isEmpty()) {
                QStringList lines;
                for (const QJsonValue& v : arr) {
                    const QJsonObject o = v.toObject();
                    QStringList vals;
                    for (const QJsonValue& x : o.value(QStringLiteral("values")).toArray())
                        vals << (x.isDouble() ? QString::number(x.toDouble(), 'g', 12) : QString());
                    QString line = o.value(QStringLiteral("name")).toString() + QStringLiteral(": ") + vals.join(QStringLiteral(", "));
                    if (o.value(QStringLiteral("axis")).toString() == QLatin1String("right"))
                        line += QStringLiteral(" [right]");
                    if (!o.value(QStringLiteral("kind")).toString().isEmpty())
                        line += QStringLiteral(" [%1]").arg(o.value(QStringLiteral("kind")).toString());
                    lines << line;
                }
                series_text = lines.join(QLatin1Char('\n'));
            } else if (!c.config.value(QStringLiteral("data")).isEmpty()) {
                series_text = c.config.value(QStringLiteral("data"));
            }
            auto* se = new QTextEdit(editor_widget_);
            se->setAcceptRichText(false);
            se->setPlainText(series_text);
            se->setMaximumHeight(110);
            se->setPlaceholderText(QStringLiteral("Revenue: 10, 12, 15\nMargin %: 21, 22, 24 [right] [line]"));
            editor_layout_->addWidget(se);
            if (!first_editor_)
                first_editor_ = se;
            connect(se, &QTextEdit::textChanged, this, [this, se]() {
                // One unnamed series without options stays the plain "data" list older builds read.
                const QStringList lines = se->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                QJsonArray out;
                QStringList first_vals;
                bool plain = lines.size() == 1;
                for (const QString& raw : lines) {
                    QString line = raw;
                    QJsonObject o;
                    for (const char* k : {"right", "bar", "line", "area"}) {
                        const QString tag = QStringLiteral("[%1]").arg(QLatin1String(k));
                        if (line.contains(tag, Qt::CaseInsensitive)) {
                            line.remove(tag, Qt::CaseInsensitive);
                            plain = false;
                            if (qstrcmp(k, "right") == 0)
                                o.insert(QStringLiteral("axis"), QStringLiteral("right"));
                            else
                                o.insert(QStringLiteral("kind"), QLatin1String(k));
                        }
                    }
                    const int colon = line.indexOf(QLatin1Char(':'));
                    const QString name = colon >= 0 ? line.left(colon).trimmed() : QString();
                    if (!name.isEmpty())
                        plain = false;
                    QJsonArray vals;
                    QStringList csv;
                    for (const QString& t : line.mid(colon + 1).split(QLatin1Char(','))) {
                        bool ok = false;
                        const double v = t.trimmed().toDouble(&ok);
                        vals.append(ok ? QJsonValue(v) : QJsonValue());
                        csv << (ok ? QString::number(v, 'g', 12) : QStringLiteral("0"));
                    }
                    o.insert(QStringLiteral("name"), name);
                    o.insert(QStringLiteral("values"), vals);
                    out.append(o);
                    if (first_vals.isEmpty())
                        first_vals = csv;
                }
                queue_edit(QStringLiteral("data"), first_vals.join(QLatin1Char(',')));
                queue_edit(QStringLiteral("series"),
                           plain ? QString() : QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact)));
                queue_edit(QStringLiteral("sample"), QString());
            });
            add_label(tr("Labels (comma separated)"));
            line_editor(QStringLiteral("labels"), c.config.value(QStringLiteral("labels")), QStringLiteral("Jan,Feb,Mar,Apr"));
        }
        build_chart_options(c, bound);
        add_label(tr("Caption (optional)"));
        line_editor(QStringLiteral("caption"), c.config.value(QStringLiteral("caption")));
        add_label(tr("Width (px)"));
        spin_row(QStringLiteral("width"), 200, 760, c.config.value(QStringLiteral("width"), QStringLiteral("640")).toInt(), 20);
    } else if (type == QLatin1String("stats_block")) {
        add_label(tr("Block title"));
        line_editor(QStringLiteral("title"), c.config.value(QStringLiteral("title"), QStringLiteral("Key Statistics")));
        if (!bound) {
            add_label(tr("Figures (one per line, Label: Value)"));
            config_text(QStringLiteral("data"), c.config.value(QStringLiteral("data")), 200,
                        QStringLiteral("P/E Ratio: 28.4\nDividend Yield: 0.51%"));
        }
    } else if (type == QLatin1String("sparkline")) {
        add_label(tr("Label"));
        line_editor(QStringLiteral("title"), c.config.value(QStringLiteral("title")), tr("e.g. AAPL"));
        if (!bound) {
            add_label(tr("Current value"));
            line_editor(QStringLiteral("current"), c.config.value(QStringLiteral("current")));
            add_label(tr("Change %"));
            line_editor(QStringLiteral("change_pct"), c.config.value(QStringLiteral("change_pct")), tr("e.g. +2.34"));
            add_label(tr("Series (comma separated)"));
            config_text(QStringLiteral("data"), c.config.value(QStringLiteral("data")), 70);
        }
    } else if (type == QLatin1String("image")) {
        add_label(tr("Image file"));
        auto* path_lbl = add_label(c.config.value(QStringLiteral("path")).isEmpty()
                                       ? tr("(none)")
                                       : QFileInfo(c.config.value(QStringLiteral("path"))).fileName(),
                                   "hint");
        add_picture_buttons(id, path_lbl);
        add_label(tr("Width (px)"));
        spin_row(QStringLiteral("width"), 40, 760, c.config.value(QStringLiteral("width"), QStringLiteral("400")).toInt(), 10);
        add_label(tr("Alignment"));
        combo_row(QStringLiteral("align"), {{"left", tr("Left")}, {"center", tr("Centre")}, {"right", tr("Right")}},
                  c.config.value(QStringLiteral("align"), QStringLiteral("left")));
        add_label(tr("Caption (optional)"));
        line_editor(QStringLiteral("caption"), c.config.value(QStringLiteral("caption")), tr("e.g. Figure 1: Revenue"));
    } else if (type == QLatin1String("cover")) {
        build_cover_editor(c);
    } else if (type == QLatin1String("page_break")) {
        add_label(tr("Starts a new page here — on screen and in the PDF."), "hint");
    } else if (type == QLatin1String("toc")) {
        add_label(tr("Lists every heading with its page number (numbers follow the page layout)."), "hint");
    } else if (type == QLatin1String("divider")) {
        add_label(tr("A thin rule across the page."), "hint");
    } else if (type == QLatin1String("market_data")) {
        add_label(tr("A live quote: pick the symbol below."), "hint");
    }

    if (!pp_sources_for(type).isEmpty())
        build_data_source(c);

    auto* rule = new QFrame(editor_widget_);
    rule->setFixedHeight(1);
    rule->setProperty("rbRole", QStringLiteral("rule"));
    editor_layout_->addWidget(rule);
    auto* row = new QWidget(editor_widget_);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    auto* dup = new QPushButton(tr("Duplicate"), row);
    auto* del = new QPushButton(tr("Delete"), row);
    del->setProperty("rbRole", QStringLiteral("danger"));
    rl->addWidget(dup);
    rl->addWidget(del);
    editor_layout_->addWidget(row);
    connect(dup, &QPushButton::clicked, this, [this, id]() {
        flush_pending();
        emit duplicate_requested(id);
    });
    connect(del, &QPushButton::clicked, this, [this, id]() {
        flush_pending();
        emit delete_requested(id);
    });
    editor_layout_->addStretch();
}

// ── Data source group ───────────────────────────────────────────────────────

void PropertiesPanel::build_data_source(const ReportComponent& c) {
    const int id = c.id;
    const QString type = c.type;
    const rep::Binding b = rep::binding_of(c);
    const QVector<PpSource> sources = pp_sources_for(type);

    auto* rule = new QFrame(editor_widget_);
    rule->setFixedHeight(1);
    rule->setProperty("rbRole", QStringLiteral("rule"));
    editor_layout_->addWidget(rule);
    add_label(tr("DATA SOURCE"), "section");

    auto emit_binding = [this, id](const rep::Binding& nb) {
        flush_pending();
        emit binding_changed(id, nb.to_string());
    };

    QVector<QPair<QString, QString>> items;
    for (const PpSource& s : sources)
        items.append({s.key, s.label});
    QComboBox* src = pp_combo(editor_widget_, items, b.is_valid() ? pp_source_key(b) : QString());
    editor_layout_->addWidget(src);
    connect(src, &QComboBox::currentIndexChanged, this, [this, src, b, type, emit_binding](int) {
        const QString key = src->currentData().toString();
        if (key.isEmpty()) {
            flush_pending();
            emit binding_changed(current_id_, QString()); // unbind, keep the numbers
            return;
        }
        emit_binding(pp_default_binding(key, b, type));
    });
    if (!b.is_refreshable()) {
        add_label(tr("Pick a source to fill this block from live data; it keeps the last numbers when offline."),
                  "hint");
        return;
    }

    const QString& s = b.source;
    const bool multi = s == QLatin1String(rep::source::kHistory) ||
                       (s == QLatin1String(rep::source::kQuotes) && b.param("universe").isEmpty());
    const bool single = s == QLatin1String(rep::source::kQuote) || s == QLatin1String(rep::source::kInfo) ||
                        s == QLatin1String(rep::source::kFinancials) || s == QLatin1String(rep::source::kPeers) ||
                        s == QLatin1String(rep::source::kNews);

    if (s == QLatin1String(rep::source::kQuotes)) {
        add_label(tr("Which symbols"));
        QComboBox* uni = pp_combo(editor_widget_,
                                  {{"", tr("My list (below)")},
                                   {"indices", tr("Major indices")},
                                   {"macro", tr("Rates, FX and commodities")},
                                   {"movers", tr("Active US stocks (movers)")},
                                   {"crypto", tr("Crypto")}},
                                  b.param("universe"));
        editor_layout_->addWidget(uni);
        connect(uni, &QComboBox::currentIndexChanged, this, [uni, b, emit_binding](int) {
            rep::Binding nb = b;
            const QString u = uni->currentData().toString();
            if (u.isEmpty())
                nb.params.remove(QStringLiteral("universe"));
            else
                nb.params.insert(QStringLiteral("universe"), u);
            emit_binding(nb);
        });
        add_label(tr("Order"));
        QComboBox* sort = pp_combo(editor_widget_,
                                   {{"", tr("As listed")},
                                    {"change_desc", tr("Best performers first")},
                                    {"change_asc", tr("Worst performers first")},
                                    {"abs_change_desc", tr("Biggest moves first")}},
                                   b.param("sort"));
        editor_layout_->addWidget(sort);
        connect(sort, &QComboBox::currentIndexChanged, this, [sort, b, emit_binding](int) {
            rep::Binding nb = b;
            nb.params.insert(QStringLiteral("sort"), sort->currentData().toString());
            emit_binding(nb);
        });
    }

    if (s == QLatin1String(rep::source::kWatchlist)) {
        add_label(tr("Watchlist"));
        auto* wl = new QComboBox(editor_widget_);
        wl->addItem(tr("My default watchlist"), QString());
        const QString current = b.param("watchlist_id");
        if (!current.isEmpty())
            wl->addItem(b.param("watchlist_name", tr("Loading…")), current);
        wl->setCurrentIndex(current.isEmpty() ? 0 : 1);
        editor_layout_->addWidget(wl);
        // The lists come from the database on the library's worker, never this thread.
        services::ReportLibrary::instance().run_on_db_worker(
            []() -> QVariant {
                QVariantList out;
                auto r = WatchlistRepository::instance().list_all();
                if (r.is_ok())
                    for (const Watchlist& w : r.value())
                        out << QVariant(QStringList{w.id, w.name});
                return out;
            },
            wl, [wl, current](const QVariant& v) {
                const QSignalBlocker block(wl);
                wl->clear();
                wl->addItem(PropertiesPanel::tr("My default watchlist"), QString());
                for (const QVariant& x : v.toList()) {
                    const QStringList p = x.toStringList();
                    if (p.size() == 2)
                        wl->addItem(p[1], p[0]);
                }
                const int i = wl->findData(current);
                wl->setCurrentIndex(i >= 0 ? i : 0);
            });
        connect(wl, &QComboBox::currentIndexChanged, this, [wl, b, emit_binding](int) {
            rep::Binding nb = b;
            nb.params.insert(QStringLiteral("watchlist_id"), wl->currentData().toString());
            nb.params.insert(QStringLiteral("watchlist_name"), wl->currentData().toString().isEmpty() ? QString() : wl->currentText());
            emit_binding(nb);
        });
        add_label(tr("Order"));
        QComboBox* sort = pp_combo(editor_widget_,
                                   {{"", tr("As listed")},
                                    {"change_desc", tr("Best performers first")},
                                    {"change_asc", tr("Worst performers first")},
                                    {"abs_change_desc", tr("Biggest moves first")}},
                                   b.param("sort"));
        editor_layout_->addWidget(sort);
        connect(sort, &QComboBox::currentIndexChanged, this, [sort, b, emit_binding](int) {
            rep::Binding nb = b;
            nb.params.insert(QStringLiteral("sort"), sort->currentData().toString());
            emit_binding(nb);
        });
    }

    if (single || multi) {
        add_label(multi ? tr("Symbols (up to 6)") : tr("Symbol"));
        auto* box = new ui::SymbolSearchBox(editor_widget_);
        box->set_compact(true);
        box->set_recent_key(QStringLiteral("report_builder"));
        box->set_raw_text_allowed(true);
        if (multi) {
            box->set_placeholder(tr("Add a symbol: ticker, name or ISIN"));
            box->set_clear_on_pick(true);
        } else {
            box->set_placeholder(tr("Ticker, name or ISIN"));
            box->set_clear_on_pick(false);
            box->set_text(b.symbol());
        }
        editor_layout_->addWidget(box);
        if (!first_editor_)
            first_editor_ = box->line_edit();
        auto apply_symbol = [b, multi, emit_binding](QString sym) {
            sym = sym.trimmed().toUpper();
            if (sym.isEmpty())
                return;
            pp_remember_symbol(sym);
            rep::Binding nb = b;
            if (multi) {
                QStringList syms = b.symbols();
                if (syms.contains(sym) || syms.size() >= 6)
                    return;
                syms << sym;
                nb.params.insert(QStringLiteral("symbols"), rep::to_json_array(syms));
                nb.params.remove(QStringLiteral("symbol"));
            } else {
                if (sym == b.symbol())
                    return;
                nb.params.insert(QStringLiteral("symbol"), sym);
            }
            emit_binding(nb);
        };
        connect(box, &ui::SymbolSearchBox::instrument_picked, this,
                [apply_symbol](const core::instrument::Instrument& inst) {
                    apply_symbol(inst.symbol.isEmpty() ? inst.display : inst.symbol);
                });
        connect(box, &ui::SymbolSearchBox::text_committed_raw, this, [apply_symbol](const QString& t) { apply_symbol(t); });
        if (multi) {
            const QStringList syms = b.symbols();
            auto* row = new QWidget(editor_widget_);
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(6);
            auto* list = new QLabel(syms.isEmpty() ? tr("(none yet)") : syms.join(QStringLiteral(", ")), row);
            list->setWordWrap(true);
            rl->addWidget(list, 1);
            auto* clear = new QPushButton(tr("Clear"), row);
            clear->setEnabled(!syms.isEmpty());
            rl->addWidget(clear);
            editor_layout_->addWidget(row);
            connect(clear, &QPushButton::clicked, this, [b, emit_binding]() {
                rep::Binding nb = b;
                nb.params.insert(QStringLiteral("symbols"), QJsonArray());
                nb.params.remove(QStringLiteral("symbol"));
                emit_binding(nb);
            });
        }
    }

    auto param_combo = [this, b, emit_binding](const QString& label, const char* key,
                                               const QVector<QPair<QString, QString>>& items, const QString& fallback) {
        add_label(label);
        QComboBox* cb = pp_combo(editor_widget_, items, b.param(key, fallback));
        editor_layout_->addWidget(cb);
        const QString k = QLatin1String(key);
        connect(cb, &QComboBox::currentIndexChanged, this, [cb, b, k, emit_binding](int) {
            rep::Binding nb = b;
            nb.params.insert(k, cb->currentData().toString());
            emit_binding(nb);
        });
    };
    auto param_spin = [this, b, emit_binding](const QString& label, const char* key, int lo, int hi, int fallback) {
        add_label(label);
        auto* sp = new QSpinBox(editor_widget_);
        sp->setRange(lo, hi);
        sp->setValue(b.int_param(key, fallback));
        editor_layout_->addWidget(sp);
        const QString k = QLatin1String(key);
        // Committed on editing finished (not every arrow click starts a fetch).
        connect(sp, &QSpinBox::editingFinished, this, [sp, b, k, emit_binding]() {
            if (sp->value() == b.int_param(k.toLatin1().constData(), -1))
                return;
            rep::Binding nb = b;
            nb.params.insert(k, sp->value());
            emit_binding(nb);
        });
    };

    if (s == QLatin1String(rep::source::kHistory)) {
        param_combo(tr("Range"), "range",
                    {{"1mo", tr("1 month")}, {"3mo", tr("3 months")}, {"6mo", tr("6 months")}, {"ytd", tr("Year to date")},
                     {"1y", tr("1 year")}, {"2y", tr("2 years")}, {"5y", tr("5 years")}, {"max", tr("Maximum")}},
                    QStringLiteral("6mo"));
        param_combo(tr("Bar size"), "interval",
                    {{"1d", tr("Daily")}, {"1wk", tr("Weekly")}, {"1mo", tr("Monthly")}, {"1h", tr("Hourly")}},
                    QStringLiteral("1d"));
        if (type == QLatin1String("chart")) {
            param_combo(tr("Show"), "mode",
                        {{"price", tr("Price")}, {"rebased", tr("Rebased to 100 (compare)")}, {"pct", tr("% change")}},
                        QStringLiteral("price"));
            param_combo(tr("Chart style"), "chart",
                        {{"", tr("As chosen above")}, {"line", tr("Line")}, {"candle", tr("Candles (one symbol)")}, {"bar", tr("Bars")}},
                        QString());
        }
    } else if (s == QLatin1String(rep::source::kFinancials)) {
        param_combo(tr("Statement"), "statement",
                    {{"income", tr("Income statement")}, {"balance", tr("Balance sheet")}, {"cashflow", tr("Cash flow")}},
                    QStringLiteral("income"));
        param_combo(tr("Periods"), "period", {{"annual", tr("Annual")}, {"quarterly", tr("Quarterly")}},
                    QStringLiteral("annual"));
        param_spin(tr("How many periods"), "n", 1, 12, 4);
        if (type == QLatin1String("chart")) {
            add_label(tr("Line items (comma separated, as the statement names them)"));
            QStringList items;
            for (const QJsonValue& v : b.params.value(QStringLiteral("items")).toArray())
                items << v.toString();
            auto* e = new QLineEdit(items.join(QStringLiteral(", ")), editor_widget_);
            e->setPlaceholderText(QStringLiteral("Total Revenue, Net Income, Diluted EPS"));
            editor_layout_->addWidget(e);
            connect(e, &QLineEdit::editingFinished, this, [e, b, emit_binding]() {
                QStringList list;
                for (const QString& t : e->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
                    if (!t.trimmed().isEmpty())
                        list << t.trimmed();
                rep::Binding nb = b;
                nb.params.insert(QStringLiteral("items"), rep::to_json_array(list));
                if (nb != b)
                    emit_binding(nb);
            });
        }
    } else if (s == QLatin1String(rep::source::kWatchlist)) {
        param_spin(tr("Show at most (0 = all)"), "limit", 0, 60, 0);
    } else if (s == QLatin1String(rep::source::kEconCalendar)) {
        // A text param committed on editing finished (never per keystroke: each change refetches).
        auto param_text = [this, b, emit_binding](const QString& label, const char* key, const QString& placeholder,
                                                  bool upper) {
            add_label(label);
            auto* e = new QLineEdit(b.param(key), editor_widget_);
            e->setPlaceholderText(placeholder);
            editor_layout_->addWidget(e);
            const QString k = QLatin1String(key);
            connect(e, &QLineEdit::editingFinished, this, [e, b, k, upper, emit_binding]() {
                const QString v = upper ? e->text().trimmed().toUpper() : e->text().trimmed();
                rep::Binding nb = b;
                nb.params.insert(k, v);
                if (nb != b)
                    emit_binding(nb);
            });
        };
        param_text(tr("Day (empty = the day of each refresh; {{date}} = the report's date)"), "date",
                   tr("{{date}}, 2026-11-17 or empty"), false);
        param_combo(tr("Days shown"), "period",
                    {{"day", tr("That day only")},
                     {"week", tr("Its week, Monday to Sunday")},
                     {"rest_of_week", tr("From that day to Sunday")}},
                    QStringLiteral("week"));
        param_combo(tr("Impact"), "impact",
                    {{"high", tr("High only")},
                     {"medium", tr("Medium and high")},
                     {"low", tr("Low to high (no holidays)")},
                     {"all", tr("All, including holidays")}},
                    QStringLiteral("medium"));
        param_text(tr("Currencies (comma separated; empty = all)"), "currencies", tr("USD, EUR, GBP"), true);
        param_spin(tr("Show at most (0 = all)"), "limit", 0, 300, 0);
        add_label(tr("Free source: Forex Factory's public weekly export — the current week only, forecast and "
                     "previous values (no actual figures), times in this computer's time zone."),
                  "hint");
    } else if (s == QLatin1String(rep::source::kPeers)) {
        param_spin(tr("How many peers"), "count", 1, 12, 5);
    } else if (s == QLatin1String(rep::source::kNews) || s == QLatin1String(rep::source::kHeadlines)) {
        param_spin(tr("How many articles"), "count", 1, 30, s == QLatin1String(rep::source::kNews) ? 8 : 10);
    } else if (s == QLatin1String(rep::source::kQuotes)) {
        param_spin(tr("Show at most (0 = all)"), "limit", 0, 50, 0);
    } else if (s == QLatin1String(rep::source::kPortfolio)) {
        add_label(tr("Portfolio"));
        auto* pc = new QComboBox(editor_widget_);
        pc->addItem(tr("My first portfolio"), QString());
        const QString current = b.param("portfolio_id");
        if (!current.isEmpty())
            pc->addItem(tr("Loading…"), current);
        pc->setCurrentIndex(current.isEmpty() ? 0 : 1);
        editor_layout_->addWidget(pc);
        // Filled asynchronously; the list never blocks the panel.
        auto& ps = services::PortfolioService::instance();
        connect(&ps, &services::PortfolioService::portfolios_loaded, pc,
                [pc, current](const QVector<portfolio::Portfolio>& list) {
                    const QSignalBlocker block(pc);
                    pc->clear();
                    pc->addItem(PropertiesPanel::tr("My first portfolio"), QString());
                    for (const auto& p : list)
                        pc->addItem(p.name, p.id);
                    const int i = pc->findData(current);
                    pc->setCurrentIndex(i >= 0 ? i : 0);
                });
        connect(pc, &QComboBox::currentIndexChanged, this, [pc, b, emit_binding](int) {
            rep::Binding nb = b;
            nb.params.insert(QStringLiteral("portfolio_id"), pc->currentData().toString());
            emit_binding(nb);
        });
        QTimer::singleShot(0, pc, []() { services::PortfolioService::instance().load_portfolios(); });
    }

    auto* row = new QWidget(editor_widget_);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 4, 0, 0);
    rl->setSpacing(6);
    refresh_btn_ = new QPushButton(tr("REFRESH"), row);
    refresh_btn_->setProperty("rbRole", QStringLiteral("primary"));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    auto* unbind = new QPushButton(tr("Unbind (keep numbers)"), row);
    unbind->setToolTip(tr("Stop refreshing this block; its current numbers stay and become editable"));
    rl->addWidget(refresh_btn_);
    rl->addWidget(unbind, 1);
    editor_layout_->addWidget(row);
    connect(refresh_btn_, &QPushButton::clicked, this, [this, id]() {
        flush_pending();
        emit refresh_requested(id);
    });
    connect(unbind, &QPushButton::clicked, this, [this, id]() {
        flush_pending();
        emit binding_changed(id, QString());
    });
    data_status_ = add_label(QString());
    update_data_status();
}

// ── AI drafting actions on a text block ─────────────────────────────────────

void PropertiesPanel::add_ai_actions(QTextEdit* editor, int id) {
    // A menu of actions; choosing one opens the proposal panel — nothing is sent from here.
    auto* row = new QWidget(editor_widget_);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    auto* btn = new QToolButton(row);
    btn->setText(tr("AI draft ▾"));
    btn->setToolTip(tr("Draft or rewrite this text with your AI model. You see what is sent and review the "
                       "proposal before anything changes."));
    btn->setPopupMode(QToolButton::InstantPopup);
    btn->setCursor(Qt::PointingHandCursor);
    auto* menu = new QMenu(btn);
    auto act = [this, menu, editor, id](const QString& label, const QString& action, const QString& tone = {}) {
        QAction* a = menu->addAction(label);
        connect(a, &QAction::triggered, this, [this, editor, id, action, tone]() {
            flush_pending();
            const QTextCursor cur = editor->textCursor();
            const bool sel = cur.hasSelection();
            emit ai_requested(id, action, tone, sel ? cur.selectionStart() : -1, sel ? cur.selectionEnd() : -1);
        });
    };
    act(tr("Draft from the report's data"), QStringLiteral("draft"));
    act(tr("Rewrite"), QStringLiteral("rewrite"));
    act(tr("Shorten"), QStringLiteral("shorten"));
    act(tr("Expand"), QStringLiteral("expand"));
    act(tr("Fix grammar and spelling"), QStringLiteral("grammar"));
    QMenu* tone = menu->addMenu(tr("Change tone"));
    for (const auto& t : QVector<QPair<QString, QString>>{{QStringLiteral("formal"), tr("Formal")},
                                                          {QStringLiteral("plain"), tr("Plain English")},
                                                          {QStringLiteral("confident"), tr("Confident")},
                                                          {QStringLiteral("neutral"), tr("Neutral")}}) {
        QAction* a = tone->addAction(t.second);
        const QString key = t.first;
        connect(a, &QAction::triggered, this, [this, editor, id, key]() {
            flush_pending();
            const QTextCursor cur = editor->textCursor();
            const bool sel = cur.hasSelection();
            emit ai_requested(id, QStringLiteral("tone"), key, sel ? cur.selectionStart() : -1, sel ? cur.selectionEnd() : -1);
        });
    }
    btn->setMenu(menu);
    rl->addWidget(btn);
    auto* hint = new QLabel(tr("works on the selection, or the whole text"), row);
    hint->setProperty("rbRole", QStringLiteral("hint"));
    rl->addWidget(hint, 1);
    const int at = editor_layout_->indexOf(editor);
    editor_layout_->insertWidget(at >= 0 ? at : editor_layout_->count(), row);
}

// ── Pictures (image block, cover logo) ──────────────────────────────────────

void PropertiesPanel::add_picture_buttons(int id, QLabel* path_lbl) {
    auto* row = new QWidget(editor_widget_);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    auto* browse = new QPushButton(tr("Browse file…"), row);
    auto* paste = new QPushButton(tr("Paste"), row);
    paste->setToolTip(tr("Paste an image from the clipboard"));
    rl->addWidget(browse, 1);
    rl->addWidget(paste, 1);
    editor_layout_->addWidget(row);
    auto set_picture = [this, id](const QString& path) {
        flush_pending();
        emit config_changed(id, QStringLiteral("path"), path);
        // A new picture: the copy carried for sync belongs to the old one.
        emit config_changed(id, QStringLiteral("asset"), QString());
        emit config_changed(id, QStringLiteral("image_b64"), QString());
    };
    connect(browse, &QPushButton::clicked, this, [this, path_lbl, set_picture]() {
        const QString p = QFileDialog::getOpenFileName(this, tr("Select image"), QString(),
                                                       tr("Images (*.png *.jpg *.jpeg *.bmp *.svg *.gif *.webp)"));
        if (p.isEmpty())
            return;
        path_lbl->setText(QFileInfo(p).fileName());
        set_picture(p);
    });
    connect(paste, &QPushButton::clicked, this, [path_lbl, set_picture]() {
        const QMimeData* mime = QApplication::clipboard()->mimeData();
        if (!mime || !mime->hasImage()) {
            path_lbl->setText(tr("(no image in the clipboard)"));
            return;
        }
        const QImage img = qvariant_cast<QImage>(mime->imageData());
        // A hand-off file in the app's own temp folder (never a fixed name in the OS temp folder: the
        // other edition's uninstaller sweeps %TEMP%\fincept_*). It is gone once managed storage has
        // its copy — and on every failure — so the report never points at it.
        const QString tmp =
            AppPaths::temp_file(QStringLiteral("report-paste-%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        if (img.isNull() || !img.save(tmp, "PNG")) {
            QFile::remove(tmp);
            path_lbl->setText(tr("(the clipboard image could not be saved)"));
            return;
        }
        auto& files = services::FileManagerService::instance();
        const QString fid = files.import_file(tmp, QStringLiteral("report_builder"));
        const services::ManagedFile mf = fid.isEmpty() ? services::ManagedFile() : files.find_by_id(fid);
        QFile::remove(tmp);
        if (mf.name.isEmpty()) {
            path_lbl->setText(tr("(the clipboard image could not be stored in the app's files)"));
            return;
        }
        path_lbl->setText(tr("(clipboard image)"));
        set_picture(files.full_path(mf.name));
    });
}

// ── Cover page ──────────────────────────────────────────────────────────────

void PropertiesPanel::build_cover_editor(const ReportComponent& c) {
    const int id = c.id;
    add_label(tr("A title page of its own; the report starts on the next page. Empty fields use the document info "
                 "(title, author, company, date). {{variables}} work here."),
              "hint");
    auto line = [this, &c](const QString& label, const char* key, const QString& placeholder) {
        add_label(label);
        auto* e = new QLineEdit(c.config.value(QLatin1String(key)), editor_widget_);
        e->setPlaceholderText(placeholder);
        const QString k = QLatin1String(key);
        connect(e, &QLineEdit::textEdited, this, [this, k](const QString& v) { queue_edit(k, v); });
        editor_layout_->addWidget(e);
        if (!first_editor_)
            first_editor_ = e;
    };
    line(tr("Title"), "title", tr("The report title"));
    line(tr("Subtitle"), "subtitle", tr("e.g. Q3 2026 results: what to expect"));
    line(tr("Byline"), "byline", tr("Author · company"));
    line(tr("Date"), "date", tr("The document date"));
    add_label(tr("Logo (optional)"));
    const QString path = c.config.value(QStringLiteral("path"));
    auto* path_lbl = add_label(path.isEmpty() ? tr("(none)") : QFileInfo(path).fileName(), "hint");
    add_picture_buttons(id, path_lbl);
    add_label(tr("Disclaimer at the foot of the page (optional)"));
    auto* d = new QTextEdit(editor_widget_);
    d->setAcceptRichText(false);
    d->setPlainText(c.config.value(QStringLiteral("disclaimer")));
    d->setMaximumHeight(90);
    connect(d, &QTextEdit::textChanged, this, [this, d]() { queue_edit(QStringLiteral("disclaimer"), d->toPlainText()); });
    editor_layout_->addWidget(d);
}

// ── Table formats ───────────────────────────────────────────────────────────

void PropertiesPanel::build_table_formats(const ReportComponent& c) {
    namespace tf = rep::tablefmt;
    const QVector<QStringList> grid = rep::table::parse(c.config.value(QStringLiteral("csv")));
    int ncols = 0;
    for (const QStringList& r : grid)
        ncols = std::max(ncols, static_cast<int>(r.size()));
    if (ncols == 0)
        ncols = std::clamp(c.config.value(QStringLiteral("cols"), QStringLiteral("3")).toInt(), 1, 20);
    auto cols = std::make_shared<QVector<tf::ColumnFormat>>(tf::columns_from_text(c.config.value(QLatin1String(tf::kColFmtKey))));
    while (cols->size() < ncols)
        cols->append(tf::ColumnFormat());
    auto style = std::make_shared<tf::TableStyle>(tf::style_from_text(c.config.value(QLatin1String(tf::kStyleKey))));
    if (format_column_block_ != c.id) {
        format_column_block_ = c.id;
        format_column_ = 0;
    }
    format_column_ = std::clamp(format_column_, 0, ncols - 1);

    auto* rule = new QFrame(editor_widget_);
    rule->setFixedHeight(1);
    rule->setProperty("rbRole", QStringLiteral("rule"));
    editor_layout_->addWidget(rule);
    add_label(tr("FORMAT"), "section");

    auto push_style = [this, style]() { queue_edit(QLatin1String(tf::kStyleKey), tf::style_to_text(*style)); };
    auto push_cols = [this, cols]() { queue_edit(QLatin1String(tf::kColFmtKey), tf::columns_to_text(*cols)); };

    // Table options.
    auto* opts = new QWidget(editor_widget_);
    auto* ol = new QHBoxLayout(opts);
    ol->setContentsMargins(0, 0, 0, 0);
    ol->setSpacing(6);
    QComboBox* zebra = pp_combo(opts, {{"auto", tr("Zebra: auto")}, {"on", tr("Zebra: on")}, {"off", tr("Zebra: off")}},
                                style->zebra);
    QComboBox* totals = pp_combo(opts, {{"none", tr("No totals row")}, {"sum", tr("Add a totals row")},
                                        {"last", tr("Last row is a total")}},
                                 style->totals);
    ol->addWidget(zebra, 1);
    ol->addWidget(totals, 1);
    editor_layout_->addWidget(opts);
    connect(zebra, &QComboBox::currentIndexChanged, this, [zebra, style, push_style](int) {
        style->zebra = zebra->currentData().toString();
        push_style();
    });
    connect(totals, &QComboBox::currentIndexChanged, this, [totals, style, push_style](int) {
        style->totals = totals->currentData().toString();
        push_style();
    });
    auto* opts2 = new QWidget(editor_widget_);
    auto* ol2 = new QHBoxLayout(opts2);
    ol2->setContentsMargins(0, 0, 0, 0);
    ol2->setSpacing(6);
    auto* repeat = new QCheckBox(tr("Repeat header on each page"), opts2);
    repeat->setChecked(style->header_repeat);
    auto* table_ccy = new QLineEdit(style->currency, opts2);
    table_ccy->setPlaceholderText(tr("Currency (USD)"));
    table_ccy->setMaxLength(8);
    ol2->addWidget(repeat, 1);
    ol2->addWidget(table_ccy);
    editor_layout_->addWidget(opts2);
    connect(repeat, &QCheckBox::toggled, this, [style, push_style](bool on) {
        style->header_repeat = on;
        push_style();
    });
    connect(table_ccy, &QLineEdit::textEdited, this, [style, push_style](const QString& v) {
        style->currency = v.trimmed().toUpper();
        push_style();
    });

    // One column at a time.
    add_label(tr("Column"));
    auto* which = new QComboBox(editor_widget_);
    for (int k = 0; k < ncols; ++k) {
        const QString head = !grid.isEmpty() && k < grid.first().size() ? grid.first()[k] : QString();
        which->addItem(head.isEmpty() ? tr("Column %1").arg(k + 1) : QStringLiteral("%1 · %2").arg(k + 1).arg(head.left(24)));
    }
    which->setCurrentIndex(format_column_);
    editor_layout_->addWidget(which);

    auto* grid_w = new QWidget(editor_widget_);
    auto* gl = new QVBoxLayout(grid_w);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(4);
    auto row = [grid_w, gl](QWidget* a, QWidget* b) {
        auto* w = new QWidget(grid_w);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(6);
        l->addWidget(a, 1);
        if (b)
            l->addWidget(b, 1);
        gl->addWidget(w);
    };
    QComboBox* type = pp_combo(grid_w, {{"auto", tr("As typed")}, {"text", tr("Text")}, {"number", tr("Number")},
                                        {"percent", tr("Percent")}, {"currency", tr("Currency")}, {"date", tr("Date")}},
                               QStringLiteral("auto"));
    auto* dec = new QSpinBox(grid_w);
    dec->setRange(-1, 8);
    dec->setSpecialValueText(tr("Auto decimals"));
    dec->setPrefix(tr("Decimals: "));
    row(type, dec);
    QComboBox* neg = pp_combo(grid_w, {{"minus", tr("Negatives: -1,234")}, {"brackets", tr("Negatives: (1,234)")}},
                              QStringLiteral("minus"));
    QComboBox* div = pp_combo(grid_w, {{"1", tr("Units as typed")}, {"1000", tr("In thousands")},
                                       {"1000000", tr("In millions")}, {"1000000000", tr("In billions")}},
                              QStringLiteral("1"));
    row(neg, div);
    QComboBox* align = pp_combo(grid_w, {{"auto", tr("Align: auto")}, {"left", tr("Align: left")},
                                         {"center", tr("Align: centre")}, {"right", tr("Align: right")}},
                                QStringLiteral("auto"));
    auto* width = new QSpinBox(grid_w);
    width->setRange(0, 100);
    width->setSuffix(QStringLiteral(" %"));
    width->setSpecialValueText(tr("Auto width"));
    row(align, width);
    QComboBox* heat = pp_combo(grid_w, {{"none", tr("No colour scale")}, {"green_red", tr("Colour: high green, low red")},
                                        {"red_green", tr("Colour: high red, low green")}, {"blue", tr("Colour: shades of blue")}},
                               QStringLiteral("none"));
    auto* ccy = new QLineEdit(grid_w);
    ccy->setPlaceholderText(tr("Currency (table's)"));
    ccy->setMaxLength(8);
    row(heat, ccy);
    editor_layout_->addWidget(grid_w);

    // Load the selected column into the editors without echoing edits back.
    auto load = [cols, type, dec, neg, div, align, width, heat, ccy](int k) {
        const tf::ColumnFormat& f = (*cols)[k];
        const QSignalBlocker b1(type), b2(dec), b3(neg), b4(div), b5(align), b6(width), b7(heat), b8(ccy);
        type->setCurrentIndex(std::max(0, type->findData(f.type)));
        dec->setValue(f.decimals);
        neg->setCurrentIndex(f.brackets ? 1 : 0);
        div->setCurrentIndex(std::max(0, div->findData(QString::number(static_cast<qint64>(f.divide)))));
        align->setCurrentIndex(std::max(0, align->findData(f.align)));
        width->setValue(static_cast<int>(f.width));
        heat->setCurrentIndex(std::max(0, heat->findData(f.heat)));
        ccy->setText(f.currency);
    };
    load(format_column_);
    connect(which, &QComboBox::currentIndexChanged, this, [this, load](int k) {
        if (k < 0)
            return;
        format_column_ = k;
        load(k);
    });
    auto edit = [this, cols, push_cols](const std::function<void(tf::ColumnFormat&)>& fn) {
        fn((*cols)[format_column_]);
        push_cols();
    };
    connect(type, &QComboBox::currentIndexChanged, this, [type, edit](int) {
        edit([type](tf::ColumnFormat& f) { f.type = type->currentData().toString(); });
    });
    connect(dec, &QSpinBox::valueChanged, this, [edit](int v) { edit([v](tf::ColumnFormat& f) { f.decimals = v; }); });
    connect(neg, &QComboBox::currentIndexChanged, this, [neg, edit](int) {
        edit([neg](tf::ColumnFormat& f) { f.brackets = neg->currentData().toString() == QLatin1String("brackets"); });
    });
    connect(div, &QComboBox::currentIndexChanged, this, [div, edit](int) {
        edit([div](tf::ColumnFormat& f) { f.divide = div->currentData().toString().toDouble(); });
    });
    connect(align, &QComboBox::currentIndexChanged, this, [align, edit](int) {
        edit([align](tf::ColumnFormat& f) { f.align = align->currentData().toString(); });
    });
    connect(width, &QSpinBox::valueChanged, this, [edit](int v) { edit([v](tf::ColumnFormat& f) { f.width = v; }); });
    connect(heat, &QComboBox::currentIndexChanged, this, [heat, edit](int) {
        edit([heat](tf::ColumnFormat& f) { f.heat = heat->currentData().toString(); });
    });
    connect(ccy, &QLineEdit::textEdited, this, [edit](const QString& v) {
        edit([v](tf::ColumnFormat& f) { f.currency = v.trimmed().toUpper(); });
    });
    add_label(tr("\"As typed\" keeps each cell's text; the other types reformat the numbers (the cells keep "
                 "their values)."),
              "hint");
}

// ── Chart options ───────────────────────────────────────────────────────────

void PropertiesPanel::build_chart_options(const ReportComponent& c, bool bound) {
    const int id = c.id;
    Q_UNUSED(id);
    auto* opts = new QWidget(editor_widget_);
    auto* ol = new QHBoxLayout(opts);
    ol->setContentsMargins(0, 0, 0, 0);
    ol->setSpacing(6);
    auto combo = [this, &c, opts, ol](const char* key, const QVector<QPair<QString, QString>>& items, const QString& fallback) {
        QComboBox* cb = pp_combo(opts, items, c.config.value(QLatin1String(key), fallback));
        const QString k = QLatin1String(key);
        connect(cb, &QComboBox::currentIndexChanged, this, [this, cb, k](int) { queue_edit(k, cb->currentData().toString()); });
        ol->addWidget(cb, 1);
        return cb;
    };
    combo("y_scale", {{"linear", tr("Linear scale")}, {"log", tr("Log scale")}}, QStringLiteral("linear"));
    combo("legend", {{"auto", tr("Legend: auto")}, {"top", tr("Legend: top")}, {"bottom", tr("Legend: bottom")},
                     {"right", tr("Legend: right")}, {"none", tr("No legend")}},
          QStringLiteral("auto"));
    editor_layout_->addWidget(opts);
    if (!bound) {
        add_label(tr("Show the values"));
        auto* t = pp_combo(editor_widget_, {{"none", tr("As entered")}, {"rebased", tr("Rebased to 100")},
                                            {"pct", tr("% change from the first value")}},
                           c.config.value(QStringLiteral("transform"), QStringLiteral("none")));
        connect(t, &QComboBox::currentIndexChanged, this, [this, t](int) { queue_edit(QStringLiteral("transform"), t->currentData().toString()); });
        editor_layout_->addWidget(t);
    }
    auto* marks = new QWidget(editor_widget_);
    auto* ml = new QHBoxLayout(marks);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(6);
    auto check = [this, &c, marks, ml](const char* key, const QString& label) {
        auto* cb = new QCheckBox(label, marks);
        cb->setChecked(c.config.value(QLatin1String(key)) == QLatin1String("1"));
        const QString k = QLatin1String(key);
        connect(cb, &QCheckBox::toggled, this, [this, k](bool on) { queue_edit(k, on ? QStringLiteral("1") : QString()); });
        ml->addWidget(cb);
    };
    check("annot_last", tr("Last value"));
    check("annot_minmax", tr("High / low"));
    ml->addStretch();
    editor_layout_->addWidget(marks);

    // Number format of the value axis.
    const rep::chart::AxisFormat af = rep::chart::parse_axis_format(c.config.value(QStringLiteral("y_format")));
    auto fmt = std::make_shared<rep::chart::AxisFormat>(af);
    auto push = [this, fmt]() { queue_edit(QStringLiteral("y_format"), rep::chart::axis_format_text(*fmt)); };
    auto* fw = new QWidget(editor_widget_);
    auto* fl = new QHBoxLayout(fw);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setSpacing(6);
    auto* prefix = new QLineEdit(af.prefix, fw);
    prefix->setPlaceholderText(tr("Prefix ($)"));
    prefix->setMaxLength(8);
    auto* suffix = new QLineEdit(af.suffix, fw);
    suffix->setPlaceholderText(tr("Suffix (%)"));
    suffix->setMaxLength(8);
    auto* dec = new QSpinBox(fw);
    dec->setRange(-1, 8);
    dec->setSpecialValueText(tr("Auto"));
    dec->setValue(af.decimals);
    dec->setToolTip(tr("Decimals on the value axis"));
    fl->addWidget(prefix, 1);
    fl->addWidget(suffix, 1);
    fl->addWidget(dec);
    add_label(tr("Value axis: prefix, suffix, decimals"));
    editor_layout_->addWidget(fw);
    connect(prefix, &QLineEdit::textEdited, this, [fmt, push](const QString& v) {
        fmt->prefix = v;
        push();
    });
    connect(suffix, &QLineEdit::textEdited, this, [fmt, push](const QString& v) {
        fmt->suffix = v;
        push();
    });
    connect(dec, &QSpinBox::valueChanged, this, [fmt, push](int v) {
        fmt->decimals = v;
        push();
    });
    auto* compact = new QCheckBox(tr("Abbreviate large numbers (1.2M)"), editor_widget_);
    compact->setChecked(af.compact);
    connect(compact, &QCheckBox::toggled, this, [fmt, push](bool on) {
        fmt->compact = on;
        push();
    });
    editor_layout_->addWidget(compact);

    if (c.config.value(QStringLiteral("chart_type")) == QLatin1String("histogram")) {
        add_label(tr("Bins (0 = automatic)"));
        auto* bins = new QSpinBox(editor_widget_);
        bins->setRange(0, 100);
        bins->setValue(c.config.value(QStringLiteral("bins")).toInt());
        connect(bins, &QSpinBox::valueChanged, this, [this](int v) { queue_edit(QStringLiteral("bins"), v > 0 ? QString::number(v) : QString()); });
        editor_layout_->addWidget(bins);
    }

    // Event markers: "x | text" per line (x = a label, a date yyyy-MM-dd or a point number).
    add_label(tr("Event markers — one per line: label or date | text"));
    QStringList lines;
    for (const QJsonValue& v : QJsonDocument::fromJson(c.config.value(QStringLiteral("events")).toUtf8()).array()) {
        const QJsonObject o = v.toObject();
        lines << o.value(QStringLiteral("x")).toVariant().toString() + QStringLiteral(" | ") + o.value(QStringLiteral("text")).toString();
    }
    auto* ev = new QTextEdit(editor_widget_);
    ev->setAcceptRichText(false);
    ev->setPlainText(lines.join(QLatin1Char('\n')));
    ev->setMaximumHeight(70);
    ev->setPlaceholderText(QStringLiteral("2026-10-30 | Q3 results"));
    connect(ev, &QTextEdit::textChanged, this, [this, ev]() {
        QJsonArray arr;
        for (const QString& line : ev->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const int bar = line.indexOf(QLatin1Char('|'));
            const QString x = (bar >= 0 ? line.left(bar) : line).trimmed();
            const QString text = bar >= 0 ? line.mid(bar + 1).trimmed() : QString();
            if (!x.isEmpty())
                arr.append(QJsonObject{{QStringLiteral("x"), x}, {QStringLiteral("text"), text}});
        }
        queue_edit(QStringLiteral("events"), arr.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    });
    editor_layout_->addWidget(ev);
}

void PropertiesPanel::update_data_status() {
    if (!data_status_ || !has_component_)
        return;
    const auto st = Binder::instance().state(current_id_);
    const QDateTime as_of = rep::as_of_of(last_component_);
    const QString when = rep::as_of_display(as_of);
    if (st.state == Binder::State::Loading || st.state == Binder::State::Queued) {
        data_status_->setText(st.state == Binder::State::Queued ? tr("Waiting for a free connection…")
                                                                : tr("Refreshing…"));
        pp_set_role(data_status_, "busy");
        if (refresh_btn_)
            refresh_btn_->setEnabled(false);
        return;
    }
    if (refresh_btn_)
        refresh_btn_->setEnabled(true);
    if (st.state == Binder::State::Failed) {
        data_status_->setText(as_of.isValid() ? tr("Could not load: %1 — showing data as of %2").arg(st.message, when)
                                              : tr("Could not load: %1").arg(st.message));
        pp_set_role(data_status_, "bad");
    } else if (!as_of.isValid()) {
        data_status_->setText(tr("Not fetched yet — press REFRESH."));
        pp_set_role(data_status_, "hint");
    } else if (rep::is_stale(last_component_, QDateTime::currentDateTime())) {
        data_status_->setText(tr("Out of date — data as of %1").arg(when));
        pp_set_role(data_status_, "warn");
    } else {
        data_status_->setText(tr("Updated %1").arg(when));
        pp_set_role(data_status_, "ok");
    }
}

void PropertiesPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PropertiesPanel::retranslateUi() {
    theme_btn_->setText(tr("Report theme…"));
    vars_btn_->setText(tr("Variables…"));
    if (has_component_)
        show_component(last_component_);
    else
        show_document(doc_meta_, doc_theme_, doc_blocks_, doc_pages_, doc_library_id_, doc_file_);
}

} // namespace fincept::screens
