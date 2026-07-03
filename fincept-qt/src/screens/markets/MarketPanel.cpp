#include "screens/markets/MarketPanel.h"
#include <cmath>
#include "core/events/EventBus.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/formatting/NumberFormat.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPointer>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

MarketPanel::MarketPanel(const MarketPanelConfig& config, QWidget* parent)
    : QWidget(parent), config_(config) {
    if (config_.column_order.isEmpty())
        config_.column_order = default_market_columns();
    setMinimumWidth(180);
    setMinimumHeight(kHeaderH + kColHeaderH + kRowH * 4);  // always room for at least 4 rows
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    build_ui();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void MarketPanel::build_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // --- Header (28px) ---
    header_ = new QWidget(this);
    header_->setFixedHeight(kHeaderH);
    auto* hhl = new QHBoxLayout(header_);
    hhl->setContentsMargins(8, 0, 4, 0);
    hhl->setSpacing(2);

    title_label_ = new QLabel(config_.title.toUpper());
    hhl->addWidget(title_label_);
    hhl->addStretch();

    auto make_hdr_btn = [](const QString& text) -> QPushButton* {
        auto* b = new QPushButton(text);
        b->setFixedHeight(20);
        b->setCursor(Qt::PointingHandCursor);
        b->setFlat(true);
        return b;
    };

    cols_btn_   = make_hdr_btn(tr("[COLS]"));
    edit_btn_   = make_hdr_btn(tr("[EDIT]"));
    delete_btn_ = make_hdr_btn(tr("[DEL]"));

    hhl->addWidget(cols_btn_);
    hhl->addWidget(edit_btn_);
    hhl->addWidget(delete_btn_);

    connect(cols_btn_,   &QPushButton::clicked, this, &MarketPanel::open_cols_dropdown);
    connect(edit_btn_,   &QPushButton::clicked, this, [this]() { emit edit_requested(config_.id); });
    connect(delete_btn_, &QPushButton::clicked, this, [this]() { emit delete_requested(config_.id); });

    vl->addWidget(header_);

    // --- Body (fills remaining height) ---
    body_ = new QWidget(this);
    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);

    // Data table
    table_ = new QTableWidget(body_);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kRowH);
    table_->horizontalHeader()->setFixedHeight(kColHeaderH);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Prevent the table's content-based sizeHint from dominating splitter layout.
    // Without this, QTableWidget returns a large sizeHint proportional to row count,
    // causing the first panel in each column to grab disproportionate space.
    table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    setup_table_columns();
    
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested,
            this, &MarketPanel::show_row_context_menu);

    table_->setVisible(false);  // hidden until first data arrives
    bl->addWidget(table_);

    // Error state (hidden by default)
    error_widget_ = new QWidget(body_);
    auto* el = new QVBoxLayout(error_widget_);
    el->setAlignment(Qt::AlignCenter);
    error_label_ = new QLabel;
    error_label_->setAlignment(Qt::AlignCenter);
    error_label_->setWordWrap(true);  // narrow panels — wrap the failure message
    retry_btn_ = new QPushButton(tr("[RETRY]"));
    retry_btn_->setCursor(Qt::PointingHandCursor);
    retry_btn_->setFlat(true);
    connect(retry_btn_, &QPushButton::clicked, this, &MarketPanel::refresh);
    el->addWidget(error_label_);
    el->addWidget(retry_btn_);
    error_widget_->setVisible(false);
    bl->addWidget(error_widget_);

    // Loading overlay (visible until first data)
    loading_widget_ = new QWidget(body_);
    auto* ll = new QVBoxLayout(loading_widget_);
    ll->setAlignment(Qt::AlignCenter);
    loading_label_ = new QLabel(tr("  %1  LOADING").arg(QString::fromUtf8("\xe2\xa0\x8b")));
    loading_label_->setAlignment(Qt::AlignCenter);
    ll->addWidget(loading_label_);
    bl->addWidget(loading_widget_);

    loading_timer_ = new QTimer(this);
    loading_timer_->setInterval(120);
    connect(loading_timer_, &QTimer::timeout, this, &MarketPanel::tick_loading_anim);

    vl->addWidget(body_, 1);
}

QString MarketPanel::column_label(const QString& code) const {
    // Maps an internal column code (a data key used in config_.column_order
    // and in the populate() switch) to its user-facing, translatable header
    // label. SYMBOL is the locked first column and renders the human-readable
    // name, so it is labelled "NAME".
    if (code == "SYMBOL") return tr("NAME");
    if (code == "LAST")   return tr("LAST");
    if (code == "CHG")    return tr("CHG");
    if (code == "CHG%")   return tr("CHG%");
    if (code == "HIGH")   return tr("HIGH");
    if (code == "LOW")    return tr("LOW");
    if (code == "VOL")    return tr("VOL");
    if (code == "BID")    return tr("BID");
    if (code == "ASK")    return tr("ASK");
    if (code == "OPEN")   return tr("OPEN");
    if (code == "TICKER") return tr("TICKER");
    if (code == "NAME")   return tr("NAME");
    return code;  // unknown code — show raw (data fallback)
}

void MarketPanel::setup_table_columns() {
    const QStringList& cols = config_.column_order;
    table_->setColumnCount(cols.size());

    // Set header labels with alignment matching cell alignment.
    // The locked first column ("SYMBOL") now renders the human-readable name,
    // so it's labelled NAME; the raw ticker is available via the TICKER column.
    for (int i = 0; i < cols.size(); ++i) {
        const QString& c = cols[i];
        const QString label = column_label(c);
        auto* hdr = new QTableWidgetItem(label);
        bool is_text = (c == "SYMBOL" || c == "NAME" || c == "TICKER");
        hdr->setTextAlignment(is_text ? (Qt::AlignLeft | Qt::AlignVCenter)
                                      : (Qt::AlignRight | Qt::AlignVCenter));
        table_->setHorizontalHeaderItem(i, hdr);
    }

    // Column sizing: the NAME column (human-readable, variable-length) gets the
    // leftover width via Stretch, while the compact numeric/ticker columns size
    // to their content. Sharing equal stretch across every column starved the
    // name column and truncated long names ("State Street SPDR S&P 500 ETF…").
    auto* hh = table_->horizontalHeader();
    for (int i = 0; i < cols.size(); ++i) {
        const QString& c = cols[i];
        if (c == "SYMBOL" || c == "NAME")
            hh->setSectionResizeMode(i, QHeaderView::Stretch);
        else
            hh->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
}

// ---------------------------------------------------------------------------
// Column configurator
// ---------------------------------------------------------------------------

void MarketPanel::open_cols_dropdown() {
    auto* menu = new QMenu(this);
    menu->setStyleSheet(
        QString("QMenu{background:%1;border:1px solid %2;color:%3;font-size:11px;font-family:monospace;}"
                "QMenu::item{padding:4px 16px;}"
                "QMenu::item:selected{background:%4;}"
                "QMenu::indicator{width:12px;height:12px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER()));

    // NAME is now the locked first column (renders the friendly name), so the
    // optional set offers TICKER instead for users who also want the raw symbol.
    const QStringList optional = {"CHG", "CHG%", "HIGH", "LOW", "VOL", "BID", "ASK", "OPEN", "TICKER"};

    for (const QString& col : optional) {
        auto* act = menu->addAction(column_label(col));
        act->setCheckable(true);
        act->setChecked(config_.column_order.contains(col));
        // Enforce max 7 columns (SYMBOL + LAST are locked = 2, so max 5 optional)
        if (!act->isChecked() && config_.column_order.size() >= 7)
            act->setEnabled(false);
        connect(act, &QAction::toggled, this, [this, col](bool checked) {
            if (checked) {
                if (!config_.column_order.contains(col) && config_.column_order.size() < 7)
                    config_.column_order.append(col);
            } else {
                config_.column_order.removeAll(col);
                // Always keep SYMBOL and LAST
                if (!config_.column_order.contains("SYMBOL"))
                    config_.column_order.prepend("SYMBOL");
                if (!config_.column_order.contains("LAST")) {
                    int idx = config_.column_order.indexOf("SYMBOL");
                    config_.column_order.insert(idx + 1, "LAST");
                }
            }
            setup_table_columns();
            if (has_data_) populate(cached_quotes_);
            emit config_changed(config_);
        });
    }

    menu->exec(cols_btn_->mapToGlobal(QPoint(0, cols_btn_->height())));
    menu->deleteLater();
}

void MarketPanel::update_config(const MarketPanelConfig& cfg) {
    config_ = cfg;
    if (config_.column_order.isEmpty())
        config_.column_order = default_market_columns();
    title_label_->setText(cfg.title.toUpper());
    setup_table_columns();
    if (has_data_) populate(cached_quotes_);
}

// ---------------------------------------------------------------------------
// Data population
// ---------------------------------------------------------------------------

void MarketPanel::refresh() {
    fetch_failed_ = false;
    title_label_->setText(config_.title.toUpper() + "  ...");

    if (!has_data_)
        show_loading();

    hub_resubscribe();
}


void MarketPanel::rebuild_from_cache() {
    cached_quotes_.clear();
    cached_quotes_.reserve(config_.symbols.size());
    for (const auto& sym : config_.symbols) {
        if (row_cache_.contains(sym))
            cached_quotes_.append(row_cache_.value(sym));
    }
    if (cached_quotes_.isEmpty())
        return;
    const bool first_data = !has_data_;
    has_data_ = true;
    title_label_->setText(config_.title.toUpper());
    if (first_data)
        hide_loading();
    show_data();
}

void MarketPanel::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();

    if (config_.symbols.isEmpty()) {
        hub.unsubscribe(this);
        hub_active_ = false;
        emit refresh_finished();
        return;
    }

    // Resolve human-readable display names so the table shows "S&P 500" / "Gold"
    // instead of "^GSPC" / "GC=F". Cached after first resolution, so this is a
    // no-op fast path on subsequent refreshes. Re-render when names arrive.
    QPointer<MarketPanel> self = this;
    services::MarketDataService::instance().resolve_names(
        config_.symbols, [self](const QHash<QString, QString>& m) {
            if (!self || m.isEmpty())
                return;
            bool changed = false;
            for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
                if (self->names_.value(it.key()) != it.value()) {
                    self->names_.insert(it.key(), it.value());
                    changed = true;
                }
            }
            if (changed && self->has_data_)
                self->populate(self->cached_quotes_);
        });

    // Fresh subscribe if never active or symbol set was reconfigured.
    // Cheaper to always re-wire since MarketsScreen rarely calls refresh()
    // outside of user action / auto-refresh tick, and the hub dedupes topics.
    hub.unsubscribe(this);
    row_cache_.clear();
    pending_initial_.clear();
    for (const auto& sym : config_.symbols)
        pending_initial_.insert(sym);
    refresh_inflight_ = true;

    QStringList topics;
    topics.reserve(config_.symbols.size());
    for (const QString& sym : config_.symbols) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        topics.append(topic);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            rebuild_from_cache();
            // Drain the initial-set counter on first delivery for this symbol.
            if (refresh_inflight_ && pending_initial_.remove(sym) && pending_initial_.isEmpty()) {
                refresh_inflight_ = false;
                emit refresh_finished();
            }
        });
        // Failure path: every requested topic ends with either publish()
        // (delivered above) or publish_error(). Without an error subscription
        // the panel never learns of a producer failure, so the LOADING overlay
        // spins forever and refresh_finished() never emits. Mirror the drain
        // here, then — once the whole initial set has resolved with nothing
        // delivered — switch to the inline error state so [RETRY] is reachable.
        hub.subscribe_errors(this, topic, [this, sym](const QString& err) {
            if (refresh_inflight_ && pending_initial_.remove(sym) && pending_initial_.isEmpty()) {
                refresh_inflight_ = false;
                emit refresh_finished();
            }
            if (!has_data_ && pending_initial_.isEmpty()) {
                fetch_failed_ = true;
                show_error(err.trimmed().isEmpty()
                               ? tr("Failed to load market data")
                               : tr("Load failed: %1").arg(err.trimmed()));
            }
        });
    }
    // force=true: on cold start, DataHub::subscribe auto-triggers a fetch,
    // but on user-initiated refresh_all() (F5 / auto-refresh tick / panel
    // reconfig) we want to bypass min_interval. Producer rate limit still
    // paces upstream calls.
    hub.request(topics, /*force=*/true);
    hub_active_ = true;
}

void MarketPanel::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
    refresh_inflight_ = false;
    pending_initial_.clear();
}


void MarketPanel::show_loading() {
    loading_frame_ = 0;
    loading_widget_->setVisible(true);
    table_->setVisible(false);
    error_widget_->setVisible(false);
    loading_timer_->start();
}

void MarketPanel::hide_loading() {
    loading_timer_->stop();
    loading_widget_->setVisible(false);
}

void MarketPanel::tick_loading_anim() {
    // Braille spinner: ⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏
    static const char* const kFrames[] = {
        "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
    };
    constexpr int kCount = 10;
    loading_frame_ = (loading_frame_ + 1) % kCount;
    loading_label_->setText(tr("  %1  LOADING").arg(QString::fromUtf8(kFrames[loading_frame_])));
}

void MarketPanel::show_data() {
    error_widget_->setVisible(false);
    table_->setVisible(true);
    // Defer populate until after the splitter has performed layout so body_->height() is valid
    QPointer<MarketPanel> self = this;
    QMetaObject::invokeMethod(this, [self]() {
        if (self) self->populate(self->cached_quotes_);
    }, Qt::QueuedConnection);
}

void MarketPanel::show_error(const QString& msg) {
    hide_loading();  // stop the spinner + hide the LOADING overlay
    table_->setVisible(false);
    error_widget_->setVisible(true);
    error_label_->setText(msg);
}

void MarketPanel::populate(const QVector<services::QuoteData>& quotes) {
    const int body_h  = body_->height();
    // Use actual visible rows if laid out, otherwise show all quotes (resizeEvent will trim)
    const int visible = (body_h > kColHeaderH + kRowH)
                            ? (body_h - kColHeaderH) / kRowH
                            : quotes.size();
    const int count   = qMin(quotes.size(), qMax(visible, 0));
    table_->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const auto& q  = quotes[row];
        bool pos          = q.change >= 0;
        const QString cc  = pos ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
        const QString arr = pos ? QString::fromUtf8("\xe2\x96\xb2") : QString::fromUtf8("\xe2\x96\xbc");
        int prec = q.price > 1.0 ? 2 : 4;

        auto mk = [](const QString& s, const QString& c,
                     Qt::Alignment a = Qt::AlignRight | Qt::AlignVCenter) -> QTableWidgetItem* {
            auto* item = new QTableWidgetItem(s);
            item->setForeground(QColor(c));
            item->setTextAlignment(a);
            return item;
        };

        // Human-readable display name: prefer the resolved yfinance name
        // (e.g. "S&P 500", "Gold"), fall back to any name on the quote, then
        // to the raw ticker. The ticker is still kept on the cell (UserRole +
        // tooltip) so copy / backtest / context-menu actions keep working.
        QString disp = names_.value(q.symbol);
        if (disp.isEmpty())
            disp = (!q.name.isEmpty() && q.name != q.symbol) ? q.name : q.symbol;

        // Currency symbol shown in front of price levels (e.g. "$", "₹").
        // Empty for forex pairs / index levels / yields, where it isn't
        // meaningful. Resolved & cached alongside the display name.
        const QString cur = services::MarketDataService::instance().currency_prefix(q.symbol);

        const QStringList& cols = config_.column_order;
        for (int ci = 0; ci < cols.size(); ++ci) {
            const QString& col = cols[ci];
            if (col == "SYMBOL") {
                auto* item = mk(disp, ui::colors::TEXT_PRIMARY(), Qt::AlignLeft | Qt::AlignVCenter);
                item->setData(Qt::UserRole, q.symbol);  // raw ticker, for copy / backtest
                // Full name + ticker on hover, so a name too long for the column
                // (e.g. "State Street SPDR S&P 500 ETF Trust") is still readable.
                item->setToolTip(disp == q.symbol ? q.symbol
                                                  : QString("%1  (%2)").arg(disp, q.symbol));
                table_->setItem(row, ci, item);
            }
            else if (col == "NAME")   table_->setItem(row, ci, mk(disp,     ui::colors::TEXT_DIM(),     Qt::AlignLeft | Qt::AlignVCenter));
            else if (col == "TICKER") table_->setItem(row, ci, mk(q.symbol, ui::colors::TEXT_DIM(),     Qt::AlignLeft | Qt::AlignVCenter));
            else if (col == "LAST")   table_->setItem(row, ci, mk(cur + QString::number(q.price, 'f', prec), ui::colors::AMBER()));
            else if (col == "CHG")    table_->setItem(row, ci, mk(QString("%1 %2").arg(arr).arg(std::abs(q.change),     0, 'f', 2), cc));
            else if (col == "CHG%")   table_->setItem(row, ci, mk(QString("%1%2%").arg(arr).arg(std::abs(q.change_pct), 0, 'f', 2), cc));
            else if (col == "HIGH")   table_->setItem(row, ci, mk(cur + QString::number(q.high, 'f', 2), ui::colors::TEXT_SECONDARY()));
            else if (col == "LOW")    table_->setItem(row, ci, mk(cur + QString::number(q.low,  'f', 2), ui::colors::TEXT_SECONDARY()));
            else if (col == "VOL")    table_->setItem(row, ci, mk(fincept::ui::formatting::format_compact_volume(static_cast<qint64>(q.volume)), ui::colors::TEXT_DIM()));
            else if (col == "BID")    table_->setItem(row, ci, mk("--", ui::colors::TEXT_DIM()));
            else if (col == "ASK")    table_->setItem(row, ci, mk("--", ui::colors::TEXT_DIM()));
            else if (col == "OPEN")   table_->setItem(row, ci, mk("--", ui::colors::TEXT_DIM()));
        }
    }
}

void MarketPanel::update_visible_rows() {
    if (!table_ || !body_) return;
    const int visible = (body_->height() - kColHeaderH) / kRowH;
    table_->setRowCount(qMax(visible, 0));
}

// ---------------------------------------------------------------------------
// Size hints — small uniform value so QSplitter distributes equal height
// ---------------------------------------------------------------------------

QSize MarketPanel::sizeHint() const {
    return QSize(180, 138);  // kHeaderH + kColHeaderH + kRowH*4
}

QSize MarketPanel::minimumSizeHint() const {
    return QSize(180, 138);
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

void MarketPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (has_data_)
        populate(cached_quotes_);
    else
        update_visible_rows();
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void MarketPanel::refresh_theme() {
    const QString ff   = ui::fonts::DATA_FAMILY;
    const int    fdata = ui::fonts::font_px(-2);   // table cell font size (base-2, e.g. 12px at base 14)
    const int    fhdr  = ui::fonts::font_px(-2);   // column header font size
    const int    fbtn  = ui::fonts::font_px(-3);   // button font size (base-3, e.g. 11px)
    const int    ftitle = ui::fonts::font_px(-1);  // panel title font size (base-1, e.g. 13px)

    setStyleSheet(
        QString("background:%1;border:1px solid %2;")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    if (header_) {
        header_->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
                .arg(ui::colors::BG_RAISED(), ui::colors::AMBER()));
    }

    const QString btn_ss =
        QString("QPushButton{background:transparent;color:%1;border:none;"
                "font-size:%2px;font-family:'%3';padding:0 4px;}"
                "QPushButton:hover{color:%4;}")
            .arg(ui::colors::TEXT_DIM()).arg(fbtn).arg(ff).arg(ui::colors::TEXT_PRIMARY());
    if (cols_btn_)   cols_btn_->setStyleSheet(btn_ss);
    if (edit_btn_)   edit_btn_->setStyleSheet(btn_ss);
    if (delete_btn_) delete_btn_->setStyleSheet(btn_ss);

    if (title_label_)
        title_label_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:700;font-family:'%3';background:transparent;")
                .arg(ui::colors::TEXT_PRIMARY()).arg(ftitle).arg(ff));

    if (error_label_)
        error_label_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-family:'%3';background:transparent;")
                .arg(ui::colors::NEGATIVE()).arg(fdata).arg(ff));

    if (loading_label_)
        loading_label_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-family:'%3';background:transparent;letter-spacing:2px;")
                .arg(ui::colors::TEXT_DIM()).arg(fdata).arg(ff));

    if (loading_widget_)
        loading_widget_->setStyleSheet(
            QString("background:%1;").arg(ui::colors::BG_BASE()));

    if (retry_btn_)
        retry_btn_->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:none;"
                    "font-size:%2px;font-family:'%3';padding:0 4px;}"
                    "QPushButton:hover{color:%4;}")
                .arg(ui::colors::TEXT_DIM()).arg(fbtn).arg(ff).arg(ui::colors::TEXT_PRIMARY()));

    if (table_) {
        table_->setStyleSheet(
            QString("QTableWidget{background:%1;alternate-background-color:%2;border:none;"
                    "font-size:%6px;font-family:'%7';}"
                    "QTableWidget::item{padding:0 4px;}"
                    "QTableWidget::item:selected{background:transparent;}"
                    "QHeaderView::section{background:%3;color:%4;border:none;"
                    "border-bottom:1px solid %5;font-size:%6px;font-weight:600;"
                    "font-family:'%7';padding:0 4px;}")
                .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BG_BASE(),
                     ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM())
                .arg(fhdr).arg(ff));
    }
}

void MarketPanel::show_row_context_menu(const QPoint& pos) {
    auto* it = table_->itemAt(pos);
    if (!it) return;

    // Column 0 now shows the friendly name; the raw ticker lives in UserRole.
    auto ticker_at = [this](int row) -> QString {
        auto* sym_item = table_->item(row, 0);
        if (!sym_item) return QString();
        const QString t = sym_item->data(Qt::UserRole).toString();
        return t.isEmpty() ? sym_item->text() : t;
    };

    QMenu menu(this);
    QAction* copy_act = menu.addAction(tr("Copy Symbol"));
    connect(copy_act, &QAction::triggered, this, [it, ticker_at]() {
        const QString t = ticker_at(it->row());
        if (!t.isEmpty())
            QApplication::clipboard()->setText(t);
    });
    QAction* backtest_act = menu.addAction(tr("Backtest This Symbol"));
    connect(backtest_act, &QAction::triggered, this, [it, ticker_at]() {
        const QString t = ticker_at(it->row());
        if (t.isEmpty()) return;
        QJsonObject config;
        QJsonArray symbols;
        symbols.append(t);
        config["symbols"] = symbols;
        fincept::services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        fincept::EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
    });
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// i18n — live language switch
// ---------------------------------------------------------------------------

void MarketPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MarketPanel::retranslateUi() {
    if (cols_btn_)   cols_btn_->setText(tr("[COLS]"));
    if (edit_btn_)   edit_btn_->setText(tr("[EDIT]"));
    if (delete_btn_) delete_btn_->setText(tr("[DEL]"));
    if (retry_btn_)  retry_btn_->setText(tr("[RETRY]"));

    // Loading spinner — keep the current animation frame, refresh the word.
    static const char* const kFrames[] = {
        "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8", "\xe2\xa0\xbc",
        "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7", "\xe2\xa0\x87", "\xe2\xa0\x8f"
    };
    if (loading_label_)
        loading_label_->setText(tr("  %1  LOADING").arg(QString::fromUtf8(kFrames[loading_frame_ % 10])));

    // Table column headers — re-derived from the translated column labels.
    setup_table_columns();
    if (has_data_) populate(cached_quotes_);
}

} // namespace fincept::screens
