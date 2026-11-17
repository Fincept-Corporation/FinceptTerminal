// ReportLibraryPage.cpp — see ReportLibraryPage.h.
//
// Chrome follows the terminal theme (tokens); the small page previews are drawn in the
// report's own paper / ink colours (they show what the document looks like).

#include "screens/report_builder/ReportLibraryPage.h"

#include "core/report/ReportBinding.h"
#include "core/report/ReportDocument.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens {

namespace rep = ::fincept::report;
using ::fincept::services::LibraryEntry;
using ::fincept::services::ReportLibrary;
using ::fincept::services::TemplateEntry;

namespace {

enum RlpRole {
    kRlpId = Qt::UserRole + 1,
    kRlpDesc,
    kRlpKinds,
    kRlpTheme,
    kRlpNeeds,
    kRlpLive,
    kRlpCategory,
    kRlpUser,
};

QString rlp_when(const QString& sqlite_utc) {
    QDateTime t = QDateTime::fromString(sqlite_utc.left(19), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!t.isValid())
        return sqlite_utc;
    t.setTimeZone(QTimeZone::UTC);
    return rep::as_of_display(t);
}

// A miniature of the page: the report's paper and ink, one shape per block.
void rlp_paint_wireframe(QPainter* p, const QRectF& r, const QStringList& kinds, const QString& theme_name) {
    const rep::ReportTheme t = rep::themes::by_name(theme_name);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->fillRect(r, QColor(t.page_bg));
    p->setPen(QPen(QColor(t.divider_color), 1));
    p->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    const double m = std::max(4.0, r.width() * 0.08);
    const double x = r.left() + m;
    const double w = r.width() - 2 * m;
    const double unit = std::max(2.0, r.height() * 0.022);
    double y = r.top() + m;
    const double bottom = r.bottom() - m;
    const QColor head(t.heading_color), accent(t.accent_color), meta(t.meta_color), divider(t.divider_color);
    QColor ink(t.text_color);
    ink.setAlphaF(0.45f);
    p->fillRect(QRectF(x, y, w * 0.62, unit * 1.6), head);
    y += unit * 2.6;
    p->fillRect(QRectF(x, y, w * 0.4, unit * 0.6), meta);
    y += unit * 1.4;
    p->fillRect(QRectF(x, y, w, std::max(1.0, unit * 0.3)), accent);
    y += unit * 1.4;
    for (const QString& k : kinds) {
        if (y > bottom - unit)
            break;
        if (k == QLatin1String("heading")) {
            y += unit * 0.6;
            p->fillRect(QRectF(x, y, w * 0.45, unit * 1.1), head);
            y += unit * 2.0;
        } else if (k == QLatin1String("text") || k == QLatin1String("quote") || k == QLatin1String("callout") ||
                   k == QLatin1String("code")) {
            for (int i = 0; i < 3 && y < bottom; ++i) {
                p->fillRect(QRectF(x, y, w * (i == 2 ? 0.6 : 1.0), unit * 0.5), ink);
                y += unit;
            }
            y += unit * 0.4;
        } else if (k == QLatin1String("list")) {
            for (int i = 0; i < 3 && y < bottom; ++i) {
                p->fillRect(QRectF(x, y, unit * 0.5, unit * 0.5), meta);
                p->fillRect(QRectF(x + unit, y, w * 0.55, unit * 0.5), ink);
                y += unit;
            }
            y += unit * 0.4;
        } else if (k == QLatin1String("table") || k == QLatin1String("stats_block")) {
            const double h = std::min(bottom - y, unit * (k == QLatin1String("table") ? 6.0 : 3.5));
            p->fillRect(QRectF(x, y, w, unit), QColor(t.table_header_bg));
            p->setPen(QPen(divider, 0.8));
            p->drawRect(QRectF(x, y, w, h));
            for (double yy = y + unit; yy < y + h; yy += unit)
                p->drawLine(QPointF(x, yy), QPointF(x + w, yy));
            p->drawLine(QPointF(x + w * 0.4, y), QPointF(x + w * 0.4, y + h));
            y += h + unit * 0.8;
        } else if (k == QLatin1String("chart") || k == QLatin1String("sparkline") || k == QLatin1String("image")) {
            const double h = std::min(bottom - y, unit * (k == QLatin1String("sparkline") ? 2.0 : 6.5));
            p->setPen(QPen(divider, 0.8));
            p->drawRect(QRectF(x + w * 0.1, y, w * 0.8, h));
            if (k == QLatin1String("image")) {
                p->drawLine(QPointF(x + w * 0.1, y), QPointF(x + w * 0.9, y + h));
                p->drawLine(QPointF(x + w * 0.9, y), QPointF(x + w * 0.1, y + h));
            } else {
                QPolygonF poly;
                for (int i = 0; i <= 8; ++i)
                    poly << QPointF(x + w * 0.12 + i * w * 0.095, y + h * (0.75 - 0.5 * ((i * 37 % 10) / 10.0)));
                p->setPen(QPen(accent, 1.2));
                p->drawPolyline(poly);
            }
            y += h + unit * 0.8;
        } else if (k == QLatin1String("market_data")) {
            p->fillRect(QRectF(x, y, w, unit * 1.6), QColor(t.quote_bg));
            p->fillRect(QRectF(x + unit * 0.4, y + unit * 0.5, w * 0.2, unit * 0.6), accent);
            y += unit * 2.4;
        } else if (k == QLatin1String("page_break")) {
            p->setPen(QPen(divider, 0.8, Qt::DashLine));
            p->drawLine(QPointF(x, y + unit), QPointF(x + w, y + unit));
            y += unit * 2;
        } else {
            p->fillRect(QRectF(x, y + unit * 0.4, w, std::max(1.0, unit * 0.2)), divider);
            y += unit;
        }
    }
    p->restore();
}

// Template card: miniature + name (+ LIVE chip).
class RlpTemplateDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static QSize card_size(const QFont& f) {
        const QFontMetrics fm(f);
        const int w = std::max(118, fm.averageCharWidth() * 17);
        return {w, static_cast<int>(w * 1.18) + fm.height() * 2 + 14};
    }
    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const override { return card_size(opt.font); }
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        p->save();
        const QRect r = opt.rect.adjusted(4, 4, -4, -4);
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hover = opt.state & QStyle::State_MouseOver;
        p->fillRect(r, QColor(selected ? ui::colors::BG_RAISED() : ui::colors::BG_SURFACE()));
        p->setPen(QPen(QColor(selected ? ui::colors::AMBER() : hover ? ui::colors::BORDER_BRIGHT() : ui::colors::BORDER_MED()),
                       selected ? 2 : 1));
        p->drawRect(r.adjusted(0, 0, -1, -1));
        const QFontMetrics fm(opt.font);
        const int text_h = fm.height() * 2 + 6;
        const QRectF thumb(r.left() + 8, r.top() + 8, r.width() - 16, r.height() - text_h - 14);
        const double tw = std::min(thumb.width(), thumb.height() / 1.414);
        rlp_paint_wireframe(p, QRectF(thumb.center().x() - tw / 2, thumb.top(), tw, tw * 1.414),
                            idx.data(kRlpKinds).toStringList(), idx.data(kRlpTheme).toString());
        const QRect tr(r.left() + 6, r.bottom() - text_h - 2, r.width() - 12, text_h);
        p->setPen(QColor(ui::colors::TEXT_PRIMARY()));
        p->setFont(opt.font);
        const QString name = idx.data(Qt::DisplayRole).toString();
        p->drawText(tr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                    fm.elidedText(name, Qt::ElideRight, tr.width() * 2 - fm.averageCharWidth()));
        if (idx.data(kRlpLive).toBool()) {
            const QString chip = ReportLibraryPage::tr("LIVE");
            QFont cf = opt.font;
            cf.setBold(true);
            cf.setPixelSize(std::max(9, cf.pixelSize() > 0 ? cf.pixelSize() - 2 : 10));
            const QFontMetrics cfm(cf);
            const QRect cr(r.right() - cfm.horizontalAdvance(chip) - 14, r.top() + 6, cfm.horizontalAdvance(chip) + 8,
                           cfm.height() + 2);
            p->fillRect(cr, QColor(ui::colors::SUCCESS()));
            p->setPen(QColor(ui::colors::TEXT_ON_ACCENT()));
            p->setFont(cf);
            p->drawText(cr, Qt::AlignCenter, chip);
        }
        p->restore();
    }
};

} // namespace

// ── Models / widgets ────────────────────────────────────────────────────────

class RbLibraryModel : public QAbstractTableModel {
  public:
    using QAbstractTableModel::QAbstractTableModel;
    void set_entries(QVector<LibraryEntry> e) {
        beginResetModel();
        entries_ = std::move(e);
        endResetModel();
    }
    const LibraryEntry* entry(int row) const { return row >= 0 && row < entries_.size() ? &entries_[row] : nullptr; }
    int row_of(qint64 id) const {
        for (int i = 0; i < entries_.size(); ++i)
            if (entries_[i].id == id)
                return i;
        return -1;
    }
    int rowCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : static_cast<int>(entries_.size()); }
    int columnCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : 4; }
    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (o != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        switch (section) {
        case 0:
            return ReportLibraryPage::tr("Report");
        case 1:
            return ReportLibraryPage::tr("Blocks");
        case 2:
            return ReportLibraryPage::tr("Updated");
        default:
            return ReportLibraryPage::tr("Size");
        }
    }
    QVariant data(const QModelIndex& i, int role) const override {
        const LibraryEntry* e = entry(i.row());
        if (!e)
            return {};
        if (role == Qt::DisplayRole) {
            switch (i.column()) {
            case 0: {
                QString t = e->title;
                if (e->unseen > 0)
                    t += ReportLibraryPage::tr("  ★ %n new", nullptr, e->unseen);
                if (e->bound)
                    t += QStringLiteral("  ↻");
                return t;
            }
            case 1:
                return e->blocks;
            case 2:
                return rlp_when(e->updated_at);
            default:
                return e->bytes >= 1024 * 1024 ? QStringLiteral("%1 MB").arg(e->bytes / 1048576.0, 0, 'f', 1)
                                                : QStringLiteral("%1 KB").arg(std::max(1, e->bytes / 1024));
            }
        }
        if (role == Qt::TextAlignmentRole && i.column() > 0)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        if (role == Qt::ToolTipRole && i.column() == 0)
            return e->outline.join(QStringLiteral(" · "));
        return {};
    }

  private:
    QVector<LibraryEntry> entries_;
};

class RbWireframe : public QWidget {
  public:
    using QWidget::QWidget;
    void show_kinds(const QStringList& kinds, const QString& theme) {
        kinds_ = kinds;
        theme_ = theme;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        if (kinds_.isEmpty() && theme_.isEmpty())
            return;
        const double h = height() - 4;
        const double w = std::min(width() - 4.0, h / 1.414);
        rlp_paint_wireframe(&p, QRectF((width() - w) / 2, 2, w, w * 1.414), kinds_, theme_);
    }

  private:
    QStringList kinds_;
    QString theme_;
};

// ── Page ────────────────────────────────────────────────────────────────────

ReportLibraryPage::ReportLibraryPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("rbLibrary"));
    setStyleSheet(
        QStringLiteral(
            "#rbLibrary { background: %1; }"
            "#rbLibrary QLabel { color: %2; background: transparent; font-size: %8px; }"
            "#rbLibrary QLabel[rbRole=\"title\"] { color: %3; font-weight: bold; font-size: %9px; }"
            "#rbLibrary QLabel[rbRole=\"section\"] { color: %4; font-weight: bold; font-size: %10px; }"
            "#rbLibrary QLabel[rbRole=\"hint\"] { color: %4; }"
            "#rbLibrary QLineEdit, #rbLibrary QComboBox { background: %5; color: %2; border: 1px solid %6; padding: 3px 6px;"
            " font-size: %8px; }"
            "#rbLibrary QPushButton { background: %7; color: %2; border: 1px solid %6; padding: 4px 12px; font-size: %8px; }"
            "#rbLibrary QPushButton:hover { background: %11; }"
            "#rbLibrary QPushButton:disabled { color: %4; }"
            "#rbLibrary QPushButton[rbRole=\"primary\"] { background: %3; color: %12; border-color: %3; font-weight: bold; }"
            "#rbLibrary QPushButton[rbRole=\"danger\"] { color: %13; border-color: %13; }"
            "#rbLibrary QListView, #rbLibrary QTableView { background: %5; color: %2; border: 1px solid %6;"
            " font-size: %8px; alternate-background-color: %14; selection-background-color: %7; selection-color: %2; }"
            "#rbLibrary QHeaderView::section { background: %7; color: %4; border: none; border-bottom: 1px solid %6;"
            " padding: 4px 6px; font-size: %10px; }"
            "#rbLibrary QWidget[rbRole=\"bar\"] { background: %7; border: 1px solid %3; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::BG_SURFACE(), ui::colors::BORDER_MED(), ui::colors::BG_RAISED(), ui::fonts::px(-1),
                 ui::fonts::px(3))
            .arg(ui::fonts::px(-2), ui::colors::BG_HOVER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::DANGER(),
                 ui::colors::ROW_ALT()));
    build_ui();

    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(250);
    connect(search_timer_, &QTimer::timeout, this, &ReportLibraryPage::reload_reports);
    undo_timer_ = new QTimer(this);
    undo_timer_->setSingleShot(true);
    undo_timer_->setInterval(8000);
    connect(undo_timer_, &QTimer::timeout, this, [this]() {
        undo_bar_->hide();
        if (undo_id_ > 0)
            emit report_deleted(undo_id_);
        undo_id_ = 0;
    });
    connect(&ReportLibrary::instance(), &ReportLibrary::library_changed, this, [this]() {
        dirty_ = true;
        if (isVisible())
            reload();
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        tpl_view_->viewport()->update();
        preview_->update();
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        const QSize card = RlpTemplateDelegate::card_size(tpl_view_->font());
        tpl_view_->setGridSize(card + QSize(4, 0));
        tpl_view_->setFixedHeight(card.height() + tpl_view_->horizontalScrollBar()->sizeHint().height() + 8);
    });
}

void ReportLibraryPage::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    // Header.
    auto* head = new QHBoxLayout();
    head->setSpacing(8);
    title_ = new QLabel(tr("REPORTS"), this);
    title_->setProperty("rbRole", QStringLiteral("title"));
    head->addWidget(title_);
    current_lbl_ = new QLabel(this);
    current_lbl_->setProperty("rbRole", QStringLiteral("hint"));
    head->addWidget(current_lbl_, 1);
    continue_btn_ = new QPushButton(tr("Continue editing"), this);
    continue_btn_->setProperty("rbRole", QStringLiteral("primary"));
    new_btn_ = new QPushButton(tr("New report"), this);
    import_btn_ = new QPushButton(tr("Import .fincept…"), this);
    for (QPushButton* b : {continue_btn_, new_btn_, import_btn_}) {
        b->setCursor(Qt::PointingHandCursor);
        head->addWidget(b);
    }
    root->addLayout(head);
    connect(continue_btn_, &QPushButton::clicked, this, &ReportLibraryPage::continue_requested);
    connect(new_btn_, &QPushButton::clicked, this, &ReportLibraryPage::new_requested);
    connect(import_btn_, &QPushButton::clicked, this, &ReportLibraryPage::import_requested);

    // Templates.
    auto* th = new QHBoxLayout();
    tpl_heading_ = new QLabel(tr("START FROM A TEMPLATE"), this);
    tpl_heading_->setProperty("rbRole", QStringLiteral("section"));
    th->addWidget(tpl_heading_);
    th->addStretch(1);
    tpl_filter_ = new QComboBox(this);
    th->addWidget(tpl_filter_);
    root->addLayout(th);
    tpl_model_ = new QStandardItemModel(this);
    tpl_view_ = new QListView(this);
    tpl_view_->setModel(tpl_model_);
    tpl_view_->setViewMode(QListView::IconMode);
    tpl_view_->setFlow(QListView::LeftToRight);
    tpl_view_->setWrapping(false);
    tpl_view_->setMovement(QListView::Static);
    tpl_view_->setUniformItemSizes(true);
    tpl_view_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tpl_view_->setMouseTracking(true);
    tpl_view_->setItemDelegate(new RlpTemplateDelegate(tpl_view_));
    tpl_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const QSize card = RlpTemplateDelegate::card_size(tpl_view_->font());
    tpl_view_->setGridSize(card + QSize(4, 0));
    tpl_view_->setFixedHeight(card.height() + 24);
    root->addWidget(tpl_view_);
    auto* tpl_row = new QHBoxLayout();
    tpl_desc_ = new QLabel(tr("Pick a template to see what it contains."), this);
    tpl_desc_->setWordWrap(true);
    tpl_row->addWidget(tpl_desc_, 1);
    tpl_param_row_ = new QWidget(this);
    auto* pr = new QHBoxLayout(tpl_param_row_);
    pr->setContentsMargins(0, 0, 0, 0);
    pr->setSpacing(6);
    tpl_param_lbl_ = new QLabel(tpl_param_row_);
    pr->addWidget(tpl_param_lbl_);
    tpl_symbol_ = new ui::SymbolSearchBox(tpl_param_row_);
    tpl_symbol_->set_compact(true);
    tpl_symbol_->set_clear_on_pick(false);
    tpl_symbol_->set_raw_text_allowed(true);
    tpl_symbol_->set_recent_key(QStringLiteral("report_builder"));
    tpl_symbol_->set_placeholder(tr("Ticker, name or ISIN"));
    pr->addWidget(tpl_symbol_);
    tpl_portfolio_ = new QComboBox(tpl_param_row_);
    pr->addWidget(tpl_portfolio_);
    tpl_row->addWidget(tpl_param_row_);
    tpl_create_btn_ = new QPushButton(tr("Create report"), this);
    tpl_create_btn_->setProperty("rbRole", QStringLiteral("primary"));
    tpl_create_btn_->setEnabled(false);
    tpl_row->addWidget(tpl_create_btn_);
    root->addLayout(tpl_row);
    tpl_param_row_->hide();
    connect(tpl_view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { on_template_selected(); });
    connect(tpl_view_, &QListView::doubleClicked, this, [this](const QModelIndex&) { create_from_template(); });
    connect(tpl_create_btn_, &QPushButton::clicked, this, &ReportLibraryPage::create_from_template);
    connect(tpl_filter_, &QComboBox::currentIndexChanged, this, [this](int) { reload_templates(); });
    connect(tpl_symbol_, &ui::SymbolSearchBox::instrument_picked, this, [this](const core::instrument::Instrument& i) {
        tpl_symbol_value_ = i.symbol.isEmpty() ? i.display : i.symbol;
        on_template_selected();
    });
    connect(tpl_symbol_, &ui::SymbolSearchBox::text_committed_raw, this, [this](const QString& t) {
        tpl_symbol_value_ = t.trimmed().toUpper();
        on_template_selected();
    });
    auto& ps = services::PortfolioService::instance();
    connect(&ps, &services::PortfolioService::portfolios_loaded, tpl_portfolio_,
            [this](const QVector<portfolio::Portfolio>& list) {
                const QString keep = tpl_portfolio_->currentData().toString();
                tpl_portfolio_->clear();
                for (const auto& p : list)
                    tpl_portfolio_->addItem(p.name, p.id);
                if (list.isEmpty())
                    tpl_portfolio_->addItem(tr("No portfolio yet"), QString());
                const int i = tpl_portfolio_->findData(keep);
                tpl_portfolio_->setCurrentIndex(i >= 0 ? i : 0);
            });

    // Reports.
    auto* rh = new QHBoxLayout();
    rep_heading_ = new QLabel(tr("YOUR REPORTS"), this);
    rep_heading_->setProperty("rbRole", QStringLiteral("section"));
    rh->addWidget(rep_heading_);
    rh->addStretch(1);
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Search titles and contents"));
    search_->setClearButtonEnabled(true);
    rh->addWidget(search_, 1);
    root->addLayout(rh);
    connect(search_, &QLineEdit::textChanged, this, [this]() { search_timer_->start(); });

    auto* split = new QSplitter(Qt::Horizontal, this);
    rep_stack_ = new QStackedWidget(split);
    rep_model_ = new RbLibraryModel(this);
    rep_view_ = new QTableView(rep_stack_);
    rep_view_->setModel(rep_model_);
    rep_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rep_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    rep_view_->setAlternatingRowColors(true);
    rep_view_->setShowGrid(false);
    rep_view_->verticalHeader()->setVisible(false);
    rep_view_->horizontalHeader()->setStretchLastSection(false);
    rep_view_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 4; ++c)
        rep_view_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    rep_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rep_stack_->addWidget(rep_view_);
    rep_empty_ = new ui::EmptyState(rep_stack_);
    rep_empty_->apply(ui::EmptyState::no_data_spec(
        tr("No saved reports yet"),
        tr("Reports you create, open from a file or receive from other tabs are kept here — and synced when "
           "Settings > Cloud Sync > Reports is on."),
        tr("NEW REPORT")));
    rep_stack_->addWidget(rep_empty_);
    connect(rep_empty_, &ui::EmptyState::primary_clicked, this, &ReportLibraryPage::new_requested);
    connect(rep_view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { on_report_selected(); });
    connect(rep_view_, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
        if (const qint64 id = selected_report_id())
            emit open_requested(id);
    });

    auto* side = new QWidget(split);
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(10, 0, 0, 0);
    sl->setSpacing(6);
    preview_ = new RbWireframe(side);
    preview_->setMinimumHeight(180);
    sl->addWidget(preview_, 1);
    rename_edit_ = new QLineEdit(side);
    rename_edit_->setPlaceholderText(tr("Title — press Enter to rename"));
    sl->addWidget(rename_edit_);
    preview_info_ = new QLabel(side);
    preview_info_->setWordWrap(true);
    preview_info_->setProperty("rbRole", QStringLiteral("hint"));
    sl->addWidget(preview_info_);
    auto* actions = new QHBoxLayout();
    open_btn_ = new QPushButton(tr("Open"), side);
    open_btn_->setProperty("rbRole", QStringLiteral("primary"));
    dup_btn_ = new QPushButton(tr("Duplicate"), side);
    export_btn_ = new QPushButton(tr("Export…"), side);
    delete_btn_ = new QPushButton(tr("Delete"), side);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the report
    delete_btn_->setProperty("rbRole", QStringLiteral("danger"));
    for (QPushButton* b : {open_btn_, dup_btn_, export_btn_, delete_btn_}) {
        b->setCursor(Qt::PointingHandCursor);
        actions->addWidget(b);
    }
    sl->addLayout(actions);
    split->addWidget(rep_stack_);
    split->addWidget(side);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);
    connect(open_btn_, &QPushButton::clicked, this, [this]() {
        if (const qint64 id = selected_report_id())
            emit open_requested(id);
    });
    connect(dup_btn_, &QPushButton::clicked, this, [this]() {
        if (const qint64 id = selected_report_id())
            ReportLibrary::instance().duplicate(id, this, {});
    });
    connect(export_btn_, &QPushButton::clicked, this, &ReportLibraryPage::export_selected);
    connect(delete_btn_, &QPushButton::clicked, this, &ReportLibraryPage::delete_selected);
    connect(rename_edit_, &QLineEdit::returnPressed, this, &ReportLibraryPage::rename_selected);

    // Undo strip for a delete.
    undo_bar_ = new QWidget(this);
    undo_bar_->setProperty("rbRole", QStringLiteral("bar"));
    undo_bar_->setAttribute(Qt::WA_StyledBackground);
    auto* ul = new QHBoxLayout(undo_bar_);
    ul->setContentsMargins(10, 4, 6, 4);
    undo_lbl_ = new QLabel(undo_bar_);
    ul->addWidget(undo_lbl_, 1);
    undo_btn_ = new QPushButton(tr("UNDO"), undo_bar_);
    undo_btn_->setProperty("rbRole", QStringLiteral("primary"));
    ul->addWidget(undo_btn_);
    root->addWidget(undo_bar_);
    undo_bar_->hide();
    connect(undo_btn_, &QPushButton::clicked, this, [this]() {
        undo_timer_->stop();
        undo_bar_->hide();
        if (undo_id_ > 0 && ReportLibrary::instance().undo_remove(undo_id_))
            emit report_restored(undo_id_);
        undo_id_ = 0;
    });

    on_report_selected();
}

void ReportLibraryPage::set_current(qint64 library_id, const QString& title, int blocks) {
    current_id_ = library_id;
    const bool has = blocks > 0 || library_id > 0;
    continue_btn_->setVisible(has);
    current_lbl_->setText(has ? tr("Open now: %1 · %n block(s)", nullptr, blocks).arg(title) : QString());
}

void ReportLibraryPage::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (dirty_)
        reload();
}

void ReportLibraryPage::reload() {
    dirty_ = false;
    reload_templates();
    reload_reports();
    services::PortfolioService::instance().load_portfolios();
}

void ReportLibraryPage::reload_reports() {
    const quint64 req = ++list_request_;
    const qint64 keep = selected_report_id();
    ReportLibrary::instance().list(search_->text(), 500, this,
                                   [this, req, keep](bool ok, const QVector<LibraryEntry>& entries, const QString& error) {
                                       if (req != list_request_)
                                           return; // a newer search answered first
                                       if (!ok) {
                                           rep_empty_->apply(ui::EmptyState::error_spec(error, tr("Could not read the library")));
                                           rep_stack_->setCurrentWidget(rep_empty_);
                                           return;
                                       }
                                       rep_model_->set_entries(entries);
                                       if (entries.isEmpty()) {
                                           rep_empty_->apply(search_->text().isEmpty()
                                                                 ? ui::EmptyState::no_data_spec(
                                                                       tr("No saved reports yet"),
                                                                       tr("Reports you create, open from a file or receive from other tabs are kept here — and synced when Settings > Cloud Sync > Reports is on."),
                                                                       tr("NEW REPORT"))
                                                                 : ui::EmptyState::no_data_spec(
                                                                       tr("Nothing matches “%1”").arg(search_->text()),
                                                                       tr("Search looks in titles and in the text of every report.")));
                                           rep_stack_->setCurrentWidget(rep_empty_);
                                       } else {
                                           rep_stack_->setCurrentWidget(rep_view_);
                                           int row = rep_model_->row_of(keep);
                                           if (row < 0)
                                               row = 0;
                                           rep_view_->selectRow(row);
                                       }
                                       on_report_selected();
                                   });
}

void ReportLibraryPage::reload_templates() {
    const QString filter = tpl_filter_->currentData().toString();
    auto fill = [this, filter](const QVector<TemplateEntry>& user) {
        QVector<TemplateEntry> all = ReportLibrary::builtin_templates();
        // Order: live-data templates, the user's own, then the built-ins.
        QVector<TemplateEntry> ordered;
        for (const auto& t : all)
            if (t.live)
                ordered << t;
        ordered += user;
        for (const auto& t : all)
            if (!t.live)
                ordered << t;
        templates_ = ordered;
        // Filter combo: rebuild only when the categories change.
        QStringList cats;
        for (const auto& t : ordered)
            if (!cats.contains(t.category))
                cats << t.category;
        if (tpl_filter_->count() != cats.size() + 1) {
            const QSignalBlocker block(tpl_filter_);
            tpl_filter_->clear();
            tpl_filter_->addItem(tr("All templates"), QString());
            for (const QString& c : cats)
                tpl_filter_->addItem(c, c);
            const int i = tpl_filter_->findData(filter);
            tpl_filter_->setCurrentIndex(i >= 0 ? i : 0);
        }
        const QString f = tpl_filter_->currentData().toString();
        const QString keep = tpl_view_->currentIndex().data(kRlpId).toString();
        tpl_model_->clear();
        for (const auto& t : ordered) {
            if (!f.isEmpty() && t.category != f)
                continue;
            auto* it = new QStandardItem(t.name);
            it->setData(t.id, kRlpId);
            it->setData(t.description, kRlpDesc);
            it->setData(t.kinds, kRlpKinds);
            it->setData(t.theme, kRlpTheme);
            it->setData(t.needs, kRlpNeeds);
            it->setData(t.live, kRlpLive);
            it->setData(t.category, kRlpCategory);
            it->setData(t.user, kRlpUser);
            it->setToolTip(t.description);
            tpl_model_->appendRow(it);
            if (t.id == keep)
                tpl_view_->setCurrentIndex(it->index());
        }
        on_template_selected();
    };
    ReportLibrary::instance().list_user_templates(this, [fill](bool, const QVector<TemplateEntry>& user, const QString&) {
        fill(user);
    });
}

void ReportLibraryPage::on_template_selected() {
    const QModelIndex idx = tpl_view_->currentIndex();
    if (!idx.isValid()) {
        tpl_desc_->setText(tr("Pick a template to see what it contains."));
        tpl_param_row_->hide();
        tpl_create_btn_->setEnabled(false);
        return;
    }
    const QString needs = idx.data(kRlpNeeds).toString();
    QString desc = QStringLiteral("<b>%1</b> — %2").arg(idx.data(Qt::DisplayRole).toString().toHtmlEscaped(),
                                                             idx.data(kRlpDesc).toString().toHtmlEscaped());
    if (idx.data(kRlpLive).toBool())
        desc += QStringLiteral("<br>") + tr("Fills itself from live data when created; Refresh Data updates it later.").toHtmlEscaped();
    tpl_desc_->setText(desc);
    tpl_param_row_->setVisible(!needs.isEmpty());
    tpl_symbol_->setVisible(needs == QLatin1String("symbol"));
    tpl_portfolio_->setVisible(needs == QLatin1String("portfolio"));
    tpl_param_lbl_->setText(needs == QLatin1String("symbol") ? tr("Symbol") : tr("Portfolio"));
    tpl_create_btn_->setEnabled(needs != QLatin1String("symbol") || !tpl_symbol_value_.isEmpty());
    tpl_create_btn_->setToolTip(tpl_create_btn_->isEnabled() ? QString() : tr("Pick a symbol first"));
}

void ReportLibraryPage::create_from_template() {
    const QModelIndex idx = tpl_view_->currentIndex();
    if (!idx.isValid())
        return;
    const QString needs = idx.data(kRlpNeeds).toString();
    QVariantMap params;
    if (needs == QLatin1String("symbol")) {
        if (tpl_symbol_value_.isEmpty()) {
            tpl_symbol_->focus_and_select();
            return;
        }
        params.insert(QStringLiteral("symbol"), tpl_symbol_value_);
    } else if (needs == QLatin1String("portfolio")) {
        params.insert(QStringLiteral("portfolio_id"), tpl_portfolio_->currentData().toString());
    }
    emit template_requested(idx.data(kRlpId).toString(), params);
}

qint64 ReportLibraryPage::selected_report_id() const {
    const LibraryEntry* e = rep_model_->entry(rep_view_->currentIndex().row());
    return e ? e->id : 0;
}

services::LibraryEntry ReportLibraryPage::selected_entry() const {
    const LibraryEntry* e = rep_model_->entry(rep_view_->currentIndex().row());
    return e ? *e : LibraryEntry{};
}

void ReportLibraryPage::on_report_selected() {
    const LibraryEntry e = selected_entry();
    const bool has = e.id > 0;
    for (QPushButton* b : {open_btn_, dup_btn_, export_btn_, delete_btn_})
        b->setEnabled(has);
    rename_edit_->setEnabled(has);
    if (!has) {
        preview_->show_kinds({}, QString());
        rename_edit_->clear();
        preview_info_->setText(rep_model_->rowCount() > 0 ? tr("Select a report to preview it.") : QString());
        return;
    }
    preview_->show_kinds(e.kinds, e.theme);
    rename_edit_->setText(e.title);
    QStringList info;
    info << tr("%n block(s)", nullptr, e.blocks);
    if (!e.outline.isEmpty())
        info << tr("Sections: %1").arg(e.outline.join(QStringLiteral(", ")));
    info << tr("Updated %1 · created %2").arg(rlp_when(e.updated_at), rlp_when(e.created_at));
    if (e.unseen > 0)
        info << tr("%n block(s) added from other tabs, not opened yet", nullptr, e.unseen);
    if (e.bound)
        info << tr("Has live data blocks (Refresh Data updates them)");
    if (e.id == current_id_)
        info << tr("This is the report open in the editor.");
    preview_info_->setText(info.join(QLatin1Char('\n')));
}

void ReportLibraryPage::rename_selected() {
    const qint64 id = selected_report_id();
    const QString title = rename_edit_->text().trimmed();
    if (id <= 0 || title.isEmpty())
        return;
    ReportLibrary::instance().rename(id, title, this, {});
}

void ReportLibraryPage::delete_selected() {
    const LibraryEntry e = selected_entry();
    if (e.id <= 0)
        return;
    // Hidden at once, deleted after the undo period (or on quit).
    if (undo_id_ > 0)
        emit report_deleted(undo_id_);
    undo_id_ = e.id;
    ReportLibrary::instance().remove_later(e.id, undo_timer_->interval());
    undo_lbl_->setText(tr("“%1” deleted.").arg(e.title));
    undo_bar_->show();
    undo_timer_->start();
}

void ReportLibraryPage::export_selected() {
    const LibraryEntry e = selected_entry();
    if (e.id <= 0)
        return;
    QString path = QFileDialog::getSaveFileName(this, tr("Export report"), e.title, tr("Fincept Report (*.fincept)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".fincept"), Qt::CaseInsensitive))
        path += QLatin1String(".fincept");
    QPointer<ReportLibraryPage> self(this);
    ReportLibrary::instance().load(e.id, this, [self, path](bool ok, const rep::ReportDocument& doc, const QString&,
                                                            const QString& error) {
        if (!self)
            return;
        if (!ok) {
            self->preview_info_->setText(error);
            return;
        }
        const QString json = doc.to_json();
        (void)QtConcurrent::run([self, path, json]() {
            QSaveFile f(path);
            const bool written = f.open(QIODevice::WriteOnly | QIODevice::Text) && f.write(json.toUtf8()) >= 0 && f.commit();
            QMetaObject::invokeMethod(
                qApp,
                [self, path, written]() {
                    if (self)
                        self->preview_info_->setText(written ? ReportLibraryPage::tr("Exported to %1").arg(path)
                                                             : ReportLibraryPage::tr("Could not write %1").arg(path));
                },
                Qt::QueuedConnection);
        });
    });
}

void ReportLibraryPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void ReportLibraryPage::retranslateUi() {
    title_->setText(tr("REPORTS"));
    continue_btn_->setText(tr("Continue editing"));
    new_btn_->setText(tr("New report"));
    import_btn_->setText(tr("Import .fincept…"));
    tpl_heading_->setText(tr("START FROM A TEMPLATE"));
    tpl_create_btn_->setText(tr("Create report"));
    rep_heading_->setText(tr("YOUR REPORTS"));
    search_->setPlaceholderText(tr("Search titles and contents"));
    open_btn_->setText(tr("Open"));
    dup_btn_->setText(tr("Duplicate"));
    export_btn_->setText(tr("Export…"));
    delete_btn_->setText(tr("Delete"));
    undo_btn_->setText(tr("UNDO"));
    dirty_ = true;
    if (isVisible())
        reload();
}

} // namespace fincept::screens
