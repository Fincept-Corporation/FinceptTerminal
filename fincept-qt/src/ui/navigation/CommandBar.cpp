#include "ui/navigation/CommandBar.h"

#include "core/events/EventBus.h"
#include "network/http/HttpClient.h"

#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QScreen>
#include <QVBoxLayout>

namespace fincept::ui {

static constexpr int kMaxResults = 10;
static constexpr int kSearchDebounceMs = 300;

// ── styling ─────────────────────────────────────────────────────────────────

static const char* INPUT_SS =
    "QLineEdit{"
    "  background:#111111;"
    "  color:#e5e5e5;"
    "  border:1px solid #2a2a2a;"
    "  padding:2px 8px 2px 24px;"
    "  font-size:12px;"
    "  font-family:'Consolas',monospace;"
    "}"
    "QLineEdit:focus{"
    "  border:1px solid #d97706;"
    "}";

static const char* DROP_SS =
    "QFrame{"
    "  background:#0e0e0e;"
    "  border:1px solid #2a2a2a;"
    "  border-top:none;"
    "}";

static const char* LIST_SS =
    "QListWidget{"
    "  background:transparent;"
    "  border:none;"
    "  outline:none;"
    "}"
    "QListWidget::item{"
    "  padding:0px;"
    "  border:none;"
    "  background:transparent;"
    "}"
    "QListWidget::item:selected{"
    "  background:#1a1a1a;"
    "  border-left:3px solid #d97706;"
    "}";

// ── yfinance symbol conversion ───────────────────────────────────────────────

/// Convert exchange + symbol to yfinance-compatible ticker.
/// The /market/search API returns clean symbols (e.g. "RELIANCE") with exchange
/// as a separate field. yfinance needs suffixed tickers for non-US exchanges.
static QString to_yfinance_symbol(const QString& symbol, const QString& exchange,
                                  const QString& country = {}) {
    // EURONEXT suffix depends on country
    if (exchange.toUpper() == "EURONEXT") {
        static const QHash<QString, QString> euronext_map = {
            {"FR", ".PA"}, {"NL", ".AS"}, {"BE", ".BR"},
            {"PT", ".LS"}, {"IE", ".IR"},
        };
        const auto it = euronext_map.find(country.toUpper());
        return symbol + (it != euronext_map.end() ? it.value() : ".PA");
    }

    static const QHash<QString, QString> suffix_map = {
        // India
        {"NSE",        ".NS"},  {"BSE",        ".BO"},
        // Asia-Pacific
        {"HKEX",       ".HK"},  {"TSE",        ".T"},   {"NAG",       ".T"},
        {"KRX",        ".KS"},  {"SGX",        ".SI"},  {"ASX",       ".AX"},
        {"IDX",        ".JK"},  {"MYX",        ".KL"},  {"SET",       ".BK"},
        {"PSE",        ".PS"},  {"TPEX",       ".TWO"},
        // Europe — Germany
        {"XETR",       ".DE"},  {"FWB",        ".F"},   {"DUS",       ".DU"},
        {"HAM",        ".HM"},  {"HAN",        ".HA"},  {"MUN",       ".MU"},
        {"SWB",        ".SG"},  {"GETTEX",     ".DE"},  {"TRADEGATE", ".DE"},
        {"LS",         ".DE"},  {"LSX",        ".DE"},
        // Europe — UK
        {"LSE",        ".L"},   {"LSIN",       ".L"},   {"AQUIS",     ".L"},
        // Europe — Other
        {"BME",        ".MC"},  {"MIL",        ".MI"},  {"EUROTLX",   ".MI"},
        {"SIX",        ".SW"},  {"BX",         ".SW"},  {"VIE",       ".VI"},
        {"LUXSE",      ".LU"},
        // Americas — Canada
        {"TSX",        ".TO"},  {"TSXV",       ".V"},   {"CSE",       ".CN"},
        {"NEO",        ".NE"},
        // Americas — Latin America
        {"BMFBOVESPA", ".SA"},  {"BMV",        ".MX"},  {"BIVA",      ".MX"},
        {"BCBA",       ".BA"},  {"BCS",        ".SN"},  {"BVC",       ".CL"},
        {"BVL",        ".LM"},
        // Middle East & Africa
        {"BIST",       ".IS"},  {"QSE",        ".QA"},  {"EGX",       ".CA"},
        {"NSENG",      ".LG"},
        // South Asia
        {"PSX",        ".KA"},  {"DSEBD",      ".BD"},
    };

    const auto it = suffix_map.find(exchange.toUpper());
    if (it != suffix_map.end())
        return symbol + it.value();
    return symbol; // NASDAQ, NYSE, AMEX, OTC, FINRA, crypto exchanges — no suffix
}

// ── command registry ─────────────────────────────────────────────────────────

void CommandBar::build_commands() {
    commands_ = {
        // Primary tabs
        {"dashboard",      "Dashboard",           "Main overview screen",               {"dash","home","overview","main"},          "F1",  {"dashboard","home","summary"}},
        {"markets",        "Markets",             "Live market data",                   {"mkts","markets","market","live"},         "F2",  {"markets","stocks","quotes","prices"}},
        {"news",           "News",                "Financial news feed",                {"news","headlines","feed"},                "F3",  {"news","headlines","articles"}},
        {"portfolio",      "Portfolio",           "Portfolio management",               {"port","portfolio","pf","holdings"},       "F4",  {"portfolio","holdings","positions"}},
        {"backtesting",    "Backtesting",         "Strategy backtesting",               {"bktest","backtest","bt"},                 "F5",  {"backtest","strategy","historical"}},
        {"watchlist",      "Watchlist",           "Manage watchlists",                  {"watch","watchlist","wl"},                 "F6",  {"watchlist","favorites","track"}},
        {"crypto_trading", "Crypto Trading",      "Cryptocurrency trading",             {"trade","trading","crypto","kraken"},      "F9",  {"trading","crypto","exchange"}},
        {"ai_chat",        "AI Chat",             "AI assistant",                       {"ai","chat","assistant","bot"},            "F10", {"ai","chat","assistant"}},
        {"notes",          "Notes",               "Notes and reports",                  {"notes","note"},                           "F11", {"notes","reports","documents"}},
        {"profile",        "Profile",             "User profile & account",             {"prof","profile","account"},              "F12", {"profile","account","user"}},
        {"settings",       "Settings",            "Application settings",               {"settings","prefs","config"},             "",    {"settings","preferences"}},
        {"forum",          "Forum",               "Community forum",                    {"forum","community"},                     "",    {"forum","community","discuss"}},
        // Trading & Portfolio
        {"equity_trading", "Equity Trading",      "Stock trading interface",            {"eqtrade","stocks","equities"},           "",    {"equity","stocks","trading"}},
        {"algo_trading",   "Algo Trading",        "Algorithmic trading",                {"algo","algotrading"},                    "",    {"algo","algorithmic","automated"}},
        {"alpha_arena",    "Alpha Arena",         "Trading competition platform",       {"alpha","arena","alphaarena"},            "",    {"alpha","competition","leaderboard"}},
        {"polymarket",     "Polymarket",          "Prediction markets",                 {"poly","polymarket","prediction"},        "",    {"polymarket","prediction","markets"}},
        {"derivatives",    "Derivatives",         "Options and derivatives",            {"deriv","derivatives","options"},         "",    {"derivatives","options","futures"}},
        // Research
        {"equity_research","Equity Research",     "Equity research tools",             {"rsrch","research","equity"},             "",    {"research","equity","fundamental"}},
        {"screener",       "Screener",            "Stock screener",                     {"scrn","screener","filter","scan"},       "",    {"screener","filter","scan"}},
        {"ma_analytics",   "M&A Analytics",       "Mergers and acquisitions",           {"ma","mna","mergers"},                    "",    {"ma","mergers","acquisitions"}},
        {"alt_investments","Alt. Investments",    "Alternative investment analysis",    {"altinv","alt","alternatives"},           "",    {"alternative","investments","hedge"}},
        {"surface_analytics","Surface Analytics", "Options vol surface & correlation",  {"surface","volsurface","3dviz","3d"},     "",    {"surface","volatility","correlation","pca"}},
        // Economics & Data
        {"economics",      "Economics",           "Economic indicators",                {"econ","economics","indicators"},         "",    {"economics","macro","indicators"}},
        {"gov_data",       "GOVT Data",           "Government securities & open data",  {"govt","gov","government","treasury"},    "",    {"government","treasury","bonds"}},
        {"dbnomics",       "DBnomics",            "Economic database",                  {"dbn","dbnomics","database"},             "",    {"dbnomics","database","economic"}},
        {"akshare",        "AKShare Data",        "Chinese financial data",             {"aks","akshare","chinese"},               "",    {"akshare","chinese","china"}},
        {"asia_markets",   "Asia Markets",        "Asian market data",                  {"asia","apac","asian"},                   "",    {"asia","asian","apac"}},
        // Geopolitics
        {"geopolitics",    "Geopolitics",         "Geopolitical analysis",              {"geo","geopolitics","politics"},          "",    {"geopolitics","politics","global"}},
        {"maritime",       "Maritime",            "Maritime intelligence",              {"marine","maritime","shipping"},          "",    {"maritime","shipping","vessels"}},
        {"relationship_map","Relationship Map",   "Entity relationship mapping",        {"relmap","relationships","map"},          "",    {"relationship","map","network"}},
        // AI / Quant
        {"ai_quant_lab",   "AI Quant Lab",        "Quantitative analysis lab",          {"quantlab","quant","lab"},                "",    {"quant","quantitative","lab"}},
        {"quantlib",       "QuantLib",            "Quantitative finance suite",         {"qlcore","ql","quantlib"},                "",    {"quantlib","math","finance","models"}},
        {"agent_config",   "Agent Config",        "Configure AI agents",                {"agents","agent","config"},               "",    {"agents","ai","configuration"}},
        // Tools
        {"mcp_servers",    "MCP Servers",         "Model Context Protocol servers",     {"mcp","servers"},                         "",    {"mcp","servers","protocol"}},
        {"node_editor",    "Node Editor",         "Visual workflow editor",             {"nodes","workflow","node"},               "",    {"nodes","workflow","visual"}},
        {"code_editor",    "Code Editor",         "Code development",                   {"code","editor","dev"},                   "",    {"code","editor","programming"}},
        {"excel",          "Excel",               "Excel workbook integration",         {"excel","spreadsheet","xls"},             "",    {"excel","spreadsheet","workbook"}},
        {"report_builder", "Report Builder",      "Create reports",                     {"report","reports","builder"},            "",    {"report","builder","document"}},
        {"data_sources",   "Data Sources",        "Manage data sources",                {"datasrc","datasources","sources"},       "",    {"data","sources","connections"}},
        {"data_mapping",   "Data Mapping",        "API integration & schema transform", {"datamap","mapping","schema"},            "",    {"data","mapping","schema","api"}},
        {"trade_viz",      "Trade Visualization", "Trade flow visualization",           {"tradeviz","tradegraph"},                 "",    {"trade","visualization","flow"}},
        // Community / Info
        {"about",          "About",               "About Fincept Terminal",             {"about","info","version"},                "",    {"about","information","version"}},
        {"support",        "Support",             "Support tickets",                    {"support","ticket","help"},               "",    {"support","ticket","assistance"}},
    };
}

// ── asset type registry ──────────────────────────────────────────────────────

void CommandBar::build_asset_types() {
    asset_types_ = {
        {"/stock",    "stock",    "Stock",    "Search stocks by symbol or company name"},
        {"/fund",     "fund",     "Fund",     "Search mutual funds and ETFs"},
        {"/dr",       "dr",       "DR",       "Search depositary receipts (ADR/GDR)"},
        {"/index",    "index",    "Index",    "Search market indices"},
        {"/forex",    "forex",    "Forex",    "Search forex currency pairs"},
        {"/crypto",   "crypto",   "Crypto",   "Search cryptocurrencies"},
        {"/futures",  "futures",  "Futures",  "Search futures contracts"},
        {"/bond",     "bond",     "Bond",     "Search bonds and fixed income"},
        {"/economic", "economic", "Economic", "Search economic indicators"},
    };
}

// ── screen command search ────────────────────────────────────────────────────

QVector<ScreenCommand> CommandBar::search(const QString& query) const {
    if (query.trimmed().isEmpty())
        return {};

    const QString q = query.trimmed().toLower();

    struct Scored { int score; const ScreenCommand* cmd; };
    QVector<Scored> scored;

    for (const auto& cmd : commands_) {
        int score = 0;
        for (const auto& alias : cmd.aliases)
            if (alias.toLower() == q) { score = 100; break; }
        if (!score) {
            for (const auto& alias : cmd.aliases)
                if (alias.toLower().startsWith(q)) { score = std::max(score, 80); }
            if (cmd.name.toLower().contains(q))      score = std::max(score, 70);
            for (const auto& alias : cmd.aliases)
                if (alias.toLower().contains(q))     score = std::max(score, 60);
            if (cmd.description.toLower().contains(q)) score = std::max(score, 40);
            for (const auto& kw : cmd.keywords)
                if (kw.toLower().contains(q))        score = std::max(score, 30);
        }
        if (score > 0)
            scored.append({score, &cmd});
    }

    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        return a.score > b.score;
    });

    QVector<ScreenCommand> result;
    for (int i = 0; i < std::min((int)scored.size(), kMaxResults); ++i)
        result.append(*scored[i].cmd);
    return result;
}

// ── constructor ──────────────────────────────────────────────────────────────

CommandBar::CommandBar(QWidget* parent) : QWidget(parent) {
    build_commands();
    build_asset_types();
    setFixedHeight(32);

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 4, 0, 4);
    hl->setSpacing(0);

    input_ = new QLineEdit(this);
    input_->setFixedHeight(24);
    input_->setPlaceholderText("> Enter Command or /type ...");
    input_->setStyleSheet(INPUT_SS);
    input_->installEventFilter(this);
    hl->addWidget(input_);

    // Dropdown — parented to top-level window so it floats above everything
    dropdown_ = new QFrame(window(), Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    dropdown_->setStyleSheet(DROP_SS);
    dropdown_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    dropdown_->setAttribute(Qt::WA_ShowWithoutActivating);

    auto* vl = new QVBoxLayout(dropdown_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    list_ = new QListWidget(dropdown_);
    list_->setStyleSheet(LIST_SS);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list_->setMouseTracking(true);
    list_->setFocusPolicy(Qt::NoFocus);
    vl->addWidget(list_);
    dropdown_->hide();

    // Debounce timer for API search
    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    connect(search_debounce_, &QTimer::timeout, this, [this]() {
        if (!pending_query_.isEmpty())
            fire_asset_search(pending_query_);
    });

    connect(input_, &QLineEdit::textChanged, this, &CommandBar::on_text_changed);
    connect(input_, &QLineEdit::returnPressed, this, &CommandBar::on_return_pressed);
    connect(list_, &QListWidget::itemClicked, this, &CommandBar::on_item_clicked);
    connect(list_, &QListWidget::itemEntered, this, &CommandBar::on_item_entered);

    // Hide dropdown when app loses focus
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
        if (!now || (now != input_ && !dropdown_->isAncestorOf(now) && now != list_))
            hide_dropdown();
    });
}

// ── event filter (keyboard nav in input) ─────────────────────────────────────

bool CommandBar::eventFilter(QObject* obj, QEvent* event) {
    if (obj != input_ || event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, event);

    auto* ke = static_cast<QKeyEvent*>(event);
    const int count = list_->count();

    switch (ke->key()) {
    case Qt::Key_Down:
        if (count > 0) {
            int next = (list_->currentRow() + 1) % count;
            list_->setCurrentRow(next);
        }
        return true;

    case Qt::Key_Up:
        if (count > 0) {
            int prev = (list_->currentRow() - 1 + count) % count;
            list_->setCurrentRow(prev);
        }
        return true;

    case Qt::Key_Tab:
        if (list_->currentItem()) {
            const QString autocomplete = list_->currentItem()->data(Qt::UserRole + 1).toString();
            input_->setText(autocomplete);
            // If we just autocompleted a slash type, activate asset mode
            if (mode_ == Mode::SlashPicker) {
                for (const auto& at : asset_types_) {
                    if (at.slash == autocomplete) {
                        activate_asset_mode(at.api_type);
                        // Put cursor after "/type " so user can type the query
                        input_->setText(autocomplete + " ");
                        input_->setCursorPosition(input_->text().length());
                        break;
                    }
                }
            } else {
                hide_dropdown();
            }
        }
        return true;

    case Qt::Key_Escape:
        input_->clear();
        mode_ = Mode::Screen;
        active_asset_type_.clear();
        search_debounce_->stop();
        hide_dropdown();
        input_->clearFocus();
        return true;

    default:
        break;
    }
    return QWidget::eventFilter(obj, event);
}

// ── slots ────────────────────────────────────────────────────────────────────

void CommandBar::on_text_changed(const QString& text) {
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty()) {
        mode_ = Mode::Screen;
        active_asset_type_.clear();
        search_debounce_->stop();
        hide_dropdown();
        return;
    }

    // ── Check if user is in asset search mode (e.g. "/stock AAPL") ──────────
    if (mode_ == Mode::AssetSearch && !active_asset_type_.isEmpty()) {
        // Find the space after the slash-type prefix
        const int space_idx = trimmed.indexOf(' ');
        if (space_idx < 0) {
            // User deleted back to just "/stock" — revert to slash picker
            if (trimmed.startsWith("/")) {
                mode_ = Mode::SlashPicker;
                show_slash_suggestions(trimmed);
            } else {
                mode_ = Mode::Screen;
                active_asset_type_.clear();
            }
            return;
        }
        const QString query = trimmed.mid(space_idx + 1).trimmed();
        if (query.isEmpty()) {
            // Just "/stock " with nothing after — show hint
            list_->clear();
            auto* item = new QListWidgetItem(list_);
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            auto* row = new QWidget;
            row->setStyleSheet("background:transparent;");
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(10, 6, 10, 6);
            auto* hint = new QLabel(QString("Type a symbol or name to search %1s...").arg(active_asset_type_));
            hint->setStyleSheet("color:#525252;font-size:11px;font-family:'Consolas',monospace;background:transparent;");
            rl->addWidget(hint);
            item->setSizeHint(QSize(0, 30));
            list_->setItemWidget(item, row);
            show_dropdown();
            return;
        }
        schedule_asset_search(query);
        return;
    }

    // ── Slash prefix detection ──────────────────────────────────────────────
    if (trimmed.startsWith("/")) {
        // Check if the user has completed a type and started typing a query
        // e.g. "/stock AAPL" — the space separates type from query
        const int space_idx = trimmed.indexOf(' ');
        if (space_idx > 0) {
            const QString slash_part = trimmed.left(space_idx).toLower();
            for (const auto& at : asset_types_) {
                if (at.slash == slash_part) {
                    activate_asset_mode(at.api_type);
                    const QString query = trimmed.mid(space_idx + 1).trimmed();
                    if (!query.isEmpty())
                        schedule_asset_search(query);
                    return;
                }
            }
        }
        // Still picking the slash type
        mode_ = Mode::SlashPicker;
        show_slash_suggestions(trimmed);
        return;
    }

    // ── Normal screen command search ────────────────────────────────────────
    mode_ = Mode::Screen;
    active_asset_type_.clear();

    const auto results = search(text);
    if (results.isEmpty()) {
        hide_dropdown();
        return;
    }

    list_->clear();
    for (const auto& cmd : results) {
        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, cmd.id);
        item->setData(Qt::UserRole + 1, cmd.aliases.first());

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 5, 10, 5);
        hl->setSpacing(6);

        auto* alias_lbl = new QLabel(cmd.aliases.first().toUpper());
        alias_lbl->setStyleSheet("color:#e5e5e5;font-size:11px;font-weight:700;"
                                 "font-family:'Consolas',monospace;background:transparent;");
        alias_lbl->setFixedWidth(72);

        auto* sep_lbl = new QLabel(QStringLiteral("\u203A"));
        sep_lbl->setStyleSheet("color:#525252;font-size:12px;background:transparent;");

        auto* name_lbl = new QLabel(cmd.name);
        name_lbl->setStyleSheet("color:#a3a3a3;font-size:11px;background:transparent;"
                                "font-family:'Consolas',monospace;");

        hl->addWidget(alias_lbl);
        hl->addWidget(sep_lbl);
        hl->addWidget(name_lbl, 1);

        if (!cmd.shortcut.isEmpty()) {
            auto* sc_lbl = new QLabel(cmd.shortcut);
            sc_lbl->setStyleSheet("color:#404040;font-size:10px;font-family:'Consolas',monospace;"
                                  "background:transparent;");
            hl->addWidget(sc_lbl);
        }

        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
    }

    list_->setCurrentRow(0);
    show_dropdown();
}

void CommandBar::on_return_pressed() {
    if (!dropdown_->isVisible() || !list_->currentItem())  {
        // In screen mode, try direct alias lookup
        if (mode_ == Mode::Screen) {
            const auto results = search(input_->text());
            if (!results.isEmpty()) {
                emit navigate_to(results.first().id);
                input_->clear();
                mode_ = Mode::Screen;
                hide_dropdown();
            }
        }
        return;
    }

    auto* item = list_->currentItem();

    if (mode_ == Mode::SlashPicker) {
        // User pressed Enter on a slash-type suggestion — activate that asset mode
        const QString slash = item->data(Qt::UserRole + 1).toString();
        for (const auto& at : asset_types_) {
            if (at.slash == slash) {
                input_->setText(slash + " ");
                input_->setCursorPosition(input_->text().length());
                activate_asset_mode(at.api_type);
                return;
            }
        }
        return;
    }

    if (mode_ == Mode::AssetSearch) {
        // User selected an asset result — navigate
        const QString symbol = item->data(Qt::UserRole).toString();
        const QString type = item->data(Qt::UserRole + 2).toString();
        if (!symbol.isEmpty()) {
            select_asset(symbol, type);
        }
        return;
    }

    // Screen mode
    execute_index(list_->currentRow());
}

void CommandBar::on_item_clicked(QListWidgetItem* item) {
    if (!item) return;

    if (mode_ == Mode::SlashPicker) {
        const QString slash = item->data(Qt::UserRole + 1).toString();
        for (const auto& at : asset_types_) {
            if (at.slash == slash) {
                input_->setText(slash + " ");
                input_->setCursorPosition(input_->text().length());
                activate_asset_mode(at.api_type);
                input_->setFocus();
                return;
            }
        }
        return;
    }

    if (mode_ == Mode::AssetSearch) {
        const QString symbol = item->data(Qt::UserRole).toString();
        const QString type = item->data(Qt::UserRole + 2).toString();
        if (!symbol.isEmpty())
            select_asset(symbol, type);
        return;
    }

    // Screen mode
    emit navigate_to(item->data(Qt::UserRole).toString());
    input_->clear();
    mode_ = Mode::Screen;
    hide_dropdown();
}

void CommandBar::on_item_entered(QListWidgetItem* item) {
    list_->setCurrentItem(item);
}

// ── slash-command helpers ────────────────────────────────────────────────────

void CommandBar::show_slash_suggestions(const QString& partial) {
    const QString q = partial.toLower();

    list_->clear();
    for (const auto& at : asset_types_) {
        if (!at.slash.startsWith(q) && q != "/")
            continue;

        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, at.api_type);
        item->setData(Qt::UserRole + 1, at.slash);

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 5, 10, 5);
        hl->setSpacing(8);

        auto* slash_lbl = new QLabel(at.slash);
        slash_lbl->setStyleSheet("color:#d97706;font-size:12px;font-weight:700;"
                                 "font-family:'Consolas',monospace;background:transparent;");
        slash_lbl->setFixedWidth(80);

        auto* sep_lbl = new QLabel(QStringLiteral("\u203A"));
        sep_lbl->setStyleSheet("color:#525252;font-size:12px;background:transparent;");

        auto* desc_lbl = new QLabel(at.description);
        desc_lbl->setStyleSheet("color:#a3a3a3;font-size:11px;background:transparent;"
                                "font-family:'Consolas',monospace;");

        hl->addWidget(slash_lbl);
        hl->addWidget(sep_lbl);
        hl->addWidget(desc_lbl, 1);

        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
    }

    if (list_->count() > 0) {
        list_->setCurrentRow(0);
        show_dropdown();
    } else {
        hide_dropdown();
    }
}

void CommandBar::activate_asset_mode(const QString& api_type) {
    mode_ = Mode::AssetSearch;
    active_asset_type_ = api_type;
    search_debounce_->stop();
}

void CommandBar::schedule_asset_search(const QString& query) {
    pending_query_ = query;
    search_debounce_->start(kSearchDebounceMs);
}

void CommandBar::fire_asset_search(const QString& query) {
    const QString url = QString("/market/search?q=%1&type=%2&limit=%3")
                            .arg(query, active_asset_type_)
                            .arg(kMaxResults);

    QPointer<CommandBar> self = this;
    HttpClient::instance().get(url, [self, query](Result<QJsonDocument> result) {
        if (!self) return;
        // Only process if user hasn't changed the query since we fired
        if (self->pending_query_ != query) return;
        if (!result.is_ok()) return;

        const auto doc = result.value();
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            // API might wrap results in { "results": [...] } or { "data": [...] }
            const auto obj = doc.object();
            if (obj.contains("results"))
                arr = obj["results"].toArray();
            else if (obj.contains("data"))
                arr = obj["data"].toArray();
        }
        self->on_asset_results(arr);
    });
}

void CommandBar::on_asset_results(const QJsonArray& results) {
    list_->clear();

    if (results.isEmpty()) {
        auto* item = new QListWidgetItem(list_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 6, 10, 6);
        auto* lbl = new QLabel("No results found");
        lbl->setStyleSheet("color:#525252;font-size:11px;font-family:'Consolas',monospace;background:transparent;");
        rl->addWidget(lbl);
        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
        show_dropdown();
        return;
    }

    for (const auto& val : results) {
        const auto obj = val.toObject();
        const QString symbol   = obj["symbol"].toString();
        const QString name     = obj["name"].toString();
        const QString exchange = obj["exchange"].toString();
        const QString country  = obj["country"].toString();
        const QString type     = obj["type"].toString(active_asset_type_);

        if (symbol.isEmpty()) continue;

        // Convert to yfinance-compatible ticker (e.g. RELIANCE → RELIANCE.NS)
        const QString yf_symbol = to_yfinance_symbol(symbol, exchange, country);

        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, yf_symbol);               // yfinance symbol for loading
        item->setData(Qt::UserRole + 1, yf_symbol);           // for Tab autocomplete
        item->setData(Qt::UserRole + 2, type);                // asset type

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 4, 10, 4);
        hl->setSpacing(8);

        auto* sym_lbl = new QLabel(yf_symbol);
        sym_lbl->setStyleSheet("color:#e5e5e5;font-size:12px;font-weight:700;"
                               "font-family:'Consolas',monospace;background:transparent;");
        sym_lbl->setFixedWidth(110);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet("color:#a3a3a3;font-size:11px;background:transparent;"
                                "font-family:'Consolas',monospace;");
        name_lbl->setMaximumWidth(200);

        hl->addWidget(sym_lbl);
        hl->addWidget(name_lbl, 1);

        if (!exchange.isEmpty()) {
            auto* exch_lbl = new QLabel(exchange);
            exch_lbl->setStyleSheet("color:#525252;font-size:10px;font-family:'Consolas',monospace;"
                                    "background:transparent;");
            hl->addWidget(exch_lbl);
        }

        // Type badge
        auto* type_lbl = new QLabel(type.toUpper());
        type_lbl->setStyleSheet("color:#d97706;font-size:9px;font-weight:700;"
                                "font-family:'Consolas',monospace;background:#1a1a1a;"
                                "padding:1px 4px;border-radius:2px;");
        hl->addWidget(type_lbl);

        item->setSizeHint(QSize(0, 32));
        list_->setItemWidget(item, row);
    }

    if (list_->count() > 0) {
        list_->setCurrentRow(0);
        show_dropdown();
    }
}

void CommandBar::select_asset(const QString& symbol, const QString& type) {
    input_->clear();
    mode_ = Mode::Screen;
    active_asset_type_.clear();
    search_debounce_->stop();
    hide_dropdown();
    input_->clearFocus();

    // Navigate to equity_research and tell it to load the symbol
    emit navigate_to("equity_research");
    EventBus::instance().publish("equity_research.load_symbol", {
        {"symbol", symbol},
        {"type", type},
    });
}

// ── dropdown helpers ─────────────────────────────────────────────────────────

void CommandBar::execute_index(int index) {
    auto* item = list_->item(index);
    if (!item) return;
    emit navigate_to(item->data(Qt::UserRole).toString());
    input_->clear();
    mode_ = Mode::Screen;
    active_asset_type_.clear();
    hide_dropdown();
    input_->clearFocus();
}

void CommandBar::show_dropdown() {
    update_position();
    dropdown_->show();
    dropdown_->raise();
}

void CommandBar::hide_dropdown() {
    dropdown_->hide();
}

void CommandBar::update_position() {
    const QPoint global_pos = input_->mapToGlobal(QPoint(0, input_->height()));
    const int w = std::max(input_->width(), 360);
    const int rows = list_->count();
    const int h = std::min(rows * 32, 320);

    dropdown_->setGeometry(global_pos.x(), global_pos.y(), w, h);
}

} // namespace fincept::ui
