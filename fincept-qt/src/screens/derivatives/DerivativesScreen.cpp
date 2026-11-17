#include "screens/derivatives/DerivativesScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolRef.h"
#include "screens/derivatives/DerivBookPage.h"
#include "screens/derivatives/DerivChainPage.h"
#include "screens/derivatives/DerivCommon.h"
#include "screens/derivatives/DerivContext.h"
#include "screens/derivatives/DerivCreditPage.h"
#include "screens/derivatives/DerivCurvesPage.h"
#include "screens/derivatives/DerivPricerPage.h"
#include "screens/derivatives/DerivScenarioPage.h"
#include "screens/derivatives/DerivStrategyPage.h"
#include "services/options/FreeOptionChainService.h"
#include "trading/AccountManager.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/ThemeIcon.h"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::screens::deriv;
namespace bk = fincept::services::options::book;

namespace {

/// Contract size of the futures roots the CURVES page lists (prefills the book's add dialog;
/// the user can change it there). 0 = unknown.
double ds_contract_size(const QString& root) {
    static const QHash<QString, double> kSizes = {
        {QStringLiteral("CL"), 1000},   {QStringLiteral("BZ"), 1000},  {QStringLiteral("NG"), 10000},
        {QStringLiteral("RB"), 42000},  {QStringLiteral("HO"), 42000}, {QStringLiteral("GC"), 100},
        {QStringLiteral("SI"), 5000},   {QStringLiteral("HG"), 25000}, {QStringLiteral("ZC"), 5000},
        {QStringLiteral("ZS"), 5000},   {QStringLiteral("ZW"), 5000},  {QStringLiteral("ES"), 50},
        {QStringLiteral("NQ"), 20},     {QStringLiteral("YM"), 5},     {QStringLiteral("RTY"), 50},
        {QStringLiteral("ZN"), 1000},   {QStringLiteral("ZB"), 1000},  {QStringLiteral("6E"), 125000},
        {QStringLiteral("6J"), 12500000}, {QStringLiteral("BTC"), 5},
    };
    return kSizes.value(root.trimmed().toUpper(), 0.0);
}

/// Deep-link page ids, in DerivativesScreen::Page order.
const char* const kDsPageIds[] = {"pricer", "chain", "strategy", "scenarios", "curves", "credit", "book"};

/// The pricer calculator a deep link names (DerivPricerPage order: bonds, equity options, FX
/// options, IR swaps, CDS); -1 = none.
int ds_pricer_instrument(const QString& sub) {
    static const QHash<QString, int> kIds = {
        {QStringLiteral("bond"), 0},          {QStringLiteral("bonds"), 0},
        {QStringLiteral("equity"), 1},        {QStringLiteral("equity_option"), 1},
        {QStringLiteral("equity_options"), 1}, {QStringLiteral("fx"), 2},
        {QStringLiteral("fx_option"), 2},     {QStringLiteral("fx_options"), 2},
        {QStringLiteral("swap"), 3},          {QStringLiteral("swaps"), 3},
        {QStringLiteral("ir_swap"), 3},       {QStringLiteral("ir_swaps"), 3},
        {QStringLiteral("irs"), 3},           {QStringLiteral("cds"), 4},
        {QStringLiteral("credit"), 4},
    };
    return kIds.value(sub, -1);
}

} // namespace

DerivativesScreen::DerivativesScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("derivScreen"));
    setStyleSheet(deriv::screen_style()); // once, before any child exists
    ctx_ = new DerivContext(this);
    setup_ui();
    LOG_DEBUG("Derivatives", "Screen constructed");
}

void DerivativesScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(create_header_bar()); // one line: title, the page segments, chip / refresh / F&O

    // One holder per page; a page is built inside its holder the first time it is shown.
    pages_ = new QStackedWidget(this);
    for (int i = 0; i < kPageCount; ++i) {
        auto* holder = new QWidget(pages_);
        auto* hl = new QVBoxLayout(holder);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        // Only the page in view counts for the stack's size (show_page), so a tall page never
        // makes the screen's minimum height, or another page's spare height, its own.
        holder->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        pages_->addWidget(holder);
        holders_.append(holder);
    }
    root->addWidget(pages_, 1);
    root->addWidget(create_status_bar());

    // Hand-offs between pages.
    connect(ctx_, &DerivContext::price_contract_requested, this, [this](const ContractRef& c) {
        show_page(kPricer);
        pricer_->load_contract(c);
    });
    connect(ctx_, &DerivContext::legs_requested, this, [this](const QVector<LegView>& legs) {
        show_page(kStrategy);
        strategy_->add_legs(legs);
    });
    connect(ctx_, &DerivContext::scenarios_requested, this, [this] {
        show_page(kScenarios);
        scenarios_->use_strategy_position();
    });
    connect(ctx_, &DerivContext::scenarios_contract_requested, this, [this](const ContractRef& c) {
        show_page(kScenarios);
        scenarios_->load_contract(c);
    });
    connect(ctx_, &DerivContext::book_scenarios_requested, this, [this] {
        show_page(kScenarios);
        scenarios_->use_book_position();
    });
    connect(ctx_, &DerivContext::book_add_requested, this, [this](const bk::BookPosition& draft) {
        show_page(kBook);
        book_->add_draft(draft);
    });
    connect(ctx_, &DerivContext::strategy_load_requested, this,
            [this](const QString& symbol, const QString& name, const QVector<bk::BookLeg>& legs) {
                show_page(kStrategy);
                strategy_->load_legs(symbol, name, legs);
            });

    // The F&O link follows the broker connections (read-only queries of the account manager).
    auto& accounts = trading::AccountManager::instance();
    connect(&accounts, &trading::AccountManager::connection_state_changed, this, [this] { update_fno_link(); });
    connect(&accounts, &trading::AccountManager::account_added, this, [this] { update_fno_link(); });
    connect(&accounts, &trading::AccountManager::account_removed, this, [this] { update_fno_link(); });
    connect(&accounts, &trading::AccountManager::account_updated, this, [this] { update_fno_link(); });
    update_fno_link();

    // Keyboard: Ctrl+1..4 switch pages, F5 refreshes the page in view.
    for (int i = 0; i < kPageCount; ++i) {
        auto* sc = new QShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this, i] { show_page(i); });
    }
    auto* f5 = new QShortcut(QKeySequence(Qt::Key_F5), this);
    f5->setContext(Qt::WidgetWithChildrenShortcut);
    connect(f5, &QShortcut::activated, this, &DerivativesScreen::refresh_current);

    show_page(kPricer, false); // the default page; it holds the persisted state

    // A new font size: the header's items have new widths (after every widget has its style).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this] { fit_header(); });
}

QWidget* DerivativesScreen::create_header_bar() {
    // One line instead of a two-line title block over a separate page row: the title, the page
    // segments (create_page_bar), then the engine chip, refresh and the F&O link on the right.
    // The sub-page list the title block used to repeat under it is the title's tooltip now.
    auto* bar = new QWidget(this);
    header_bar_ = bar;
    bar->setObjectName(QStringLiteral("derivHeader"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 2, 12, 2);
    hl->setSpacing(8);
    header_title_ = new QLabel(bar);
    header_title_->setObjectName(QStringLiteral("derivHeaderTitle"));
    hl->addWidget(header_title_, 0, Qt::AlignVCenter);
    header_sub_ = new QLabel(bar);
    header_sub_->setObjectName(QStringLiteral("derivHeaderSub"));
    header_sub_->setVisible(false); // its text is the title's tooltip (built once, never toggled)
    hl->addWidget(create_page_bar(), 0, Qt::AlignVCenter);
    hl->addStretch(1);
    header_badge_ = new QLabel(bar);
    header_badge_->setObjectName(QStringLiteral("derivHeaderBadge"));
    hl->addWidget(header_badge_, 0, Qt::AlignVCenter);
    refresh_btn_ = new QToolButton(bar);
    refresh_btn_->setObjectName(QStringLiteral("derivHeaderIcon"));
    refresh_btn_->setIcon(ui::theme_icon(ui::ThemeGlyph::Refresh)); // follows the theme (no emoji, no font glyph)
    refresh_btn_->setAutoRaise(true);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QToolButton::clicked, this, &DerivativesScreen::refresh_current);
    hl->addWidget(refresh_btn_, 0, Qt::AlignVCenter);
    fno_btn_ = new QPushButton(bar);
    fno_btn_->setObjectName(QStringLiteral("derivLinkBtn"));
    fno_btn_->setCursor(Qt::PointingHandCursor);
    connect(fno_btn_, &QPushButton::clicked, this, [] {
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"), {{QStringLiteral("screen_id"), QStringLiteral("fno")}});
    });
    hl->addWidget(fno_btn_, 0, Qt::AlignVCenter);
    return bar;
}

QWidget* DerivativesScreen::create_page_bar() {
    // The page segments, inside the header line (create_header_bar).
    auto* bar = new QWidget(header_bar_ ? header_bar_ : this);
    page_bar_ = bar;
    bar->setObjectName(QStringLiteral("derivPageBar"));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(2);
    for (int i = 0; i < kPageCount; ++i) {
        auto* b = new QPushButton(bar);
        b->setObjectName(QStringLiteral("derivPageBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("active", i == 0);
        connect(b, &QPushButton::clicked, this, [this, i] { show_page(i); });
        hl->addWidget(b);
        page_btns_.append(b);
    }
    return bar;
}

void DerivativesScreen::fit_header() {
    // Narrow window or a large font: the engine chip gives way first, then the title; the page
    // segments, refresh and the F&O link always stay. Widths from the items' own size hints.
    if (!header_bar_ || !page_bar_)
        return;
    const auto* hl = header_bar_->layout();
    const QMargins m = hl->contentsMargins();
    const int gap = hl->spacing();
    const int fixed = m.left() + m.right() + page_bar_->sizeHint().width() + refresh_btn_->sizeHint().width() +
                      fno_btn_->sizeHint().width() + 3 * gap;
    const int title = header_title_->sizeHint().width() + gap;
    const int badge = header_badge_->sizeHint().width() + gap;
    const int avail = header_bar_->width();
    const bool show_badge = avail >= fixed + title + badge;
    const bool show_title = avail >= fixed + title;
    if (header_badge_->isVisibleTo(header_bar_) != show_badge)
        header_badge_->setVisible(show_badge);
    if (header_title_->isVisibleTo(header_bar_) != show_title)
        header_title_->setVisible(show_title);
}

void DerivativesScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    fit_header();
}

QWidget* DerivativesScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("derivStatusBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 2, 12, 2);
    status_left_ = new QLabel(bar);
    status_left_->setObjectName(QStringLiteral("derivStatusText"));
    hl->addWidget(status_left_);
    hl->addStretch(1);
    status_page_ = new QLabel(bar);
    status_page_->setObjectName(QStringLiteral("derivStatusText"));
    hl->addWidget(status_page_);
    hl->addSpacing(16);
    status_engine_ = new QLabel(bar);
    status_engine_->setObjectName(QStringLiteral("derivStatusEngine"));
    hl->addWidget(status_engine_);
    retranslate();
    return bar;
}

QString DerivativesScreen::page_name(int index) const {
    switch (index) {
        case kPricer: return tr("PRICER");
        case kChain: return tr("CHAIN & VOL");
        case kStrategy: return tr("STRATEGY");
        case kScenarios: return tr("SCENARIOS");
        case kCurves: return tr("CURVES");
        case kCredit: return tr("CREDIT");
        case kBook: return tr("BOOK");
        default: return {};
    }
}

QWidget* DerivativesScreen::ensure_page(int index) {
    QWidget* holder = holders_.value(index);
    if (!holder)
        return nullptr;
    QWidget* page = nullptr;
    switch (index) {
        case kPricer:
            if (!pricer_) {
                pricer_ = new DerivPricerPage(ctx_, holder);
                connect(pricer_, &DerivPricerPage::state_changed, this,
                        [this] { ScreenStateManager::instance().notify_changed(this); });
                connect(pricer_, &DerivPricerPage::instrument_changed, this, [this](const QString& name) {
                    pricer_instrument_ = name;
                    if (page_ == kPricer)
                        status_page_->setText(tr("PRICER · %1").arg(name));
                });
                pricer_instrument_ = pricer_->instrument_name(pricer_->active_instrument());
                page = pricer_;
            }
            break;
        case kChain:
            if (!chain_)
                page = chain_ = new DerivChainPage(ctx_, holder);
            break;
        case kStrategy:
            if (!strategy_)
                page = strategy_ = new DerivStrategyPage(ctx_, holder);
            break;
        case kScenarios:
            if (!scenarios_)
                page = scenarios_ = new DerivScenarioPage(ctx_, holder);
            break;
        case kCurves:
            if (!curves_) {
                page = curves_ = new DerivCurvesPage(holder);
                connect(curves_, &DerivCurvesPage::add_swap_to_book, this,
                        [this](double start, double tenor, double rate, int freq, double notional, bool payer) {
                            bk::BookPosition p;
                            p.kind = QStringLiteral("swap");
                            p.underlying = QStringLiteral("USD rates");
                            p.multiplier = 1;
                            p.entry_date = QDate::currentDate();
                            bk::BookLeg l;
                            l.type = bk::LegType::Swap;
                            l.quantity = 1;
                            l.swap_start = start;
                            l.swap_tenor = tenor;
                            l.swap_rate = rate;
                            l.swap_freq = freq;
                            l.notional = notional;
                            l.payer = payer;
                            p.legs = {l};
                            emit ctx_->book_add_requested(p);
                        });
                connect(curves_, &DerivCurvesPage::add_future_to_book, this,
                        [this](const QString& contract, const QString& root, double price) {
                            bk::BookPosition p;
                            p.kind = QStringLiteral("future");
                            p.underlying = root;
                            const double size = ds_contract_size(root);
                            p.multiplier = size > 0 ? size : 1.0;
                            p.entry_date = QDate::currentDate();
                            bk::BookLeg l;
                            l.type = bk::LegType::Future;
                            l.quantity = 1;
                            l.contract = contract;
                            l.entry_price = price;
                            p.legs = {l};
                            emit ctx_->book_add_requested(p);
                        });
            }
            break;
        case kCredit:
            if (!credit_)
                page = credit_ = new DerivCreditPage(holder);
            break;
        case kBook:
            if (!book_)
                page = book_ = new DerivBookPage(ctx_, holder);
            break;
        default:
            break;
    }
    if (page)
        holder->layout()->addWidget(page);
    return holder;
}

void DerivativesScreen::show_page(int index, bool persist) {
    if (index < 0 || index >= kPageCount)
        return;
    ensure_page(index);
    page_ = index;
    for (int i = 0; i < holders_.size(); ++i) {
        const QSizePolicy::Policy p = i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored;
        if (holders_[i]->sizePolicy().verticalPolicy() != p)
            holders_[i]->setSizePolicy(p, p);
    }
    pages_->setCurrentIndex(index);
    for (int i = 0; i < page_btns_.size(); ++i) {
        page_btns_[i]->setProperty("active", i == index);
        page_btns_[i]->style()->unpolish(page_btns_[i]);
        page_btns_[i]->style()->polish(page_btns_[i]);
    }
    status_page_->setText(index == kPricer && !pricer_instrument_.isEmpty()
                              ? tr("PRICER · %1").arg(pricer_instrument_)
                              : page_name(index));
    if (persist)
        ScreenStateManager::instance().notify_changed(this);
}

void DerivativesScreen::refresh_current() {
    switch (page_) {
        case kChain:
            if (chain_)
                chain_->refresh();
            break;
        case kStrategy:
            if (strategy_)
                strategy_->refresh();
            break;
        case kScenarios:
            if (scenarios_)
                scenarios_->refresh();
            break;
        case kCurves:
            if (curves_)
                curves_->refresh();
            break;
        case kCredit:
            if (credit_)
                credit_->refresh();
            break;
        case kBook:
            if (book_)
                book_->refresh();
            break;
        default:
            if (pricer_)
                pricer_->refresh();
            break;
    }
}

// ── Language ─────────────────────────────────────────────────────────────────

void DerivativesScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void DerivativesScreen::retranslate() {
    if (header_title_)
        header_title_->setText(tr("DERIVATIVES"));
    if (header_sub_) {
        header_sub_->setText(tr("PRICING · FREE DELAYED OPTION CHAINS · STRATEGIES · SCENARIOS · CURVES · CREDIT · BOOK"));
        if (header_title_)
            header_title_->setToolTip(header_sub_->text());
    }
    if (header_badge_)
        header_badge_->setText(tr("LOCAL ENGINE"));
    if (refresh_btn_)
        refresh_btn_->setToolTip(tr("Recalculate or reload the page in view (F5)"));
    if (fno_btn_) {
        fno_btn_->setText(tr("TRADE IN F&&O"));
        update_fno_link(); // its tooltip depends on the broker connection
    }
    for (int i = 0; i < page_btns_.size(); ++i) {
        page_btns_[i]->setText(ui::literal_ampersands(page_name(i))); // "CHAIN & VOL", not a mnemonic
        page_btns_[i]->setToolTip(tr("Ctrl+%1").arg(i + 1));
        page_btns_[i]->setAccessibleName(page_name(i));
    }
    fit_header();
    if (status_left_)
        status_left_->setText(tr("DERIVATIVES"));
    if (status_engine_)
        status_engine_->setText(tr("C++ ENGINE · FREE DELAYED DATA"));
    if (status_page_ && !page_btns_.isEmpty())
        status_page_->setText(page_ == kPricer && !pricer_instrument_.isEmpty() ? tr("PRICER · %1").arg(pricer_instrument_)
                                                                               : page_name(page_));
}

// ── Broker link / symbols from other tabs ────────────────────────────────────

void DerivativesScreen::update_fno_link() {
    if (!fno_btn_)
        return;
    // Read-only: is any active broker account connected? (The built-in paper account is not a
    // broker and is never in active_accounts().)
    auto& am = trading::AccountManager::instance();
    bool connected = false;
    for (const trading::BrokerAccount& a : am.active_accounts())
        if (am.connection_state(a.account_id) == trading::ConnectionState::Connected) {
            connected = true;
            break;
        }
    fno_btn_->setEnabled(connected);
    fno_btn_->setToolTip(connected ? tr("Live trading with your connected broker happens in the F&O tab")
                                   : tr("Connect a broker in Settings to trade F&O"));
}

void DerivativesScreen::open_chain_symbol(const QString& symbol) {
    last_symbol_ = symbol;
    ensure_page(kChain);
    chain_->open_symbol(symbol);
    if (page_ == kPricer || page_ == kChain)
        show_page(kChain);
}

void DerivativesScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    // Options are listed on stocks, ETFs and indices only.
    if (!ref.is_valid() || (!ref.asset_class.isEmpty() && ref.asset_class != QLatin1String("equity") &&
                            ref.asset_class != QLatin1String("etf") && ref.asset_class != QLatin1String("index")))
        return;
    const QString sym = ref.symbol.trimmed();
    if (sym.isEmpty())
        return;
    if (!isVisible()) {
        pending_symbol_ = sym; // nothing loads behind the user's back: applied when the tab is shown
        return;
    }
    pending_symbol_.clear();
    open_chain_symbol(sym);
}

fincept::SymbolRef DerivativesScreen::current_symbol() const {
    QString s = last_symbol_;
    if (s.isEmpty() && ctx_ && ctx_->chain())
        s = ctx_->chain()->symbol;
    return s.isEmpty() ? fincept::SymbolRef{} : fincept::SymbolRef::equity(s);
}

void DerivativesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!pending_symbol_.isEmpty()) {
        const QString sym = pending_symbol_;
        pending_symbol_.clear();
        open_chain_symbol(sym);
    }
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

bool DerivativesScreen::open_section(const QString& section, const QString& focus) {
    // The page segments' own path (show_page), the pricer's instrument buttons and the chain's
    // "open underlying" (open_chain_symbol, as for a linked symbol). Validated before anything moves.
    QString key = section.trimmed().toLower();
    QString sub;
    const qsizetype colon = key.indexOf(QLatin1Char(':'));
    const qsizetype sep = colon >= 0 ? colon : key.indexOf(QLatin1Char('/'));
    if (sep >= 0) {
        sub = key.mid(sep + 1).trimmed();
        key = key.left(sep).trimmed();
    }
    const QString symbol = focus.trimmed();
    if (key.isEmpty() && sub.isEmpty()) {
        if (!symbol.isEmpty())
            open_chain_symbol(symbol); // on the page in view, like a linked symbol
        return true;
    }
    int page = -1;
    for (int i = 0; i < kPageCount; ++i)
        if (key == QLatin1String(kDsPageIds[i]))
            page = i;
    if (page < 0)
        return false;
    int instrument = -1;
    if (sep >= 0) {
        if (page != kPricer || (instrument = ds_pricer_instrument(sub)) < 0)
            return false;
    }
    show_page(page);
    if (instrument >= 0 && pricer_)
        pricer_->select_instrument(instrument);
    if (page == kChain && !symbol.isEmpty())
        open_chain_symbol(symbol);
    return true;
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap DerivativesScreen::save_state() const {
    // The pricer's inputs under the long-standing keys, plus the open page.
    QVariantMap s = pricer_ ? pricer_->save_state() : QVariantMap{};
    s.insert(QStringLiteral("page"), page_);
    return s;
}

void DerivativesScreen::restore_state(const QVariantMap& state) {
    if (pricer_)
        pricer_->restore_state(state);
    const int page = state.value(QStringLiteral("page"), static_cast<int>(kPricer)).toInt();
    if (page > kPricer && page < kPageCount)
        show_page(page);
}

} // namespace fincept::screens
