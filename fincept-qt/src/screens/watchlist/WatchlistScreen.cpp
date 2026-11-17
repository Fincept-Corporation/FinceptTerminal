// WatchlistScreen.cpp — UI construction, theme, lifecycle, saved state, symbol link.
// Data flow lives in WatchlistScreen_Data.cpp, user actions in WatchlistScreen_Actions.cpp.

#include "screens/watchlist/WatchlistScreen.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/market/MarketProfile.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "screens/common/CommandRows.h"
#include "screens/watchlist/WatchlistAnalyticsViews.h"
#include "screens/watchlist/WatchlistColumns.h"
#include "screens/watchlist/WatchlistDetailPane.h"
#include "screens/watchlist/WatchlistScreenInternal.h"
#include "screens/watchlist/WatchlistView.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/markets/FxRateService.h"
#include "services/markets/MarketSearchService.h"
#include "services/markets/WatchlistAlertService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace fincept::screens {

using namespace fincept::ui;

namespace {

using wl_internal::kListColorRole;
using wl_internal::kListCountRole;
using wl_internal::kListDefaultRole;

// Sees every mouse press the ADD-box completer popup receives, before QCompleter does
// (QCompleter eats a press outside its popup only to close it). `fn` returns true to eat it.
class WlsPopupPressFilter final : public QObject {
  public:
    WlsPopupPressFilter(QObject* parent, std::function<bool(const QPoint&)> fn)
        : QObject(parent), fn_(std::move(fn)) {}

  protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() == QEvent::MouseButtonPress)
            return fn_(static_cast<QMouseEvent*>(e)->globalPosition().toPoint());
        return QObject::eventFilter(o, e);
    }

  private:
    std::function<bool(const QPoint&)> fn_;
};

// ── Style builders (live theme tokens; never cached) ─────────────────────────

QString wls_accent_btn_style() {
    return QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3; padding:0 12px; "
                          "min-height:24px; font-size:%4px; font-weight:700; }"
                          "QPushButton:hover { background:%2; color:%5; }"
                          "QPushButton:disabled { color:%6; border-color:%7; background:transparent; }")
        .arg(colors::alpha(colors::AMBER, 0.1), colors::AMBER(), colors::AMBER_DIM())
        .arg(fonts::px(-2))
        .arg(colors::TEXT_ON_ACCENT(), colors::TEXT_TERTIARY(), colors::BORDER_DIM());
}

QString wls_std_btn_style() {
    return QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3; padding:0 10px; "
                          "min-height:24px; font-size:%4px; font-weight:700; }"
                          "QPushButton:hover { color:%5; background:%6; }"
                          "QPushButton:checked { color:%7; border-color:%7; background:%8; }"
                          "QPushButton:disabled { color:%9; }"
                          "QPushButton::menu-indicator { width:0; }")
        .arg(colors::BG_RAISED(), colors::TEXT_SECONDARY(), colors::BORDER_DIM())
        .arg(fonts::px(-2))
        .arg(colors::TEXT_PRIMARY(), colors::BG_HOVER(), colors::AMBER(), colors::alpha(colors::AMBER, 0.1),
             colors::TEXT_TERTIARY());
}

QString wls_danger_btn_style() {
    return QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3; padding:0 10px; "
                          "min-height:24px; font-size:%4px; font-weight:700; }"
                          "QPushButton:hover { background:%2; color:%5; }"
                          "QPushButton:disabled { color:%6; border-color:%7; background:transparent; }")
        .arg(colors::alpha(colors::DANGER, 0.1), colors::DANGER(), colors::alpha(colors::DANGER, 0.3))
        .arg(fonts::px(-2))
        .arg(colors::TEXT_ON_ACCENT(), colors::TEXT_TERTIARY(), colors::BORDER_DIM());
}

QString wls_input_style() {
    return QStringLiteral("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:3px 6px; "
                          "font-size:%4px; }"
                          "QLineEdit:focus { border-color:%5; }"
                          "QLineEdit::selection { background:%6; color:%7; }")
        .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
        .arg(fonts::px(-1))
        .arg(colors::BORDER_BRIGHT(), colors::AMBER(), colors::TEXT_ON_ACCENT());
}

QString wls_combo_style() {
    return QStringLiteral("QComboBox { background:%1; color:%2; border:1px solid %3; padding:2px 6px; "
                          "font-size:%4px; min-height:22px; }")
        .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
        .arg(fonts::px(-2));
}

QString wls_caption_style(const char* color) {
    return QStringLiteral("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.5px; background:transparent;")
        .arg(QString::fromLatin1(color))
        .arg(fonts::px(-2));
}

/// setStyleSheet() re-polishes the widget and its whole subtree even when the text did not
/// change. The containers get their stylesheet when they are created (before their children
/// exist), so the refresh_theme() pass at the end of construction must not apply the same
/// text again — that re-styled the entire screen (~0.3 s of the first-open stall).
void wls_set_css(QWidget* w, const QString& css) {
    if (w && w->styleSheet() != css)
        w->setStyleSheet(css);
}

QString wls_root_css() {
    return QStringLiteral("background:%1;").arg(colors::BG_BASE());
}

/// What "ADD <REGION> LARGE CAPS" adds on an empty list: the first ten of the
/// region's large-cap universe (MarketProfile::movers, largest first).
QStringList wls_region_large_caps(const core::market::MarketProfile& profile) {
    return profile.movers.mid(0, 10);
}

QString wls_splitter_css() {
    return QStringLiteral("QSplitter::handle { background:%1; width:1px; }").arg(colors::BORDER_DIM());
}

QString wls_sidebar_css() {
    return QStringLiteral("background:%1; border-right:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM());
}

/// Header strips: sidebar header and the top bar.
QString wls_raised_bar_css() {
    return QStringLiteral("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM());
}

QString wls_add_bar_css() {
    return QStringLiteral("background:%1; border-bottom:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM());
}

/// Feedback bar colours by tone (0 Info, 1 Success, 2 Warning, 3 Error) — status colours.
void wls_style_feedback(QWidget* bar, QLabel* label, int tone) {
    QString fg;
    switch (tone) {
        case 1:
            fg = colors::SUCCESS();
            break;
        case 2:
            fg = colors::WARNING();
            break;
        case 3:
            fg = colors::DANGER();
            break;
        default:
            fg = colors::TEXT_SECONDARY();
            break;
    }
    wls_set_css(bar, QStringLiteral("background:%1; border-bottom:1px solid %2;")
                         .arg(colors::tint(QColor(fg), 0.10), colors::BORDER_DIM()));
    wls_set_css(label, QStringLiteral("color:%1; font-size:%2px; background:transparent; border:none;")
                           .arg(fg)
                           .arg(fonts::px(-1)));
}

/// Sidebar row: colour stripe, name, symbol count, default marker.
class WatchlistSidebarDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, idx);
        const QString name = opt.text;
        opt.text.clear();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget);

        p->save();
        const QRect r = option.rect;
        const QColor stripe(idx.data(kListColorRole).toString());
        if (stripe.isValid())
            p->fillRect(QRect(r.left(), r.top() + 3, 3, r.height() - 6), colors::legible_color(stripe));

        const bool selected = (option.state & QStyle::State_Selected) != 0;
        QFont f = opt.font;
        f.setBold(selected);
        p->setFont(f);
        const QFontMetrics fm(f);

        QString right = QString::number(idx.data(kListCountRole).toInt());
        const bool is_default = idx.data(kListDefaultRole).toBool();
        const int right_w = fm.horizontalAdvance(right) + (is_default ? fm.horizontalAdvance(QStringLiteral("★ ")) : 0);
        const QRect text_r = r.adjusted(12, 0, -(right_w + 14), 0);
        p->setPen(QColor(selected ? colors::TEXT_PRIMARY() : colors::TEXT_SECONDARY()));
        p->drawText(text_r, Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(name, Qt::ElideRight, text_r.width()));

        QRect count_r = r.adjusted(0, 0, -10, 0);
        p->setPen(QColor(colors::TEXT_TERTIARY()));
        p->drawText(count_r, Qt::AlignVCenter | Qt::AlignRight, right);
        if (is_default) {
            count_r.setRight(count_r.right() - fm.horizontalAdvance(right) - 4);
            p->setPen(QColor(colors::AMBER()));
            p->drawText(count_r, Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("★"));
        }
        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& idx) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, idx);
        s.setHeight(std::max(s.height(), option.fontMetrics.height() + 14));
        return s;
    }
};

/// ccy_box_ property: true while the watchlist has no currency of its own (none picked and
/// saved) and follows the display currency from Settings.
constexpr const char* kWlsCcyFollowsProp = "wlsCcyFollowsSettings";

/// Selects `ccy` in the currency box without emitting, appending it when the curated list
/// lacks it (a display currency such as PLN from Settings).
void wls_select_ccy(QComboBox* box, const QString& ccy) {
    int i = box->findData(ccy);
    QSignalBlocker block(box);
    if (i < 0) {
        box->addItem(ccy, ccy);
        i = box->count() - 1;
    }
    box->setCurrentIndex(i);
}

/// Entry 0 of the currency box (data ""): Automatic — the display currency from Settings,
/// followed when it changes. Its text names the currency it stands for now.
void wls_sync_auto_ccy_item(QComboBox* box) {
    if (!box || box->count() == 0 || !box->itemData(0).toString().isEmpty())
        return;
    const QString ccy = formatting::FormatSettings::instance().display_currency();
    box->setItemText(0, WatchlistScreen::tr("Automatic · %1").arg(ccy));
    box->setItemData(0,
                     WatchlistScreen::tr("Automatic (follows Settings): the display currency set in Settings, "
                                         "followed when it changes"),
                     Qt::ToolTipRole);
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

WatchlistScreen::WatchlistScreen(QWidget* parent) : QWidget(parent) {
    // Built on the GUI thread when the tab opens: time the parts, report when slow.
    QElapsedTimer build_clock;
    build_clock.start();
    QStringList build_marks;
    qint64 last_mark = 0;
    const auto mark = [&build_clock, &build_marks, &last_mark](const char* what) {
        const qint64 now = build_clock.elapsed();
        build_marks << QStringLiteral("%1 %2 ms").arg(QLatin1String(what)).arg(now - last_mark);
        last_mark = now;
    };
    const auto make_timer = [this](int ms, bool single) {
        auto* t = new QTimer(this);
        t->setSingleShot(single);
        t->setInterval(ms);
        return t;
    };
    layout_save_timer_ = make_timer(700, true);
    connect(layout_save_timer_, &QTimer::timeout, this, &WatchlistScreen::save_layout_now);
    footer_timer_ = make_timer(200, true);
    connect(footer_timer_, &QTimer::timeout, this, &WatchlistScreen::update_footer);
    clock_timer_ = make_timer(15'000, false);
    connect(clock_timer_, &QTimer::timeout, this, [this]() {
        model_->refresh_time_cells();
        update_last_updated();
    });
    history_timer_ = make_timer(250, true);
    connect(history_timer_, &QTimer::timeout, this, &WatchlistScreen::request_visible_history);
    views_timer_ = make_timer(400, true);
    connect(views_timer_, &QTimer::timeout, this, [this]() {
        if (view_stack_->currentWidget() == map_view_)
            refresh_map_view();
        else if (view_stack_->currentWidget() == corr_view_)
            refresh_correlation_view();
    });
    feedback_timer_ = make_timer(7000, true);
    connect(feedback_timer_, &QTimer::timeout, this, &WatchlistScreen::clear_feedback);
    autocomplete_timer_ = make_timer(250, true);
    connect(autocomplete_timer_, &QTimer::timeout, this, &WatchlistScreen::run_autocomplete);

    model_ = new WatchlistModel(this);
    proxy_ = new WatchlistProxyModel(this);
    proxy_->setSourceModel(model_);

    mark("models");
    // Screen stylesheet BEFORE the children exist: every widget is then styled once, at
    // birth. Set afterwards (refresh_theme() below) it re-styled the whole built tree.
    setStyleSheet(wls_root_css());
    build_ui();
    mark("build_ui");
    install_shortcuts();
    retranslateUi(); // every user-visible text is set there
    mark("texts");

    // Display currency for the converted-price column and footer totals: the one picked here
    // (saved), else the display currency from Settings, followed while nothing is picked.
    {
        auto r = SettingsRepository::instance().get(QString::fromLatin1(wl_internal::kDisplayCcyKey));
        const bool picked = r.is_ok() && !r.value().isEmpty();
        ccy_box_->setProperty(kWlsCcyFollowsProp, !picked);
        wls_sync_auto_ccy_item(ccy_box_);
        // Nothing picked: the "Automatic" entry (index 0), standing for the Settings currency.
        wls_select_ccy(ccy_box_, picked ? r.value() : QString());
        model_->set_display_currency(picked ? r.value() : formatting::FormatSettings::instance().display_currency());
    }

    mark("currency");
    load_watchlists();
    mark("load_watchlists");

    // Stylesheets, fixed sizes and the sidebar rebuild run once an appearance change has
    // settled (never inside theme_changed); refresh_theme() sets only changed stylesheets.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
    connect(&formatting::FormatSettings::instance(), &formatting::FormatSettings::changed, this, [this]() {
        // No currency picked here ("Automatic"): follow the display currency from Settings.
        wls_sync_auto_ccy_item(ccy_box_);
        if (ccy_box_->property(kWlsCcyFollowsProp).toBool()) {
            const QString ccy = formatting::FormatSettings::instance().display_currency();
            if (ccy != model_->display_currency()) {
                model_->set_display_currency(ccy);
                sync_fx();
            }
        }
        model_->refresh_all();
        schedule_footer();
        update_last_updated();
    });
    refresh_theme();
    mark("theme");

    // Reload from the local cache when a cloud pull updates watchlists. load_watchlists()
    // re-selects the current list, which already reloads its entries.
    connect(&fincept::services::cloud::CloudSyncEngine::instance(),
            &fincept::services::cloud::CloudSyncEngine::cloud_data_changed, this, [this](const QString& entity) {
                if (entity == QLatin1String("watchlist"))
                    load_watchlists();
            });

    auto& alerts = services::WatchlistAlertService::instance();
    connect(&alerts, &services::WatchlistAlertService::alerts_changed, this,
            &WatchlistScreen::refresh_alert_summaries);
    connect(&alerts, &services::WatchlistAlertService::alert_fired, this,
            [this](qint64, const QString& symbol, const QString& message) {
                if (isVisible() && model_->state(symbol))
                    show_feedback(message, FeedbackTone::Warning, 12'000);
            });
    mark("cloud sync hook");
    alerts.start(); // idempotent; also started at app launch when main.cpp has the hook
    mark("alerts");

    connect(&services::FxRateService::instance(), &services::FxRateService::rates_changed, this, [this]() {
        model_->refresh_time_cells();
        schedule_footer();
        schedule_views();
    });

    auto& search = services::MarketSearchService::instance();
    connect(&search, &services::MarketSearchService::results_ready, this,
            [this](const QString& request_id, const QString&, const QList<services::MarketSearchService::Item>& items) {
                if (request_id != autocomplete_request_ || !add_input_->hasFocus())
                    return;
                QStringList rows;
                for (const auto& it : items) {
                    const QString sym = it.yf_symbol.isEmpty() ? it.symbol : it.yf_symbol;
                    if (sym.isEmpty())
                        continue;
                    QString row = sym;
                    if (!it.name.isEmpty())
                        row += QStringLiteral("  —  ") + it.name;
                    if (!it.exchange.isEmpty())
                        row += QStringLiteral("  (") + it.exchange + QLatin1Char(')');
                    rows << row;
                }
                completion_model_->setStringList(rows);
                if (!rows.isEmpty()) {
                    completer_->complete();
                    // UnfilteredPopupCompletion makes row 0 current, so a plain Enter used to
                    // add the first suggestion (TCS.NS for a typed TCS.BO). No row is picked
                    // until the user moves to one (Down / hover) or clicks it.
                    completer_->popup()->setCurrentIndex(QModelIndex());
                }
            });
    mark("services");
    if (build_clock.elapsed() >= 150)
        LOG_DEBUG("WatchlistScreen", QStringLiteral("built in %1 ms (%2)")
                                        .arg(build_clock.elapsed())
                                        .arg(build_marks.join(QStringLiteral(", "))));
}

WatchlistScreen::~WatchlistScreen() {
    if (layout_save_timer_ && layout_save_timer_->isActive())
        save_layout_now();
}

void WatchlistScreen::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setStyleSheet(wls_splitter_css()); // before its children exist (see wls_set_css)
    splitter_->setChildrenCollapsible(true);
    // The three panes are created as children of the splitter, not moved into it once
    // built: a move re-styles the whole moved subtree.
    splitter_->addWidget(build_sidebar());
    splitter_->addWidget(build_main_panel());
    detail_ = new WatchlistDetailPane(splitter_);
    detail_->set_model(model_);
    splitter_->addWidget(detail_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setStretchFactor(2, 0);
    splitter_->setCollapsible(1, false);
    splitter_->setSizes({220, 900, 380});
    root->addWidget(splitter_);

    connect(detail_, &WatchlistDetailPane::notes_edited, this, [this](const QString& sym, const QString& notes) {
        if (current_wl_id_.isEmpty())
            return;
        auto r = WatchlistRepository::instance().set_stock_notes(current_wl_id_, sym, notes);
        if (r.is_err()) {
            show_feedback(tr("Could not save the note: %1").arg(QString::fromStdString(r.error())), FeedbackTone::Error);
            return;
        }
        for (auto& s : stocks_) {
            if (s.symbol == sym) {
                s.notes = notes;
                model_->set_stock_fields(sym, notes, s.group_name);
            }
        }
    });
    connect(detail_, &WatchlistDetailPane::open_requested, this, &WatchlistScreen::open_symbol_in);
    connect(detail_, &WatchlistDetailPane::close_requested, this, [this]() { set_detail_visible(false); });
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_sidebar() {
    sidebar_ = new QWidget(splitter_);
    sidebar_->setStyleSheet(wls_sidebar_css());
    sidebar_->setMinimumWidth(150);

    auto* lay = new QVBoxLayout(sidebar_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    sidebar_header_ = new QWidget(sidebar_);
    sidebar_header_->setStyleSheet(wls_raised_bar_css());
    sidebar_header_->setMinimumHeight(34);
    auto* hl = new QHBoxLayout(sidebar_header_);
    hl->setContentsMargins(12, 0, 8, 0);
    hl->setSpacing(6);
    sidebar_title_ = new QLabel(tr("WATCHLISTS"), sidebar_header_);
    hl->addWidget(sidebar_title_);
    hl->addStretch();
    add_wl_btn_ = new QPushButton(QStringLiteral("+"), sidebar_header_);
    add_wl_btn_->setToolTip(tr("New watchlist"));
    connect(add_wl_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_add_watchlist);
    hl->addWidget(add_wl_btn_);
    lay->addWidget(sidebar_header_);

    wl_list_ = new QListWidget(sidebar_);
    wl_list_->setItemDelegate(new WatchlistSidebarDelegate(wl_list_));
    wl_list_->setDragDropMode(QAbstractItemView::InternalMove);
    wl_list_->setDefaultDropAction(Qt::MoveAction);
    wl_list_->setToolTip(tr("Drag to reorder · double-click to rename · right-click for more"));
    connect(wl_list_, &QListWidget::currentRowChanged, this, &WatchlistScreen::on_watchlist_selected);
    connect(wl_list_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { rename_watchlist(wl_list_->currentRow()); });
    wl_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(wl_list_, &QListWidget::customContextMenuRequested, this, &WatchlistScreen::show_watchlist_menu);
    connect(wl_list_->model(), &QAbstractItemModel::rowsMoved, this,
            [this]() { QTimer::singleShot(0, this, &WatchlistScreen::persist_sidebar_order); });
    lay->addWidget(wl_list_, 1);

    wl_count_ = new QLabel(sidebar_);
    wl_count_->setMinimumHeight(26);
    wl_count_->setAlignment(Qt::AlignCenter);
    lay->addWidget(wl_count_);
    return sidebar_;
}

// ── Main panel ───────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_top_bar() {
    top_bar_ = new QWidget(main_panel_);
    top_bar_->setStyleSheet(wls_raised_bar_css());
    top_bar_->setMinimumHeight(36);
    auto* tl = new QHBoxLayout(top_bar_);
    tl->setContentsMargins(14, 0, 10, 0);
    tl->setSpacing(8);

    panel_title_ = new QLabel(top_bar_);
    tl->addWidget(panel_title_);
    stock_count_ = new QLabel(top_bar_);
    tl->addWidget(stock_count_);
    updated_label_ = new QLabel(top_bar_);
    tl->addWidget(updated_label_);
    tl->addStretch();

    const auto view_btn = [this](const QString& view) {
        auto* b = new QPushButton(top_bar_);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, view]() { set_view_mode(view); });
        return b;
    };
    view_table_btn_ = view_btn(QStringLiteral("table"));
    view_map_btn_ = view_btn(QStringLiteral("map"));
    view_corr_btn_ = view_btn(QStringLiteral("correlation"));
    tl->addWidget(view_table_btn_);
    tl->addWidget(view_map_btn_);
    tl->addWidget(view_corr_btn_);
    tl->addSpacing(6);

    detail_btn_ = new QPushButton(top_bar_);
    detail_btn_->setCheckable(true);
    detail_btn_->setCursor(Qt::PointingHandCursor);
    connect(detail_btn_, &QPushButton::toggled, this, [this](bool on) { set_detail_visible(on); });
    tl->addWidget(detail_btn_);

    refresh_btn_ = new QPushButton(top_bar_);
    connect(refresh_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_refresh);
    tl->addWidget(refresh_btn_);

    list_menu_btn_ = new QPushButton(top_bar_);
    connect(list_menu_btn_, &QPushButton::clicked, this, &WatchlistScreen::show_list_actions_menu);
    tl->addWidget(list_menu_btn_);
    return top_bar_;
}

QWidget* WatchlistScreen::build_add_bar() {
    add_bar_ = new QWidget(main_panel_);
    add_bar_->setStyleSheet(wls_add_bar_css());
    add_bar_->setMinimumHeight(36);
    auto* al = new QHBoxLayout(add_bar_);
    al->setContentsMargins(14, 0, 10, 0);
    al->setSpacing(6);

    add_label_ = new QLabel(add_bar_);
    al->addWidget(add_label_);
    add_input_ = new QLineEdit(add_bar_);
    add_input_->setMinimumHeight(26);
    add_input_->setClearButtonEnabled(true);
    al->addWidget(add_input_, 2);
    connect(add_input_, &QLineEdit::returnPressed, this, [this]() {
        if (QAbstractItemView* popup = completer_->popup(); popup && popup->isVisible()) {
            // A row the user moved to (arrow keys, hover) is added by the completer itself
            // right after this (on_completion_chosen). With no row picked, Enter adds what
            // was typed — before, it only closed the popup and nothing was added.
            if (popup->currentIndex().isValid())
                return;
            popup->hide();
        }
        on_add_stock();
    });
    connect(add_input_, &QLineEdit::textEdited, this, &WatchlistScreen::on_add_text_edited);

    completion_model_ = new QStringListModel(this);
    completer_ = new QCompleter(completion_model_, this);
    completer_->setWidget(add_input_);
    completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setMaxVisibleItems(10);
    connect(completer_, qOverload<const QString&>(&QCompleter::activated), this,
            &WatchlistScreen::on_completion_chosen);
    // ADD clicked while the popup is open: QCompleter would swallow the click just to close
    // the popup, so the first click on ADD did nothing. Treat it as the click it was.
    // (popup() creates the popup and QCompleter's own filter first; this one runs before it.)
    completer_->popup()->installEventFilter(new WlsPopupPressFilter(this, [this](const QPoint& global) {
        QAbstractItemView* popup = completer_->popup();
        if (!popup->isVisible() || popup->rect().contains(popup->mapFromGlobal(global)))
            return false; // a press on a row is the completer's
        if (!add_btn_ || !add_btn_->isVisible() || !add_btn_->isEnabled() ||
            !add_btn_->rect().contains(add_btn_->mapFromGlobal(global)))
            return false;
        popup->hide();
        QTimer::singleShot(0, this, &WatchlistScreen::on_add_stock); // empty box by then (replayed click): no-op
        return true;
    }));

    add_btn_ = new QPushButton(add_bar_);
    connect(add_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_add_stock);
    al->addWidget(add_btn_);
    remove_btn_ = new QPushButton(add_bar_);
    remove_btn_->setProperty("fincept_agent_risk", "delete");
    connect(remove_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_remove_stock);
    al->addWidget(remove_btn_);
    al->addSpacing(12);

    filter_input_ = new QLineEdit(add_bar_);
    filter_input_->setClearButtonEnabled(true);
    filter_input_->setMinimumHeight(26);
    connect(filter_input_, &QLineEdit::textChanged, this, [this](const QString& t) {
        proxy_->set_filter_text(t);
        if (!t.trimmed().isEmpty())
            view_->expandAll();
        update_empty_state();
        schedule_footer();
        schedule_views();
        history_timer_->start();
    });
    al->addWidget(filter_input_, 1);

    group_label_ = new QLabel(add_bar_);
    al->addWidget(group_label_);
    group_box_ = new QComboBox(add_bar_);
    for (WatchlistGrouping g : watchlist_groupings())
        group_box_->addItem(watchlist_grouping_label(g), watchlist_grouping_key(g));
    connect(group_box_, &QComboBox::currentIndexChanged, this, [this]() {
        if (applying_layout_)
            return;
        layout_.grouping = group_box_->currentData().toString();
        layout_.collapsed.clear();
        model_->set_grouping(watchlist_grouping_from_key(layout_.grouping));
        view_->setRootIsDecorated(model_->grouped());
        update_data_needs();
        layout_edited();
    });
    al->addWidget(group_box_);

    ccy_label_ = new QLabel(add_bar_);
    al->addWidget(ccy_label_);
    ccy_box_ = new QComboBox(add_bar_);
    // First "Automatic" (data "": the display currency from Settings, followed; text set by
    // wls_sync_auto_ccy_item), then a curated list of common conversion targets (deliberate: the
    // full table would bury them); a saved pick outside the list is added when restored.
    ccy_box_->addItem(QString(), QString());
    for (const char* c : {"USD", "EUR", "GBP", "JPY", "CHF", "CNY", "HKD", "INR", "CAD", "AUD", "SGD", "KRW", "BRL",
                          "ZAR", "SEK", "NOK", "MXN", "AED", "SAR", "TWD"})
        ccy_box_->addItem(QString::fromLatin1(c), QString::fromLatin1(c));
    connect(ccy_box_, &QComboBox::currentIndexChanged, this, [this]() {
        const QString picked = ccy_box_->currentData().toString();
        if (picked.isEmpty()) {
            // Automatic: forget the saved pick and follow the display currency from Settings again.
            ccy_box_->setProperty(kWlsCcyFollowsProp, true);
            (void)SettingsRepository::instance().set(QString::fromLatin1(wl_internal::kDisplayCcyKey), QString(),
                                                     QStringLiteral("watchlist"));
            const QString ccy = formatting::FormatSettings::instance().display_currency();
            if (ccy != model_->display_currency()) {
                model_->set_display_currency(ccy);
                sync_fx();
                schedule_footer();
            }
            return;
        }
        ccy_box_->setProperty(kWlsCcyFollowsProp, false); // picked here: saved, no longer follows Settings
        set_display_currency(picked);
    });
    al->addWidget(ccy_box_);
    return add_bar_;
}

QWidget* WatchlistScreen::build_feedback_bar() {
    feedback_bar_ = new QWidget(main_panel_);
    auto* fl = new QHBoxLayout(feedback_bar_);
    fl->setContentsMargins(14, 4, 8, 4);
    fl->setSpacing(6);
    feedback_label_ = new QLabel(feedback_bar_);
    feedback_label_->setWordWrap(true);
    feedback_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fl->addWidget(feedback_label_, 1);
    feedback_actions_ = new QHBoxLayout;
    feedback_actions_->setSpacing(4);
    fl->addLayout(feedback_actions_);
    feedback_close_ = new QPushButton(QStringLiteral("×"), feedback_bar_);
    feedback_close_->setCursor(Qt::PointingHandCursor);
    connect(feedback_close_, &QPushButton::clicked, this, &WatchlistScreen::clear_feedback);
    fl->addWidget(feedback_close_);
    feedback_bar_->setVisible(false);
    return feedback_bar_;
}

QWidget* WatchlistScreen::build_main_panel() {
    main_panel_ = new QWidget(splitter_);
    main_panel_->setStyleSheet(wls_root_css());
    auto* lay = new QVBoxLayout(main_panel_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    lay->addWidget(build_top_bar());
    lay->addWidget(build_add_bar());
    lay->addWidget(build_feedback_bar());

    // Quote-failure banner: when every symbol's quote is failing (Python not set
    // up, offline, rate limited) the shared reason goes here once.
    status_banner_ = new QLabel(main_panel_);
    status_banner_->setWordWrap(true);
    status_banner_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status_banner_->setVisible(false);
    lay->addWidget(status_banner_);

    view_stack_ = new QStackedWidget(main_panel_);

    // Table page: table + aggregate footer, or the empty-state guidance.
    table_page_ = new QWidget(view_stack_);
    auto* tl = new QVBoxLayout(table_page_);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);
    view_ = new WatchlistView(table_page_);
    view_->set_models(model_, proxy_);
    view_->setAccessibleName(tr("Watchlist quotes"));
    tl->addWidget(view_, 1);
    footer_ = new WatchlistFooter(view_, table_page_);
    tl->addWidget(footer_);
    // The empty state (no list / empty list / filter matching nothing) is a
    // ui::EmptyState added below the table on first need (update_empty_state).
    view_stack_->addWidget(table_page_);

    map_view_ = new WatchlistMapView(view_stack_);
    view_stack_->addWidget(map_view_);
    corr_view_ = new WatchlistCorrelationView(view_stack_);
    view_stack_->addWidget(corr_view_);
    lay->addWidget(view_stack_, 1);

    // ── Table wiring ────────────────────────────────────────────────────────
    // Drops are applied on the next event-loop turn: they reload the list (a model
    // reset), which must not happen inside QAbstractItemView::dropEvent.
    model_->set_move_handler([this](const QStringList& moved, const QString& anchor, bool after, const QString& gkey,
                                    bool has_group) {
        QTimer::singleShot(0, this, [this, moved, anchor, after, gkey, has_group]() {
            move_rows(moved, anchor, after, gkey, has_group);
        });
    });
    model_->set_external_drop_handler([this](const QList<SymbolRef>& refs) {
        QTimer::singleShot(0, this, [this, refs]() { add_symbols_direct(refs); });
    });

    connect(view_, &WatchlistView::layout_edited, this, &WatchlistScreen::layout_edited);
    // A sort by a performance column needs every row's history.
    connect(view_->header(), &QHeaderView::sortIndicatorChanged, this, [this](int, Qt::SortOrder) {
        if (!applying_layout_)
            update_data_needs();
    });
    connect(view_, &WatchlistView::visible_columns_changed, this, [this]() {
        if (applying_layout_)
            return;
        update_data_needs();
        schedule_footer();
    });
    connect(view_, &WatchlistView::expansion_edited, this, &WatchlistScreen::layout_edited);
    connect(view_, &WatchlistView::viewport_rows_changed, this, [this]() { history_timer_->start(); });
    // A click changes the current index BEFORE it updates the selection, and
    // current_symbol() only trusts a selected current row: handled at once, the
    // detail pane showed the previously clicked row (or none). Settle both first.
    const auto schedule_current = [this]() {
        if (current_row_pending_)
            return;
        current_row_pending_ = true;
        QTimer::singleShot(0, this, [this]() {
            current_row_pending_ = false;
            on_current_row_changed();
        });
    };
    connect(view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [schedule_current](const QModelIndex&, const QModelIndex&) { schedule_current(); });
    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this, schedule_current]() {
        update_list_actions();
        schedule_current();
    });
    connect(view_, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QString sym = idx.data(WatchlistModel::SymbolRole).toString();
        if (!sym.isEmpty())
            open_symbol_in(QStringLiteral("equity_research"), sym);
    });
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QWidget::customContextMenuRequested, this, &WatchlistScreen::show_stock_menu);

    // Selection, expansion and footer survive model resets (regroup, reload).
    connect(proxy_, &QAbstractItemModel::modelAboutToBeReset, this, &WatchlistScreen::save_selection);
    connect(proxy_, &QAbstractItemModel::modelReset, this, [this]() {
        expansion_restore_pending_ = true;
        QTimer::singleShot(0, this, [this]() {
            expansion_restore_pending_ = false;
            view_->setRootIsDecorated(model_->grouped());
            view_->restore_expansion(layout_.collapsed);
            restore_selection();
            history_timer_->start();
            schedule_footer();
            schedule_views();
            update_empty_state();
            // Nothing selected after the reload (first open, list switch, removal):
            // show the first row in the detail pane. One more turn later, so rows
            // revealed by an add (reveal_added, queued after this) win.
            QTimer::singleShot(0, this, &WatchlistScreen::select_first_row_if_none);
        });
    });
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, [this]() { history_timer_->start(); });
    connect(model_, &WatchlistModel::row_data_changed, this, [this](const QString& sym) {
        schedule_footer();
        schedule_views();
        if (detail_->symbol() == sym) {
            if (const auto* r = model_->state(sym))
                detail_->update_row(*r);
        }
    });

    // ── Analytics views ─────────────────────────────────────────────────────
    const auto select_symbol = [this](const QString& sym) {
        view_->select_symbols({sym}, sym);
        on_current_row_changed();
    };
    connect(map_view_, &WatchlistMapView::symbol_clicked, this, select_symbol);
    connect(map_view_, &WatchlistMapView::symbol_activated, this,
            [this](const QString& sym) { open_symbol_in(QStringLiteral("equity_research"), sym); });
    connect(map_view_, &WatchlistMapView::mode_changed, this, [this](const QString& size, const QString& horizon) {
        layout_.map_size = size;
        layout_.map_horizon = horizon;
        layout_edited();
        update_data_needs();
        refresh_map_view();
    });
    connect(corr_view_, &WatchlistCorrelationView::symbol_pair_clicked, this,
            [select_symbol](const QString& a, const QString&) { select_symbol(a); });

    // Drop: a symbol dropped anywhere on the panel (toolbar, empty area) is
    // added to the current list. Drops on the table itself go through the
    // model (reorder, or add when it comes from elsewhere).
    symbol_dnd::installDropFilter(main_panel_, [this](const SymbolRef& ref, SymbolGroup) {
        if (!current_wl_id_.isEmpty() && ref.is_valid())
            add_symbols_direct({ref});
    });
    return main_panel_;
}

void WatchlistScreen::install_shortcuts() {
    const auto bind = [this](const QKeySequence& ks, QWidget* scope, Qt::ShortcutContext ctx, auto fn) {
        auto* sc = new QShortcut(ks, scope);
        sc->setContext(ctx);
        connect(sc, &QShortcut::activated, this, fn);
        return sc;
    };
    const auto focus_add = [this]() {
        add_input_->setFocus(Qt::ShortcutFocusReason);
        add_input_->selectAll();
    };
    bind(QKeySequence(Qt::CTRL | Qt::Key_F), this, Qt::WidgetWithChildrenShortcut, [this]() {
        if (view_stack_->currentWidget() != table_page_)
            set_view_mode(QStringLiteral("table"));
        filter_input_->setFocus(Qt::ShortcutFocusReason);
        filter_input_->selectAll();
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_N), this, Qt::WidgetWithChildrenShortcut, focus_add);
    bind(QKeySequence(Qt::Key_Insert), this, Qt::WidgetWithChildrenShortcut, focus_add);
    bind(QKeySequence(Qt::CTRL | Qt::Key_R), this, Qt::WidgetWithChildrenShortcut, [this]() { on_refresh(); });
    bind(QKeySequence(Qt::CTRL | Qt::Key_E), this, Qt::WidgetWithChildrenShortcut, [this]() { on_export_csv(); });
    bind(QKeySequence(Qt::CTRL | Qt::Key_I), this, Qt::WidgetWithChildrenShortcut, [this]() { on_import_csv(); });
    // Del removes the selected rows, as REMOVE does (agents operating the UI judge the shortcut).
    bind(QKeySequence::Delete, view_, Qt::WidgetShortcut, [this]() { on_remove_stock(); })
        ->setProperty(fincept::core::agent::kAgentRiskProperty, QStringLiteral("delete"));
    bind(QKeySequence(Qt::CTRL | Qt::Key_Up), view_, Qt::WidgetShortcut, [this]() { move_selection(-1); });
    bind(QKeySequence(Qt::CTRL | Qt::Key_Down), view_, Qt::WidgetShortcut, [this]() { move_selection(1); });
    bind(QKeySequence(Qt::Key_Return), view_, Qt::WidgetShortcut, [this]() {
        const QString sym = view_->current_symbol();
        if (!sym.isEmpty())
            open_symbol_in(QStringLiteral("equity_research"), sym);
    });
    bind(QKeySequence(Qt::Key_Escape), filter_input_, Qt::WidgetShortcut, [this]() {
        filter_input_->clear();
        view_->setFocus();
    });
}

// ── Theme ────────────────────────────────────────────────────────────────────

void WatchlistScreen::refresh_theme() {
    // wls_set_css(): only a stylesheet whose text changed (a theme / font switch) is
    // applied — at construction the containers already carry theirs.
    wls_set_css(this, wls_root_css());
    wls_set_css(splitter_, wls_splitter_css());

    wls_set_css(sidebar_, wls_sidebar_css());
    wls_set_css(sidebar_header_, wls_raised_bar_css());
    wls_set_css(sidebar_title_, wls_caption_style(colors::AMBER()));
    // Square icon button: the shared accent style's 12 px side padding would leave no room for the "+".
    wls_set_css(add_wl_btn_, wls_accent_btn_style() +
                                 QStringLiteral("QPushButton { padding:0; font-size:%1px; }").arg(fonts::px(1)));
    const int side = std::max(24, fonts::font_px(-2) + 12);
    add_wl_btn_->setFixedSize(side, side);
    wls_set_css(wl_list_, QStringLiteral("QListWidget { background:%1; border:none; font-size:%2px; outline:0; }"
                                         "QListWidget::item { border-bottom:1px solid %3; }"
                                         "QListWidget::item:selected { background:%4; }"
                                         "QListWidget::item:hover { background:%4; }")
                              .arg(colors::BG_SURFACE())
                              .arg(fonts::px(-1))
                              .arg(colors::BORDER_DIM(), colors::BG_HOVER()));
    wls_set_css(wl_count_, QStringLiteral("background:%1; color:%2; font-size:%3px; border-top:1px solid %4;")
                               .arg(colors::BG_SURFACE(), colors::TEXT_TERTIARY())
                               .arg(fonts::px(-2))
                               .arg(colors::BORDER_DIM()));

    wls_set_css(main_panel_, wls_root_css());
    wls_set_css(top_bar_, wls_raised_bar_css());
    wls_set_css(panel_title_, QStringLiteral("color:%1; font-size:%2px; font-weight:700; background:transparent;")
                                  .arg(colors::TEXT_PRIMARY())
                                  .arg(fonts::px(0)));
    wls_set_css(stock_count_, QStringLiteral("color:%1; font-size:%2px; background:transparent;")
                                  .arg(colors::TEXT_SECONDARY())
                                  .arg(fonts::px(-2)));
    for (QPushButton* b : {view_table_btn_, view_map_btn_, view_corr_btn_, detail_btn_, refresh_btn_, list_menu_btn_})
        wls_set_css(b, wls_std_btn_style());

    wls_set_css(add_bar_, wls_add_bar_css());
    for (QLabel* l : {add_label_, group_label_, ccy_label_})
        wls_set_css(l, wls_caption_style(colors::TEXT_SECONDARY()));
    wls_set_css(add_input_, wls_input_style());
    wls_set_css(filter_input_, wls_input_style());
    wls_set_css(add_btn_, wls_accent_btn_style());
    wls_set_css(remove_btn_, wls_danger_btn_style());
    wls_set_css(group_box_, wls_combo_style());
    wls_set_css(ccy_box_, wls_combo_style());
    wls_set_css(feedback_close_, QStringLiteral("QPushButton { background:transparent; color:%1; border:none; "
                                                "font-size:%2px; padding:0 6px; } QPushButton:hover { color:%3; }")
                                     .arg(colors::TEXT_TERTIARY())
                                     .arg(fonts::px(1))
                                     .arg(colors::TEXT_PRIMARY()));
    // (The empty state is a ui::EmptyState: it follows the theme by itself.)

    // Feedback bar colours follow its tone (stored on the label); its action buttons are restyled too.
    wls_style_feedback(feedback_bar_, feedback_label_, feedback_label_->property("tone").toInt());
    for (QPushButton* b : feedback_bar_->findChildren<QPushButton*>()) {
        if (b != feedback_close_)
            wls_set_css(b, wls_std_btn_style());
    }

    status_banner_css_.clear();
    update_quote_status();
    update_last_updated();
    style_view_buttons();
    rebuild_sidebar();
    model_->refresh_all();
    footer_->update();
    detail_->refresh_theme();
    map_view_->refresh_theme();
    corr_view_->refresh_theme();
}

void WatchlistScreen::style_view_buttons() {
    const QString view = layout_.view;
    view_table_btn_->setChecked(view == QLatin1String("table"));
    view_map_btn_->setChecked(view == QLatin1String("map"));
    view_corr_btn_->setChecked(view == QLatin1String("correlation"));
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void WatchlistScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Lists change while hidden (MCP / AI chat tools, Equity Trading's watchlist
    // panel, cloud sync), and the event subscriptions below only exist while
    // visible: re-read on every show. This also (re)subscribes the quotes.
    load_watchlists();
    subscribe_mcp_events();
    clock_timer_->start();
    fincept::services::cloud::CloudSyncEngine::instance().request_pull(QStringLiteral("watchlist"));
}

void WatchlistScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    hub_unsubscribe_all();
    unsubscribe_mcp_events();
    clock_timer_->stop();
    footer_timer_->stop();
    views_timer_->stop();
    history_timer_->stop();
    if (layout_save_timer_->isActive())
        save_layout_now();
}

void WatchlistScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void WatchlistScreen::retranslateUi() {
    sidebar_title_->setText(tr("WATCHLISTS"));
    wl_count_->setText(tr("%n list(s)", "", static_cast<int>(watchlists_.size())));
    add_wl_btn_->setToolTip(tr("New watchlist"));
    wl_list_->setToolTip(tr("Drag to reorder · double-click to rename · right-click for more"));
    if (current_wl_id_.isEmpty())
        panel_title_->setText(tr("Select a watchlist"));
    else
        panel_title_->setText(current_list_name().toUpper()); // user data, not translated

    view_table_btn_->setText(tr("TABLE"));
    view_table_btn_->setToolTip(tr("Quote table"));
    view_map_btn_->setText(tr("MAP"));
    view_map_btn_->setToolTip(tr("Market map: size by market cap, colour by performance"));
    view_corr_btn_->setText(tr("CORRELATION"));
    view_corr_btn_->setToolTip(tr("Correlation matrix of daily returns"));
    detail_btn_->setText(tr("DETAIL"));
    detail_btn_->setToolTip(tr("Show the chart, key figures and headlines of the selected row"));
    refresh_btn_->setText(tr("REFRESH"));
    refresh_btn_->setToolTip(tr("Refresh quotes (F5 / Ctrl+R)"));
    list_menu_btn_->setText(tr("LIST ▾"));
    list_menu_btn_->setToolTip(tr("Import, export, duplicate, merge, backtest or delete this list"));

    add_label_->setText(tr("ADD:"));
    add_input_->setPlaceholderText(tr("Tickers, e.g. AAPL, RELIANCE, VOD.L, EURUSD=X — or type a name and pick a match (Ctrl+N)"));
    add_btn_->setText(tr("ADD"));
    remove_btn_->setText(tr("REMOVE"));
    remove_btn_->setToolTip(tr("Remove the selected rows (Delete); undo from the message bar"));
    filter_input_->setPlaceholderText(tr("Filter (Ctrl+F)"));
    filter_input_->setToolTip(tr("Matches symbol, name, notes, group, sector, exchange"));
    group_label_->setText(tr("GROUP"));
    for (int i = 0; i < group_box_->count(); ++i)
        group_box_->setItemText(i, watchlist_grouping_label(watchlist_grouping_from_key(group_box_->itemData(i).toString())));
    ccy_label_->setText(tr("CCY"));
    ccy_box_->setToolTip(tr("Display currency for the converted price column and totals"));
    wls_sync_auto_ccy_item(ccy_box_);
    feedback_close_->setToolTip(tr("Dismiss"));
    view_->setAccessibleName(tr("Watchlist quotes"));

    update_quote_status();
    update_last_updated();
    update_empty_state();
    model_->refresh_all();
    schedule_footer();
}

void WatchlistScreen::update_empty_state() {
    const bool no_list = current_wl_id_.isEmpty();
    const bool empty = stocks_.isEmpty();
    const bool filtered_out = !empty && proxy_->rowCount() == 0;
    const bool show = no_list || empty || filtered_out;
    view_->setVisible(!show);
    footer_->setVisible(!show);
    sync_detail_visibility();
    if (!show) {
        empty_kind_ = EmptyKind::None;
        if (empty_state_)
            empty_state_->hide();
        return;
    }
    // Not built while the screen has never been shown (the texts pass at construction):
    // the first show reloads the lists and comes back here.
    if (!empty_state_ && !isVisible())
        return;
    if (!empty_state_) {
        empty_state_ = new ui::EmptyState(table_page_);
        if (auto* tl = qobject_cast<QVBoxLayout*>(table_page_->layout()))
            tl->addWidget(empty_state_, 1);
        connect(empty_state_, &ui::EmptyState::primary_clicked, this, [this]() {
            switch (empty_kind_) {
                case EmptyKind::NoList:
                    on_add_watchlist(); // the + button's dialog
                    break;
                case EmptyKind::EmptyList:
                    add_region_large_caps();
                    break;
                case EmptyKind::FilteredOut:
                    filter_input_->clear(); // textChanged updates the view
                    break;
                case EmptyKind::None:
                    break;
            }
        });
        connect(empty_state_, &ui::EmptyState::secondary_clicked, this, [this]() {
            if (empty_kind_ == EmptyKind::EmptyList)
                on_import_csv(); // LIST ▾ › Import symbols
        });
    }

    if (no_list) {
        empty_kind_ = EmptyKind::NoList;
        empty_state_->apply(ui::EmptyState::no_data_spec(
            tr("No watchlist selected"), tr("Select a watchlist on the left, or create a new one."), tr("NEW WATCHLIST")));
    } else if (empty) {
        empty_kind_ = EmptyKind::EmptyList;
        const auto& profile = core::market::MarketProfileStore::instance().current();
        const QStringList caps = wls_region_large_caps(profile);
        ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
            tr("This watchlist is empty"),
            tr("Add the symbols you want to follow — live quotes, a chart and headlines for each appear here."),
            caps.isEmpty() ? QString()
                           : tr("ADD %1 LARGE CAPS").arg(core::market::market_profile_name(profile).toUpper()));
        spec.secondary = tr("IMPORT…");
        spec.details_heading = tr("WAYS TO ADD SYMBOLS");
        spec.details = {
            {tr("ADD box"), tr("type tickers or company names above, comma-separated (Ctrl+N)")},
            {tr("Drag"), tr("drop a symbol here from any other panel")},
            {tr("Import"), tr("LIST ▾ › Import symbols (CSV / JSON), Ctrl+I")},
        };
        if (!caps.isEmpty()) {
            // Say exactly what the button adds before it is pressed.
            const QString shown = caps.size() > 5 ? caps.mid(0, 5).join(QStringLiteral(", ")) + QStringLiteral(" …")
                                                  : caps.join(QStringLiteral(", "));
            spec.note = tr("Adds %n symbol(s): %1", "", int(caps.size())).arg(shown);
            spec.note_tone = ui::EmptyState::Tone::Info;
        }
        spec.illustration = true;
        spec.illustration_shape = ui::LoadingSkeleton::Shape::Table;
        empty_state_->apply(spec);
    } else {
        empty_kind_ = EmptyKind::FilteredOut;
        empty_state_->apply(ui::EmptyState::no_data_spec(
            tr("No symbol matches \"%1\"").arg(filter_input_->text().trimmed()),
            tr("The filter matches symbol, name, notes, group, sector and exchange."), tr("CLEAR FILTER")));
    }
    empty_state_->show();
}

void WatchlistScreen::sync_detail_visibility() {
    const bool want = layout_.detail_visible && !current_wl_id_.isEmpty() && !stocks_.isEmpty();
    if (detail_->isHidden() == !want)
        return;
    detail_->setVisible(want);
    if (!want)
        return;
    const QString sym = view_->current_symbol();
    detail_->set_row(sym.isEmpty() ? nullptr : model_->state(sym));
    if (!sym.isEmpty()) {
        ensure_info({sym});
        ensure_history({sym});
    }
}

void WatchlistScreen::add_region_large_caps() {
    if (current_wl_id_.isEmpty())
        return;
    QList<SymbolRef> refs;
    for (const QString& sym : wls_region_large_caps(core::market::MarketProfileStore::instance().current())) {
        SymbolRef ref;
        ref.symbol = sym;
        ref.asset_class = watchlist_asset_class(sym);
        refs << ref;
    }
    add_symbols_direct(refs); // the drop / drag add path: repository, reload, reveal, feedback
}

void WatchlistScreen::update_list_actions() {
    const bool has_list = !current_wl_id_.isEmpty();
    add_btn_->setEnabled(has_list);
    add_input_->setEnabled(has_list);
    list_menu_btn_->setEnabled(has_list);
    remove_btn_->setEnabled(has_list && !view_->selected_symbols().isEmpty());
}

void WatchlistScreen::show_feedback(const QString& text, FeedbackTone tone, int ms, const QString& details) {
    // Drop any previous action buttons.
    while (QLayoutItem* it = feedback_actions_->takeAt(0)) {
        if (QWidget* w = it->widget())
            w->deleteLater();
        delete it;
    }
    if (text.isEmpty()) {
        clear_feedback();
        return;
    }
    wls_style_feedback(feedback_bar_, feedback_label_, static_cast<int>(tone));
    feedback_label_->setProperty("tone", static_cast<int>(tone));
    feedback_label_->setProperty("suggestion", false);
    feedback_label_->setText(text);
    feedback_label_->setToolTip(details);
    feedback_bar_->setVisible(true);
    if (ms > 0)
        feedback_timer_->start(ms);
    else
        feedback_timer_->stop();
}

void WatchlistScreen::clear_feedback() {
    feedback_timer_->stop();
    while (QLayoutItem* it = feedback_actions_->takeAt(0)) {
        if (QWidget* w = it->widget())
            w->deleteLater();
        delete it;
    }
    feedback_bar_->setVisible(false);
    // Dismissing a "did you mean" skips that symbol; the next one (if any) follows.
    if (feedback_label_->property("suggestion").toBool() && !suggestions_.isEmpty())
        suggestions_.removeFirst();
    feedback_label_->setProperty("suggestion", false);
    if (!suggestions_.isEmpty())
        QTimer::singleShot(0, this, [this]() { show_next_suggestion(); });
}

void WatchlistScreen::set_view_mode(const QString& view, bool user) {
    QWidget* page = table_page_;
    QString v = view;
    if (view == QLatin1String("map"))
        page = map_view_;
    else if (view == QLatin1String("correlation"))
        page = corr_view_;
    else
        v = QStringLiteral("table");
    view_stack_->setCurrentWidget(page);
    layout_.view = v;
    style_view_buttons();
    update_data_needs();
    if (page == map_view_)
        refresh_map_view();
    else if (page == corr_view_)
        refresh_correlation_view();
    if (user)
        layout_edited();
}

void WatchlistScreen::set_detail_visible(bool on, bool user) {
    // An empty list (or none) keeps the pane hidden behind the list's own empty
    // state (sync_detail_visibility shows it again with the first row).
    const bool show = on && !current_wl_id_.isEmpty() && !stocks_.isEmpty();
    detail_->setVisible(show);
    {
        QSignalBlocker block(detail_btn_);
        detail_btn_->setChecked(on);
    }
    layout_.detail_visible = on;
    if (show) {
        const QString sym = view_->current_symbol();
        detail_->set_row(sym.isEmpty() ? nullptr : model_->state(sym));
        if (!sym.isEmpty()) {
            ensure_info({sym});
            ensure_history({sym});
        }
    }
    if (user)
        layout_edited();
}

// ── MCP-driven UI sync ───────────────────────────────────────────────────────
// MCP watchlist tools publish watchlist.created / .deleted / .updated when the
// LLM mutates watchlists via AI Chat or Finagent. Always reload from the DB —
// the events carry only id / symbol.

void WatchlistScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty())
        return; // idempotent
    QPointer<WatchlistScreen> self = this;
    auto on_watchlists_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (self)
                    self->load_watchlists(); // keeps the current list selected and reloads it
            },
            Qt::QueuedConnection);
    };
    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe(this, "watchlist.created", on_watchlists_changed));
    mcp_event_subs_.append(bus.subscribe(this, "watchlist.deleted", on_watchlists_changed));
    mcp_event_subs_.append(bus.subscribe(this, "watchlist.updated", on_watchlists_changed));
}

void WatchlistScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap WatchlistScreen::save_state() const {
    return {
        {"watchlist_id", current_wl_id_},
        {"splitter", QString::fromLatin1(splitter_->saveState().toBase64())},
    };
}

void WatchlistScreen::restore_state(const QVariantMap& state) {
    const QByteArray split = QByteArray::fromBase64(state.value("splitter").toString().toLatin1());
    if (!split.isEmpty())
        splitter_->restoreState(split);
    const QString wl_id = state.value("watchlist_id").toString();
    if (wl_id.isEmpty())
        return;
    const int i = row_of_list(wl_id);
    if (i < 0)
        return; // deleted meanwhile: keep the default selection
    // setCurrentRow() runs on_watchlist_selected() through currentRowChanged; call
    // it directly only when the row is already current (no signal then).
    if (wl_list_->currentRow() == i)
        on_watchlist_selected(i);
    else
        wl_list_->setCurrentRow(i);
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

void WatchlistScreen::set_group(SymbolGroup g) {
    link_group_ = g;
    model_->set_link_group(g);
}

void WatchlistScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    // Match case-insensitively against the listed symbols; no-op when not listed.
    for (const QString& s : model_->symbols()) {
        if (s.compare(ref.symbol, Qt::CaseInsensitive) != 0)
            continue;
        view_->select_symbols({s}, s); // signals blocked: not re-published to the group
        const QModelIndex idx = proxy_->mapFromSource(model_->index_of_symbol(s, 0));
        if (idx.isValid())
            view_->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        if (detail_->isVisible())
            detail_->set_row(model_->state(s));
        return;
    }
}

SymbolRef WatchlistScreen::current_symbol() const {
    const QString sym = view_ ? view_->current_symbol() : QString();
    if (sym.isEmpty())
        return {};
    if (const auto* r = model_->state(sym))
        return r->symbol_ref(); // the right asset class (index, fx, crypto, future, equity)
    SymbolRef ref;
    ref.symbol = sym;
    ref.asset_class = watchlist_asset_class(sym);
    return ref;
}

void WatchlistScreen::publish_selection_to_group() {
    if (link_group_ == SymbolGroup::None)
        return;
    const SymbolRef ref = current_symbol();
    if (ref.is_valid())
        SymbolContext::instance().set_group_symbol(link_group_, ref, this);
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

bool WatchlistScreen::open_section(const QString& section, const QString& focus) {
    const QString key = section.trimmed();
    const QString lower = key.toLower();
    const QString wanted = focus.trimmed();
    // The listed symbol `sym` names (exact, then any case); "" when it is not in the list on view.
    auto listed = [this](const QString& sym) {
        const QStringList all = model_->symbols();
        if (sym.isEmpty() || all.contains(sym))
            return sym;
        for (const QString& s : all)
            if (s.compare(sym, Qt::CaseInsensitive) == 0)
                return s;
        return QString();
    };
    // focus = a listed symbol: selected as a linked symbol is (scrolled to, in the detail pane) — on
    // the next turn, after a list switch's model reset has put its own selection back.
    auto select_focus_later = [this, wanted, listed]() {
        if (wanted.isEmpty())
            return;
        QMetaObject::invokeMethod(
            this,
            [this, wanted, listed]() {
                const QString s = listed(wanted);
                if (!s.isEmpty())
                    on_group_symbol_changed(SymbolRef::equity(s));
            },
            Qt::QueuedConnection);
    };
    if (key.isEmpty()) {
        select_focus_later();
        return true;
    }
    if (lower == QLatin1String("table") || lower == QLatin1String("map") || lower == QLatin1String("correlation")) {
        set_view_mode(lower); // the view button's click
        select_focus_later();
        return true;
    }
    if (lower == QLatin1String("new")) {
        // The + button's name dialog (modal): after the link has landed; only its OK creates a list.
        QMetaObject::invokeMethod(this, [this]() { on_add_watchlist(); }, Qt::QueuedConnection);
        return true;
    }
    if (lower == QLatin1String("alerts")) {
        // The row menu's Alerts… for the focus symbol, else the current row (the first row when none).
        QMetaObject::invokeMethod(
            this,
            [this, wanted, listed]() {
                QString sym = listed(wanted);
                if (!sym.isEmpty()) {
                    on_group_symbol_changed(SymbolRef::equity(sym));
                } else {
                    select_first_row_if_none();
                    sym = view_->current_symbol();
                }
                if (sym.isEmpty())
                    add_input_->setFocus(Qt::OtherFocusReason); // an empty list: add a symbol first
                else
                    open_alerts(sym);
            },
            Qt::QueuedConnection);
        return true;
    }
    QString rest;
    if (command_rows::split_kind(key, "list", &rest)) {
        int row = row_of_list(rest);
        for (int i = 0; row < 0 && !rest.isEmpty() && i < watchlists_.size(); ++i)
            if (watchlists_[i].id.compare(rest, Qt::CaseInsensitive) == 0)
                row = i;
        if (row < 0)
            return false;
        // As restore_state: setCurrentRow() runs on_watchlist_selected() through currentRowChanged.
        if (wl_list_->currentRow() == row)
            on_watchlist_selected(row);
        else
            wl_list_->setCurrentRow(row);
        select_focus_later();
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> wl_command_rows() {
    namespace rows = command_rows;
    const QString where = WatchlistScreen::tr("Watchlist");
    const QString screen = QStringLiteral("watchlist");
    return {
        rows::section(screen, QStringLiteral("table"), WatchlistScreen::tr("Quote table"),
                      {QStringLiteral("table"), QStringLiteral("quotes"), QStringLiteral("list view"),
                       QStringLiteral("columns")},
                      where),
        rows::section(screen, QStringLiteral("map"), WatchlistScreen::tr("Market map"),
                      {QStringLiteral("map"), QStringLiteral("treemap"), QStringLiteral("heatmap"),
                       QStringLiteral("heat map")},
                      where),
        rows::section(screen, QStringLiteral("correlation"), WatchlistScreen::tr("Correlation matrix"),
                      {QStringLiteral("correlation"), QStringLiteral("correlations"), QStringLiteral("matrix"),
                       QStringLiteral("diversification")},
                      where),
    };
}

void wl_install_commands() {
    command_rows::install(QStringLiteral("watchlist"), &wl_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(wl_install_commands)

} // namespace fincept::screens
