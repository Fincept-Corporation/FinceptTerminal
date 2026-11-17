#include "screens/markets/MarketPanel.h"

#include "core/events/EventBus.h"
#include "core/instrument/VenueSymbols.h"
#include "core/symbol/SymbolGroup.h"
#include "screens/equity_research/EquityResearchLinks.h"
#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketPanelModel.h"
#include "screens/markets/MarketPanelView.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsQuoteStore.h"
#include "screens/markets/MarketsStyle.h"
#include "screens/markets/MarketsSymbolEntry.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/charts/ChartExport.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/ThemeIcon.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QTimer>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace fincept::screens {

namespace colors = fincept::ui::colors;

namespace {

/// One sheet for the panel root, set before its children exist; children are
/// styled by objectName / property selectors (no per-label setStyleSheet).
QString mp_panel_ss() {
    return QStringLiteral("#mpRoot{background:%1;border:1px solid %2;}"
                          "QLabel#mpBadge{color:%3;background:%4;padding:0px 6px;font-size:%5px;font-weight:600;}"
                          "QLabel#mpBadge[warn=\"true\"]{color:%6;}"
                          "QToolButton#mpIcon{background:transparent;border:none;padding:2px;}"
                          "QToolButton#mpIcon:hover{background:%7;}"
                          "QLabel#mpEmpty{color:%8;background:transparent;font-size:%9px;}"
                          "#mpAddRow{background:%10;border-bottom:1px solid %2;}")
        .arg(colors::BG_BASE(), colors::BORDER_DIM(), colors::TEXT_TERTIARY(), colors::BG_RAISED())
        .arg(ui::fonts::px(-4))
        .arg(colors::WARNING(), colors::BG_HOVER(), colors::TEXT_TERTIARY())
        .arg(ui::fonts::px(-2))
        .arg(colors::BG_SURFACE());
}

void mp_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// Application-wide key filter while a panel is dragged by its header: Esc cancels the
/// drag (ahead of any shortcut bound to it), every other key waits until the drop.
class MpDragKeyFilter : public QObject {
  public:
    MpDragKeyFilter(std::function<void()> on_escape, QObject* parent)
        : QObject(parent), on_escape_(std::move(on_escape)) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride)
            return QObject::eventFilter(watched, event);
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape &&
            on_escape_) {
            const std::function<void()> fn = on_escape_; // the call removes this filter
            fn();
        }
        event->accept();
        return true;
    }

  private:
    std::function<void()> on_escape_;
};

} // namespace

MarketPanel::MarketPanel(const MarketPanelConfig& config, MarketsQuoteStore* store, QWidget* parent)
    : QWidget(parent), config_(config), store_(store) {
    config_.columns = mcols::sanitize_ids(config_.columns.isEmpty() ? default_market_columns() : config_.columns);
    setObjectName(QStringLiteral("mpRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    markets_set_sheet(this, mp_panel_ss()); // before any child exists
    setMinimumWidth(ui::metrics::width(200));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    setAccessibleName(config_.title);
    build_ui();
    if (store_)
        connect(store_, &MarketsQuoteStore::coverage_changed, this, &MarketPanel::update_badge);
    update_badge();
}

void MarketPanel::build_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(1, 1, 1, 1);
    vl->setSpacing(0);

    // Header — the shared panel header (ui::PanelHeader: tick + title + controls slot).
    const ui::PanelHeaderParts hdr = ui::make_panel_header(config_.title.toUpper(), this);
    header_ = hdr.header;
    title_label_ = hdr.title_label;
    // Left-drag = move the panel, double-click = maximise, right-click = panel menu.
    header_->installEventFilter(this);
    header_->setCursor(Qt::OpenHandCursor); // the + / menu buttons keep their own cursor
    auto* slot = qobject_cast<QHBoxLayout*>(hdr.controls_slot->layout());

    badge_ = new QLabel(hdr.controls_slot);
    badge_->setObjectName(QStringLiteral("mpBadge"));
    badge_->setAlignment(Qt::AlignCenter);
    slot->addWidget(badge_);

    auto make_icon = [&](ui::ThemeGlyph glyph) {
        auto* b = new QToolButton(hdr.controls_slot);
        b->setObjectName(QStringLiteral("mpIcon"));
        b->setIcon(ui::theme_icon(glyph));
        b->setAutoRaise(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
        const int s = ui::metrics::height(20);
        b->setFixedSize(s, s);
        slot->addWidget(b);
        return b;
    };
    add_btn_ = make_icon(ui::ThemeGlyph::Plus);
    menu_btn_ = make_icon(ui::ThemeGlyph::Menu);
    connect(add_btn_, &QToolButton::clicked, this, &MarketPanel::open_add_row);
    connect(menu_btn_, &QToolButton::clicked, this,
            [this]() { show_panel_menu(menu_btn_->mapToGlobal(QPoint(0, menu_btn_->height()))); });
    vl->addWidget(header_);

    // Inline add row (increment 5): hidden until + / Insert.
    add_row_ = new QWidget(this);
    add_row_->setObjectName(QStringLiteral("mpAddRow"));
    add_row_->setAttribute(Qt::WA_StyledBackground, true);
    auto* arl = new QHBoxLayout(add_row_);
    arl->setContentsMargins(8, 2, 8, 2);
    arl->setSpacing(6);
    add_row_->setVisible(false);
    vl->addWidget(add_row_);

    // Body: the table, or the empty-panel hint.
    body_ = new QStackedWidget(this);
    model_ = new MarketPanelModel(store_, this);
    proxy_ = new MarketPanelProxy(this);
    proxy_->setSourceModel(model_);
    view_ = new MarketPanelView(body_);
    view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    model_->set_symbols(config_.symbols);
    view_->set_models(model_, proxy_);
    view_->apply_config(config_);
    body_->addWidget(view_);

    empty_label_ = new QLabel(body_);
    empty_label_->setObjectName(QStringLiteral("mpEmpty"));
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setWordWrap(true);
    body_->addWidget(empty_label_);
    vl->addWidget(body_, 1);

    connect(view_, &MarketPanelView::symbol_selected, this,
            [this](const QString& s) { emit symbol_selected(config_.id, s); });
    connect(view_, &MarketPanelView::symbol_activated, this, &MarketPanel::symbol_activated);
    connect(view_, &MarketPanelView::layout_edited, this, &MarketPanel::capture_and_emit);
    connect(view_, &MarketPanelView::context_menu_requested, this, &MarketPanel::show_row_menu);
    connect(view_, &MarketPanelView::remove_requested, this,
            [this](const QString& s) { emit remove_symbol_requested(config_.id, s); });
    connect(view_, &MarketPanelView::move_requested, this, &MarketPanel::move_symbol);
    connect(view_, &MarketPanelView::add_row_requested, this, &MarketPanel::open_add_row);
    connect(view_, &MarketPanelView::detail_toggle_requested, this, &MarketPanel::detail_toggle_requested);
    connect(view_, &MarketPanelView::maximise_requested, this, [this]() { emit maximise_requested(config_.id); });
    connect(view_, &MarketPanelView::panel_move_requested, this,
            [this](Qt::ArrowType dir) { emit move_requested(config_.id, dir); });
    connect(view_, &MarketPanelView::focus_panel_requested, this,
            [this](int delta) { emit focus_panel_requested(config_.id, delta); });
    connect(view_, &MarketPanelView::escape_pressed, this, &MarketPanel::escape_pressed);
    connect(view_, &MarketPanelView::symbol_dropped, this,
            [this](const QString& s) { emit add_symbol_requested(config_.id, s); });
    connect(view_, &MarketPanelView::viewport_rows_changed, this, &MarketPanel::viewport_rows_changed);

    retranslateUi();
    update_empty_state();
}

void MarketPanel::capture_and_emit() {
    view_->capture_config(config_);
    config_.column_order = mcols::legacy_from_ids(config_.columns);
    emit config_changed(config_);
}

void MarketPanel::update_config(const MarketPanelConfig& cfg) {
    const QString cur = view_->current_symbol();
    config_ = cfg;
    config_.columns = mcols::sanitize_ids(config_.columns.isEmpty() ? default_market_columns() : config_.columns);
    title_label_->setText(config_.title.toUpper());
    setAccessibleName(config_.title);
    model_->set_symbols(config_.symbols);
    view_->apply_config(config_);
    if (!cur.isEmpty())
        view_->select_symbol(cur, false);
    update_empty_state();
    update_badge();
}

void MarketPanel::set_columns(const QStringList& ids) {
    config_.columns = mcols::sanitize_ids(ids);
    view_->apply_config(config_);
    capture_and_emit();
}

void MarketPanel::set_maximised(bool on) {
    maximised_ = on;
    // No drag while maximised: the header shows the plain cursor then.
    if (on)
        end_header_drag(QCursor::pos(), false);
    if (header_)
        header_->setCursor(on ? Qt::ArrowCursor : Qt::OpenHandCursor);
}

void MarketPanel::set_link_group(int group) {
    const auto g = static_cast<SymbolGroup>(group);
    model_->set_link_group(g);
    view_->set_link_group(g);
    view_->viewport()->update();
}

void MarketPanel::refresh_theme() {
    // No setStyleSheet here (the central re-style pass rewrites the sheets): cells read
    // the tokens live, so a dataChanged repaint is all a theme change needs.
    model_->refresh_all();
    view_->viewport()->update();
}

void MarketPanel::refresh_metrics() {
    const int s = ui::metrics::height(20);
    for (QToolButton* b : {add_btn_, menu_btn_})
        if (b)
            b->setFixedSize(s, s);
    view_->refresh_metrics();
}

// ── Symbols ─────────────────────────────────────────────────────────────────

bool MarketPanel::insert_symbol(const QString& symbol, int at) {
    if (symbol.isEmpty() || config_.symbols.contains(symbol))
        return false;
    at = (at < 0 || at > config_.symbols.size()) ? static_cast<int>(config_.symbols.size()) : at;
    config_.symbols.insert(at, symbol);
    model_->insert_symbol(symbol, at);
    update_empty_state();
    update_badge();
    capture_and_emit();
    return true;
}

bool MarketPanel::remove_symbol(const QString& symbol) {
    if (!config_.symbols.contains(symbol))
        return false;
    config_.symbols.removeAll(symbol);
    model_->remove_symbol(symbol);
    update_empty_state();
    update_badge();
    capture_and_emit();
    return true;
}

void MarketPanel::move_symbol(const QString& symbol, int delta) {
    if (view_->is_sorted())
        return; // only the panel's own order can be edited
    const int from = static_cast<int>(config_.symbols.indexOf(symbol));
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= config_.symbols.size())
        return;
    config_.symbols.move(from, to);
    model_->move_row(from, to);
    view_->select_symbol(symbol, false);
    capture_and_emit();
}

void MarketPanel::move_symbol_to_end(const QString& symbol, bool top) {
    if (view_->is_sorted())
        return;
    const int from = static_cast<int>(config_.symbols.indexOf(symbol));
    const int to = top ? 0 : static_cast<int>(config_.symbols.size()) - 1;
    if (from < 0 || from == to)
        return;
    config_.symbols.move(from, to);
    model_->move_row(from, to);
    view_->select_symbol(symbol, true);
    capture_and_emit();
}

// ── Header badge / empty state ──────────────────────────────────────────────

void MarketPanel::update_badge() {
    const int total = static_cast<int>(config_.symbols.size());
    QStringList failed;
    for (const QString& s : std::as_const(config_.symbols)) {
        const MarketRow* r = store_ ? store_->row(s) : nullptr;
        if (r && !r->error.isEmpty())
            failed << QStringLiteral("%1: %2").arg(store_->display_name(s), r->error);
    }
    const bool warn = !failed.isEmpty();
    const QString text = warn ? QStringLiteral("%1/%2").arg(total - failed.size()).arg(total) : QString::number(total);
    QString tip;
    if (warn) {
        QStringList lines = failed.mid(0, 15);
        if (failed.size() > 15)
            lines << tr("...and %1 more").arg(failed.size() - 15);
        tip = lines.join(QLatin1Char('\n'));
    } else {
        tip = tr("%n symbol(s) in this panel", nullptr, total);
    }
    if (text != badge_text_) {
        badge_text_ = text;
        badge_->setText(text);
    }
    if (tip != badge_tip_) {
        badge_tip_ = tip;
        badge_->setToolTip(tip);
    }
    if (warn != badge_warn_) {
        badge_warn_ = warn;
        badge_->setProperty("warn", warn);
        mp_repolish(badge_);
    }
}

void MarketPanel::update_empty_state() {
    QWidget* want = config_.symbols.isEmpty() ? static_cast<QWidget*>(empty_label_) : static_cast<QWidget*>(view_);
    if (body_->currentWidget() != want)
        body_->setCurrentWidget(want);
}

// ── Menus ───────────────────────────────────────────────────────────────────

void MarketPanel::show_panel_menu(const QPoint& global_pos) {
    QMenu menu(this);
    markets_set_sheet(&menu, markets_menu_css());
    QMenu* cols = menu.addMenu(tr("Columns"));
    const QStringList chosen = view_->chosen_column_ids();
    for (int c = 0; c < mcols::count(); ++c) {
        if (mcols::def(c).internal)
            continue;
        QAction* act = cols->addAction(mcols::header(c));
        act->setCheckable(true);
        act->setChecked(chosen.contains(mcols::id_at(c)));
        if (mcols::locked(c)) {
            act->setEnabled(false);
            continue;
        }
        const QString id = mcols::id_at(c);
        connect(act, &QAction::toggled, this, [this, id](bool on) { view_->set_column_visible(id, on); });
    }
    connect(menu.addAction(tr("Edit panel...")), &QAction::triggered, this,
            [this]() { emit edit_requested(config_.id); });
    QAction* max = menu.addAction(maximised_ ? tr("Restore") : tr("Maximise"));
    max->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(max, &QAction::triggered, this, [this]() { emit maximise_requested(config_.id); });
    QMenu* move = menu.addMenu(tr("Move"));
    const struct {
        const char* label;
        Qt::ArrowType dir;
        Qt::Key key;
    } kMoves[] = {{QT_TR_NOOP("Left"), Qt::LeftArrow, Qt::Key_Left},
                  {QT_TR_NOOP("Right"), Qt::RightArrow, Qt::Key_Right},
                  {QT_TR_NOOP("Up"), Qt::UpArrow, Qt::Key_Up},
                  {QT_TR_NOOP("Down"), Qt::DownArrow, Qt::Key_Down}};
    for (const auto& m : kMoves) {
        const Qt::ArrowType dir = m.dir;
        QAction* act = move->addAction(tr(m.label));
        // The keyboard way, shown next to each entry (the table handles the keys itself).
        act->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | m.key));
        connect(act, &QAction::triggered, this, [this, dir]() { emit move_requested(config_.id, dir); });
    }
    move->setEnabled(!maximised_);
    connect(menu.addAction(tr("Export (CSV)...")), &QAction::triggered, this, &MarketPanel::export_csv);
    menu.addSeparator();
    connect(menu.addAction(tr("Remove panel...")), &QAction::triggered, this,
            [this]() { emit delete_requested(config_.id); });
    menu.actions().constLast()->setProperty("fincept_agent_risk", "delete");
    menu.exec(global_pos);
}

void MarketPanel::show_row_menu(const QString& symbol, const QPoint& global_pos) {
    // `symbol` was captured when the menu opened; a tick or re-sort while it is open
    // must not change what the actions act on.
    if (symbol.isEmpty())
        return;
    const MarketRow* r = store_ ? store_->row(symbol) : nullptr;
    const auto cls = core::instrument::asset_class_of(symbol, r ? r->quote.quote_type : QString());
    const QString cls_slug = core::instrument::asset_class_slug(cls);
    auto open_in = [symbol, cls_slug](const QString& screen_id) {
        fincept::EventBus::instance().publish(
            "nav.open_symbol", {{"screen_id", screen_id}, {"symbol", symbol}, {"asset_class", cls_slug}});
    };

    QMenu menu(this);
    markets_set_sheet(&menu, markets_menu_css());
    connect(menu.addAction(tr("Open in Equity Research")), &QAction::triggered, this,
            [open_in]() { open_in(QStringLiteral("equity_research")); });
    if (cls == core::instrument::AssetClass::Equity || cls == core::instrument::AssetClass::Etf)
        connect(menu.addAction(tr("Open in Equity Trading")), &QAction::triggered, this,
                [open_in]() { open_in(QStringLiteral("equity_trading")); });
    connect(menu.addAction(tr("Open in News")), &QAction::triggered, this,
            [open_in]() { open_in(QStringLiteral("news")); });
    menu.addSeparator();
    QMenu* wl = menu.addMenu(tr("Add to Watchlist"));
    equity_links::add_watchlist_entries(wl, this, symbol);
    connect(menu.addAction(tr("Show in detail pane")), &QAction::triggered, this,
            [this, symbol]() { emit detail_requested(symbol); });
    if (successors_.contains(symbol)) { // the resolver reported a rename: offered, never applied silently
        const QString to = successors_.value(symbol);
        const QString shown = core::instrument::display_symbol(to).isEmpty() ? to : core::instrument::display_symbol(to);
        connect(menu.addAction(tr("Replace with %1").arg(shown)), &QAction::triggered, this,
                [this, symbol, to]() { emit replace_symbol_requested(config_.id, symbol, to); });
    }
    menu.addSeparator();
    const bool sorted = view_->is_sorted();
    QAction* top = menu.addAction(tr("Move to top"));
    QAction* bottom = menu.addAction(tr("Move to bottom"));
    top->setEnabled(!sorted);
    bottom->setEnabled(!sorted);
    connect(top, &QAction::triggered, this, [this, symbol]() { move_symbol_to_end(symbol, true); });
    connect(bottom, &QAction::triggered, this, [this, symbol]() { move_symbol_to_end(symbol, false); });
    connect(menu.addAction(tr("Copy symbol")), &QAction::triggered, this, [symbol]() {
        const QString d = core::instrument::display_symbol(symbol);
        QApplication::clipboard()->setText(d.isEmpty() ? symbol : d);
    });
    connect(menu.addAction(tr("Backtest this symbol")), &QAction::triggered, this, [symbol]() {
        QJsonObject config;
        QJsonArray symbols;
        symbols.append(symbol);
        config["symbols"] = symbols;
        fincept::services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        fincept::EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
    });
    menu.addSeparator();
    QAction* remove = menu.addAction(tr("Remove from panel"));
    remove->setProperty("fincept_agent_risk", "delete");
    remove->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(remove, &QAction::triggered, this, [this, symbol]() { emit remove_symbol_requested(config_.id, symbol); });
    menu.exec(global_pos);
}

// ── Export ──────────────────────────────────────────────────────────────────

void MarketPanel::export_table(bool with_panel_column, QStringList* headers, QVector<QStringList>* rows) const {
    QStringList cols = view_->chosen_column_ids();
    cols.removeAll(QStringLiteral("spark")); // no scalar to write
    QStringList h;
    if (with_panel_column)
        h << tr("PANEL");
    for (const QString& id : cols)
        h << model_->export_headers(mcols::index_of(id));
    if (headers && headers->isEmpty())
        *headers = h;
    if (!rows)
        return;
    for (const QString& sym : view_->display_order()) {
        const int r = model_->row_of(sym);
        if (r < 0)
            continue;
        QStringList line;
        if (with_panel_column)
            line << config_.title;
        for (const QString& id : cols)
            line << model_->export_cells(r, mcols::index_of(id));
        rows->append(line);
    }
}

void MarketPanel::export_csv() {
    QStringList headers;
    QVector<QStringList> rows;
    export_table(false, &headers, &rows);
    QString name = config_.title;
    name.replace(QLatin1Char(' '), QLatin1Char('_'));
    ui::ChartExport::save_csv(headers, rows, QStringLiteral("markets_%1").arg(name.toLower()), this);
}

// ── Inline add row ──────────────────────────────────────────────────────────

void MarketPanel::open_add_row() {
    if (!add_entry_) {
        MarketsSymbolEntryOptions o;
        o.multi = true; // "AAPL, RELIANCE; BTC" or a pasted column
        o.compact = true;
        o.clear_on_pick = true;
        o.recent_key = QStringLiteral("markets");
        o.placeholder = tr("Add to %1: ticker, name or ISIN").arg(config_.title);
        add_entry_ = markets_make_symbol_entry(
            add_row_, o,
            [this](const QVector<MarketsSymbolPick>& picks) {
                for (const MarketsSymbolPick& p : picks) {
                    markets_learn(p);
                    emit add_symbol_requested(config_.id, p.symbol); // the screen adds and selects it
                }
            },
            [this](const QStringList&, const QString& message) { emit unresolved_entry(message); });
        add_row_->layout()->addWidget(add_entry_);
        // Esc (with the result list closed) or leaving the row closes it.
        for (QWidget* w : add_entry_->findChildren<QWidget*>())
            w->installEventFilter(this);
        add_entry_->installEventFilter(this);
    }
    if (!add_row_->isVisible())
        add_row_->setVisible(true);
    markets_symbol_entry_focus(add_entry_);
}

void MarketPanel::close_add_row() {
    if (add_row_ && add_row_->isVisible()) {
        add_row_->setVisible(false);
        view_->setFocus(Qt::OtherFocusReason);
    }
}

void MarketPanel::set_resolver_findings(const QHash<QString, QString>& notes, const QHash<QString, QString>& successors) {
    QHash<QString, QString> mine;
    successors_.clear();
    for (const QString& s : std::as_const(config_.symbols)) {
        if (notes.contains(s))
            mine.insert(s, notes.value(s));
        if (successors.contains(s))
            successors_.insert(s, successors.value(s));
    }
    model_->set_notes(mine);
}

// ── Events ──────────────────────────────────────────────────────────────────

bool MarketPanel::eventFilter(QObject* watched, QEvent* event) {
    if (add_entry_ && watched != header_ && (watched == add_entry_ || add_entry_->isAncestorOf(qobject_cast<QWidget*>(watched)))) {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape &&
            !markets_symbol_entry_popup_visible(add_entry_)) {
            close_add_row();
            return true;
        }
        if (event->type() == QEvent::FocusOut) {
            // Deferred: focus may be moving into the entry's own list.
            QPointer<MarketPanel> self = this;
            QTimer::singleShot(150, this, [self]() {
                if (self && self->add_entry_ && !markets_symbol_entry_has_focus(self->add_entry_) &&
                    !markets_symbol_entry_popup_visible(self->add_entry_))
                    self->close_add_row();
            });
        }
        return false;
    }
    if (watched == header_) {
        if (handle_header_drag(event))
            return true;
        if (event->type() == QEvent::MouseButtonDblClick) {
            emit maximise_requested(config_.id);
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::RightButton) {
                show_panel_menu(me->globalPosition().toPoint());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool MarketPanel::handle_header_drag(QEvent* event) {
    // The header's own mouse events (the implicit grab while the button is held reaches
    // it through its labels): no QDrag, so the tables' symbol drag-and-drop is untouched.
    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (dragging_) { // another button during a drag cancels it
                end_header_drag(me->globalPosition().toPoint(), false);
                return true;
            }
            if (me->button() != Qt::LeftButton || maximised_)
                return false; // right-click = menu
            drag_armed_ = true;
            drag_press_global_ = me->globalPosition().toPoint();
            return true;
        }
        case QEvent::MouseMove: {
            if (!drag_armed_ && !dragging_)
                return false;
            auto* me = static_cast<QMouseEvent*>(event);
            const QPoint global = me->globalPosition().toPoint();
            if (!(me->buttons() & Qt::LeftButton)) { // the release never reached us
                drag_armed_ = false;
                end_header_drag(global, false);
                return false;
            }
            if (!dragging_) {
                if (maximised_ || (global - drag_press_global_).manhattanLength() < QApplication::startDragDistance())
                    return true;
                dragging_ = true;
                QApplication::setOverrideCursor(Qt::ClosedHandCursor);
                // Esc cancels, wherever the keyboard focus is.
                QPointer<MarketPanel> self = this;
                drag_key_filter_ = new MpDragKeyFilter(
                    [self]() {
                        if (self)
                            self->end_header_drag(QCursor::pos(), false);
                    },
                    this);
                qApp->installEventFilter(drag_key_filter_);
            }
            emit header_drag_moved(config_.id, global);
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            const bool was_armed = drag_armed_;
            drag_armed_ = false;
            if (dragging_ && me->button() == Qt::LeftButton) {
                end_header_drag(me->globalPosition().toPoint(), true);
                return true;
            }
            return was_armed && me->button() == Qt::LeftButton;
        }
        case QEvent::Hide:
            // Re-parented by a grid rebuild, the screen hidden, the panel removed.
            drag_armed_ = false;
            end_header_drag(QCursor::pos(), false);
            return false;
        default:
            return false;
    }
}

void MarketPanel::end_header_drag(const QPoint& global_pos, bool drop) {
    drag_armed_ = false; // after Esc the button is still down: no new drag until a new press
    if (!dragging_)
        return;
    dragging_ = false;
    QApplication::restoreOverrideCursor();
    if (drag_key_filter_) {
        qApp->removeEventFilter(drag_key_filter_);
        drag_key_filter_->deleteLater(); // may be running its eventFilter right now (Esc)
        drag_key_filter_ = nullptr;
    }
    emit header_drag_finished(config_.id, global_pos, drop);
}

void MarketPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MarketPanel::retranslateUi() {
    add_btn_->setToolTip(tr("Add a symbol to this panel (Insert)"));
    add_btn_->setAccessibleName(tr("Add a symbol to this panel"));
    menu_btn_->setToolTip(tr("Panel menu"));
    menu_btn_->setAccessibleName(tr("Panel menu"));
    header_->setToolTip(tr("Drag to move this panel (keyboard: Ctrl+Shift+Arrow) · double-click to maximise · "
                           "right-click for the panel menu"));
    empty_label_->setText(tr("No symbols in this panel.\nPress + to add one, or open the panel menu > Edit panel."));
    badge_tip_.clear(); // re-derived below in the new language
    update_badge();
    model_->refresh_all(); // headers and n/a / state words
}

} // namespace fincept::screens
