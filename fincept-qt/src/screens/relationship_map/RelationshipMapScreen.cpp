// src/screens/relationship_map/RelationshipMapScreen.cpp
// Construction, loading (per-instance service requests, streamed sections, retries),
// history, state, deep links and symbol-group linking. The detail pane, context menu and
// exports are in RelationshipMapScreen_Detail.cpp.
#include "screens/relationship_map/RelationshipMapScreen.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/events/EventBus.h"
#include "core/report/ReportBlocks.h"
#include "core/instrument/VenueSymbols.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "screens/relationship_map/RelationshipGraphScene.h"
#include "screens/relationship_map/RelationshipLinkedViews.h"
#include "screens/relationship_map/RelationshipMapFormat.h"
#include "services/instruments/InstrumentResolver.h"
#include "services/relationship_map/RelationshipMapParse.h"
#include "services/relationship_map/RelationshipMapService.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/report/AddToReportButton.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::relmap;
namespace fmt = fincept::ui::formatting;

namespace {

/// The opening company when there is no last ticker, no linked symbol and no company in
/// the user's watchlists (owner decision: a liquid, well-covered default).
const char* const kRmscrDefaultTicker = "AAPL";
/// A map older than this is refreshed quietly when shown again.
constexpr int kRmscrMaxAgeSec = 24 * 3600;
/// relationship_map.py has its own 45 s budget; the pane gives up after this.
constexpr int kRmscrTimeoutSec = 150;
constexpr int kRmscrHistoryCap = 30;

int rmscr_text_width(int chars, int offset, int weight = QFont::Normal) {
    return QFontMetrics(fonts::font(offset, weight)).horizontalAdvance(QString(chars, QLatin1Char('M')));
}

/// Yahoo-form key of a linked symbol: the publisher's `yahoo`, else the shared venue table,
/// else the symbol as given. Table lookups only.
QString rmscr_ref_key(const SymbolRef& ref) {
    if (!ref.yahoo.trimmed().isEmpty())
        return ref.yahoo.trimmed().toUpper();
    const QString sym = ref.symbol.trimmed();
    if (!sym.isEmpty() && !ref.exchange.trimmed().isEmpty()) {
        const QString yahoo = core::instrument::to_yahoo(sym, ref.exchange.trimmed());
        if (!yahoo.isEmpty())
            return yahoo.toUpper();
    }
    return sym.toUpper();
}

/// The map is about a company or a fund: indices, FX, crypto and futures have no holders,
/// officers or peers (they get a designed "no relationships" page when opened anyway).
bool rmscr_is_mappable(const QString& yahoo_symbol) {
    const auto parts = core::instrument::from_yahoo(yahoo_symbol);
    return parts.valid() && (parts.cls == core::instrument::AssetClass::Equity ||
                             parts.cls == core::instrument::AssetClass::Etf ||
                             parts.cls == core::instrument::AssetClass::Fund);
}

/// Legend over the graph: what the shapes, the edge thickness and the arrows mean. Painted
/// from the live theme.
class RmscrLegend : public QWidget {
  public:
    explicit RmscrLegend(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }
    void set_metric_text(const QString& t) {
        metric_ = t;
        refit();
        update();
    }
    void refit() {
        const QFontMetrics fm(fonts::font(-4));
        const int rows = 6;
        const int w = std::max(fm.horizontalAdvance(QString(28, QLatin1Char('M'))), 160);
        setFixedSize(w, fm.height() * (rows + 2) + 14);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QColor bg(colors::BG_SURFACE());
        bg.setAlphaF(0.92f);
        p.setPen(QPen(QColor(colors::BORDER_DIM()), 1.0));
        p.setBrush(bg);
        p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
        const QFont f = fonts::font(-4);
        const QFontMetrics fm(f);
        const int lh = fm.height() + 1;
        const int x = 8;
        int y = 6;
        p.setFont(fonts::font(-4, QFont::Bold));
        p.setPen(QColor(colors::TEXT_TERTIARY()));
        p.drawText(QRect(x, y, width() - 2 * x, lh), Qt::AlignLeft | Qt::AlignVCenter,
                   RelationshipMapScreen::tr("LEGEND"));
        y += lh + 2;
        p.setFont(f);
        struct Entry {
            int shape; // 0 square 1 diamond 2 circle 3 triangle 4 hollow square 5 ring
            QString text;
        };
        const Entry rows[] = {{0, RelationshipMapScreen::tr("company")},  {1, RelationshipMapScreen::tr("institution")},
                              {4, RelationshipMapScreen::tr("fund")},     {2, RelationshipMapScreen::tr("person")},
                              {3, RelationshipMapScreen::tr("ETF")},      {5, RelationshipMapScreen::tr("index")}};
        const int ms = std::max(6, fm.height() / 2);
        const int col_w = (width() - 2 * x) / 2;
        for (int i = 0; i < 6; ++i) {
            const int cx = x + (i % 2) * col_w;
            const int cy = y + (i / 2) * lh;
            const QRectF m(cx, cy + (lh - ms) / 2.0, ms, ms);
            const QColor c(colors::TEXT_SECONDARY());
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            switch (rows[i].shape) {
                case 0:
                    p.drawRect(m);
                    break;
                case 1:
                    p.drawPolygon(QPolygonF{QPointF(m.center().x(), m.top()), QPointF(m.right(), m.center().y()),
                                            QPointF(m.center().x(), m.bottom()), QPointF(m.left(), m.center().y())});
                    break;
                case 2:
                    p.drawEllipse(m);
                    break;
                case 3:
                    p.drawPolygon(QPolygonF{QPointF(m.center().x(), m.top()), m.bottomRight(), m.bottomLeft()});
                    break;
                case 4:
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(c, 1.4));
                    p.drawRect(m.adjusted(0.7, 0.7, -0.7, -0.7));
                    break;
                default:
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(c, 1.4));
                    p.drawEllipse(m.adjusted(0.7, 0.7, -0.7, -0.7));
                    break;
            }
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            p.drawText(QRect(cx + ms + 6, cy, col_w - ms - 8, lh), Qt::AlignLeft | Qt::AlignVCenter, rows[i].text);
        }
        y += 3 * lh + 4;
        // Edge thickness: thin -> thick = the chosen metric; arrow = direction of ownership.
        const QColor ink(colors::TEXT_TERTIARY());
        for (int i = 0; i < 3; ++i) {
            p.setPen(QPen(ink, 0.8 + 1.7 * i));
            const int lx = x + i * 18;
            p.drawLine(QPointF(lx, y + lh / 2.0), QPointF(lx + 14, y + lh / 2.0));
        }
        p.setPen(QColor(colors::TEXT_SECONDARY()));
        p.drawText(QRect(x + 58, y, width() - x - 62, lh), Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(RelationshipMapScreen::tr("width = %1").arg(metric_), Qt::ElideRight, width() - x - 62));
        y += lh;
        p.setPen(QPen(ink, 1.2));
        p.drawLine(QPointF(x, y + lh / 2.0), QPointF(x + 40, y + lh / 2.0));
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawPolygon(QPolygonF{QPointF(x + 46, y + lh / 2.0), QPointF(x + 38, y + lh / 2.0 - 4),
                                QPointF(x + 38, y + lh / 2.0 + 4)});
        p.setPen(QColor(colors::TEXT_SECONDARY()));
        p.drawText(QRect(x + 58, y, width() - x - 62, lh), Qt::AlignLeft | Qt::AlignVCenter,
                   RelationshipMapScreen::tr("owns / holds"));
    }

  private:
    QString metric_;
};

QString rmscr_metric_text(RelEdgeMetric m) {
    switch (m) {
        case RelEdgeMetric::Percent:
            return RelationshipMapScreen::tr("% held");
        case RelEdgeMetric::Value:
            return RelationshipMapScreen::tr("position value");
        case RelEdgeMetric::Change:
            return RelationshipMapScreen::tr("change");
        case RelEdgeMetric::Correlation:
            return RelationshipMapScreen::tr("correlation");
    }
    return {};
}

QString rmscr_chip_text(RelChip c) {
    switch (c) {
        case RelChip::Owners:
            return RelationshipMapScreen::tr("OWNERS");
        case RelChip::Insiders:
            return RelationshipMapScreen::tr("INSIDERS");
        case RelChip::Management:
            return RelationshipMapScreen::tr("MANAGEMENT");
        case RelChip::Peers:
            return RelationshipMapScreen::tr("PEERS");
        case RelChip::HoldingsFunds:
            return RelationshipMapScreen::tr("HOLDINGS / FUNDS");
        case RelChip::Subsidiaries:
            return RelationshipMapScreen::tr("SUBSIDIARIES");
        case RelChip::Correlated:
            return RelationshipMapScreen::tr("CORRELATED");
        case RelChip::SupplyChain:
            return RelationshipMapScreen::tr("SUPPLY CHAIN");
        case RelChip::Count:
            break;
    }
    return {};
}

} // namespace

// ── Construction ────────────────────────────────────────────────────────────

RelationshipMapScreen::RelationshipMapScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    auto& svc = services::RelationshipMapService::instance();
    connect(&svc, &services::RelationshipMapService::request_planned, this, &RelationshipMapScreen::on_planned);
    connect(&svc, &services::RelationshipMapService::request_section, this, &RelationshipMapScreen::on_section);
    connect(&svc, &services::RelationshipMapService::request_finished, this, &RelationshipMapScreen::on_finished);
    connect(&svc, &services::RelationshipMapService::request_failed, this, &RelationshipMapScreen::on_failed);

    // Painted items read the theme at paint time; a new font size needs new card sizes —
    // measured once the appearance has settled (never inside theme_changed).
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        if (legend_)
            static_cast<RmscrLegend*>(legend_)->refit();
        if (has_data_)
            scene_->relayout();
    });
    // Number / currency format changed: every formatted text again, from memory.
    fmt::on_format_changed(this, [this]() {
        if (!has_data_)
            return;
        scene_->refresh_text();
        update_ribbon();
        linked_->set_data(current_data_, model_);
        show_detail(selected_id_);
    });
}

RelationshipMapScreen::~RelationshipMapScreen() {
    cancel_requests();
}

void RelationshipMapScreen::build_ui() {
    options_.top_n = 6; // compact by default; TOP N widens it
    setObjectName(QStringLiteral("RelMapRoot"));
    setStyleSheet(QStringLiteral("QWidget#RelMapRoot { background: %1; }").arg(colors::BG_BASE()));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_strip());
    build_command_bar();
    root->addWidget(bar_);
    build_ribbon();
    root->addWidget(ribbon_);

    // Graph (left) + detail pane (right) over the linked tables.
    vsplit_ = new QSplitter(Qt::Vertical, this);
    vsplit_->setObjectName(QStringLiteral("relmapVSplit"));
    vsplit_->setChildrenCollapsible(false);
    vsplit_->setHandleWidth(3);
    hsplit_ = new QSplitter(Qt::Horizontal, vsplit_);
    hsplit_->setChildrenCollapsible(false);
    hsplit_->setHandleWidth(3);

    graph_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, hsplit_);
    graph_stack_ = new QStackedWidget(graph_pane_->content_parent());
    // Every page fills the pane: the stack never asks for the tallest page's height.
    graph_stack_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    scene_ = new RelationshipGraphScene(this);
    view_ = new RelationshipGraphView(scene_, graph_stack_);
    graph_stack_->addWidget(view_);
    matrix_ = new ui::MatrixHeatmap(graph_stack_);
    matrix_->set_diverging_scale(-1.0, 1.0, 0.0);
    matrix_->set_show_values(true);
    matrix_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    graph_stack_->addWidget(matrix_);
    no_rel_page_ = new ui::EmptyState(graph_stack_);
    no_rel_page_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    graph_stack_->addWidget(no_rel_page_);
    graph_pane_->set_content(graph_stack_);
    graph_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Chart);
    graph_pane_->set_loading_text(tr("Mapping owners, insiders, management, peers and filings…"));
    graph_pane_->set_persist_limit(0); // the service remembers the payload ("relmap:v2:<ticker>")
    graph_pane_->set_max_age(kRmscrMaxAgeSec);
    graph_pane_->set_timeout(kRmscrTimeoutSec);
    graph_pane_->set_requirements({ui::Requirement::python()});
    graph_pane_->set_start([this](quint64 id) { start_analysis(id); });
    graph_pane_->set_cancel([this](quint64 id) {
        if (pane_request_ != id)
            return;
        pane_request_ = 0;
        if (svc_request_ != 0)
            services::RelationshipMapService::instance().cancel(std::exchange(svc_request_, 0));
        load_label_->clear();
    });
    graph_pane_->set_render([this](const QJsonValue&, const QDateTime&, bool) { render_result(); });
    // The partial-result notice's RETRY: only the sections that did not load.
    connect(graph_pane_, &ui::AutoLoadPane::notice_action, this, [this]() { retry_sections({}); });

    legend_ = new RmscrLegend(view_);
    static_cast<RmscrLegend*>(legend_)->set_metric_text(rmscr_metric_text(metric_));
    legend_->move(10, 10);
    legend_->hide();

    connect(no_rel_page_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (!loaded_ticker_.isEmpty())
            EventBus::instance().publish("nav.open_symbol",
                                         {{"screen_id", QStringLiteral("markets")}, {"symbol", loaded_ticker_}});
    });
    connect(matrix_, &ui::MatrixHeatmap::cell_clicked, this, [this](int row, int col) {
        const QStringList& syms = current_data_.correlation.symbols;
        const QString sym = syms.value(col == 0 ? row : col);
        for (const auto& n : model_.nodes)
            if (!sym.isEmpty() && n.symbol.compare(sym, Qt::CaseInsensitive) == 0 && n.kind != RelNodeKind::Center) {
                select_entity(n.id, false);
                return;
            }
    });

    detail_scroll_ = qobject_cast<QScrollArea*>(build_detail_pane());
    hsplit_->addWidget(graph_pane_);
    hsplit_->addWidget(detail_scroll_);
    hsplit_->setStretchFactor(0, 4);
    hsplit_->setStretchFactor(1, 1);

    linked_ = new RelationshipLinkedViews(vsplit_);
    vsplit_->addWidget(hsplit_);
    vsplit_->addWidget(linked_);
    vsplit_->setStretchFactor(0, 7);
    vsplit_->setStretchFactor(1, 3);
    root->addWidget(vsplit_, 1);

    // Status bar.
    using SB = ui::ScreenStatusBar;
    status_ = new ui::ScreenStatusBar(this);
    status_->add_item(QStringLiteral("counts"), SB::Segment::Left, tr("READY"), SB::Tone::Normal, 90);
    status_->add_item(QStringLiteral("sections"), SB::Segment::Left, QString(), SB::Tone::Dim, 80);
    status_->add_item(QStringLiteral("sources"), SB::Segment::Centre, QString(), SB::Tone::Dim, 40);
    status_->add_item(QStringLiteral("ccy"), SB::Segment::Right, QString(), SB::Tone::Dim, 60);
    status_->add_item(QStringLiteral("timing"), SB::Segment::Right, QString(), SB::Tone::Dim, 20);
    status_->add_item(QStringLiteral("as_of"), SB::Segment::Right, QString(), SB::Tone::Normal, 70);
    graph_pane_->set_status_label(status_->item(QStringLiteral("as_of")));
    root->addWidget(status_);

    // Graph <-> screen.
    connect(scene_, &RelationshipGraphScene::node_clicked, this, [this](const QString& id) { select_entity(id, false); });
    connect(scene_, &RelationshipGraphScene::node_double_clicked, this, &RelationshipMapScreen::activate_entity);
    connect(view_, &RelationshipGraphView::activate_requested, this, &RelationshipMapScreen::activate_entity);
    connect(scene_, &RelationshipGraphScene::node_context_menu, this, &RelationshipMapScreen::show_context_menu);
    connect(scene_, &RelationshipGraphScene::background_clicked, this, [this]() {
        selected_id_.clear();
        linked_->set_selected(QString());
        show_detail(model_.center_id);
    });
    connect(scene_, &RelationshipGraphScene::cluster_clicked, this, [this](int group) {
        options_.expanded_groups.insert(group);
        rebuild(true);
    });
    connect(scene_, &RelationshipGraphScene::retry_clicked, this,
            [this](const QString& section) { retry_sections({section}); });
    connect(scene_, &RelationshipGraphScene::layout_applied, this, [this](qint64 layout_us, qint64 build_us) {
        if (refit_pending_) {
            refit_pending_ = false;
            view_->fit_to_content();
        }
        status_->set_text(QStringLiteral("timing"), tr("layout %1 ms · scene %2 ms")
                                                         .arg(fmt::number(layout_us / 1000.0, 1))
                                                         .arg(fmt::number(build_us / 1000.0, 1)));
    });
    connect(view_, &RelationshipGraphView::zoom_changed, this, [this](double z) {
        bar_->set_action_tooltip(QStringLiteral("fit"), tr("Fit the map to the view (Home) · zoom %1 %").arg(qRound(z * 100)));
    });

    // Tables <-> screen.
    connect(linked_, &RelationshipLinkedViews::node_selected, this, [this](const QString& id) { select_entity(id, true); });
    connect(linked_, &RelationshipLinkedViews::node_activated, this, &RelationshipMapScreen::activate_entity);
    connect(linked_, &RelationshipLinkedViews::retry_requested, this, &RelationshipMapScreen::retry_sections);
    connect(linked_, &RelationshipLinkedViews::view_changed, this,
            [this](const QString&) { ScreenStateManager::instance().notify_changed(this); });

    // Layout keys 1..6 while the graph has the focus (never the shell's reserved keys).
    const RelLayoutMode modes[] = {RelLayoutMode::Tiered, RelLayoutMode::Hierarchy, RelLayoutMode::Radial,
                                   RelLayoutMode::Force,  RelLayoutMode::Sankey,    RelLayoutMode::Matrix};
    for (int i = 0; i < 6; ++i) {
        auto* sc = new QShortcut(QKeySequence(Qt::Key_1 + i), view_);
        sc->setContext(Qt::WidgetShortcut);
        const RelLayoutMode m = modes[i];
        connect(sc, &QShortcut::activated, this, [this, m]() { set_layout_mode(m); });
    }
    // Back / forward through the companies walked (narrowest shortcut: this panel only).
    auto* back_sc = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Left), this);
    back_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(back_sc, &QShortcut::activated, back_btn_, &QPushButton::click);
    auto* fwd_sc = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Right), this);
    fwd_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(fwd_sc, &QShortcut::activated, fwd_btn_, &QPushButton::click);
    auto* find_sc = new QShortcut(QKeySequence(Qt::Key_Slash), view_);
    find_sc->setContext(Qt::WidgetShortcut);
    connect(find_sc, &QShortcut::activated, this, [this]() {
        find_edit_->setFocus();
        find_edit_->selectAll();
    });
    update_history_ui();
}

QWidget* RelationshipMapScreen::build_strip() {
    strip_ = new QWidget(this);
    strip_->setObjectName(QStringLiteral("relmapStrip"));
    strip_->setAttribute(Qt::WA_StyledBackground, true);
    strip_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    strip_->setStyleSheet(
        QStringLiteral("QWidget#relmapStrip { background: %1; border-bottom: 1px solid %2; }"
                       "QLabel#relmapTitle { color: %3; font-weight: 700; letter-spacing: 1px; font-size:%4px; }"
                       "QLabel#relmapCrumbs { color: %5; font-size:%6px; }"
                       "QLabel#relmapLoad { color: %5; font-size:%7px; }"
                       "QPushButton#relmapNav { background: transparent; color: %3; border: 1px solid %2;"
                       " padding: 2px 8px; font-size:%6px; }"
                       "QPushButton#relmapNav:disabled { color: %5; }"
                       "QPushButton#relmapNav:hover:enabled { border-color: %8; color: %8; }"
                       "QPushButton#relmapAnalyze { background: %9; color: %8; border: 1px solid %10;"
                       " padding: 2px 14px; font-weight: 700; font-size:%6px; }"
                       "QPushButton#relmapAnalyze:hover { background: %8; color: %11; }")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM(), colors::TEXT_PRIMARY(), fonts::px(-2),
                 colors::TEXT_TERTIARY(), fonts::px(-3), fonts::px(-4), colors::AMBER(),
                 colors::alpha(colors::AMBER, 0.12), colors::AMBER_DIM(), colors::TEXT_ON_ACCENT()));
    auto* hl = new QHBoxLayout(strip_);
    const int pad = std::max(8, QFontMetrics(fonts::font(-2)).height() / 2);
    hl->setContentsMargins(pad + 2, pad / 2, pad + 2, pad / 2);
    hl->setSpacing(pad);

    auto* tick = new QWidget(strip_);
    tick->setFixedSize(3, QFontMetrics(fonts::font(-2, QFont::Bold)).capHeight() + 6);
    tick->setStyleSheet(QStringLiteral("background: %1;").arg(colors::AMBER()));
    hl->addWidget(tick);
    title_ = new QLabel(tr("RELATIONSHIP MAP"), strip_);
    title_->setObjectName(QStringLiteral("relmapTitle"));
    hl->addWidget(title_);

    symbol_box_ = new ui::SymbolSearchBox(strip_);
    symbol_box_->set_compact(true);
    symbol_box_->set_clear_on_pick(false); // selector: the box shows the company on the map
    symbol_box_->set_recent_key(QStringLiteral("relationship_map"));
    symbol_box_->set_placeholder(tr("Company or fund: ticker, name or ISIN"));
    {
        services::ResolveOptions opts;
        opts.ctx.allowed_classes = {core::instrument::AssetClass::Equity, core::instrument::AssetClass::Etf,
                                    core::instrument::AssetClass::Fund};
        symbol_box_->set_options(opts);
    }
    symbol_box_->setAccessibleName(tr("Company to map"));
    symbol_box_->setMinimumWidth(std::max(220, rmscr_text_width(22, -2)));
    symbol_box_->setMaximumWidth(std::max(360, rmscr_text_width(40, -2)));
    connect(symbol_box_, &ui::SymbolSearchBox::instrument_picked, this,
            [this](const core::instrument::Instrument& inst) {
                if (!inst.symbol.isEmpty())
                    analyze(inst.symbol);
            });
    connect(symbol_box_, &ui::SymbolSearchBox::unresolved, this,
            [this](const QStringList&, const QString& message) { load_label_->setText(message); });
    hl->addWidget(symbol_box_, 1);

    analyze_btn_ = new QPushButton(tr("ANALYZE"), strip_);
    analyze_btn_->setObjectName(QStringLiteral("relmapAnalyze"));
    analyze_btn_->setCursor(Qt::PointingHandCursor);
    // Agents: starts the relationship-map download (free sources, but a heavy multi-source run).
    analyze_btn_->setProperty(core::agent::kAgentRiskProperty, "spend");
    connect(analyze_btn_, &QPushButton::clicked, this, &RelationshipMapScreen::on_analyze_clicked);
    hl->addWidget(analyze_btn_);

    back_btn_ = new QPushButton(QStringLiteral("◀"), strip_);
    back_btn_->setObjectName(QStringLiteral("relmapNav"));
    back_btn_->setToolTip(tr("Back to the previous company (Alt+Left)"));
    back_btn_->setAccessibleName(tr("Back"));
    connect(back_btn_, &QPushButton::clicked, this, [this]() {
        if (history_pos_ > 0) {
            --history_pos_;
            analyze(history_.value(history_pos_), false);
        }
    });
    hl->addWidget(back_btn_);
    fwd_btn_ = new QPushButton(QStringLiteral("▶"), strip_);
    fwd_btn_->setObjectName(QStringLiteral("relmapNav"));
    fwd_btn_->setToolTip(tr("Forward (Alt+Right)"));
    fwd_btn_->setAccessibleName(tr("Forward"));
    connect(fwd_btn_, &QPushButton::clicked, this, [this]() {
        if (history_pos_ + 1 < history_.size()) {
            ++history_pos_;
            analyze(history_.value(history_pos_), false);
        }
    });
    hl->addWidget(fwd_btn_);

    crumbs_ = new QLabel(strip_);
    crumbs_->setObjectName(QStringLiteral("relmapCrumbs"));
    crumbs_->setTextFormat(Qt::RichText);
    crumbs_->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    connect(crumbs_, &QLabel::linkActivated, this, [this](const QString& link) {
        bool ok = false;
        const int i = link.toInt(&ok);
        if (ok && i >= 0 && i < history_.size() && i != history_pos_) {
            history_pos_ = i;
            analyze(history_.value(i), false);
        }
    });
    hl->addWidget(crumbs_, 2);

    load_label_ = new QLabel(strip_);
    load_label_->setObjectName(QStringLiteral("relmapLoad"));
    hl->addWidget(load_label_);

    report_btn_ = new ui::AddToReportButton(QStringLiteral("Relationship Map"), strip_);
    report_btn_->set_show_result(false);
    report_btn_->set_payload_provider([this]() {
        report::ReportPayload p;
        if (!has_data_)
            return p;
        const CompanyInfo& co = current_data_.company;
        p.title = tr("Relationship map — %1").arg(co.name.isEmpty() ? co.ticker : co.name);
        p.source = QStringLiteral("Relationship Map");
        p.as_of = as_of_.isValid() ? as_of_.toLocalTime() : QDateTime::currentDateTime();
        p.kpi_title = tr("Ownership and market");
        for (int i = 0; i < ribbon_->count(); ++i) {
            const StatTile* t = ribbon_->tile(i);
            if (t && !t->value_text().isEmpty() && t->value_text() != fmt::placeholder())
                p.kpis.append({t->label(), t->value_text(), QString(), QString()});
        }
        report::PayloadTable holders;
        holders.title = tr("Top institutional holders");
        holders.columns = {tr("Holder"), tr("% held"), tr("Value"), tr("Change"), tr("Reported")};
        for (const auto& h : current_data_.institutional_holders)
            holders.rows.append({h.name, fmt::pct(h.percentage, 2, false), amount_text(h.value, co.currency),
                                 fmt::pct(h.change_percent, 2), h.date_reported});
        if (!holders.rows.isEmpty())
            p.tables.append(holders);
        report::PayloadTable peers;
        peers.title = tr("Peers");
        peers.columns = {tr("Ticker"), tr("Name"), tr("Market cap"), tr("P/E"), tr("ρ 1y")};
        for (const auto& pc : current_data_.peers) {
            const double rho = current_data_.correlation.value(co.ticker, pc.ticker);
            peers.rows.append({pc.ticker, pc.name, amount_text(pc.market_cap, pc.currency),
                               pc.pe_ratio > 0 ? fmt::ratio(pc.pe_ratio, 1) : fmt::placeholder(),
                               std::isfinite(rho) ? fmt::number(rho, 2) : fmt::placeholder()});
        }
        if (!peers.rows.isEmpty())
            p.tables.append(peers);
        const QImage img = render_graph_image(1.0);
        if (!img.isNull()) {
            report::PayloadChart chart;
            chart.title = tr("Relationship map");
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            chart.image = bytes;
            chart.caption = tr("Edge width = %1").arg(rmscr_metric_text(metric_));
            p.charts.append(chart);
        }
        p.inputs = {{tr("Layout"), layout_key(layout_mode_)}, {tr("Edge metric"), rmscr_metric_text(metric_)},
                    {tr("Depth"), QString::number(options_.depth)}};
        p.notes = tr("Sources: Yahoo Finance (holders, insiders, peers, prices), SEC EDGAR Exhibit 21 "
                     "(subsidiaries), correlations computed from daily returns.");
        return p;
    });
    hl->addWidget(report_btn_);
    return strip_;
}

void RelationshipMapScreen::build_command_bar() {
    using Seg = ui::ScreenCommandBar::Segment;
    bar_ = new ui::ScreenCommandBar(this);
    bar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    bar_->add_segmented(QStringLiteral("layout"),
                        {Seg{QStringLiteral("tiered"), tr("TIERED"), QStringLiteral("1"),
                             tr("Ranked columns around the company card (1)")},
                         Seg{QStringLiteral("hierarchy"), tr("HIERARCHY"), QStringLiteral("2"),
                             tr("Owners above, the market below (2)")},
                         Seg{QStringLiteral("radial"), tr("RADIAL"), QStringLiteral("3"),
                             tr("Groups in sectors, bubbles sized by value (3)")},
                         Seg{QStringLiteral("force"), tr("FORCE"), QStringLiteral("4"), tr("Force-directed (4)")},
                         Seg{QStringLiteral("sankey"), tr("SANKEY"), QStringLiteral("5"),
                             tr("Ownership flows into the company (5)")},
                         Seg{QStringLiteral("matrix"), tr("MATRIX"), QStringLiteral("6"),
                             tr("Correlation matrix of the peer set (6)")}},
                        90);
    bar_->set_segment(QStringLiteral("layout"), layout_key(layout_mode_));
    bar_->add_separator(85);
    bar_->add_segmented(QStringLiteral("metric"),
                        {Seg{QStringLiteral("pct"), tr("% HELD"), QString(), tr("Edge width = percent held")},
                         Seg{QStringLiteral("value"), tr("VALUE"), QString(), tr("Edge width = position value")},
                         Seg{QStringLiteral("change"), tr("CHANGE"), QString(),
                             tr("Edge width = change of the position / return")},
                         Seg{QStringLiteral("corr"), tr("ρ CORR"), QString(),
                             tr("Edge width = correlation of daily returns")}},
                        70);
    bar_->set_segment(QStringLiteral("metric"), metric_key(metric_));
    bar_->add_segmented(QStringLiteral("depth"),
                        {Seg{QStringLiteral("1"), tr("1 HOP"), QString(), tr("Relationships of the company")},
                         Seg{QStringLiteral("2"), tr("2 HOPS"), QString(),
                             tr("Also the links among them: peer correlations, fund families")}},
                        60);
    bar_->set_segment(QStringLiteral("depth"), QStringLiteral("1"));
    bar_->add_separator(55);
    bar_->add_action(QStringLiteral("fit"), tr("FIT"), QStringLiteral("Home"), tr("Fit the map to the view (Home)"),
                     ui::ScreenCommandBar::Role::Normal, 50);
    bar_->add_toggle(QStringLiteral("legend"), tr("LEGEND"), true, QString(), tr("Shapes and edge widths"), 30);
    bar_->add_toggle(QStringLiteral("tables"), tr("TABLES"), true, QString(),
                     tr("The tables and charts under the map"), 35);
    auto* export_menu = new QMenu(this);
    export_menu->setStyleSheet(ui::ScreenCommandBar::menu_css());
    QAction* a_png = export_menu->addAction(tr("Graph as PNG…"));
    QAction* a_nodes = export_menu->addAction(tr("Nodes as CSV…"));
    QAction* a_edges = export_menu->addAction(tr("Edges as CSV…"));
    QAction* a_table = export_menu->addAction(tr("Current table as CSV…"));
    for (QAction* a : {a_png, a_nodes, a_edges, a_table})
        a->setProperty(core::agent::kAgentRiskProperty, "file"); // writes a file the user names
    connect(a_png, &QAction::triggered, this, &RelationshipMapScreen::export_png);
    connect(a_nodes, &QAction::triggered, this, [this]() { export_csv(false); });
    connect(a_edges, &QAction::triggered, this, [this]() { export_csv(true); });
    connect(a_table, &QAction::triggered, this, &RelationshipMapScreen::export_table_csv);
    bar_->add_menu_action(QStringLiteral("export"), tr("EXPORT"), export_menu, QString(),
                          tr("PNG of the graph, CSV of nodes, edges or the table"), 45);
    QPushButton* refresh = bar_->add_action(QStringLiteral("refresh"), tr("REFRESH"), QString(),
                                            tr("Fetch the map again (bypasses the remembered copy)"),
                                            ui::ScreenCommandBar::Role::Normal, 80);
    refresh->setProperty(core::agent::kAgentRiskProperty, "spend");

    // Row 2: type chips with counts, TOP N, find in graph.
    bar_->set_chips_label(tr("SHOW"));
    for (int i = 0; i < kRelChipCount; ++i) {
        const auto c = static_cast<RelChip>(i);
        bar_->add_chip(chip_key(c), rmscr_chip_text(c), tr("Show or hide this relationship type"), 50 - i);
        bar_->set_chip_checked(chip_key(c), true);
    }
    QWidget* right = bar_->chips_right_area();
    auto* top_label = new QLabel(tr("TOP"), right);
    top_label->setStyleSheet(QStringLiteral("color: %1; font-size:%2px;").arg(colors::TEXT_TERTIARY(), fonts::px(-4)));
    top_n_spin_ = new QSpinBox(right);
    top_n_spin_->setRange(3, 25);
    top_n_spin_->setValue(options_.top_n);
    top_n_spin_->setToolTip(tr("Entities shown per group; the rest fold into \"+ n more\""));
    top_n_spin_->setAccessibleName(tr("Top N per group"));
    find_edit_ = new QLineEdit(right);
    find_edit_->setPlaceholderText(tr("Find in graph  /"));
    find_edit_->setAccessibleName(tr("Find in graph"));
    find_edit_->setClearButtonEnabled(true);
    find_edit_->setMinimumWidth(rmscr_text_width(12, -3));
    if (auto* lay = qobject_cast<QHBoxLayout*>(right->layout())) {
        lay->addWidget(top_label);
        lay->addWidget(top_n_spin_);
        lay->addWidget(find_edit_);
    }
    connect(top_n_spin_, &QSpinBox::valueChanged, this, [this](int n) {
        options_.top_n = n;
        options_.expanded_groups.clear();
        if (has_data_)
            rebuild(true);
        ScreenStateManager::instance().notify_changed(this);
    });
    connect(find_edit_, &QLineEdit::returnPressed, this, [this]() {
        const QString id = scene_->find_and_pulse(find_edit_->text());
        if (id.isEmpty()) {
            status_->show_notice(tr("Nothing on the map matches \"%1\"").arg(find_edit_->text().trimmed()));
            return;
        }
        select_entity(id, false);
        view_->setFocus();
    });

    connect(bar_, &ui::ScreenCommandBar::segment_selected, this, [this](const QString& id, const QString& key) {
        if (id == QLatin1String("layout")) {
            RelLayoutMode m;
            if (layout_from_key(key, &m))
                set_layout_mode(m);
        } else if (id == QLatin1String("metric")) {
            const RelEdgeMetric ms[] = {RelEdgeMetric::Percent, RelEdgeMetric::Value, RelEdgeMetric::Change,
                                        RelEdgeMetric::Correlation};
            for (RelEdgeMetric m : ms)
                if (metric_key(m) == key)
                    metric_ = m;
            static_cast<RmscrLegend*>(legend_)->set_metric_text(rmscr_metric_text(metric_));
            if (has_data_)
                rebuild(true);
            ScreenStateManager::instance().notify_changed(this);
        } else if (id == QLatin1String("depth")) {
            options_.depth = key == QLatin1String("2") ? 2 : 1;
            if (has_data_)
                rebuild(true);
            ScreenStateManager::instance().notify_changed(this);
        }
    });
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("fit")) {
            view_->fit_to_content();
        } else if (id == QLatin1String("refresh")) {
            if (!loaded_ticker_.isEmpty() || !requested_ticker_.isEmpty())
                analyze(loaded_ticker_.isEmpty() ? requested_ticker_ : loaded_ticker_, false, true);
        }
    });
    connect(bar_, &ui::ScreenCommandBar::action_toggled, this, [this](const QString& id, bool on) {
        if (id == QLatin1String("legend"))
            legend_->setVisible(on && has_data_ && layout_mode_ != RelLayoutMode::Matrix);
        else if (id == QLatin1String("tables"))
            linked_->setVisible(on);
    });
    connect(bar_, &ui::ScreenCommandBar::chip_toggled, this, [this](const QString& id, bool on) {
        for (int i = 0; i < kRelChipCount; ++i)
            if (chip_key(static_cast<RelChip>(i)) == id)
                options_.chips[static_cast<size_t>(i)] = on;
        if (has_data_)
            rebuild(true);
        ScreenStateManager::instance().notify_changed(this);
    });
}

void RelationshipMapScreen::build_ribbon() {
    ribbon_ = new ui::KpiStrip(this);
    ribbon_->setObjectName(QStringLiteral("relmapRibbon"));
    ribbon_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ribbon_->set_spacing(1);
    ribbon_->set_max_columns(10);
    ribbon_->set_min_tile_width(rmscr_text_width(9, -3));
    const QPair<const char*, QString> tiles[] = {
        {"mcap", tr("MKT CAP")},        {"inst", tr("INST % HELD")},  {"insider", tr("INSIDER % HELD")},
        {"holders", tr("INST HOLDERS")}, {"top", tr("TOP HOLDER")},    {"net_ins", tr("NET INSIDER 6M")},
        {"float", tr("FLOAT")},          {"short", tr("SHORT % FLOAT")}, {"peers", tr("PEERS")},
        {"beta", tr("β · ρ SECTOR")}};
    for (const auto& t : tiles) {
        StatTile* tile = ribbon_->add_tile(QString::fromLatin1(t.first), t.second);
        tile->set_compact(true);
        tile->set_value(fmt::placeholder());
    }
}

// ── Loading ─────────────────────────────────────────────────────────────────

void RelationshipMapScreen::analyze(const QString& ticker, bool push, bool force) {
    const QString t = ticker.trimmed().toUpper();
    if (t.isEmpty())
        return;
    if (resolve_request_ != 0)
        services::InstrumentResolver::instance().cancel(std::exchange(resolve_request_, 0));
    if (push && (history_pos_ < 0 || history_.value(history_pos_) != t)) {
        while (history_.size() > history_pos_ + 1)
            history_.removeLast();
        history_ << t;
        while (history_.size() > kRmscrHistoryCap)
            history_.removeFirst();
        history_pos_ = static_cast<int>(history_.size()) - 1;
    }
    requested_ticker_ = t;
    force_next_ = force;
    if (symbol_box_->text().trimmed().compare(t, Qt::CaseInsensitive) != 0)
        symbol_box_->set_text(t);
    graph_pane_->set_loading_text(tr("Mapping %1: owners, insiders, management, peers and filings…").arg(t));
    update_history_ui();
    graph_pane_->run(); // a user / link run: bypasses the start gate, supersedes a run in flight
    ScreenStateManager::instance().notify_changed(this);
}

void RelationshipMapScreen::on_analyze_clicked() {
    const auto picked = symbol_box_->current_instrument();
    if (picked.is_valid() && !picked.symbol.isEmpty()) {
        analyze(picked.symbol);
        return;
    }
    const QString text = symbol_box_->text().trimmed();
    if (text.isEmpty()) {
        symbol_box_->focus_and_select();
        return;
    }
    if (text.compare(loaded_ticker_, Qt::CaseInsensitive) == 0 ||
        text.compare(requested_ticker_, Qt::CaseInsensitive) == 0) {
        analyze(text, false);
        return;
    }
    // Typed text: resolved first (local index at once, backend / Yahoo after), never used raw.
    auto& resolver = services::InstrumentResolver::instance();
    if (resolve_request_ != 0)
        resolver.cancel(resolve_request_);
    load_label_->setText(tr("Looking up %1…").arg(text));
    resolve_request_ = resolver.resolve(text, symbol_box_->options(), this, [this, text](const services::ResolveResult& r) {
        resolve_request_ = 0;
        if (r.exact && !r.matches.isEmpty() && services::is_quotable(r.matches.first())) {
            symbol_box_->set_instrument(r.matches.first());
            analyze(r.matches.first().symbol);
            return;
        }
        load_label_->setText(r.matches.isEmpty()
                                 ? tr("No company matches \"%1\" — try a ticker, the name or an ISIN.").arg(text)
                                 : tr("\"%1\" matches more than one listing — pick one from the list.").arg(text));
        load_label_->setToolTip(r.note);
        symbol_box_->focus_and_select();
    });
}

void RelationshipMapScreen::start_analysis(quint64 pane_request) {
    pane_request_ = pane_request;
    if (requested_ticker_.isEmpty()) {
        start_with_default_ticker(pane_request);
        return;
    }
    // A quiet refresh of an old map (automatic run over a result) or REFRESH skips the
    // remembered copy; everything else may be answered from it.
    const bool force = force_next_ || (graph_pane_->has_result() && graph_pane_->is_automatic_run(pane_request));
    force_next_ = false;
    auto& svc = services::RelationshipMapService::instance();
    if (svc_request_ != 0)
        svc.cancel(svc_request_);
    streaming_shown_ = false;
    services::RelMapRequestOptions opts;
    opts.force = force;
    svc_request_ = svc.request(requested_ticker_, opts);
    load_label_->setText(tr("Loading %1…").arg(requested_ticker_));
    if (user_holdings_.isEmpty() && user_watchlist_.isEmpty())
        load_user_symbols();
}

void RelationshipMapScreen::start_with_default_ticker(quint64 pane_request) {
    // 1. The linked group's company / fund.
    if (link_group_ != SymbolGroup::None) {
        const QString key = rmscr_ref_key(SymbolContext::instance().group_symbol(link_group_));
        if (!key.isEmpty() && rmscr_is_mappable(key)) {
            requested_ticker_ = key;
            symbol_box_->set_text(key);
            start_analysis(pane_request);
            return;
        }
    }
    // 2. The first company in the user's watchlists (local SQLite, off the GUI thread); 3. a default.
    graph_pane_->set_loading_text(tr("Opening on a company from your watchlists…"));
    QPointer<RelationshipMapScreen> self = this;
    (void)QtConcurrent::run([self, pane_request]() {
        QString found;
        auto& repo = fincept::WatchlistRepository::instance();
        if (auto lists = repo.list_all(); lists.is_ok()) {
            for (const auto& wl : lists.value()) {
                auto stocks = repo.get_stocks(wl.id);
                if (stocks.is_err())
                    continue;
                for (const auto& st : stocks.value()) {
                    QString sym = st.symbol.trimmed();
                    if (sym.isEmpty())
                        continue;
                    if (!st.exchange.trimmed().isEmpty()) {
                        const QString yahoo = core::instrument::to_yahoo(sym, st.exchange.trimmed());
                        if (!yahoo.isEmpty())
                            sym = yahoo;
                    }
                    if (rmscr_is_mappable(sym)) {
                        found = sym.toUpper();
                        break;
                    }
                }
                if (!found.isEmpty())
                    break;
            }
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, pane_request, found]() {
                if (!self || !self->graph_pane_->is_current(pane_request) || !self->requested_ticker_.isEmpty())
                    return; // cancelled (screen hidden) or superseded meanwhile
                const QString ticker = found.isEmpty() ? QString::fromLatin1(kRmscrDefaultTicker) : found;
                self->requested_ticker_ = ticker;
                self->history_ = {ticker};
                self->history_pos_ = 0;
                self->update_history_ui();
                self->symbol_box_->set_text(ticker);
                self->graph_pane_->set_loading_text(
                    tr("Mapping %1: owners, insiders, management, peers and filings…").arg(ticker));
                self->start_analysis(pane_request);
            },
            Qt::QueuedConnection);
    });
}

void RelationshipMapScreen::load_user_symbols() {
    // "You hold this" markers and the watchlist menu: portfolios + watchlists, read off the
    // GUI thread (each repository hands a worker thread its own connection).
    QPointer<RelationshipMapScreen> self = this;
    (void)QtConcurrent::run([self]() {
        QSet<QString> held, watched;
        QVector<QPair<QString, QString>> lists_out;
        auto& wrepo = fincept::WatchlistRepository::instance();
        if (auto lists = wrepo.list_all(); lists.is_ok()) {
            for (const auto& wl : lists.value()) {
                lists_out.append({wl.id, wl.name});
                auto stocks = wrepo.get_stocks(wl.id);
                if (stocks.is_err())
                    continue;
                for (const auto& st : stocks.value()) {
                    QString sym = st.symbol.trimmed();
                    if (!st.exchange.trimmed().isEmpty()) {
                        const QString yahoo = core::instrument::to_yahoo(sym, st.exchange.trimmed());
                        if (!yahoo.isEmpty())
                            sym = yahoo;
                    }
                    if (!sym.isEmpty())
                        watched.insert(sym.toUpper());
                }
            }
        }
        auto& prepo = fincept::PortfolioRepository::instance();
        if (auto ports = prepo.list_portfolios(); ports.is_ok()) {
            for (const auto& pf : ports.value()) {
                auto assets = prepo.get_assets(pf.id);
                if (assets.is_err())
                    continue;
                for (const auto& a : assets.value())
                    if (a.quantity != 0 && !a.symbol.trimmed().isEmpty())
                        held.insert(a.symbol.trimmed().toUpper());
            }
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, held, watched, lists_out]() {
                if (!self)
                    return;
                self->user_holdings_ = held;
                self->user_watchlist_ = watched;
                self->watchlists_ = lists_out;
                if (self->has_data_)
                    self->rebuild(true);
            },
            Qt::QueuedConnection);
    });
}

void RelationshipMapScreen::on_planned(quint64 id, const QString& ticker, const QStringList& sections) {
    if (id != svc_request_ && id != retry_request_)
        return;
    load_label_->setText(tr("Loading %1 · 0 of %2 sections").arg(ticker).arg(sections.size()));
}

void RelationshipMapScreen::on_section(quint64 id, const RelationshipData& snapshot, const QString& section_id) {
    if (id == retry_request_) {
        load_label_->setText(tr("Retrying · %1 loaded").arg(section_id));
        return;
    }
    if (id != svc_request_)
        return;
    int done = 0, total = 0;
    for (auto it = snapshot.sections.begin(); it != snapshot.sections.end(); ++it) {
        if (it.value().state == RelSectionState::Unknown)
            continue;
        ++total;
        if (it.value().state != RelSectionState::Loading)
            ++done;
    }
    load_label_->setText(tr("Loading %1 · %2 of %3 sections").arg(snapshot.company.ticker).arg(done).arg(total));
    // The map appears as soon as the company profile is in; every later section joins it.
    if (snapshot.company.current_price <= 0 && snapshot.company.market_cap <= 0)
        return;
    current_data_ = snapshot;
    if (!streaming_shown_ && graph_pane_->is_current(pane_request_)) {
        streaming_shown_ = true;
        graph_pane_->deliver_partial(pane_request_, QJsonObject{{"ticker", snapshot.company.ticker}});
    } else if (streaming_shown_ && has_data_ && loaded_ticker_ == snapshot.company.ticker) {
        rebuild(true);
    }
}

void RelationshipMapScreen::on_finished(quint64 id, const RelationshipData& data, const QDateTime& as_of,
                                        bool from_cache) {
    if (id == retry_request_) {
        retry_request_ = 0;
        load_label_->clear();
        if (data.company.ticker.compare(loaded_ticker_, Qt::CaseInsensitive) != 0)
            return;
        current_data_ = data;
        as_of_ = as_of;
        rebuild(true);
        status_->show_notice(data.partial ? tr("Some sections still did not load") : tr("All sections loaded"));
        return;
    }
    if (id != svc_request_)
        return;
    svc_request_ = 0;
    current_data_ = data;
    as_of_ = as_of;
    load_label_->setText(data.elapsed_s > 0 && !from_cache
                             ? tr("Loaded in %1 s").arg(fmt::number(data.elapsed_s, 1))
                             : (from_cache ? tr("Remembered map") : QString()));
    if (!graph_pane_->is_current(pane_request_))
        return;
    const QString ticker = data.company.ticker.toUpper();
    const bool from_group = group_driven_ticker_ == ticker;
    group_driven_ticker_.clear();
    if (!from_group && link_group_ != SymbolGroup::None)
        SymbolContext::instance().set_group_symbol(link_group_, SymbolRef::equity(data.company.ticker), this);
    graph_pane_->deliver(std::exchange(pane_request_, 0), QJsonObject{{"ticker", data.company.ticker}}, as_of);
    // A remembered map older than a day: shown at once, then refreshed quietly.
    if (from_cache && as_of.isValid() && as_of.secsTo(QDateTime::currentDateTimeUtc()) > kRmscrMaxAgeSec) {
        QPointer<ui::AutoLoadPane> pane = graph_pane_;
        QMetaObject::invokeMethod(
            this,
            [pane]() {
                if (pane)
                    pane->invalidate();
            },
            Qt::QueuedConnection);
    }
}

void RelationshipMapScreen::on_failed(quint64 id, const QString& error) {
    if (id == retry_request_) {
        retry_request_ = 0;
        load_label_->clear();
        status_->show_notice(tr("Retry failed — %1").arg(error.left(160)), QString(), {}, 9000,
                             ui::ScreenStatusBar::Tone::Warn);
        return;
    }
    if (id != svc_request_)
        return;
    svc_request_ = 0;
    const quint64 pane_id = std::exchange(pane_request_, 0);
    const QString ticker = requested_ticker_;
    const QString message = error.simplified();
    load_label_->setText(tr("Could not map %1").arg(ticker));
    load_label_->setToolTip(message);
    if (!graph_pane_->is_current(pane_id))
        return;
    if (graph_pane_->has_result()) {
        graph_pane_->fail(pane_id, message); // the previous map stays, marked "refresh failed"
        return;
    }
    // Nothing on screen: the error page. A stored symbol that was renamed says what it is now
    // (local index only, never a silent migration) so the user can pick the successor.
    services::InstrumentResolver::instance().resolve_stored(
        ticker, QString(), this, [this, pane_id, ticker, message](const services::ResolveResult& r) {
            QString text = message.isEmpty() ? tr("The relationship data could not be loaded.") : message;
            if (r.matches.size() >= 2 && !r.note.isEmpty())
                text += QStringLiteral("\n") + r.note;
            graph_pane_->fail(pane_id, text, tr("Could not map %1").arg(ticker));
        });
}

void RelationshipMapScreen::retry_sections(const QStringList& sections) {
    if (loaded_ticker_.isEmpty())
        return;
    QStringList ids = sections;
    ids.removeAll(QString());
    if (ids.isEmpty())
        ids = failed_sections(current_data_);
    if (ids.isEmpty()) {
        analyze(loaded_ticker_, false, true);
        return;
    }
    auto& svc = services::RelationshipMapService::instance();
    if (retry_request_ != 0)
        svc.cancel(retry_request_);
    services::RelMapRequestOptions opts;
    opts.sections = ids;
    retry_request_ = svc.request(loaded_ticker_, opts);
    // The groups of those sections show "loading…" until the retry lands.
    for (const QString& id : std::as_const(ids)) {
        RelSectionStatus& s = current_data_.sections[id];
        s.state = RelSectionState::Loading;
    }
    load_label_->setText(tr("Retrying %1…").arg(ids.join(QStringLiteral(", "))));
    rebuild(true);
}

void RelationshipMapScreen::cancel_requests() {
    auto& svc = services::RelationshipMapService::instance();
    if (svc_request_ != 0)
        svc.cancel(std::exchange(svc_request_, 0));
    if (retry_request_ != 0)
        svc.cancel(std::exchange(retry_request_, 0));
}

// ── Rendering ───────────────────────────────────────────────────────────────

void RelationshipMapScreen::render_result() {
    if (current_data_.company.ticker.isEmpty())
        return;
    const bool new_company = loaded_ticker_.compare(current_data_.company.ticker, Qt::CaseInsensitive) != 0;
    has_data_ = true;
    loaded_ticker_ = current_data_.company.ticker;
    if (new_company) {
        options_.expanded_groups.clear();
        selected_id_.clear();
        refit_pending_ = true;
    }
    if (symbol_box_->text().trimmed().compare(loaded_ticker_, Qt::CaseInsensitive) != 0 &&
        !symbol_box_->current_instrument().is_valid())
        symbol_box_->set_text(loaded_ticker_);
    rebuild(!new_company);
    legend_->setVisible(bar_->action(QStringLiteral("legend")) && bar_->action(QStringLiteral("legend"))->isChecked() &&
                        layout_mode_ != RelLayoutMode::Matrix && !current_data_.has_no_relationships());
}

void RelationshipMapScreen::rebuild(bool animate) {
    if (current_data_.company.ticker.isEmpty())
        return;
    options_.user_holdings = user_holdings_;
    options_.watchlist = user_watchlist_;
    model_ = build_graph_model(current_data_, options_);
    const RelLayoutMode scene_mode = layout_mode_ == RelLayoutMode::Matrix ? RelLayoutMode::Tiered : layout_mode_;
    scene_->set_graph(current_data_, model_, scene_mode, metric_, animate);
    if (selected_id_.isEmpty() || !model_.index.contains(selected_id_))
        selected_id_ = model_.center_id;
    scene_->select_node(selected_id_ == model_.center_id ? QString() : selected_id_);
    linked_->set_data(current_data_, model_);
    if (selected_id_ != model_.center_id)
        linked_->set_selected(selected_id_);
    // Correlation matrix (MATRIX layout).
    const CorrelationSet& c = current_data_.correlation;
    if (c.present)
        matrix_->set_matrix(c.symbols, c.symbols, c.matrix);
    else
        matrix_->clear();
    update_ribbon();
    update_chips();
    update_status();
    update_partial_notice();
    show_mode_page();
    show_detail(selected_id_);
}

void RelationshipMapScreen::show_mode_page() {
    if (has_data_ && current_data_.has_no_relationships()) {
        const QString t = current_data_.company.ticker;
        auto spec = ui::EmptyState::no_data_spec(
            tr("%1 has no corporate relationships").arg(t),
            tr("Owners, insiders, management, peers and filings exist for companies and funds. %1 is a %2 — "
               "its price and history are in Markets.")
                .arg(t, current_data_.company.quote_type.toLower()),
            tr("OPEN IN MARKETS"));
        spec.glyph = QStringLiteral("○");
        no_rel_page_->apply(spec);
        graph_stack_->setCurrentWidget(no_rel_page_);
        legend_->hide();
        return;
    }
    if (layout_mode_ == RelLayoutMode::Matrix && current_data_.correlation.present) {
        graph_stack_->setCurrentWidget(matrix_);
        legend_->hide();
        return;
    }
    if (layout_mode_ == RelLayoutMode::Matrix) {
        auto spec = ui::EmptyState::no_data_spec(
            tr("No correlation matrix for %1").arg(current_data_.company.ticker),
            current_data_.section(QString::fromLatin1(section::kCorrelation)).state == RelSectionState::Loading
                ? tr("The price history of the peer set is still loading.")
                : tr("The peer set's price history did not load — use RETRY on the notice, or another layout."));
        no_rel_page_->apply(spec);
        graph_stack_->setCurrentWidget(no_rel_page_);
        legend_->hide();
        return;
    }
    graph_stack_->setCurrentWidget(view_);
    legend_->setVisible(has_data_ && bar_->action(QStringLiteral("legend")) &&
                        bar_->action(QStringLiteral("legend"))->isChecked());
}

void RelationshipMapScreen::set_layout_mode(RelLayoutMode m, bool notify) {
    if (m == layout_mode_ && graph_stack_->currentWidget() != no_rel_page_) {
        bar_->set_segment(QStringLiteral("layout"), layout_key(m));
        return;
    }
    layout_mode_ = m;
    bar_->set_segment(QStringLiteral("layout"), layout_key(m));
    refit_pending_ = true;
    if (has_data_)
        rebuild(false);
    else
        show_mode_page();
    if (notify)
        ScreenStateManager::instance().notify_changed(this);
}

void RelationshipMapScreen::update_ribbon() {
    const RelationshipData& d = current_data_;
    const CompanyInfo& co = d.company;
    using Tone = StatTile::Tone;
    auto tile = [this](const char* key) { return ribbon_->tile(QString::fromLatin1(key)); };
    auto loading = [&d](const char* sec) {
        return d.sections.value(QString::fromLatin1(sec)).state == RelSectionState::Loading;
    };
    auto set = [&](const char* key, const QString& value, const QString& sub = QString(), Tone tone = Tone::Neutral,
                   const char* sec = nullptr, const QString& label = QString()) {
        StatTile* t = tile(key);
        if (!t)
            return;
        if (!label.isEmpty())
            t->set_label(label);
        if (sec && loading(sec) && value.isEmpty()) {
            t->set_state(StatTile::State::Loading);
            return;
        }
        t->set_state(StatTile::State::Normal);
        t->set_value(value.isEmpty() ? fmt::placeholder() : value);
        t->set_sub_label(sub);
        t->set_tone(tone);
    };
    const bool fund = d.is_fund();
    if (fund) {
        set("mcap", amount_text(d.fund.total_assets, co.currency), d.fund.family, Tone::Neutral, section::kFund, tr("AUM"));
        const double top = d.fund.holdings.isEmpty() ? 0 : d.fund.holdings.first().weight;
        double top10 = 0;
        for (int i = 0; i < d.fund.holdings.size() && i < 10; ++i)
            top10 += d.fund.holdings[i].weight;
        set("inst", top10 > 0 ? fmt::fraction_pct(top10, 1, false) : QString(), tr("top 10 holdings"), Tone::Neutral,
            section::kFund, tr("TOP-10 WEIGHT"));
        set("insider", d.fund.sector_weights.isEmpty() ? QString() : d.fund.sector_weights.first().first,
            d.fund.sector_weights.isEmpty() ? QString() : fmt::fraction_pct(d.fund.sector_weights.first().second, 1, false),
            Tone::Neutral, section::kFund, tr("TOP SECTOR"));
        set("holders", d.fund.holdings.isEmpty() ? QString() : QString::number(d.fund.holdings.size()), tr("listed"),
            Tone::Neutral, section::kFund, tr("HOLDINGS"));
        set("top", top > 0 ? fmt::fraction_pct(top, 2, false) : QString(),
            d.fund.holdings.isEmpty() ? QString() : d.fund.holdings.first().symbol, Tone::Neutral, section::kFund,
            tr("TOP HOLDING"));
        set("net_ins", d.fund.category, d.fund.legal_type, Tone::Neutral, nullptr, tr("CATEGORY"));
        set("float", co.dividend_yield > 0 ? fmt::pct(co.dividend_yield, 2, false) : QString(), QString(),
            Tone::Neutral, nullptr, tr("YIELD"));
        set("short", d.technicals.week52_change_pct != 0 ? fmt::fraction_pct(d.technicals.week52_change_pct, 1) : QString(),
            QString(),
            d.technicals.week52_change_pct > 0 ? Tone::Positive : (d.technicals.week52_change_pct < 0 ? Tone::Negative : Tone::Neutral),
            nullptr, tr("52W CHANGE"));
    } else {
        set("mcap", co.market_cap > 0 ? amount_text(co.market_cap, co.currency) : QString(), co.exchange, Tone::Neutral,
            nullptr, tr("MKT CAP"));
        const MajorHolders& mh = d.major_holders;
        set("inst", mh.present && mh.institutions_pct > 0 ? fmt::fraction_pct(mh.institutions_pct, 1, false) : QString(),
            mh.institutions_float_pct > 0 ? tr("%1 of float").arg(fmt::fraction_pct(mh.institutions_float_pct, 1, false))
                                          : QString(),
            Tone::Neutral, section::kHolders, tr("INST % HELD"));
        set("insider", mh.present && mh.insiders_pct > 0 ? fmt::fraction_pct(mh.insiders_pct, 2, false) : QString(),
            QString(), Tone::Neutral, section::kHolders, tr("INSIDER % HELD"));
        set("holders", mh.institutions_count > 0 ? fmt::number(mh.institutions_count, 0) : QString(), tr("institutions"),
            Tone::Neutral, section::kHolders, tr("INST HOLDERS"));
        QString top, top_sub;
        if (!d.institutional_holders.isEmpty()) {
            const auto best = std::max_element(d.institutional_holders.begin(), d.institutional_holders.end(),
                                               [](const auto& a, const auto& b) { return a.percentage < b.percentage; });
            top = fmt::pct(best->percentage, 2, false);
            top_sub = holder_display_name(best->name);
        }
        set("top", top, top_sub, Tone::Neutral, section::kHolders, tr("TOP HOLDER"));
        const RelInsiderSummary& s = d.insider_summary;
        if (s.present && !s.currency.isEmpty() && (s.buy_value > 0 || s.sell_value > 0)) {
            // Open-market trades only (Yahoo's 6-month "purchases" also count awards).
            const double net = s.buy_value - s.sell_value;
            const QString v = (net > 0 ? QStringLiteral("▲") : (net < 0 ? QStringLiteral("▼") : QString())) +
                              amount_text(std::abs(net), s.currency);
            set("net_ins", v, tr("open market · %1 buys · %2 sells").arg(s.purchases_count).arg(s.sales_count),
                net > 0 ? Tone::Positive : (net < 0 ? Tone::Negative : Tone::Neutral), section::kInsiders,
                tr("NET INSIDER 6M"));
        } else if (s.present && (s.purchases_count > 0 || s.sales_count > 0 || s.net_shares != 0)) {
            const QString v = (s.net_shares > 0 ? QStringLiteral("▲") : (s.net_shares < 0 ? QStringLiteral("▼") : QString())) +
                              fmt::compact(std::abs(s.net_shares));
            set("net_ins", v, tr("shares incl. awards · %1 buys · %2 sells").arg(s.purchases_count).arg(s.sales_count),
                s.net_shares > 0 ? Tone::Positive : (s.net_shares < 0 ? Tone::Negative : Tone::Neutral),
                section::kInsiders, tr("NET INSIDER 6M"));
        } else {
            set("net_ins", QString(), tr("shares"), Tone::Neutral, section::kInsiders, tr("NET INSIDER 6M"));
        }
        set("float", co.float_shares > 0 ? fmt::compact(co.float_shares) : QString(), tr("shares"), Tone::Neutral,
            nullptr, tr("FLOAT"));
        const double sp = d.short_interest.short_pct_float;
        set("short", sp > 0 ? fmt::fraction_pct(sp, 2, false) : QString(),
            d.short_interest.short_ratio > 0 ? tr("%1 days to cover").arg(fmt::number(d.short_interest.short_ratio, 1))
                                             : QString(),
            sp > 0.10 ? Tone::Warning : Tone::Neutral, nullptr, tr("SHORT % FLOAT"));
    }
    set("peers", d.peers.isEmpty() ? (fund ? QString() : QStringLiteral("0")) : QString::number(d.peers.size()),
        d.peer_source.startsWith(QLatin1String("screen")) ? tr("industry screen")
                                                          : (d.peer_source == QLatin1String("generic_fallback")
                                                                 ? tr("generic — not comparable")
                                                                 : QString()),
        Tone::Neutral, section::kPeers, tr("PEERS"));
    // Beta, and the correlation with the sector ETF (else the index).
    QString rho_txt, rho_sub;
    for (const QString& sym : d.correlation.symbols) {
        const QString role = d.correlation.roles.value(sym);
        if (role != QLatin1String("sector_etf") && role != QLatin1String("index"))
            continue;
        const double r = d.correlation.value(co.ticker, sym);
        if (!std::isfinite(r))
            continue;
        rho_txt = fmt::number(r, 2);
        rho_sub = sym;
        if (role == QLatin1String("sector_etf"))
            break;
    }
    QString beta = d.technicals.beta > 0 ? fmt::number(d.technicals.beta, 2) : QString();
    QString v = beta;
    if (!rho_txt.isEmpty())
        v = (beta.isEmpty() ? fmt::placeholder() : beta) + QStringLiteral(" · ρ ") + rho_txt;
    set("beta", v, rho_sub.isEmpty() ? QString() : tr("ρ vs %1, 1y daily").arg(rho_sub), Tone::Neutral,
        section::kCorrelation, tr("β · ρ SECTOR"));
}

void RelationshipMapScreen::update_chips() {
    const bool fund = current_data_.is_fund();
    for (int i = 0; i < kRelChipCount; ++i) {
        const auto c = static_cast<RelChip>(i);
        const int n = chip_count(current_data_, c);
        QString text = rmscr_chip_text(c);
        if (c == RelChip::HoldingsFunds)
            text = fund ? tr("HOLDINGS") : tr("FUNDS");
        if (c == RelChip::Owners && fund)
            text = tr("ISSUER");
        if (!has_data_) {
            bar_->set_chip_text(chip_key(c), text);
            continue;
        }
        if (n < 0)
            text += QStringLiteral(" …");
        else if (c == RelChip::SupplyChain && n == 0)
            text += QStringLiteral(" —");
        else
            text += QStringLiteral(" ") + QString::number(n);
        bar_->set_chip_text(chip_key(c), text);
        bar_->set_chip_tooltip(chip_key(c), c == RelChip::SupplyChain && n == 0
                                                ? tr("Customers and suppliers are not available from free sources")
                                                : tr("Show or hide this relationship type"));
    }
}

void RelationshipMapScreen::update_status() {
    using SB = ui::ScreenStatusBar;
    const RelationshipData& d = current_data_;
    const int real = model_.real_node_count();
    int edges = 0;
    for (const auto& e : model_.edges)
        if (!e.second_hop)
            ++edges;
    status_->set_text(QStringLiteral("counts"), tr("%1 NODES · %2 EDGES").arg(real).arg(edges));
    int ok = 0, total = 0, loading = 0;
    QStringList failed;
    QSet<QString> sources;
    QStringList section_lines;
    QStringList ids = d.sections.keys();
    std::sort(ids.begin(), ids.end());
    for (const QString& id : std::as_const(ids)) {
        const RelSectionStatus& s = d.sections[id];
        if (s.state == RelSectionState::Unknown || s.state == RelSectionState::NotApplicable)
            continue;
        ++total;
        if (s.state == RelSectionState::Ok || s.state == RelSectionState::Empty || s.state == RelSectionState::Unavailable)
            ++ok;
        if (s.state == RelSectionState::Loading)
            ++loading;
        if (section_failed(s.state))
            failed << id;
        if (!s.source.isEmpty() && s.state == RelSectionState::Ok)
            sources.insert(s.source);
        section_lines << QStringLiteral("%1: %2%3%4")
                             .arg(id, section_state_text(s.state),
                                  s.source.isEmpty() ? QString() : QStringLiteral(" · ") + s.source,
                                  s.as_of.isEmpty() ? QString() : QStringLiteral(" · as of ") + s.as_of);
    }
    const SB::Tone tone = !failed.isEmpty() ? SB::Tone::Warn : (loading > 0 ? SB::Tone::Info : SB::Tone::Ok);
    status_->set_item(QStringLiteral("sections"),
                      loading > 0 ? tr("%1 of %2 sections · loading").arg(ok).arg(total)
                                  : (failed.isEmpty() ? tr("%1 of %2 sections").arg(ok).arg(total)
                                                      : tr("%1 of %2 sections · missing %3")
                                                            .arg(ok)
                                                            .arg(total)
                                                            .arg(failed.join(QStringLiteral(", ")))),
                      tone);
    status_->set_chip(QStringLiteral("sections"), !failed.isEmpty());
    status_->set_tooltip(QStringLiteral("sections"), section_lines.join(QLatin1Char('\n')));
    QStringList src = sources.values();
    std::sort(src.begin(), src.end());
    status_->set_text(QStringLiteral("sources"), src.join(QStringLiteral(" · ")));
    const QString q = d.company.currency, f = d.company.financial_currency;
    status_->set_text(QStringLiteral("ccy"), q.isEmpty() ? QString()
                                                         : (f.isEmpty() || f == q ? tr("quoted in %1").arg(q)
                                                                                  : tr("quoted in %1 · reports in %2").arg(q, f)));
}

void RelationshipMapScreen::update_partial_notice() {
    const QStringList failed = failed_sections(current_data_);
    if (failed.isEmpty() && !current_data_.partial) {
        graph_pane_->clear_notice();
        return;
    }
    const QString what = failed.isEmpty() ? current_data_.missing_sections.join(QStringLiteral(", "))
                                          : failed.join(QStringLiteral(", "));
    graph_pane_->set_notice(tr("Partial map — not loaded: %1").arg(what), ui::EmptyState::Tone::Warning,
                            tr("RETRY THESE"));
}

void RelationshipMapScreen::update_history_ui() {
    back_btn_->setEnabled(history_pos_ > 0);
    fwd_btn_->setEnabled(history_pos_ >= 0 && history_pos_ + 1 < history_.size());
    // Breadcrumb: the companies walked, the current one in the accent colour.
    QStringList parts;
    const int first = std::max(0, history_pos_ - 4);
    const int last = std::min(static_cast<int>(history_.size()) - 1, history_pos_ + 2);
    if (first > 0)
        parts << QStringLiteral("…");
    for (int i = first; i <= last; ++i) {
        const QString t = history_.value(i).toHtmlEscaped();
        if (i == history_pos_)
            parts << QStringLiteral("<span style=\"color:%1; font-weight:700;\">%2</span>").arg(colors::AMBER(), t);
        else
            parts << QStringLiteral("<a href=\"%1\" style=\"color:%2; text-decoration:none;\">%3</a>")
                         .arg(QString::number(i), colors::TEXT_SECONDARY(), t);
    }
    crumbs_->setText(parts.join(QStringLiteral(" <span style=\"color:%1;\">›</span> ").arg(colors::TEXT_DIM())));
}

// ── Show / hide / language ──────────────────────────────────────────────────

void RelationshipMapScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!pending_group_symbol_.isEmpty()) {
        restore_pending_ = false;
        const QString s = std::exchange(pending_group_symbol_, QString());
        if (!(has_data_ && s == loaded_ticker_.toUpper())) {
            group_driven_ticker_ = s;
            analyze(s);
        }
        return;
    }
    if (std::exchange(restore_pending_, false) && graph_pane_->has_result() &&
        requested_ticker_.compare(loaded_ticker_, Qt::CaseInsensitive) != 0)
        analyze(requested_ticker_, false);
}

void RelationshipMapScreen::hideEvent(QHideEvent* e) {
    // Leaving cancels the scripts (the pane re-runs on the next visit when nothing is shown).
    if (svc_request_ != 0)
        graph_pane_->cancel();
    auto& svc = services::RelationshipMapService::instance();
    if (svc_request_ != 0)
        svc.cancel(std::exchange(svc_request_, 0));
    if (retry_request_ != 0) {
        svc.cancel(std::exchange(retry_request_, 0));
        load_label_->clear();
    }
    QWidget::hideEvent(e);
}

void RelationshipMapScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void RelationshipMapScreen::retranslate() {
    title_->setText(tr("RELATIONSHIP MAP"));
    symbol_box_->set_placeholder(tr("Company or fund: ticker, name or ISIN"));
    symbol_box_->setAccessibleName(tr("Company to map"));
    analyze_btn_->setText(tr("ANALYZE"));
    find_edit_->setPlaceholderText(tr("Find in graph  /"));
    bar_->set_segment_text(QStringLiteral("layout"), QStringLiteral("tiered"), tr("TIERED"));
    bar_->set_segment_text(QStringLiteral("layout"), QStringLiteral("hierarchy"), tr("HIERARCHY"));
    bar_->set_segment_text(QStringLiteral("layout"), QStringLiteral("radial"), tr("RADIAL"));
    bar_->set_segment_text(QStringLiteral("layout"), QStringLiteral("force"), tr("FORCE"));
    bar_->set_segment_text(QStringLiteral("layout"), QStringLiteral("sankey"), tr("SANKEY"));
    bar_->set_segment_text(QStringLiteral("layout"), QStringLiteral("matrix"), tr("MATRIX"));
    bar_->set_segment_text(QStringLiteral("metric"), QStringLiteral("pct"), tr("% HELD"));
    bar_->set_segment_text(QStringLiteral("metric"), QStringLiteral("value"), tr("VALUE"));
    bar_->set_segment_text(QStringLiteral("metric"), QStringLiteral("change"), tr("CHANGE"));
    bar_->set_segment_text(QStringLiteral("metric"), QStringLiteral("corr"), tr("ρ CORR"));
    bar_->set_segment_text(QStringLiteral("depth"), QStringLiteral("1"), tr("1 HOP"));
    bar_->set_segment_text(QStringLiteral("depth"), QStringLiteral("2"), tr("2 HOPS"));
    bar_->set_action_text(QStringLiteral("fit"), tr("FIT"));
    bar_->set_action_text(QStringLiteral("refresh"), tr("REFRESH"));
    bar_->set_action_text(QStringLiteral("export"), tr("EXPORT"));
    bar_->set_action_text(QStringLiteral("legend"), tr("LEGEND"));
    bar_->set_action_text(QStringLiteral("tables"), tr("TABLES"));
    bar_->set_chips_label(tr("SHOW"));
    update_chips();
    static_cast<RmscrLegend*>(legend_)->set_metric_text(rmscr_metric_text(metric_));
    if (has_data_) {
        update_chips();
        update_ribbon();
        update_status();
        scene_->refresh_text();
        show_detail(selected_id_);
    }
}

// ── IGroupLinked ────────────────────────────────────────────────────────────

void RelationshipMapScreen::on_group_symbol_changed(const SymbolRef& ref) {
    const QString symbol = rmscr_ref_key(ref);
    if (symbol.isEmpty())
        return;
    if (!rmscr_is_mappable(symbol)) {
        // An index, FX pair, coin or future has no holders / officers / peers: keep the map.
        load_label_->setText(tr("%1 is not a company or fund — the map follows those only").arg(symbol));
        return;
    }
    if (!isVisible()) {
        pending_group_symbol_ = symbol;
        return;
    }
    if (has_data_ && symbol == loaded_ticker_.toUpper())
        return;
    group_driven_ticker_ = symbol;
    analyze(symbol);
}

SymbolRef RelationshipMapScreen::current_symbol() const {
    if (loaded_ticker_.isEmpty())
        return {};
    SymbolRef ref = SymbolRef::equity(loaded_ticker_);
    ref.yahoo = loaded_ticker_.toUpper(); // already the market-data key
    return ref;
}

// ── ISectionLinked ──────────────────────────────────────────────────────────

bool RelationshipMapScreen::open_section(const QString& section, const QString& focus) {
    const QString s = section.trimmed().toLower();
    if (s == QLatin1String("company")) {
        if (focus.trimmed().isEmpty())
            return true;
        analyze(focus);
        return true;
    }
    RelLayoutMode m;
    if (layout_from_key(s, &m)) {
        set_layout_mode(m);
    } else {
        bool chip = false;
        for (int i = 0; i < kRelChipCount; ++i) {
            if (chip_key(static_cast<RelChip>(i)) != s)
                continue;
            chip = true;
            options_.chips[static_cast<size_t>(i)] = true;
            bar_->set_chip_checked(s, true);
            if (has_data_)
                rebuild(true);
        }
        // The matching table, when there is one ("owners" -> holders, "correlated" -> correlation).
        QString view = s;
        if (s == QLatin1String("owners"))
            view = QStringLiteral("holders");
        else if (s == QLatin1String("correlated"))
            view = QStringLiteral("correlation");
        const bool table = linked_->open_view(view);
        if (!chip && !table)
            return false;
    }
    if (!focus.trimmed().isEmpty() && has_data_) {
        const QString id = scene_->find_and_pulse(focus);
        if (!id.isEmpty())
            select_entity(id, false);
    }
    return true;
}

// ── IStatefulScreen ─────────────────────────────────────────────────────────

QVariantMap RelationshipMapScreen::save_state() const {
    QStringList chips_off;
    for (int i = 0; i < kRelChipCount; ++i)
        if (!options_.chips[static_cast<size_t>(i)])
            chips_off << chip_key(static_cast<RelChip>(i));
    return {
        {"ticker", !loaded_ticker_.isEmpty() ? loaded_ticker_ : requested_ticker_},
        {"layout", layout_key(layout_mode_)},
        {"metric", metric_key(metric_)},
        {"depth", options_.depth},
        {"top_n", options_.top_n},
        {"chips_off", chips_off},
        {"history", history_},
        {"history_pos", history_pos_},
        {"view", linked_ ? linked_->current_view() : QString()},
        {"splitter_v", vsplit_ ? QString::fromLatin1(vsplit_->saveState().toBase64()) : QString()},
        {"splitter_h", hsplit_ ? QString::fromLatin1(hsplit_->saveState().toBase64()) : QString()},
    };
}

void RelationshipMapScreen::restore_state(const QVariantMap& state) {
    // Version 1 kept {"ticker", "layout_mode": 0 layered | 1 radial}.
    RelLayoutMode m = RelLayoutMode::Tiered;
    if (state.contains(QStringLiteral("layout")))
        layout_from_key(state.value(QStringLiteral("layout")).toString(), &m);
    else if (state.value(QStringLiteral("layout_mode"), 0).toInt() == 1)
        m = RelLayoutMode::Radial;
    set_layout_mode(m, false);
    const QString metric = state.value(QStringLiteral("metric")).toString();
    const RelEdgeMetric ms[] = {RelEdgeMetric::Percent, RelEdgeMetric::Value, RelEdgeMetric::Change,
                                RelEdgeMetric::Correlation};
    for (RelEdgeMetric x : ms)
        if (metric_key(x) == metric)
            metric_ = x;
    bar_->set_segment(QStringLiteral("metric"), metric_key(metric_));
    static_cast<RmscrLegend*>(legend_)->set_metric_text(rmscr_metric_text(metric_));
    options_.depth = state.value(QStringLiteral("depth"), 1).toInt() == 2 ? 2 : 1;
    bar_->set_segment(QStringLiteral("depth"), QString::number(options_.depth));
    {
        const QSignalBlocker block(top_n_spin_);
        options_.top_n = std::clamp(state.value(QStringLiteral("top_n"), 6).toInt(), 3, 25);
        top_n_spin_->setValue(options_.top_n);
    }
    const QStringList off = state.value(QStringLiteral("chips_off")).toStringList();
    for (int i = 0; i < kRelChipCount; ++i) {
        const QString key = chip_key(static_cast<RelChip>(i));
        options_.chips[static_cast<size_t>(i)] = !off.contains(key);
        bar_->set_chip_checked(key, !off.contains(key));
    }
    const QStringList hist = state.value(QStringLiteral("history")).toStringList();
    if (!hist.isEmpty()) {
        history_ = hist;
        history_pos_ = std::clamp(state.value(QStringLiteral("history_pos"), hist.size() - 1).toInt(), 0,
                                  static_cast<int>(hist.size()) - 1);
    }
    const QByteArray sv = QByteArray::fromBase64(state.value(QStringLiteral("splitter_v")).toString().toLatin1());
    const QByteArray sh = QByteArray::fromBase64(state.value(QStringLiteral("splitter_h")).toString().toLatin1());
    if (!sv.isEmpty())
        vsplit_->restoreState(sv);
    if (!sh.isEmpty())
        hsplit_->restoreState(sh);
    const QString view = state.value(QStringLiteral("view")).toString();
    if (!view.isEmpty())
        linked_->open_view(view);

    const QString restored = state.value(QStringLiteral("ticker")).toString().trimmed().toUpper();
    if (!restored.isEmpty()) {
        if (history_.isEmpty() || history_.value(history_pos_) != restored) {
            history_ << restored;
            history_pos_ = static_cast<int>(history_.size()) - 1;
        }
        update_history_ui();
        // Restored possibly while hidden: the pane's first-show run opens on this ticker (no
        // script for a screen nobody has opened); over another company's map it loads on show.
        if (isVisible()) {
            analyze(restored, false);
        } else {
            requested_ticker_ = restored;
            symbol_box_->set_text(restored);
            restore_pending_ = true;
        }
    }
}

} // namespace fincept::screens
