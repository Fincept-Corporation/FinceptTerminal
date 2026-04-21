// src/screens/relationship_map/RelationshipMapScreen.cpp
#include "screens/relationship_map/RelationshipMapScreen.h"

#include "core/events/EventBus.h"
#include "core/session/ScreenStateManager.h"
#include "network/http/HttpClient.h"
#include "screens/relationship_map/RelationshipGraphScene.h"
#include "services/relationship_map/RelationshipMapService.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::relmap;

static inline QString MF() {
    return QStringLiteral("font-family:'Consolas','Courier New',monospace;");
}

static constexpr int kSearchDebounceMs = 300;
static constexpr int kMaxSearchResults = 10;

/// Convert exchange + symbol to yfinance-compatible ticker.
static QString to_yfinance_symbol(const QString& symbol, const QString& exchange, const QString& country = {}) {
    if (exchange.toUpper() == "EURONEXT") {
        static const QHash<QString, QString> m = {
            {"FR", ".PA"}, {"NL", ".AS"}, {"BE", ".BR"}, {"PT", ".LS"}, {"IE", ".IR"},
        };
        auto it = m.find(country.toUpper());
        return symbol + (it != m.end() ? it.value() : ".PA");
    }
    static const QHash<QString, QString> s = {
        {"NSE", ".NS"}, {"BSE", ".BO"}, {"HKEX", ".HK"}, {"TSE", ".T"},
        {"KRX", ".KS"}, {"SGX", ".SI"}, {"ASX", ".AX"}, {"IDX", ".JK"},
        {"XETR", ".DE"}, {"FWB", ".F"}, {"LSE", ".L"}, {"BME", ".MC"},
        {"MIL", ".MI"}, {"SIX", ".SW"}, {"VIE", ".VI"}, {"TSX", ".TO"},
        {"TSXV", ".V"}, {"BMFBOVESPA", ".SA"}, {"BMV", ".MX"}, {"BIST", ".IS"},
    };
    auto it = s.find(exchange.toUpper());
    return it != s.end() ? symbol + it.value() : symbol;
}

// ── Constructor ──────────────────────────────────────────────────────────────

RelationshipMapScreen::RelationshipMapScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    auto& svc = services::RelationshipMapService::instance();
    connect(&svc, &services::RelationshipMapService::data_ready, this, &RelationshipMapScreen::on_data_ready);
    connect(&svc, &services::RelationshipMapService::fetch_failed, this, &RelationshipMapScreen::on_fetch_failed);
    connect(&svc, &services::RelationshipMapService::progress_changed, this, &RelationshipMapScreen::on_progress);
}

void RelationshipMapScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    hide_dropdown();
}

void RelationshipMapScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    hide_dropdown();
}

bool RelationshipMapScreen::eventFilter(QObject* obj, QEvent* event) {
    if (obj == search_input_) {
        if (event->type() == QEvent::FocusIn) {
            search_input_focused_ = true;
        } else if (event->type() == QEvent::FocusOut) {
            search_input_focused_ = false;
            // Small delay so a click on a dropdown item is registered first.
            QTimer::singleShot(150, this, [this]() { hide_dropdown(); });
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void RelationshipMapScreen::build_ui() {
    setStyleSheet(QString("QWidget#RelMapRoot { background: %1; }").arg(colors::BG_BASE()));
    setObjectName("RelMapRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header Bar ───────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(44);
    header->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    hhl->setSpacing(10);

    auto* title = new QLabel("CORPORATE INTELLIGENCE MAP");
    title->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700; "
                                 "letter-spacing: 0.5px; %2")
                             .arg(colors::AMBER(), MF()));
    hhl->addWidget(title);

    // Search with autocomplete
    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search assets (AAPL, Tesla, RELIANCE...)");
    search_input_->setFixedWidth(320);
    search_input_->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                "padding: 4px 10px; font-size: 12px; %4 }"
                "QLineEdit:focus { border-color: %5; }")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), MF(), colors::AMBER()));
    connect(search_input_, &QLineEdit::returnPressed, this, &RelationshipMapScreen::on_search);
    connect(search_input_, &QLineEdit::textChanged, this, &RelationshipMapScreen::on_search_text_changed);
    // Track focus via eventFilter — dropdown only appears when user is actively typing.
    search_input_->installEventFilter(this);
    hhl->addWidget(search_input_);

    // Autocomplete dropdown — child widget, no window flags that steal focus
    search_dropdown_ = new QListWidget(this);
    search_dropdown_->setFocusPolicy(Qt::NoFocus);
    search_dropdown_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    search_dropdown_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    search_dropdown_->setStyleSheet(
        QString("QListWidget { background: %1; border: 1px solid %2; font-size: 11px; %3 }"
                "QListWidget::item { padding: 4px 8px; border-bottom: 1px solid %4; }"
                "QListWidget::item:selected { background: %5; }"
                "QListWidget::item:hover { background: %5; }")
            .arg(colors::BG_SURFACE(), colors::AMBER_DIM(), MF(), colors::BORDER_DIM(), colors::BG_RAISED()));
    search_dropdown_->setFixedWidth(420);
    search_dropdown_->setMaximumHeight(320);
    search_dropdown_->hide();
    search_dropdown_->raise();
    auto select_dropdown_item = [this](QListWidgetItem* item) {
        if (!item) return;
        QString symbol = item->data(Qt::UserRole).toString();
        if (symbol.isEmpty()) return;
        search_input_focused_ = false;
        search_input_->blockSignals(true);
        search_input_->setText(symbol);
        search_input_->blockSignals(false);
        hide_dropdown();
        on_search();
    };
    connect(search_dropdown_, &QListWidget::itemClicked,    this, select_dropdown_item);
    connect(search_dropdown_, &QListWidget::itemActivated,  this, select_dropdown_item);

    // Debounce timer
    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    connect(search_debounce_, &QTimer::timeout, this, [this]() {
        if (!pending_query_.isEmpty())
            fire_asset_search(pending_query_);
    });

    auto* search_btn = new QPushButton("ANALYZE");
    search_btn->setCursor(Qt::PointingHandCursor);
    search_btn->setFixedHeight(28);
    search_btn->setStyleSheet(
        QString("QPushButton { background: rgba(217,119,6,0.15); color: %1; border: 1px solid %3; "
                "padding: 0 14px; font-size: 11px; font-weight: 700; %2 }"
                "QPushButton:hover { background: %1; color: %4; }")
            .arg(colors::AMBER(), MF(), colors::AMBER_DIM(), colors::BG_BASE()));
    connect(search_btn, &QPushButton::clicked, this, &RelationshipMapScreen::on_search);
    hhl->addWidget(search_btn);

    hhl->addStretch();

    // Fit / reset zoom button
    auto* fit_btn = new QPushButton("FIT");
    fit_btn->setCursor(Qt::PointingHandCursor);
    fit_btn->setFixedHeight(28);
    fit_btn->setToolTip("Fit graph to view (or press Home)");
    fit_btn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                "padding: 0 10px; font-size: 10px; %3 }"
                "QPushButton:hover { color: %4; border-color: %4; }")
            .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(), MF(), colors::TEXT_PRIMARY()));
    connect(fit_btn, &QPushButton::clicked, this, [this]() {
        if (view_) view_->fit_to_content();
    });
    hhl->addWidget(fit_btn);

    // Layout selector
    layout_combo_ = new QComboBox;
    layout_combo_->addItem("LAYERED", (int)LayoutMode::Layered);
    layout_combo_->addItem("RADIAL", (int)LayoutMode::Radial);
    layout_combo_->addItem("FORCE", (int)LayoutMode::Force);
    layout_combo_->setStyleSheet(QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                                         "padding: 3px 8px; font-size: 10px; %4 }")
                                     .arg(colors::BG_SURFACE(), colors::TEXT_SECONDARY(), colors::BORDER_DIM(), MF()));
    connect(layout_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        layout_mode_ = static_cast<LayoutMode>(layout_combo_->itemData(idx).toInt());
        if (has_data_)
            rebuild_graph();
        ScreenStateManager::instance().notify_changed(this);
    });
    hhl->addWidget(layout_combo_);

    // Filter toggle
    filter_btn_ = new QPushButton("FILTERS");
    filter_btn_->setCursor(Qt::PointingHandCursor);
    filter_btn_->setCheckable(true);
    filter_btn_->setFixedHeight(28);
    filter_btn_->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                "padding: 0 10px; font-size: 10px; %3 }"
                "QPushButton:hover { color: %5; } "
                "QPushButton:checked { background: rgba(217,119,6,0.1); color: %4; border-color: %6; }")
            .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(), MF(), colors::AMBER(), colors::TEXT_PRIMARY(),
                 colors::AMBER_DIM()));
    connect(filter_btn_, &QPushButton::toggled, this, [this](bool checked) { filter_panel_->setVisible(checked); });
    hhl->addWidget(filter_btn_);

    root->addWidget(header);

    // ── Progress bar ─────────────────────────────────────────────────────
    auto* prog_row = new QWidget(this);
    prog_row->setFixedHeight(20);
    prog_row->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* phl = new QHBoxLayout(prog_row);
    phl->setContentsMargins(12, 2, 12, 2);

    progress_label_ = new QLabel("Ready");
    progress_label_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::TEXT_DIM(), MF()));
    phl->addWidget(progress_label_);

    progress_bar_ = new QProgressBar;
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(false);
    progress_bar_->setFixedHeight(4);
    progress_bar_->setStyleSheet(QString("QProgressBar { background: %1; border: none; }"
                                         "QProgressBar::chunk { background: %2; }")
                                     .arg(colors::BG_RAISED(), colors::AMBER()));
    progress_bar_->hide();
    phl->addWidget(progress_bar_, 1);

    root->addWidget(prog_row);

    // ── Main content area ────────────────────────────────────────────────
    auto* content = new QWidget(this);
    auto* chl = new QHBoxLayout(content);
    chl->setContentsMargins(0, 0, 0, 0);
    chl->setSpacing(0);

    // Filter panel (left, hidden by default)
    filter_panel_ = build_filter_panel();
    filter_panel_->setVisible(false);
    chl->addWidget(filter_panel_);

    // Graph view (center)
    scene_ = new RelationshipGraphScene(this);
    view_ = new RelationshipGraphView(scene_, this);
    chl->addWidget(view_, 1);

    // Detail panel (right, always visible when node selected)
    detail_panel_ = build_detail_panel();
    detail_panel_->setVisible(false);
    chl->addWidget(detail_panel_);

    root->addWidget(content, 1);

    // Legend (floating over graph)
    legend_widget_ = build_legend();
    legend_widget_->setParent(view_);
    legend_widget_->move(10, 10);
    legend_widget_->hide();

    // ── Status bar ───────────────────────────────────────────────────────
    auto* status = new QWidget(this);
    status->setFixedHeight(24);
    status->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* shl = new QHBoxLayout(status);
    shl->setContentsMargins(12, 0, 12, 0);

    status_nodes_ = new QLabel("READY");
    status_nodes_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::TEXT_DIM(), MF()));
    shl->addWidget(status_nodes_);

    shl->addStretch();

    status_quality_ = new QLabel("");
    status_quality_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::TEXT_DIM(), MF()));
    shl->addWidget(status_quality_);

    shl->addStretch();

    status_brand_ = new QLabel("FINCEPT TERMINAL");
    status_brand_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: 700; %2").arg(colors::AMBER(), MF()));
    shl->addWidget(status_brand_);

    root->addWidget(status);

    // Center card click → navigate to equity research
    connect(scene_, &relmap::RelationshipGraphScene::center_card_clicked,
            this, [](const QString& ticker) {
                fincept::EventBus::instance().publish("equity_research.load_symbol",
                    {{"symbol", ticker}, {"type", "equity"}});
            });
}

// ── Filter Panel ─────────────────────────────────────────────────────────────

QWidget* RelationshipMapScreen::build_filter_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(200);
    panel->setStyleSheet(
        QString("background: %1; border-right: 1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* title = new QLabel("FILTERS");
    title->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                 "letter-spacing: 0.5px; %2")
                             .arg(colors::AMBER(), MF()));
    vl->addWidget(title);

    auto make_check = [&](const QString& label, bool& state, NodeCategory cat) {
        auto* cb = new QCheckBox(label);
        cb->setChecked(state);
        QColor col = category_color(cat);
        cb->setStyleSheet(QString("QCheckBox { color: %1; font-size: 11px; %2; spacing: 6px; }"
                                  "QCheckBox::indicator { width: 12px; height: 12px; "
                                  "border: 1px solid %3; background: transparent; }"
                                  "QCheckBox::indicator:checked { background: %3; }")
                              .arg(colors::TEXT_SECONDARY(), MF(), col.name()));
        connect(cb, &QCheckBox::toggled, this, [this, &state](bool checked) {
            state = checked;
            if (has_data_)
                rebuild_graph();
        });
        vl->addWidget(cb);
    };

    make_check("Peers",        filters_.show_peers,         NodeCategory::Peer);
    make_check("Institutional",filters_.show_institutional, NodeCategory::Institutional);
    make_check("Mutual Funds", filters_.show_institutional, NodeCategory::MutualFund);
    make_check("Insiders",     filters_.show_insiders,      NodeCategory::Insider);
    make_check("Officers",     filters_.show_officers,      NodeCategory::Officer);
    make_check("Analysts",     filters_.show_analysts,      NodeCategory::Analyst);
    make_check("Metrics",      filters_.show_metrics,       NodeCategory::Metrics);
    make_check("Events",       filters_.show_events,        NodeCategory::Event);
    make_check("Supply Chain", filters_.show_supply_chain,  NodeCategory::SupplyChain);

    vl->addStretch();
    return panel;
}

// ── Detail Panel ─────────────────────────────────────────────────────────────

QWidget* RelationshipMapScreen::build_detail_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(260);
    panel->setStyleSheet(
        QString("background: %1; border-left: 1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* header = new QHBoxLayout;
    detail_title_ = new QLabel("SELECT A NODE");
    detail_title_->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: 700; %2").arg(colors::TEXT_PRIMARY(), MF()));
    header->addWidget(detail_title_);
    header->addStretch();

    auto* close_btn = new QPushButton("X");
    close_btn->setFixedSize(20, 20);
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                     "font-size: 11px; %2 } QPushButton:hover { color: %3; }")
                                 .arg(colors::TEXT_DIM(), MF(), colors::TEXT_PRIMARY()));
    connect(close_btn, &QPushButton::clicked, this, [this]() {
        detail_panel_->hide();
        scene_->clearSelection();
    });
    header->addWidget(close_btn);
    vl->addLayout(header);

    detail_category_ = new QLabel("");
    detail_category_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: 700; "
                                            "letter-spacing: 0.5px; %2")
                                        .arg(colors::TEXT_TERTIARY(), MF()));
    vl->addWidget(detail_category_);

    // Scrollable properties
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    detail_props_container_ = new QWidget(this);
    detail_props_container_->setStyleSheet("background: transparent;");
    new QVBoxLayout(detail_props_container_);

    scroll->setWidget(detail_props_container_);
    vl->addWidget(scroll, 1);

    return panel;
}

// ── Legend ────────────────────────────────────────────────────────────────────

QWidget* RelationshipMapScreen::build_legend() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(160);
    panel->setStyleSheet(QString("background: rgba(10,10,10,0.9); border: 1px solid %1; "
                                 "border-radius: 2px;")
                             .arg(colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(3);

    auto* title = new QLabel("LEGEND");
    title->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: 700; "
                                 "letter-spacing: 0.5px; %2")
                             .arg(colors::TEXT_DIM(), MF()));
    vl->addWidget(title);

    auto add_entry = [&](NodeCategory cat) {
        auto* row = new QWidget(this);
        row->setStyleSheet("background: transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);

        auto* dot = new QLabel;
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QString("background: %1; border-radius: 4px;").arg(category_color(cat).name()));
        hl->addWidget(dot);

        auto* lbl = new QLabel(category_label(cat));
        lbl->setStyleSheet(QString("color: %1; font-size: 8px; %2").arg(colors::TEXT_TERTIARY(), MF()));
        hl->addWidget(lbl);
        hl->addStretch();
        vl->addWidget(row);
    };

    add_entry(NodeCategory::Company);
    add_entry(NodeCategory::Peer);
    add_entry(NodeCategory::Institutional);
    add_entry(NodeCategory::MutualFund);
    add_entry(NodeCategory::Insider);
    add_entry(NodeCategory::Officer);
    add_entry(NodeCategory::Analyst);
    add_entry(NodeCategory::Governance);
    add_entry(NodeCategory::Technicals);
    add_entry(NodeCategory::ShortInterest);
    add_entry(NodeCategory::Earnings);
    add_entry(NodeCategory::Metrics);
    add_entry(NodeCategory::Event);
    add_entry(NodeCategory::SupplyChain);

    return panel;
}

// ── Actions ──────────────────────────────────────────────────────────────────

void RelationshipMapScreen::on_search() {
    // If the autocomplete dropdown is open, prefer the selected/first result.
    if (!search_dropdown_->isHidden() && search_dropdown_->count() > 0) {
        QListWidgetItem* chosen = search_dropdown_->currentItem();
        if (!chosen) chosen = search_dropdown_->item(0);
        if (chosen) {
            QString symbol = chosen->data(Qt::UserRole).toString();
            if (!symbol.isEmpty()) {
                search_input_->blockSignals(true);
                search_input_->setText(symbol);
                search_input_->blockSignals(false);
            }
        }
    }
    hide_dropdown();
    search_input_focused_ = false;

    QString ticker = search_input_->text().trimmed().toUpper();
    if (ticker.isEmpty())
        return;

    search_input_->blockSignals(true);
    search_input_->setText(ticker);
    search_input_->blockSignals(false);
    progress_bar_->show();
    progress_bar_->setValue(0);
    detail_panel_->hide();
    legend_widget_->hide();
    has_data_ = false;

    services::RelationshipMapService::instance().fetch(ticker);
    ScreenStateManager::instance().notify_changed(this);
}

// ── Autocomplete search ─────────────────────────────────────────────────────

void RelationshipMapScreen::on_search_text_changed(const QString& text) {
    // Only show suggestions when the user is actively typing in the focused field.
    // Never show suggestions if the text matches what's already loaded in the graph.
    if (!search_input_focused_) {
        hide_dropdown();
        return;
    }
    QString query = text.trimmed();
    if (query.length() < 2) {
        hide_dropdown();
        return;
    }
    // If the typed text exactly matches the currently loaded ticker, don't re-suggest.
    if (!loaded_ticker_.isEmpty() && query.toUpper() == loaded_ticker_.toUpper()) {
        hide_dropdown();
        return;
    }
    pending_query_ = query;
    search_debounce_->start(kSearchDebounceMs);
}

void RelationshipMapScreen::fire_asset_search(const QString& query) {
    const QString url = QString("/market/search?q=%1&type=stock&limit=%2").arg(query).arg(kMaxSearchResults);

    QPointer<RelationshipMapScreen> self = this;
    fincept::HttpClient::instance().get(url, [self, query](Result<QJsonDocument> result) {
        if (!self) return;
        if (self->pending_query_ != query) return;
        if (!result.is_ok()) return;

        const auto doc = result.value();
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            const auto obj = doc.object();
            if (obj.contains("results")) arr = obj["results"].toArray();
            else if (obj.contains("data")) arr = obj["data"].toArray();
        }
        self->on_asset_results(arr);
    });
}

void RelationshipMapScreen::on_asset_results(const QJsonArray& results) {
    search_dropdown_->clear();

    if (results.isEmpty()) {
        auto* item = new QListWidgetItem(search_dropdown_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);
        auto* lbl = new QLabel("No results found");
        lbl->setStyleSheet(QString("color:%1;font-size:11px;%2;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY.get(), MF()));
        rl->addWidget(lbl);
        item->setSizeHint(QSize(0, 28));
        search_dropdown_->setItemWidget(item, row);
        show_dropdown();
        return;
    }

    for (const auto& val : results) {
        auto obj = val.toObject();
        QString symbol   = obj["symbol"].toString();
        QString name     = obj["name"].toString();
        QString exchange = obj["exchange"].toString();
        QString country  = obj["country"].toString();
        QString type     = obj["type"].toString("stock");

        if (symbol.isEmpty()) continue;

        QString yf_symbol = to_yfinance_symbol(symbol, exchange, country);

        auto* item = new QListWidgetItem(search_dropdown_);
        item->setData(Qt::UserRole, yf_symbol);

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(8, 3, 8, 3);
        hl->setSpacing(8);

        auto* sym_lbl = new QLabel(yf_symbol);
        sym_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;%2;background:transparent;")
                                   .arg(ui::colors::AMBER.get(), MF()));
        sym_lbl->setFixedWidth(100);
        hl->addWidget(sym_lbl);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(QString("color:%1;font-size:11px;%2;background:transparent;")
                                    .arg(ui::colors::TEXT_SECONDARY.get(), MF()));
        name_lbl->setMaximumWidth(180);
        hl->addWidget(name_lbl, 1);

        if (!exchange.isEmpty()) {
            auto* exch_lbl = new QLabel(exchange);
            exch_lbl->setStyleSheet(QString("color:%1;font-size:9px;%2;background:transparent;")
                                        .arg(ui::colors::TEXT_TERTIARY.get(), MF()));
            hl->addWidget(exch_lbl);
        }

        auto* type_lbl = new QLabel(type.toUpper());
        type_lbl->setStyleSheet(QString("color:%1;font-size:8px;font-weight:700;%2;"
                                        "background:%3;padding:1px 4px;border-radius:2px;")
                                    .arg(ui::colors::AMBER.get(), MF(), ui::colors::BG_RAISED.get()));
        hl->addWidget(type_lbl);

        item->setSizeHint(QSize(0, 30));
        search_dropdown_->setItemWidget(item, row);
    }

    if (search_dropdown_->count() > 0)
        search_dropdown_->setCurrentRow(0);
    show_dropdown();
}

void RelationshipMapScreen::show_dropdown() {
    if (search_dropdown_->count() == 0) { hide_dropdown(); return; }
    // Position below the search input, as a child of this widget
    QPoint pos = search_input_->mapTo(this, QPoint(0, search_input_->height()));
    search_dropdown_->move(pos);
    int h = 0;
    for (int i = 0; i < search_dropdown_->count(); ++i)
        h += search_dropdown_->sizeHintForRow(i);
    search_dropdown_->setFixedHeight(qMin(h + 4, 320));
    search_dropdown_->show();
    search_dropdown_->raise();
}

void RelationshipMapScreen::hide_dropdown() {
    search_dropdown_->hide();
}

void RelationshipMapScreen::on_progress(int percent, const QString& message) {
    progress_bar_->setValue(percent);
    progress_label_->setText(message);

    if (percent >= 100) {
        progress_bar_->hide();
    }
}

void RelationshipMapScreen::on_data_ready(const RelationshipData& payload) {
    current_data_ = payload;
    has_data_ = true;
    loaded_ticker_ = payload.company.ticker;
    progress_bar_->hide();
    progress_label_->setText("Complete");
    legend_widget_->show();
    rebuild_graph();
    update_status_bar();

    // Update search field to show the loaded ticker without triggering autocomplete.
    // Block signals so on_search_text_changed doesn't fire.
    search_input_->blockSignals(true);
    search_input_->setText(loaded_ticker_);
    search_input_->blockSignals(false);
    hide_dropdown();

    view_->fit_to_content();
}

void RelationshipMapScreen::on_fetch_failed(const QString& error) {
    progress_bar_->hide();
    progress_label_->setText("Error: " + error);
    progress_label_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::NEGATIVE(), MF()));
}

void RelationshipMapScreen::rebuild_graph() {
    scene_->build_graph(current_data_, filters_, layout_mode_);
    update_status_bar();
}

void RelationshipMapScreen::on_node_selected() {
    auto items = scene_->selectedItems();
    if (items.isEmpty()) {
        detail_panel_->hide();
        return;
    }

    // Find the GraphNodeItem
    for (auto* item : items) {
        // Dynamic cast to our custom type
        auto* rect = dynamic_cast<QGraphicsRectItem*>(item);
        if (!rect)
            continue;

        // Access node data through scene items — we stored it in the item
        // Since GraphNodeItem is defined in the .cpp, we use a different approach:
        // Get the data from item's tooltip or child text items
        auto children = rect->childItems();
        QString label, category_text, sublabel;
        QMap<QString, QString> props;

        if (children.size() >= 2) {
            auto* badge_item = dynamic_cast<QGraphicsTextItem*>(children[0]);
            auto* label_item = dynamic_cast<QGraphicsTextItem*>(children[1]);
            if (badge_item)
                category_text = badge_item->toPlainText();
            if (label_item)
                label = label_item->toPlainText();
            if (children.size() >= 3) {
                auto* sub_item = dynamic_cast<QGraphicsTextItem*>(children[2]);
                if (sub_item)
                    sublabel = sub_item->toPlainText();
            }
        }

        detail_title_->setText(label.isEmpty() ? "NODE" : label);
        detail_category_->setText(category_text);
        detail_category_->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; %2").arg(colors::AMBER(), MF()));

        // Clear and repopulate properties
        auto* layout = detail_props_container_->layout();
        QLayoutItem* child;
        while ((child = layout->takeAt(0)) != nullptr) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }

        if (!sublabel.isEmpty()) {
            auto* sub = new QLabel(sublabel);
            sub->setWordWrap(true);
            sub->setStyleSheet(QString("color: %1; font-size: 11px; %2").arg(colors::TEXT_SECONDARY(), MF()));
            layout->addWidget(sub);
        }

        // Look up full node data from current_data_ to show rich properties
        // (match by label/ticker)
        if (!label.isEmpty() && has_data_) {
            auto* sep = new QWidget(this);
            sep->setFixedHeight(1);
            sep->setStyleSheet(QString("background: %1;").arg(colors::BORDER_DIM()));
            layout->addWidget(sep);

            // Try to match with company
            if (label == current_data_.company.ticker) {
                auto add_prop = [&](const QString& key, const QString& val) {
                    auto* row = new QLabel(QString("%1:  %2").arg(key, val));
                    row->setStyleSheet(QString("color: %1; font-size: 10px; %2").arg(colors::TEXT_SECONDARY(), MF()));
                    layout->addWidget(row);
                };
                add_prop("Sector", current_data_.company.sector);
                add_prop("Industry", current_data_.company.industry);
                add_prop("Mkt Cap", QString("$%1B").arg(current_data_.company.market_cap / 1e9, 0, 'f', 1));
                add_prop("Price", QString("$%1").arg(current_data_.company.current_price, 0, 'f', 2));
                add_prop("P/E", QString::number(current_data_.company.pe_ratio, 'f', 1));
                add_prop("ROE", QString("%1%").arg(current_data_.company.roe * 100, 0, 'f', 1));
                add_prop("Growth", QString("%1%").arg(current_data_.company.revenue_growth * 100, 0, 'f', 1));
                add_prop("Margins", QString("%1%").arg(current_data_.company.profit_margins * 100, 0, 'f', 1));
                add_prop("Employees", QString::number(current_data_.company.employees));
                add_prop("Signal", current_data_.valuation.action);
            }

            // Try to match peer
            for (const auto& p : current_data_.peers) {
                if (p.ticker == label) {
                    auto add_prop = [&](const QString& key, const QString& val) {
                        auto* row = new QLabel(QString("%1:  %2").arg(key, val));
                        row->setStyleSheet(QString("color: %1; font-size: 10px; %2").arg(colors::TEXT_SECONDARY(), MF()));
                        layout->addWidget(row);
                    };
                    add_prop("Mkt Cap", QString("$%1B").arg(p.market_cap / 1e9, 0, 'f', 1));
                    add_prop("Price", QString("$%1").arg(p.current_price, 0, 'f', 2));
                    add_prop("P/E", QString::number(p.pe_ratio, 'f', 1));
                    add_prop("ROE", QString("%1%").arg(p.roe * 100, 0, 'f', 1));
                    add_prop("Growth", QString("%1%").arg(p.revenue_growth * 100, 0, 'f', 1));
                    break;
                }
            }
        }

        detail_panel_->show();
        break;
    }
}

void RelationshipMapScreen::update_status_bar() {
    if (!has_data_) {
        status_nodes_->setText("READY");
        status_quality_->setText("");
        return;
    }
    int node_count = scene_->items().size(); // approximate
    status_nodes_->setText(QString("%1 ITEMS | %2 PEERS | %3 HOLDERS")
                               .arg(node_count)
                               .arg(current_data_.peers.size())
                               .arg(current_data_.institutional_holders.size()));
    status_quality_->setText(QString("QUALITY: %1%").arg(current_data_.data_quality));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap RelationshipMapScreen::save_state() const {
    return {
        {"ticker", search_input_ ? search_input_->text().trimmed() : QString{}},
        {"layout_mode", static_cast<int>(layout_mode_)},
    };
}

void RelationshipMapScreen::restore_state(const QVariantMap& state) {
    const QString ticker = state.value("ticker").toString();
    const int layout = state.value("layout_mode", 0).toInt();

    if (layout_combo_) {
        // Find the combo item whose data matches layout_mode
        for (int i = 0; i < layout_combo_->count(); ++i) {
            if (layout_combo_->itemData(i).toInt() == layout) {
                layout_combo_->setCurrentIndex(i);
                break;
            }
        }
    }

    if (!ticker.isEmpty() && search_input_) {
        search_input_->setText(ticker);
        on_search();
    }
}

} // namespace fincept::screens
