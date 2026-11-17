#include "screens/derivatives/DerivBookPage.h"

#include "core/logging/Logger.h"
#include "services/markets/MarketDataService.h"
#include "services/options/CurveDataService.h"
#include "services/options/DerivBookStore.h"
#include "services/options/FreeOptionChainService.h"
#include "ui/charts/BarChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace fincept::screens::deriv {

namespace bk = services::options::book;
namespace mk = services::options::market;
namespace fmt = fincept::ui::formatting;
using services::options::CurveDataService;
using services::options::DerivBookStore;
using services::options::FreeOptionChainService;

namespace {

constexpr const char* kDbpCtx = "fincept::screens::deriv::DerivBookPage";

QString dbp_tr(const char* s) {
    return QCoreApplication::translate(kDbpCtx, s);
}

QString dbp_dash() {
    return QStringLiteral("—");
}

QString dbp_date(const QDate& d) {
    return d.isValid() ? d.toString(QStringLiteral("d MMM yy")) : dbp_dash();
}

/// What a position needs fetched: "C:<key>" a chain, "Q:<key>" a spot quote, "F:<root>" futures months.
QStringList dbp_items(const bk::BookPosition& p) {
    QStringList out;
    bool option = false, stock = false, future = false;
    for (const bk::BookLeg& l : p.legs) {
        option = option || l.is_option();
        stock = stock || l.type == bk::LegType::Stock;
        future = future || l.type == bk::LegType::Future;
    }
    const QString key = bk::underlying_key(p.underlying);
    if (key.isEmpty())
        return out;
    if (option)
        out << QStringLiteral("C:") + key;
    else if (stock)
        out << QStringLiteral("Q:") + key;
    if (future)
        out << QStringLiteral("F:") + key;
    return out;
}

bool dbp_has_swap(const bk::BookPosition& p) {
    for (const bk::BookLeg& l : p.legs)
        if (l.type == bk::LegType::Swap)
            return true;
    return false;
}

/// Net premium paid (+) / received (−) at entry for option and stock legs.
double dbp_net_entry(const bk::BookPosition& p) {
    double v = 0;
    for (const bk::BookLeg& l : p.legs) {
        if (l.is_option())
            v += l.quantity * p.multiplier * l.entry_price;
        else if (l.type == bk::LegType::Stock)
            v += l.quantity * l.entry_price;
    }
    return v;
}

QStandardItem* dbp_cell(const QString& text, double sort_value = std::nan("")) {
    auto* it = new QStandardItem(text);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (std::isfinite(sort_value))
        it->setData(sort_value, Qt::UserRole);
    it->setEditable(false);
    return it;
}

QStandardItem* dbp_text_cell(const QString& text) {
    auto* it = new QStandardItem(text);
    it->setEditable(false);
    return it;
}

void dbp_setup_table(QTableView* t) {
    dense_table(t); // zebra, no row numbers, row height from the table font (re-fitted on a font change)
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->horizontalHeader()->setStretchLastSection(true);
}

// ── Add / edit dialog ────────────────────────────────────────────────────────

/// One position: book, name, underlying, the instrument's fields for its kind, entry date
/// and notes. A strategy's legs are shown, not edited (edit them on the STRATEGY page).
class DbpEditDialog : public QDialog {
  public:
    DbpEditDialog(const bk::BookPosition& p, const QStringList& books, bool is_new, QWidget* parent)
        : QDialog(parent), in_(p) {
        setObjectName(QStringLiteral("derivDialog"));
        setWindowTitle(is_new ? dbp_tr("Add a position") : dbp_tr("Edit position"));
        setModal(true);
        const QFontMetrics fm(font());
        setMinimumWidth(fm.horizontalAdvance(QLatin1Char('M')) * 46);
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(16, 14, 16, 14);
        vl->setSpacing(10);

        auto* common = new QWidget(this);
        auto* g = new QGridLayout(common);
        g->setContentsMargins(0, 0, 0, 0);
        g->setHorizontalSpacing(10);
        g->setVerticalSpacing(4);
        book_ = new QComboBox(common);
        book_->setEditable(true);
        book_->addItems(books.isEmpty() ? QStringList{QStringLiteral("Main")} : books);
        book_->setCurrentText(p.book.isEmpty() ? QStringLiteral("Main") : p.book);
        kind_ = make_combo(common, {dbp_tr("Option"), dbp_tr("Stock / ETF"), dbp_tr("Future"), dbp_tr("Interest rate swap"),
                                    dbp_tr("Strategy (legs)")});
        name_ = new QLineEdit(p.name, common);
        name_->setPlaceholderText(dbp_tr("Optional: a name for this position"));
        underlying_ = new QLineEdit(p.underlying, common);
        underlying_->setPlaceholderText(dbp_tr("SPY, ^SPX, AAPL … (futures: the root, e.g. CL)"));
        currency_ = new QLineEdit(p.currency.isEmpty() ? QStringLiteral("USD") : p.currency, common);
        currency_->setMaxLength(3);
        entry_date_ = make_date(common, p.entry_date.isValid() ? p.entry_date : QDate::currentDate());
        add_field(g, 0, 0, dbp_tr("BOOK"), book_);
        add_field(g, 0, 1, dbp_tr("KIND"), kind_);
        add_field(g, 1, 0, dbp_tr("NAME"), name_);
        add_field(g, 1, 1, dbp_tr("UNDERLYING"), underlying_);
        add_field(g, 2, 0, dbp_tr("CURRENCY"), currency_);
        add_field(g, 2, 1, dbp_tr("ENTRY DATE"), entry_date_);
        vl->addWidget(common);

        const bk::BookLeg first = p.legs.isEmpty() ? bk::BookLeg{} : p.legs.first();

        // Option
        option_ = new QWidget(this);
        auto* og = new QGridLayout(option_);
        og->setContentsMargins(0, 0, 0, 0);
        og->setHorizontalSpacing(10);
        og->setVerticalSpacing(4);
        opt_type_ = make_combo(option_, {dbp_tr("Call"), dbp_tr("Put")});
        opt_type_->setCurrentIndex(first.type == bk::LegType::Put ? 1 : 0);
        opt_strike_ = make_spin(option_, first.strike > 0 ? first.strike : 100.0, 0.0001, 1e9, 2);
        opt_expiry_ = make_date(option_, first.expiry.isValid() ? first.expiry : QDate::currentDate().addDays(30));
        opt_qty_ = make_spin(option_, first.quantity != 0 ? first.quantity : 1.0, -1e7, 1e7, 0);
        opt_mult_ = make_spin(option_, p.multiplier > 0 ? p.multiplier : 100.0, 0.0001, 1e9, 2);
        opt_price_ = make_spin(option_, first.entry_price, 0, 1e9, 4);
        opt_iv_ = make_spin(option_, first.entry_iv > 0 ? first.entry_iv * 100.0 : 25.0, 0.01, 1000, 2, QStringLiteral("%"));
        add_field(og, 0, 0, dbp_tr("TYPE"), opt_type_);
        add_field(og, 0, 1, dbp_tr("STRIKE"), opt_strike_);
        add_field(og, 0, 2, dbp_tr("EXPIRY"), opt_expiry_);
        add_field(og, 1, 0, dbp_tr("CONTRACTS (− = SHORT)"), opt_qty_);
        add_field(og, 1, 1, dbp_tr("MULTIPLIER"), opt_mult_);
        add_field(og, 1, 2, dbp_tr("ENTRY PRICE (PER SHARE)"), opt_price_);
        add_field(og, 2, 0, dbp_tr("IV AT ENTRY"), opt_iv_);
        mid_btn_ = make_button(option_, dbp_tr("USE CHAIN MID"), false);
        mid_btn_->setToolTip(dbp_tr("Fill the entry price and IV with the delayed chain's mid for this contract"));
        og->addWidget(mid_btn_, 5, 1);
        mid_status_ = make_note(option_, QString());
        og->addWidget(mid_status_, 5, 2);
        QObject::connect(mid_btn_, &QPushButton::clicked, this, [this] { use_chain_mid(); });
        vl->addWidget(option_);

        // Stock
        stock_ = new QWidget(this);
        auto* sg = new QGridLayout(stock_);
        sg->setContentsMargins(0, 0, 0, 0);
        sg->setHorizontalSpacing(10);
        stk_qty_ = make_spin(stock_, first.type == bk::LegType::Stock ? first.quantity : 100.0, -1e9, 1e9, 0);
        stk_price_ = make_spin(stock_, first.type == bk::LegType::Stock ? first.entry_price : 0.0, 0, 1e9, 4);
        add_field(sg, 0, 0, dbp_tr("SHARES (− = SHORT)"), stk_qty_);
        add_field(sg, 0, 1, dbp_tr("ENTRY PRICE"), stk_price_);
        vl->addWidget(stock_);

        // Future
        future_ = new QWidget(this);
        auto* fg = new QGridLayout(future_);
        fg->setContentsMargins(0, 0, 0, 0);
        fg->setHorizontalSpacing(10);
        fg->setVerticalSpacing(4);
        fut_contract_ = new QLineEdit(first.type == bk::LegType::Future ? first.contract : QString(), future_);
        fut_contract_->setPlaceholderText(dbp_tr("Exchange symbol, e.g. CLZ26.NYM"));
        fut_expiry_ = make_date(future_, first.type == bk::LegType::Future && first.expiry.isValid()
                                             ? first.expiry
                                             : QDate::currentDate().addMonths(1));
        fut_qty_ = make_spin(future_, first.type == bk::LegType::Future ? first.quantity : 1.0, -1e7, 1e7, 0);
        fut_size_ = make_spin(future_, p.kind == QLatin1String("future") && p.multiplier > 0 ? p.multiplier : 1000.0,
                              0.0001, 1e9, 2);
        fut_price_ = make_spin(future_, first.type == bk::LegType::Future ? first.entry_price : 0.0, -1e9, 1e9, 4);
        add_field(fg, 0, 0, dbp_tr("CONTRACT"), fut_contract_);
        add_field(fg, 0, 1, dbp_tr("LAST TRADING DAY"), fut_expiry_);
        add_field(fg, 0, 2, dbp_tr("CONTRACTS (− = SHORT)"), fut_qty_);
        add_field(fg, 1, 0, dbp_tr("CONTRACT SIZE"), fut_size_);
        add_field(fg, 1, 1, dbp_tr("ENTRY PRICE"), fut_price_);
        vl->addWidget(future_);

        // Swap
        swap_ = new QWidget(this);
        auto* wg = new QGridLayout(swap_);
        wg->setContentsMargins(0, 0, 0, 0);
        wg->setHorizontalSpacing(10);
        wg->setVerticalSpacing(4);
        const bool is_swap = first.type == bk::LegType::Swap;
        sw_start_ = make_spin(swap_, is_swap ? first.swap_start : 0.0, 0, 50, 2);
        sw_tenor_ = make_spin(swap_, is_swap && first.swap_tenor > 0 ? first.swap_tenor : 5.0, 0.1, 50, 2);
        sw_rate_ = make_spin(swap_, is_swap ? first.swap_rate * 100.0 : 4.0, -10, 50, 4, QStringLiteral("%"));
        sw_freq_ = make_combo(swap_, {dbp_tr("Annual"), dbp_tr("Semi-annual"), dbp_tr("Quarterly")});
        sw_freq_->setCurrentIndex(is_swap ? (first.swap_freq == 1 ? 0 : first.swap_freq == 4 ? 2 : 1) : 1);
        sw_notional_ = make_spin(swap_, is_swap && first.notional > 0 ? first.notional : 1e7, 1, 1e13, 0);
        sw_side_ = make_combo(swap_, {dbp_tr("Pay fixed"), dbp_tr("Receive fixed")});
        sw_side_->setCurrentIndex(is_swap && !first.payer ? 1 : 0);
        sw_pv_ = make_spin(swap_, is_swap ? first.entry_price : 0.0, -1e13, 1e13, 2);
        sw_pv_->setToolTip(dbp_tr("The swap's value when you booked it (0 for a swap at the market rate)"));
        add_field(wg, 0, 0, dbp_tr("START (YEARS FROM ENTRY)"), sw_start_);
        add_field(wg, 0, 1, dbp_tr("TENOR (YEARS)"), sw_tenor_);
        add_field(wg, 0, 2, dbp_tr("FIXED RATE"), sw_rate_);
        add_field(wg, 1, 0, dbp_tr("FREQUENCY"), sw_freq_);
        add_field(wg, 1, 1, dbp_tr("NOTIONAL"), sw_notional_);
        add_field(wg, 1, 2, dbp_tr("SIDE"), sw_side_);
        add_field(wg, 2, 0, dbp_tr("VALUE AT ENTRY"), sw_pv_);
        vl->addWidget(swap_);

        // Strategy: the legs, read-only.
        legs_note_ = make_note(this, QString());
        vl->addWidget(legs_note_);

        notes_ = new QPlainTextEdit(p.notes, this);
        notes_->setPlaceholderText(dbp_tr("Notes (optional)"));
        notes_->setMaximumHeight(fm.height() * 4 + 12);
        vl->addWidget(notes_);
        error_ = make_note(this, QString());
        vl->addWidget(error_);
        auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        box->button(QDialogButtonBox::Ok)->setText(is_new ? dbp_tr("ADD TO BOOK") : dbp_tr("SAVE"));
        box->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("derivCalcBtn"));
        box->button(QDialogButtonBox::Cancel)->setText(dbp_tr("CANCEL"));
        box->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("derivToolBtn"));
        QObject::connect(box, &QDialogButtonBox::accepted, this, [this] { try_accept(); });
        QObject::connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        vl->addWidget(box);

        int kind = 0;
        if (p.kind == QLatin1String("stock"))
            kind = 1;
        else if (p.kind == QLatin1String("future"))
            kind = 2;
        else if (p.kind == QLatin1String("swap"))
            kind = 3;
        else if (p.kind == QLatin1String("strategy") || p.legs.size() > 1)
            kind = 4;
        kind_->setCurrentIndex(kind);
        auto* km = qobject_cast<QStandardItemModel*>(kind_->model());
        if (km && km->item(4))
            km->item(4)->setEnabled(kind == 4); // strategies come from the STRATEGY page
        kind_->setEnabled(kind != 4 && is_new);
        QObject::connect(kind_, &QComboBox::currentIndexChanged, this, [this] { show_kind(); });
        show_kind();
    }

    ~DbpEditDialog() override {
        if (req_)
            FreeOptionChainService::instance().cancel(req_);
    }

    bk::BookPosition result() const { return out_; }

  private:
    void show_kind() {
        const int k = kind_->currentIndex();
        option_->setVisible(k == 0);
        stock_->setVisible(k == 1);
        future_->setVisible(k == 2);
        swap_->setVisible(k == 3);
        legs_note_->setVisible(k == 4);
        if (k == 3 && underlying_->text().trimmed().isEmpty())
            underlying_->setText(currency_->text().trimmed().toUpper() + QStringLiteral(" rates"));
        if (k == 4) {
            QStringList lines;
            for (const bk::BookLeg& l : in_.legs) {
                const QString side = l.quantity >= 0 ? dbp_tr("Buy") : dbp_tr("Sell");
                if (l.is_option())
                    lines << QStringLiteral("%1 %2 %3 %4 @ %5")
                                 .arg(side, fmt::quantity(std::fabs(l.quantity)),
                                      l.type == bk::LegType::Call ? dbp_tr("call") : dbp_tr("put"),
                                      fmt::number(l.strike, 2) + QLatin1Char(' ') + dbp_date(l.expiry),
                                      fmt::number(l.entry_price, 2));
                else if (l.type == bk::LegType::Stock)
                    lines << QStringLiteral("%1 %2 %3 @ %4").arg(side, fmt::quantity(std::fabs(l.quantity)), dbp_tr("shares"),
                                                                 fmt::number(l.entry_price, 2));
            }
            legs_note_->setText(dbp_tr("Legs (edit them on the STRATEGY page and add the strategy again):") +
                                QLatin1Char('\n') + lines.join(QLatin1Char('\n')));
        }
    }

    void use_chain_mid() {
        const QString sym = underlying_->text().trimmed();
        if (sym.isEmpty()) {
            mid_status_->setText(dbp_tr("Type the underlying first."));
            return;
        }
        auto fill = [this](const services::options::FreeOptionChainService::ChainPtr& c) {
            if (!c || !c->ok())
                return false;
            const QDate d = opt_expiry_->date();
            for (int e = 0; e < c->expiries.size(); ++e) {
                if (c->expiries[e].date != d)
                    continue;
                double mid = 0, iv = 0;
                if (mk::find_quote(*c, e, opt_strike_->value(), opt_type_->currentIndex() == 0, &mid, &iv) && mid > 0) {
                    opt_price_->setValue(mid);
                    if (iv > 0)
                        opt_iv_->setValue(iv * 100.0);
                    mid_status_->setText(dbp_tr("Mid %1, IV %2 (%3, delayed)")
                                             .arg(fmt::number(mid, 2), vol_pct(iv), c->source_label));
                    return true;
                }
                mid_status_->setText(dbp_tr("The chain has no quote for this strike on that expiry."));
                return true;
            }
            mid_status_->setText(dbp_tr("The chain has no expiry on %1.").arg(dbp_date(d)));
            return true;
        };
        auto& svc = FreeOptionChainService::instance();
        if (fill(svc.cached(sym)))
            return;
        mid_status_->setText(dbp_tr("Loading the delayed chain for %1…").arg(sym));
        mid_btn_->setEnabled(false);
        QPointer<DbpEditDialog> self(this);
        req_ = svc.fetch(sym, QStringLiteral("auto"), false, this,
                         [self, fill](const services::options::FreeOptionChainService::Reply& r) {
                             if (!self)
                                 return;
                             if (r.stage == services::options::FreeOptionChainService::Stage::Saved)
                                 return; // wait for the fresh chain
                             self->mid_btn_->setEnabled(true);
                             if (r.stage == services::options::FreeOptionChainService::Stage::Failed) {
                                 self->mid_status_->setText(r.message);
                                 return;
                             }
                             fill(r.chain);
                         });
    }

    void try_accept() {
        bk::BookPosition p = in_;
        p.book = book_->currentText().trimmed();
        p.name = name_->text().trimmed();
        p.underlying = underlying_->text().trimmed();
        p.currency = currency_->text().trimmed().toUpper();
        p.entry_date = entry_date_->date();
        p.notes = notes_->toPlainText().trimmed();
        const int k = kind_->currentIndex();
        auto fail = [this](const QString& why) { error_->setText(why); };
        if (k != 3 && p.underlying.isEmpty())
            return fail(dbp_tr("Type the underlying (for example SPY)."));
        if (k == 0) {
            bk::BookLeg l = in_.legs.isEmpty() || !in_.legs.first().is_option() ? bk::BookLeg{} : in_.legs.first();
            l.type = opt_type_->currentIndex() == 0 ? bk::LegType::Call : bk::LegType::Put;
            l.strike = opt_strike_->value();
            l.expiry = opt_expiry_->date();
            l.quantity = opt_qty_->value();
            l.entry_price = opt_price_->value();
            l.entry_iv = opt_iv_->value() / 100.0;
            if (l.quantity == 0)
                return fail(dbp_tr("The number of contracts cannot be zero."));
            p.kind = QStringLiteral("option");
            p.multiplier = opt_mult_->value();
            p.legs = {l};
        } else if (k == 1) {
            bk::BookLeg l;
            l.type = bk::LegType::Stock;
            l.quantity = stk_qty_->value();
            l.entry_price = stk_price_->value();
            if (l.quantity == 0)
                return fail(dbp_tr("The number of shares cannot be zero."));
            p.kind = QStringLiteral("stock");
            p.multiplier = 1;
            p.legs = {l};
        } else if (k == 2) {
            bk::BookLeg l;
            l.type = bk::LegType::Future;
            l.contract = fut_contract_->text().trimmed().toUpper();
            l.expiry = fut_expiry_->date();
            l.quantity = fut_qty_->value();
            l.entry_price = fut_price_->value();
            if (l.contract.isEmpty())
                return fail(dbp_tr("Type the futures contract symbol (for example CLZ26.NYM): its last price marks the position."));
            if (l.quantity == 0)
                return fail(dbp_tr("The number of contracts cannot be zero."));
            p.kind = QStringLiteral("future");
            p.multiplier = fut_size_->value();
            p.legs = {l};
        } else if (k == 3) {
            bk::BookLeg l;
            l.type = bk::LegType::Swap;
            l.quantity = 1;
            l.swap_start = sw_start_->value();
            l.swap_tenor = sw_tenor_->value();
            l.swap_rate = sw_rate_->value() / 100.0;
            l.swap_freq = sw_freq_->currentIndex() == 0 ? 1 : sw_freq_->currentIndex() == 1 ? 2 : 4;
            l.notional = sw_notional_->value();
            l.payer = sw_side_->currentIndex() == 0;
            l.entry_price = sw_pv_->value();
            p.kind = QStringLiteral("swap");
            p.multiplier = 1;
            if (p.underlying.isEmpty())
                p.underlying = (p.currency.isEmpty() ? QStringLiteral("USD") : p.currency) + QStringLiteral(" rates");
            p.legs = {l};
        } else {
            p.kind = QStringLiteral("strategy");
            if (p.legs.isEmpty())
                return fail(dbp_tr("This strategy has no legs."));
        }
        out_ = p;
        accept();
    }

    bk::BookPosition in_, out_;
    quint64 req_ = 0;
    QComboBox* book_ = nullptr;
    QComboBox* kind_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* underlying_ = nullptr;
    QLineEdit* currency_ = nullptr;
    QDateEdit* entry_date_ = nullptr;
    QWidget* option_ = nullptr;
    QComboBox* opt_type_ = nullptr;
    QDoubleSpinBox* opt_strike_ = nullptr;
    QDateEdit* opt_expiry_ = nullptr;
    QDoubleSpinBox* opt_qty_ = nullptr;
    QDoubleSpinBox* opt_mult_ = nullptr;
    QDoubleSpinBox* opt_price_ = nullptr;
    QDoubleSpinBox* opt_iv_ = nullptr;
    QPushButton* mid_btn_ = nullptr;
    QLabel* mid_status_ = nullptr;
    QWidget* stock_ = nullptr;
    QDoubleSpinBox* stk_qty_ = nullptr;
    QDoubleSpinBox* stk_price_ = nullptr;
    QWidget* future_ = nullptr;
    QLineEdit* fut_contract_ = nullptr;
    QDateEdit* fut_expiry_ = nullptr;
    QDoubleSpinBox* fut_qty_ = nullptr;
    QDoubleSpinBox* fut_size_ = nullptr;
    QDoubleSpinBox* fut_price_ = nullptr;
    QWidget* swap_ = nullptr;
    QDoubleSpinBox* sw_start_ = nullptr;
    QDoubleSpinBox* sw_tenor_ = nullptr;
    QDoubleSpinBox* sw_rate_ = nullptr;
    QComboBox* sw_freq_ = nullptr;
    QDoubleSpinBox* sw_notional_ = nullptr;
    QComboBox* sw_side_ = nullptr;
    QDoubleSpinBox* sw_pv_ = nullptr;
    QLabel* legs_note_ = nullptr;
    QPlainTextEdit* notes_ = nullptr;
    QLabel* error_ = nullptr;
};

} // namespace

// ── Model ────────────────────────────────────────────────────────────────────

void DerivBookModel::set_positions(const QVector<bk::BookPosition>& positions) {
    beginResetModel();
    rows_ = positions;
    QHash<QString, bk::PositionResult> keep;
    for (const bk::BookPosition& p : rows_)
        if (results_.contains(p.id))
            keep.insert(p.id, results_.value(p.id));
    results_ = keep;
    endResetModel();
}

void DerivBookModel::set_results(const QVector<bk::PositionResult>& results) {
    for (const bk::PositionResult& r : results)
        results_.insert(r.id, r);
    if (!rows_.isEmpty())
        emit dataChanged(index(0, kEntry), index(rows_.size() - 1, kCols - 1));
}

const bk::BookPosition* DerivBookModel::position(int row) const {
    return row >= 0 && row < rows_.size() ? &rows_[row] : nullptr;
}

const bk::PositionResult* DerivBookModel::result(int row) const {
    if (row < 0 || row >= rows_.size())
        return nullptr;
    const auto it = results_.constFind(rows_[row].id);
    return it == results_.constEnd() ? nullptr : &it.value();
}

int DerivBookModel::row_of(const QString& id) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].id == id)
            return i;
    return -1;
}

int DerivBookModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : rows_.size();
}

int DerivBookModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : kCols;
}

QString DerivBookModel::source_text(bk::MarkSource s) {
    switch (s) {
        case bk::MarkSource::ChainMid: return tr("chain mid");
        case bk::MarkSource::ModelChainIv: return tr("model at chain IV");
        case bk::MarkSource::ModelEntryIv: return tr("model at entry IV");
        case bk::MarkSource::Intrinsic: return tr("expired: intrinsic");
        case bk::MarkSource::Spot: return tr("spot");
        case bk::MarkSource::FuturesLast: return tr("futures last");
        case bk::MarkSource::Curve: return tr("Treasury curve");
        case bk::MarkSource::Entry: return tr("entry price (no market data)");
    }
    return {};
}

QString DerivBookModel::kind_text(const QString& kind) {
    if (kind == QLatin1String("option"))
        return tr("Option");
    if (kind == QLatin1String("stock"))
        return tr("Stock");
    if (kind == QLatin1String("future"))
        return tr("Future");
    if (kind == QLatin1String("swap"))
        return tr("Swap");
    if (kind == QLatin1String("strategy"))
        return tr("Strategy");
    return kind;
}

QString DerivBookModel::detail_text(const bk::BookPosition& p) {
    if (p.legs.isEmpty())
        return {};
    const bk::BookLeg& l = p.legs.first();
    if (p.legs.size() > 1) {
        QDate nearest;
        for (const bk::BookLeg& x : p.legs)
            if (x.expiry.isValid() && (!nearest.isValid() || x.expiry < nearest))
                nearest = x.expiry;
        return tr("%1 legs, first expiry %2").arg(p.legs.size()).arg(dbp_date(nearest));
    }
    switch (l.type) {
        case bk::LegType::Call:
        case bk::LegType::Put:
            return tr("%1 × %2 %3 %4")
                .arg(fmt::signed_number(l.quantity, 0), fmt::number(l.strike, 2),
                     l.type == bk::LegType::Call ? tr("C") : tr("P"), dbp_date(l.expiry));
        case bk::LegType::Stock:
            return tr("%1 shares").arg(fmt::signed_number(l.quantity, 0));
        case bk::LegType::Future:
            return tr("%1 × %2").arg(fmt::signed_number(l.quantity, 0), l.contract);
        case bk::LegType::Swap:
            return tr("%1 %2 %3 y, %4")
                .arg(l.payer ? tr("Pay") : tr("Receive"), fmt::fraction_pct(l.swap_rate, 3, false),
                     fmt::number(l.swap_tenor, 1), fmt::compact(l.notional));
    }
    return {};
}

QVariant DerivBookModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const bk::BookPosition& p = rows_[index.row()];
    const bk::PositionResult* r = result(index.row());
    const QString ccy = p.currency;
    const int c = index.column();
    if (role == Qt::TextAlignmentRole) {
        const bool left = c == kName || c == kKind || c == kUnderlying || c == kDetail || c == kMarked || c == kNotes;
        return QVariant::fromValue(left ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
    }
    if (role == Qt::ForegroundRole) {
        // Signed money only: P&L in the price-direction colours.
        if (c == kPnl && r && std::fabs(r->total.pnl) > 1e-9)
            return QColor(r->total.pnl > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        if (c == kMarked && r && r->stale)
            return QColor(ui::colors::WARNING());
        return {};
    }
    if (role == Qt::ToolTipRole) {
        if (c == kName && !p.notes.isEmpty())
            return p.notes;
        if (c == kMarked && r) {
            QStringList parts;
            for (int i = 0; i < r->legs.size(); ++i)
                parts << tr("Leg %1: %2").arg(i + 1).arg(source_text(r->legs[i].source));
            return parts.join(QLatin1Char('\n'));
        }
        return {};
    }
    const bool sort = role == Qt::UserRole;
    if (role != Qt::DisplayRole && !sort)
        return {};
    auto num_or_dash = [&](double v, const QString& text) -> QVariant {
        if (sort)
            return std::isfinite(v) ? QVariant(v) : QVariant(-1e300);
        return std::isfinite(v) ? QVariant(text) : QVariant(dbp_dash());
    };
    switch (c) {
        case kName:
            return p.name.isEmpty() ? detail_text(p) : p.name;
        case kKind:
            return kind_text(p.kind);
        case kUnderlying:
            return p.underlying;
        case kDetail:
            return detail_text(p);
        case kEntry: {
            if (p.legs.isEmpty())
                return num_or_dash(std::nan(""), {});
            if (p.legs.size() == 1 && p.legs.first().type != bk::LegType::Swap) {
                const double v = p.legs.first().entry_price;
                return num_or_dash(v, fmt::number(v, 2));
            }
            const double v = p.kind == QLatin1String("swap") ? p.legs.first().entry_price : dbp_net_entry(p);
            return num_or_dash(v, fmt::signed_money(v, ccy));
        }
        case kMark: {
            if (!r || p.kind == QLatin1String("swap"))
                return num_or_dash(std::nan(""), {});
            return num_or_dash(r->total.mark, fmt::number(r->total.mark, 2));
        }
        case kValue:
            return r ? num_or_dash(r->total.value, fmt::money(r->total.value, ccy)) : num_or_dash(std::nan(""), {});
        case kPnl:
            return r ? num_or_dash(r->total.pnl, fmt::signed_money(r->total.pnl, ccy)) : num_or_dash(std::nan(""), {});
        case kDelta:
            if (!r || p.kind == QLatin1String("swap"))
                return num_or_dash(std::nan(""), {});
            return num_or_dash(r->total.delta, fmt::number(r->total.delta, 1));
        case kDollarDelta:
            return r ? num_or_dash(r->total.dollar_delta, fmt::money(r->total.dollar_delta, ccy, fmt::Compact::None, 0))
                     : num_or_dash(std::nan(""), {});
        case kGamma:
            return r ? num_or_dash(r->total.gamma, fmt::number(r->total.gamma, 3)) : num_or_dash(std::nan(""), {});
        case kDollarGamma:
            return r ? num_or_dash(r->total.dollar_gamma, fmt::money(r->total.dollar_gamma, ccy, fmt::Compact::None, 0))
                     : num_or_dash(std::nan(""), {});
        case kVega:
            return r ? num_or_dash(r->total.vega, fmt::signed_money(r->total.vega, ccy)) : num_or_dash(std::nan(""), {});
        case kTheta:
            return r ? num_or_dash(r->total.theta, fmt::signed_money(r->total.theta, ccy)) : num_or_dash(std::nan(""), {});
        case kRho:
            return r ? num_or_dash(r->total.rho, fmt::signed_money(r->total.rho, ccy)) : num_or_dash(std::nan(""), {});
        case kExpiry: {
            if (!r || !r->nearest_expiry.isValid()) {
                if (sort)
                    return QVariant(1e300);
                return r && r->all_expired ? tr("expired") : dbp_dash();
            }
            const qint64 days = QDate::currentDate().daysTo(r->nearest_expiry);
            if (sort)
                return QVariant(static_cast<double>(days));
            return tr("%1 (%2 d)").arg(dbp_date(r->nearest_expiry)).arg(days);
        }
        case kMarked: {
            if (!r)
                return sort ? QVariant(QString()) : QVariant(tr("not valued yet"));
            QString s = r->legs.size() == 1 ? source_text(r->legs.first().source)
                                             : (r->stale ? tr("partly at entry prices") : tr("market data"));
            if (r->as_of.isValid())
                s += tr(" · %1").arg(r->as_of.toLocalTime().toString(QStringLiteral("d MMM HH:mm")));
            return s;
        }
        case kNotes:
            return p.notes.simplified();
        default:
            return {};
    }
}

QVariant DerivBookModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::ToolTipRole) {
        switch (section) {
            case kEntry: return tr("Entry price per share (single legs) or the net premium paid (+) / received (−)");
            case kMark: return tr("Current mark per share / unit");
            case kValue: return tr("Market value; futures: open P&L (marked to market daily); swaps: present value");
            case kDelta: return tr("Delta in units of the underlying (shares)");
            case kDollarDelta: return tr("Delta × spot");
            case kDollarGamma: return tr("Change of dollar delta for a 1% move of the underlying");
            case kVega: return tr("Value change for +1 volatility point");
            case kTheta: return tr("Value change for one calendar day");
            case kRho: return tr("Value change for +1% of rates (swaps: DV01 × 100)");
            case kMarked: return tr("Where the mark came from, and its time (delayed data)");
            default: return {};
        }
    }
    if (role != Qt::DisplayRole)
        return {};
    switch (section) {
        case kName: return tr("POSITION");
        case kKind: return tr("KIND");
        case kUnderlying: return tr("UNDERLYING");
        case kDetail: return tr("CONTRACT");
        case kEntry: return tr("ENTRY");
        case kMark: return tr("MARK");
        case kValue: return tr("MKT VALUE");
        case kPnl: return tr("P&L");
        case kDelta: return tr("DELTA");
        case kDollarDelta: return tr("$ DELTA");
        case kGamma: return tr("GAMMA");
        case kDollarGamma: return tr("$ GAMMA 1%");
        case kVega: return tr("VEGA");
        case kTheta: return tr("THETA/DAY");
        case kRho: return tr("RHO 1%");
        case kExpiry: return tr("EXPIRY");
        case kMarked: return tr("MARKED FROM");
        case kNotes: return tr("NOTES");
        default: return {};
    }
}

// ── Page ─────────────────────────────────────────────────────────────────────

DerivBookPage::DerivBookPage(DerivContext* ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx), book_("fincept::screens::deriv::DerivBookPage") {
    build();
}

DerivBookPage::~DerivBookPage() {
    cancel_marking();
    if (value_cancel_)
        value_cancel_->store(true);
}

void DerivBookPage::build() {
    // A one-line toolbar strip across the page, then the book with a narrow margin.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(build_toolbar());
    auto* root = new QVBoxLayout;
    root->setContentsMargins(10, 8, 10, 6);
    root->setSpacing(6);
    outer->addLayout(root, 1);

    states_ = new QStackedWidget(this);
    skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, states_);
    skeleton_->set_message(tr("Opening the position book…"));
    states_->addWidget(skeleton_);
    auto* content = new QWidget(states_);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    kpis_ = new ui::KpiStrip(content);
    cl->addWidget(kpis_);
    auto* split = new QSplitter(Qt::Vertical, content);
    split->setChildrenCollapsible(false);
    split->addWidget(build_table(split));
    split->addWidget(build_detail(split));
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    cl->addWidget(split, 1);
    states_->addWidget(content);
    empty_ = new ui::EmptyState(states_);
    connect(empty_, &ui::EmptyState::primary_clicked, this, [this] {
        if (empty_is_error_) {
            reload_positions();
            return;
        }
        bk::BookPosition p;
        p.kind = QStringLiteral("option");
        p.underlying = ctx_ && ctx_->chain() ? ctx_->chain()->symbol : QStringLiteral("SPY");
        p.book = book_combo_->currentData().toString();
        edit_position(p, true);
    });
    connect(empty_, &ui::EmptyState::secondary_clicked, this, [this] {
        if (!ctx_ || !ctx_->position().valid)
            return;
        const DerivContext::Position& sp = ctx_->position();
        bk::BookPosition p;
        p.kind = sp.legs.size() == 1 ? QStringLiteral("option") : QStringLiteral("strategy");
        p.name = sp.name;
        p.underlying = sp.symbol;
        p.currency = sp.currency;
        p.multiplier = sp.multiplier;
        for (const LegView& v : sp.legs) {
            bk::BookLeg l;
            l.type = v.leg.kind == services::options::strategy::LegKind::Call  ? bk::LegType::Call
                     : v.leg.kind == services::options::strategy::LegKind::Put ? bk::LegType::Put
                                                                                : bk::LegType::Stock;
            l.quantity = v.leg.quantity;
            l.strike = v.leg.strike;
            l.expiry = QDate::fromString(v.expiry_label, Qt::ISODate);
            l.entry_price = v.leg.entry_price;
            l.entry_iv = v.leg.iv;
            if (v.leg.active)
                p.legs.append(l);
        }
        if (!p.legs.isEmpty())
            edit_position(p, true);
    });
    states_->addWidget(empty_);
    root->addWidget(states_, 1);
    note_ = make_note(this, QString());
    root->addWidget(note_);

    tick_ = new QTimer(this);
    tick_->setInterval(kTickMs);
    connect(tick_, &QTimer::timeout, this, &DerivBookPage::on_tick);
    revalue_timer_ = new QTimer(this);
    revalue_timer_->setSingleShot(true);
    revalue_timer_->setInterval(120);
    connect(revalue_timer_, &QTimer::timeout, this, &DerivBookPage::revalue);

    memory_ = new ui::InputMemory(QStringLiteral("derivatives"), QStringLiteral("book"), this);
    memory_->track(QStringLiteral("book"), book_memory_);
    memory_->track(QStringLiteral("detail"), detail_memory_);
    memory_->track(QStringLiteral("exposure_metric"), exposure_metric_memory_);
    memory_->track(QStringLiteral("ladder_metric"), ladder_metric_memory_);
    memory_->restore();
    // Connected after the restore: restoring must not start anything.
    connect(book_combo_, &QComboBox::currentIndexChanged, this, [this] {
        book_memory_->setText(book_combo_->currentData().toString());
        model_->set_positions({});
        show_state();
        reload_positions();
    });
    connect(&DerivBookStore::instance(), &DerivBookStore::positions_changed, this, [this] {
        if (loaded_ && !loading_ && isVisible())
            reload_positions();
    });
    connect(&CurveDataService::instance(), &CurveDataService::treasury_updated, this, [this] {
        ensure_curve();
        schedule_revalue();
    });
    set_detail_view(std::clamp(detail_memory_->value(), 0, 2));
    retranslate();
    states_->setCurrentIndex(0);
}

QWidget* DerivBookPage::build_toolbar() {
    QHBoxLayout* hl = nullptr;
    QWidget* bar = make_strip(this, &hl);
    hl->setContentsMargins(10, 3, 10, 3);
    hl->setSpacing(8);
    auto* cap = new QLabel(tr("BOOK"), bar);
    cap->setObjectName(QStringLiteral("derivLabel"));
    book_.label(cap, QT_TR_NOOP("BOOK"));
    hl->addWidget(cap);
    book_combo_ = new QComboBox(bar);
    book_combo_->addItem(tr("All books"), QString());
    book_combo_->setMinimumContentsLength(14);
    book_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    cap->setBuddy(book_combo_);
    hl->addWidget(book_combo_);
    add_btn_ = make_button(bar, tr("ADD POSITION"), true);
    book_.button(add_btn_, QT_TR_NOOP("ADD POSITION"));
    auto* menu = new QMenu(add_btn_);
    add_menu(menu);
    add_btn_->setMenu(menu);
    hl->addWidget(add_btn_);
    refresh_btn_ = make_button(bar, tr("REFRESH MARKS"), false);
    book_.button(refresh_btn_, QT_TR_NOOP("REFRESH MARKS"));
    book_.tooltip(refresh_btn_, QT_TR_NOOP("Re-mark every position from the free delayed data, one underlying at a time (F5)"));
    connect(refresh_btn_, &QPushButton::clicked, this, &DerivBookPage::refresh);
    hl->addWidget(refresh_btn_);
    status_ = new QLabel(bar);
    status_->setObjectName(QStringLiteral("derivStatusLine"));
    status_->setWordWrap(true);
    hl->addWidget(status_, 1);
    result_bar_ = new DerivResultBar(QStringLiteral("derivatives_book"), bar);
    result_bar_->set_table([this] { return export_table(); });
    result_bar_->set_image([this] { return export_image(); });
    result_bar_->set_payload([this] { return report_payload(); });
    hl->addWidget(result_bar_);
    book_memory_ = new QLineEdit(bar);
    book_memory_->hide();
    detail_memory_ = new QSpinBox(bar);
    detail_memory_->setRange(0, 2);
    detail_memory_->hide();
    exposure_metric_memory_ = new QSpinBox(bar);
    exposure_metric_memory_->setRange(0, 3);
    exposure_metric_memory_->hide();
    ladder_metric_memory_ = new QSpinBox(bar);
    ladder_metric_memory_->setRange(0, 3);
    ladder_metric_memory_->hide();
    return bar;
}

void DerivBookPage::add_menu(QMenu* menu) {
    struct K {
        const char* src;
        const char* kind;
    };
    for (const K& k : {K{QT_TR_NOOP("Option…"), "option"}, K{QT_TR_NOOP("Stock or ETF…"), "stock"},
                       K{QT_TR_NOOP("Future…"), "future"}, K{QT_TR_NOOP("Interest rate swap…"), "swap"}}) {
        const QString kind = QString::fromLatin1(k.kind);
        menu->addAction(tr(k.src), this, [this, kind] {
            bk::BookPosition p;
            p.kind = kind;
            p.book = book_combo_->currentData().toString();
            if (kind == QLatin1String("option") || kind == QLatin1String("stock"))
                p.underlying = ctx_ && ctx_->chain() ? ctx_->chain()->symbol : QStringLiteral("SPY");
            if (kind == QLatin1String("option") && ctx_ && ctx_->chain() && ctx_->chain()->ok()) {
                bk::BookLeg l;
                l.strike = std::round(ctx_->chain()->spot);
                l.expiry = QDate::currentDate().addDays(30);
                p.legs = {l};
            }
            edit_position(p, true);
        });
    }
}

QWidget* DerivBookPage::build_table(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    table_ = new QTableView(w);
    dbp_setup_table(table_);
    model_ = new DerivBookModel(table_);
    proxy_ = new QSortFilterProxyModel(table_);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(Qt::UserRole);
    table_->setModel(proxy_);
    table_->setSortingEnabled(true);
    table_->sortByColumn(-1, Qt::AscendingOrder);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionsMovable(true);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &DerivBookPage::row_menu);
    connect(table_, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& index) {
        if (const bk::BookPosition* p = model_->position(proxy_->mapToSource(index).row()))
            edit_position(*p, false);
    });
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this] { update_detail(); });
    vl->addWidget(table_, 1);
    refit();
    // A new font size / density: widths again once every widget has its new style.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, &DerivBookPage::refit);
    return w;
}

void DerivBookPage::refit() {
    if (!table_)
        return;
    // Widths from font metrics: wide enough for the column titles and typical values.
    const QFontMetrics fm(table_->font());
    for (int c = 0; c < DerivBookModel::kCols; ++c) {
        const int chars = c == DerivBookModel::kName || c == DerivBookModel::kDetail || c == DerivBookModel::kMarked ? 22
                          : c == DerivBookModel::kNotes                                                         ? 18
                                                                                                                 : 11;
        table_->setColumnWidth(c, fm.horizontalAdvance(QLatin1Char('0')) * chars + 16);
    }
    // Row heights: dense_table() (dbp_setup_table) re-fits every table of the page itself.
}

QWidget* DerivBookPage::build_detail(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    QHBoxLayout* tl = nullptr;
    QWidget* tabs = make_strip(w, &tl); // the detail views' tab strip (one line, hairline under it)
    tl->setContentsMargins(0, 0, 4, 0);
    tl->setSpacing(2);
    static const char* const kTabs[] = {QT_TR_NOOP("EXPOSURE BY UNDERLYING"), QT_TR_NOOP("EXPIRY LADDER"),
                                        QT_TR_NOOP("LEGS OF THE SELECTED POSITION")};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(ui::literal_ampersands(tr(kTabs[i])), tabs);
        b->setObjectName(QStringLiteral("derivSubBtn"));
        b->setCursor(Qt::PointingHandCursor);
        book_.button(b, kTabs[i]);
        connect(b, &QPushButton::clicked, this, [this, i] { set_detail_view(i); });
        tl->addWidget(b);
        detail_btns_.append(b);
    }
    tl->addStretch(1);
    vl->addWidget(tabs);
    detail_ = new QStackedWidget(w);
    for (int i = 0; i < 3; ++i) {
        auto* holder = new QWidget(detail_);
        auto* hl = new QVBoxLayout(holder);
        hl->setContentsMargins(0, 0, 0, 0);
        detail_->addWidget(holder);
        detail_views_.append(nullptr);
    }
    vl->addWidget(detail_, 1);
    return w;
}

/// Built the first time it is shown.
QWidget* DerivBookPage::ensure_detail_view(int index) {
    if (index < 0 || index >= detail_views_.size())
        return nullptr;
    if (detail_views_[index])
        return detail_views_[index];
    QWidget* holder = detail_->widget(index);
    auto* view = new QWidget(holder);
    auto* hl = new QHBoxLayout(view);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    auto* table = new QTableView(view);
    dbp_setup_table(table);
    table->setSortingEnabled(true);
    auto* sm = new QStandardItemModel(table);
    sm->setSortRole(Qt::UserRole);
    table->setModel(sm);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    if (index == 2) {
        auto* col = new QVBoxLayout;
        col->setSpacing(4);
        legs_caption_ = new QLabel(view);
        legs_caption_->setObjectName(QStringLiteral("derivStatusLine"));
        col->addWidget(legs_caption_);
        col->addWidget(table, 1);
        hl->addLayout(col, 1);
        legs_model_ = sm;
    } else {
        hl->addWidget(table, 3);
        auto* right = new QWidget(view);
        auto* rl = new QVBoxLayout(right);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(4);
        auto* metric = make_combo(right, {tr("Dollar delta"), tr("Vega"), tr("Theta per day"), tr("Market value")});
        book_.combo(metric, {QT_TR_NOOP("Dollar delta"), QT_TR_NOOP("Vega"), QT_TR_NOOP("Theta per day"),
                             QT_TR_NOOP("Market value")});
        QSpinBox* remembered = index == 0 ? exposure_metric_memory_ : ladder_metric_memory_;
        metric->setCurrentIndex(std::clamp(remembered->value(), 0, 3));
        connect(metric, &QComboBox::currentIndexChanged, remembered, &QSpinBox::setValue);
        rl->addWidget(metric);
        auto* chart = new ui::BarChart(right);
        chart->set_signed_colors(true);
        chart->set_show_values(false);
        chart->setMinimumHeight(QFontMetrics(chart->font()).height() * 10);
        rl->addWidget(chart, 1);
        hl->addWidget(right, 2);
        connect(metric, &QComboBox::currentIndexChanged, this, [this] { update_detail(); });
        if (index == 0) {
            exposure_model_ = sm;
            exposure_metric_ = metric;
            exposure_chart_ = chart;
        } else {
            ladder_model_ = sm;
            ladder_metric_ = metric;
            ladder_chart_ = chart;
        }
    }
    holder->layout()->addWidget(view);
    detail_views_[index] = view;
    return view;
}

void DerivBookPage::set_detail_view(int index) {
    ensure_detail_view(index);
    detail_->setCurrentIndex(index);
    detail_memory_->setValue(index);
    for (int i = 0; i < detail_btns_.size(); ++i) {
        detail_btns_[i]->setProperty("active", i == index);
        detail_btns_[i]->style()->unpolish(detail_btns_[i]);
        detail_btns_[i]->style()->polish(detail_btns_[i]);
    }
    update_detail();
}

// ── Loading / saving ─────────────────────────────────────────────────────────

void DerivBookPage::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!loaded_ && !loading_) {
        reload_positions();
    } else if (loaded_) {
        ensure_curve();
        on_tick();
    }
    tick_->start();
}

void DerivBookPage::hideEvent(QHideEvent* e) {
    // Nothing refreshes while the page is not in view.
    cancel_marking();
    QWidget::hideEvent(e);
}

void DerivBookPage::reload_books() {
    DerivBookStore::instance().load_books(this, [this](const QStringList& books, const QString& error) {
        if (!error.isEmpty())
            return;
        books_ = books;
        const QString keep = book_memory_->text();
        QSignalBlocker block(book_combo_);
        while (book_combo_->count() > 1)
            book_combo_->removeItem(1);
        for (const QString& b : books)
            book_combo_->addItem(b, b);
        const int at = keep.isEmpty() ? 0 : book_combo_->findData(keep);
        book_combo_->setCurrentIndex(std::max(0, at));
    });
}

void DerivBookPage::reload_positions() {
    loading_ = true;
    if (!loaded_)
        states_->setCurrentIndex(0);
    reload_books();
    const QString book = book_memory_->text();
    DerivBookStore::instance().load_positions(
        book, this, [this](const QVector<bk::BookPosition>& rows, const QString& error) {
            loading_ = false;
            loaded_ = error.isEmpty();
            load_error_ = error;
            if (error.isEmpty()) {
                positions_ = rows;
                // Marks other pages already hold (the chain in view, saved copies): instant values.
                for (const bk::BookPosition& p : positions_) {
                    const QString key = bk::underlying_key(p.underlying);
                    if (key.isEmpty() || market_.underlyings.value(key).chain)
                        continue;
                    if (auto c = FreeOptionChainService::instance().cached(p.underlying); c && c->ok()) {
                        bk::UnderlyingQuote& q = market_.underlyings[key];
                        q.chain = c;
                        q.as_of = c->quote_time.isValid() ? c->quote_time : c->fetched_at;
                        q.source = c->source_label;
                    }
                }
                model_->set_positions(positions_);
                ensure_curve();
                revalue();
                if (isVisible())
                    on_tick();
            } else {
                LOG_WARN("DerivBook", "Position book could not be loaded");
            }
            show_state();
            if (pending_draft_ && loaded_) {
                pending_draft_ = false;
                edit_position(draft_, true);
            }
        });
}

void DerivBookPage::show_state() {
    if (loading_ && !loaded_) {
        states_->setCurrentIndex(0);
        return;
    }
    if (!load_error_.isEmpty()) {
        empty_is_error_ = true;
        empty_->apply(ui::EmptyState::error_spec(load_error_, tr("Could not open the position book"), tr("RETRY")));
        states_->setCurrentIndex(2);
        return;
    }
    if (positions_.isEmpty()) {
        empty_is_error_ = false;
        ui::EmptyState::Spec s = ui::EmptyState::no_data_spec(
            tr("No positions in this book yet"),
            tr("Save options, stock, futures, swaps or whole strategies here to follow their marks, P&L since entry and "
               "Greeks on free delayed data. Add one by hand, from a chain row (right-click), or with ADD TO BOOK on the "
               "STRATEGY page."),
            tr("ADD A POSITION"));
        if (ctx_ && ctx_->position().valid)
            s.secondary = tr("ADD THE STRATEGY PAGE'S POSITION");
        empty_->apply(s);
        states_->setCurrentIndex(2);
        update_status();
        return;
    }
    states_->setCurrentIndex(1);
}

void DerivBookPage::add_draft(const bk::BookPosition& draft) {
    if (!loaded_) {
        draft_ = draft;
        pending_draft_ = true;
        if (!loading_)
            reload_positions();
        return;
    }
    edit_position(draft, true);
}

void DerivBookPage::edit_position(const bk::BookPosition& p, bool is_new) {
    bk::BookPosition seed = p;
    if (seed.book.isEmpty())
        seed.book = book_combo_->currentData().toString().isEmpty() ? QStringLiteral("Main")
                                                                     : book_combo_->currentData().toString();
    DbpEditDialog dlg(seed, books_, is_new, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    save_position(dlg.result());
}

void DerivBookPage::save_position(const bk::BookPosition& p) {
    status_->setText(tr("Saving…"));
    DerivBookStore::instance().save_position(p, this, [this](const bk::BookPosition& saved, const QString& error) {
        if (!error.isEmpty()) {
            status_->setText(error);
            return;
        }
        LOG_INFO("DerivBook", QStringLiteral("Book position saved kind=%1").arg(saved.kind));
        // Show it at once (the reload that follows brings the stored rows).
        const QString filter = book_memory_->text();
        if (!filter.isEmpty() && filter != saved.book) {
            // Moved to another book: it leaves this view.
            positions_.erase(std::remove_if(positions_.begin(), positions_.end(),
                                            [&saved](const bk::BookPosition& x) { return x.id == saved.id; }),
                             positions_.end());
            model_->set_positions(positions_);
            show_state();
            revalue();
        } else {
            bool found = false;
            for (bk::BookPosition& x : positions_)
                if (x.id == saved.id) {
                    x = saved;
                    found = true;
                }
            if (!found)
                positions_.append(saved);
            model_->set_positions(positions_);
            show_state();
            revalue();
            enqueue(dbp_items(saved), false);
        }
        status_->setText(tr("Saved “%1” in %2.").arg(saved.name.isEmpty() ? DerivBookModel::detail_text(saved) : saved.name,
                                                      saved.book));
    });
}

void DerivBookPage::delete_position(const QString& id) {
    DerivBookStore::instance().remove_position(id, this, [this, id](const QString& error) {
        if (!error.isEmpty()) {
            status_->setText(error);
            return;
        }
        LOG_INFO("DerivBook", "Book position deleted");
        positions_.erase(std::remove_if(positions_.begin(), positions_.end(),
                                        [&id](const bk::BookPosition& p) { return p.id == id; }),
                         positions_.end());
        model_->set_positions(positions_);
        show_state();
        revalue();
    });
}

// ── Marks ────────────────────────────────────────────────────────────────────

QStringList DerivBookPage::keys_in_view() const {
    QStringList out;
    if (!table_ || proxy_->rowCount() == 0)
        return out;
    int top = table_->rowAt(0);
    int bottom = table_->rowAt(table_->viewport()->height() - 1);
    if (top < 0)
        top = 0;
    if (bottom < 0)
        bottom = proxy_->rowCount() - 1;
    for (int r = top; r <= bottom; ++r) {
        const bk::BookPosition* p = model_->position(proxy_->mapToSource(proxy_->index(r, 0)).row());
        if (!p)
            continue;
        for (const QString& it : dbp_items(*p))
            if (!out.contains(it))
                out << it;
    }
    return out;
}

QStringList DerivBookPage::keys_all() const {
    QStringList out;
    for (const bk::BookPosition& p : model_->positions())
        for (const QString& it : dbp_items(p))
            if (!out.contains(it))
                out << it;
    return out;
}

void DerivBookPage::enqueue(const QStringList& items, bool force) {
    for (const QString& it : items) {
        if (it == in_flight_ || queue_.contains(it))
            continue;
        queue_.append(it);
        if (force)
            force_keys_.insert(it);
    }
    pump();
}

void DerivBookPage::pump() {
    if (!in_flight_.isEmpty() || queue_.isEmpty())
        return;
    in_flight_ = queue_.takeFirst();
    const bool force = force_keys_.contains(in_flight_);
    force_keys_.remove(in_flight_);
    fetched_at_.insert(in_flight_, QDateTime::currentDateTimeUtc());
    update_status();
    const QString key = in_flight_.mid(2);
    if (in_flight_.startsWith(QLatin1String("C:")))
        fetch_option_key(key, force);
    else if (in_flight_.startsWith(QLatin1String("Q:")))
        fetch_quote_keys({key});
    else
        fetch_future_root(key, force);
}

void DerivBookPage::fetch_option_key(const QString& key, bool force) {
    // The symbol as the user typed it (an index keeps its caret: "^SPX").
    QString sym = key;
    for (const bk::BookPosition& p : model_->positions())
        if (bk::underlying_key(p.underlying) == key) {
            sym = p.underlying;
            break;
        }
    const QString item = in_flight_;
    chain_req_ = FreeOptionChainService::instance().fetch(
        sym, QStringLiteral("auto"), force, this, [this, key, item](const FreeOptionChainService::Reply& r) {
            if (in_flight_ != item)
                return;
            if (r.chain && r.chain->ok()) {
                bk::UnderlyingQuote& q = market_.underlyings[key];
                q.chain = r.chain;
                q.spot = std::nan("");
                q.as_of = r.chain->quote_time.isValid() ? r.chain->quote_time : r.chain->fetched_at;
                q.source = r.chain->source_label;
                failures_.remove(key);
                schedule_revalue();
            }
            if (r.stage == FreeOptionChainService::Stage::Saved)
                return; // the fresh chain follows
            if (r.stage == FreeOptionChainService::Stage::Failed && r.status != QLatin1String("cancelled"))
                failures_.insert(key, r.message);
            chain_req_ = 0;
            in_flight_.clear();
            update_status();
            pump();
        });
}

void DerivBookPage::fetch_quote_keys(const QStringList& keys) {
    QStringList syms;
    for (const QString& key : keys) {
        QString sym = key;
        for (const bk::BookPosition& p : model_->positions())
            if (bk::underlying_key(p.underlying) == key) {
                sym = p.underlying;
                break;
            }
        syms << sym;
    }
    const QString item = in_flight_;
    QPointer<DerivBookPage> self(this);
    services::MarketDataService::instance().fetch_quotes(syms, [self, keys, item](bool ok, QVector<services::QuoteData> quotes) {
        if (!self || self->in_flight_ != item)
            return;
        for (int i = 0; i < keys.size(); ++i) {
            const services::QuoteData* hit = nullptr;
            for (const auto& q : quotes)
                if (bk::underlying_key(q.symbol) == keys[i] && q.price > 0)
                    hit = &q;
            if (!hit && quotes.size() == keys.size() && quotes[i].price > 0)
                hit = &quotes[i];
            if (ok && hit) {
                bk::UnderlyingQuote& u = self->market_.underlyings[keys[i]];
                u.spot = hit->price;
                u.as_of = hit->market_time > 0 ? QDateTime::fromSecsSinceEpoch(hit->market_time, QTimeZone::UTC)
                                               : QDateTime::currentDateTimeUtc();
                u.source = DerivBookPage::tr("delayed quote");
                self->failures_.remove(keys[i]);
            } else {
                self->failures_.insert(keys[i], DerivBookPage::tr("no price found"));
            }
        }
        self->in_flight_.clear();
        self->schedule_revalue();
        self->update_status();
        self->pump();
    });
}

void DerivBookPage::fetch_future_root(const QString& root, bool force) {
    const QString item = in_flight_;
    future_req_ = CurveDataService::instance().futures_curve(
        root, 12, force, this, [this, root, item](const CurveDataService::FuturesReply& rep) {
            if (in_flight_ != item)
                return;
            if (rep.curve && rep.curve->ok()) {
                const QDateTime at = rep.curve->fetched_at.isValid() ? rep.curve->fetched_at : QDateTime::currentDateTimeUtc();
                for (const mk::FuturesContract& c : rep.curve->contracts) {
                    if (!(c.last > 0))
                        continue;
                    market_.futures.insert(c.symbol.toUpper(), c.last);
                    market_.futures_as_of.insert(c.symbol.toUpper(), at);
                }
                failures_.remove(QStringLiteral("F:") + root);
                schedule_revalue();
            } else {
                failures_.insert(QStringLiteral("F:") + root,
                                 rep.message.isEmpty() ? tr("futures months unavailable") : rep.message);
            }
            future_req_ = 0;
            in_flight_.clear();
            update_status();
            pump();
        });
}

void DerivBookPage::ensure_curve() {
    bool need = false;
    for (const bk::BookPosition& p : positions_)
        need = need || dbp_has_swap(p);
    if (!need)
        return;
    auto& svc = CurveDataService::instance();
    if (auto c = svc.treasury_now(); c && c->ok()) {
        if (market_.curve_date != c->date || market_.curve.empty()) {
            market_.curve = mk::zero_curve(*c);
            market_.curve_date = c->date;
        }
        return;
    }
    if (curve_req_)
        return;
    curve_req_ = svc.treasury_curve(false, this, [this](const CurveDataService::TreasuryReply& r) {
        curve_req_ = 0;
        if (!r.curve || !r.curve->ok()) {
            failures_.insert(QStringLiteral("RATES"), r.message.isEmpty() ? tr("Treasury curve unavailable") : r.message);
            update_status();
            return;
        }
        market_.curve = mk::zero_curve(*r.curve);
        market_.curve_date = r.curve->date;
        failures_.remove(QStringLiteral("RATES"));
        schedule_revalue();
    });
}

void DerivBookPage::on_tick() {
    if (!isVisible() || !loaded_)
        return;
    // Only the rows in view, only what was not fetched in the last kFreshSeconds, at most kMaxPerTick.
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QStringList due;
    for (const QString& it : keys_in_view()) {
        const QDateTime at = fetched_at_.value(it);
        if (!at.isValid() || at.secsTo(now) >= kFreshSeconds)
            due << it;
    }
    if (due.isEmpty())
        return;
    tick_offset_ = tick_offset_ % due.size();
    QStringList pick;
    for (int i = 0; i < due.size() && pick.size() < kMaxPerTick; ++i)
        pick << due[(tick_offset_ + i) % due.size()];
    tick_offset_ += pick.size();
    enqueue(pick, false);
}

void DerivBookPage::refresh() {
    if (!loaded_) {
        reload_positions();
        return;
    }
    ensure_curve();
    enqueue(keys_all(), true);
}

void DerivBookPage::cancel_marking() {
    if (tick_)
        tick_->stop();
    queue_.clear();
    force_keys_.clear();
    if (chain_req_)
        FreeOptionChainService::instance().cancel(chain_req_);
    if (future_req_)
        CurveDataService::instance().cancel(future_req_);
    chain_req_ = future_req_ = 0;
    if (!in_flight_.isEmpty())
        fetched_at_.remove(in_flight_); // not fetched after all: due again on the next visit
    in_flight_.clear();
}

void DerivBookPage::schedule_revalue() {
    if (revalue_timer_)
        revalue_timer_->start();
}

void DerivBookPage::revalue() {
    const QVector<bk::BookPosition> rows = model_->positions();
    bk::MarketView m = market_;
    m.today = QDate::currentDate();
    const quint64 id = ++value_id_;
    if (value_cancel_)
        value_cancel_->store(true);
    value_cancel_ = std::make_shared<std::atomic<bool>>(false);
    auto cancel = value_cancel_;
    QPointer<DerivBookPage> self(this);
    (void)QtConcurrent::run([self, id, rows, m, cancel]() {
        QVector<bk::PositionResult> res = bk::value_book(rows, m);
        if (cancel->load())
            return;
        bk::BookSummary sum = bk::summarise(rows, res, m.today);
        if (cancel->load() || !self)
            return; // superseded, or the page is gone (its destructor sets the flag)
        QMetaObject::invokeMethod(
            self.data(),
            [self, id, res, sum]() {
                if (self && id == self->value_id_)
                    self->apply_values(res, sum);
            },
            Qt::QueuedConnection);
    });
}

void DerivBookPage::apply_values(const QVector<bk::PositionResult>& results, const bk::BookSummary& summary) {
    model_->set_results(results);
    summary_ = summary;
    valued_at_ = QDateTime::currentDateTime();
    update_kpis();
    update_detail();
    update_status();
}

void DerivBookPage::update_kpis() {
    kpis_->clear();
    const bk::Exposure& t = summary_.total;
    const bool one = summary_.currencies.size() == 1;
    const QString ccy = one ? summary_.currencies.first() : QString();
    auto tile = [this](const QString& label, const QString& value, const QString& sub, double sign = 0) {
        ui::StatTile* s = kpis_->add_tile(label);
        s->set_value(value);
        s->set_compact(true);
        if (!sub.isEmpty())
            s->set_sub_label(sub);
        if (sign != 0)
            s->set_tone(sign > 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative);
        return s;
    };
    auto m = [&](double v) { return one ? fmt::money(v, ccy, fmt::Compact::None, 0) : tr("mixed currencies"); };
    auto sm = [&](double v) { return one ? fmt::signed_money(v, ccy, fmt::Compact::None, 0) : tr("mixed currencies"); };
    tile(tr("POSITIONS"), QString::number(t.positions), tr("%1 underlyings").arg(summary_.by_underlying.size()));
    tile(tr("MARKET VALUE"), m(t.value), tr("futures: open P&L"));
    tile(tr("P&L SINCE ENTRY"), sm(t.pnl), QString(), one ? t.pnl : 0.0);
    tile(tr("$ DELTA"), sm(t.dollar_delta), tr("delta × spot"));
    tile(tr("$ GAMMA"), sm(t.dollar_gamma), tr("per 1% move"));
    tile(tr("VEGA"), sm(t.vega), tr("per vol point"));
    tile(tr("THETA"), sm(t.theta), tr("per day"));
    tile(tr("RHO"), sm(t.rho), tr("per 1% rates"));
    QDateTime oldest;
    for (int r = 0; r < model_->rowCount(); ++r)
        if (const bk::PositionResult* res = model_->result(r); res && res->as_of.isValid() &&
                                                               (!oldest.isValid() || res->as_of < oldest))
            oldest = res->as_of;
    ui::StatTile* marks = tile(tr("MARKS AS OF"),
                               oldest.isValid() ? oldest.toLocalTime().toString(QStringLiteral("d MMM HH:mm")) : dbp_dash(),
                               summary_.stale > 0 ? tr("%1 at entry price").arg(summary_.stale) : tr("delayed data"));
    marks->set_tooltip_text(tr("The oldest market time behind the marks on screen. Quotes are delayed (about 15 minutes "
                               "for Cboe; Yahoo Finance varies)."));
    if (summary_.stale > 0)
        marks->set_tone(ui::StatTile::Tone::Warning);
    note_->setText(tr("Options are marked at the delayed chain's mid, else Black-Scholes-Merton at the chain's IV "
                      "for that strike, else at the IV recorded at entry (European approximation of American "
                      "contracts); stock at the spot; futures at the contract's last price (value = open P&L); swaps off "
                      "the U.S. Treasury curve (a proxy: no free OIS curve exists). Greeks are position totals. Totals "
                      "%1. Marks refresh only while this page is in view: up to %2 underlyings a minute for the rows on "
                      "screen.")
                       .arg(one ? tr("are in %1").arg(ccy) : tr("mix currencies and are not meaningful"))
                       .arg(kMaxPerTick));
}

void DerivBookPage::update_detail() {
    const int view = detail_ ? detail_->currentIndex() : 0;
    const QString ccy = summary_.currencies.size() == 1 ? summary_.currencies.first() : QString();
    if (view == 0 && exposure_model_) {
        exposure_model_->clear();
        exposure_model_->setHorizontalHeaderLabels({tr("UNDERLYING"), tr("POSITIONS"), tr("MKT VALUE"), tr("P&L"),
                                                    tr("DELTA"), tr("$ DELTA"), tr("$ GAMMA 1%"), tr("VEGA"),
                                                    tr("THETA/DAY"), tr("RHO 1%")});
        QStringList cats;
        QVector<double> vals;
        const int metric = exposure_metric_->currentIndex();
        for (const bk::Exposure& e : summary_.by_underlying) {
            const QString c = e.currency;
            const bool rates = e.key.startsWith(QLatin1String("RATES:"));
            exposure_model_->appendRow({dbp_text_cell(e.label), dbp_cell(QString::number(e.positions), e.positions),
                                        dbp_cell(fmt::money(e.value, c), e.value), dbp_cell(fmt::signed_money(e.pnl, c), e.pnl),
                                        dbp_cell(rates ? dbp_dash() : fmt::number(e.delta, 1), e.delta),
                                        dbp_cell(fmt::signed_money(e.dollar_delta, c, fmt::Compact::None, 0), e.dollar_delta),
                                        dbp_cell(fmt::signed_money(e.dollar_gamma, c, fmt::Compact::None, 0), e.dollar_gamma),
                                        dbp_cell(fmt::signed_money(e.vega, c), e.vega),
                                        dbp_cell(fmt::signed_money(e.theta, c), e.theta),
                                        dbp_cell(fmt::signed_money(e.rho, c), e.rho)});
            cats << e.label;
            vals << (metric == 0 ? e.dollar_delta : metric == 1 ? e.vega : metric == 2 ? e.theta : e.value);
        }
        exposure_chart_->set_data(cats, vals, exposure_metric_->currentText());
        exposure_chart_->set_value_formatter([ccy](double v) { return fmt::compact(v, fmt::Compact::FromSettings, ccy); });
        exposure_chart_->set_state(cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                                   tr("No positions to show"));
    } else if (view == 1 && ladder_model_) {
        static const char* const kBuckets[] = {QT_TR_NOOP("Expired"),   QT_TR_NOOP("≤ 7 days"),    QT_TR_NOOP("8–30 days"),
                                               QT_TR_NOOP("31–90 days"), QT_TR_NOOP("91–180 days"), QT_TR_NOOP("181–365 days"),
                                               QT_TR_NOOP("> 1 year")};
        ladder_model_->clear();
        ladder_model_->setHorizontalHeaderLabels({tr("EXPIRY"), tr("DAYS"), tr("BUCKET"), tr("LEGS"), tr("MKT VALUE"),
                                                  tr("$ DELTA"), tr("GAMMA"), tr("VEGA"), tr("THETA/DAY")});
        QVector<double> by_bucket(bk::kExpiryBuckets, 0.0);
        const int metric = ladder_metric_->currentIndex();
        for (const bk::LadderRow& l : summary_.ladder) {
            ladder_model_->appendRow({dbp_cell(dbp_date(l.expiry), l.expiry.toJulianDay()), dbp_cell(QString::number(l.days), l.days),
                                      dbp_text_cell(tr(kBuckets[std::clamp(l.bucket, 0, bk::kExpiryBuckets - 1)])),
                                      dbp_cell(QString::number(l.legs), l.legs), dbp_cell(fmt::money(l.value, ccy), l.value),
                                      dbp_cell(fmt::signed_money(l.dollar_delta, ccy, fmt::Compact::None, 0), l.dollar_delta),
                                      dbp_cell(fmt::number(l.gamma, 3), l.gamma), dbp_cell(fmt::signed_money(l.vega, ccy), l.vega),
                                      dbp_cell(fmt::signed_money(l.theta, ccy), l.theta)});
            by_bucket[std::clamp(l.bucket, 0, bk::kExpiryBuckets - 1)] +=
                metric == 0 ? l.dollar_delta : metric == 1 ? l.vega : metric == 2 ? l.theta : l.value;
        }
        QStringList cats;
        for (const char* b : kBuckets)
            cats << tr(b);
        ladder_chart_->set_data(cats, by_bucket, ladder_metric_->currentText());
        ladder_chart_->set_value_formatter([ccy](double v) { return fmt::compact(v, fmt::Compact::FromSettings, ccy); });
        ladder_chart_->set_state(summary_.ladder.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                                 tr("No options or futures with an expiry"));
    } else if (view == 2 && legs_model_) {
        legs_model_->clear();
        legs_model_->setHorizontalHeaderLabels({tr("TYPE"), tr("QTY"), tr("STRIKE"), tr("EXPIRY"), tr("ENTRY"), tr("MARK"),
                                                tr("IV"), tr("MARKED FROM"), tr("VALUE"), tr("P&L"), tr("DELTA"), tr("VEGA"),
                                                tr("THETA/DAY")});
        const QModelIndex cur = table_->currentIndex();
        const int row = cur.isValid() ? proxy_->mapToSource(cur).row() : -1;
        const bk::BookPosition* p = model_->position(row);
        const bk::PositionResult* r = model_->result(row);
        if (!p) {
            legs_caption_->setText(tr("Select a position above to see its legs."));
            return;
        }
        legs_caption_->setText(tr("%1 · %2 · multiplier %3")
                                   .arg(p->name.isEmpty() ? DerivBookModel::detail_text(*p) : p->name, p->underlying,
                                        fmt::number(p->multiplier, 0)));
        static const char* const kTypes[] = {QT_TR_NOOP("Call"), QT_TR_NOOP("Put"), QT_TR_NOOP("Stock"), QT_TR_NOOP("Future"),
                                             QT_TR_NOOP("Swap")};
        for (int i = 0; i < p->legs.size(); ++i) {
            const bk::BookLeg& l = p->legs[i];
            const bk::LegResult* lr = r && i < r->legs.size() ? &r->legs[i] : nullptr;
            const QString c = p->currency;
            auto opt = [&](double v, const QString& t) { return lr && std::isfinite(v) ? dbp_cell(t, v) : dbp_cell(dbp_dash()); };
            legs_model_->appendRow({dbp_text_cell(tr(kTypes[static_cast<int>(l.type)])),
                                    dbp_cell(fmt::signed_number(l.type == bk::LegType::Swap ? l.notional : l.quantity, 0)),
                                    dbp_cell(l.is_option() ? fmt::number(l.strike, 2) : dbp_dash()),
                                    dbp_cell(l.type == bk::LegType::Swap
                                                 ? tr("%1 y + %2 y").arg(fmt::number(l.swap_start, 2), fmt::number(l.swap_tenor, 2))
                                                 : dbp_date(l.expiry)),
                                    dbp_cell(l.type == bk::LegType::Swap ? fmt::fraction_pct(l.swap_rate, 3, false)
                                                                         : fmt::number(l.entry_price, 2)),
                                    opt(lr ? lr->mark : std::nan(""), lr ? fmt::number(lr->mark, 2) : QString()),
                                    opt(lr ? lr->iv : std::nan(""), lr ? vol_pct(lr->iv, 1) : QString()),
                                    dbp_text_cell(lr ? DerivBookModel::source_text(lr->source) : tr("not valued yet")),
                                    opt(lr ? lr->value : std::nan(""), lr ? fmt::money(lr->value, c) : QString()),
                                    opt(lr ? lr->pnl : std::nan(""), lr ? fmt::signed_money(lr->pnl, c) : QString()),
                                    opt(lr ? lr->delta : std::nan(""), lr ? fmt::number(lr->delta, 1) : QString()),
                                    opt(lr ? lr->vega : std::nan(""), lr ? fmt::signed_money(lr->vega, c) : QString()),
                                    opt(lr ? lr->theta : std::nan(""), lr ? fmt::signed_money(lr->theta, c) : QString())});
        }
    }
}

void DerivBookPage::update_status() {
    if (!status_)
        return;
    QStringList parts;
    if (!in_flight_.isEmpty())
        parts << tr("Refreshing marks for %1…").arg(in_flight_.mid(2));
    if (!queue_.isEmpty())
        parts << tr("%1 more queued").arg(queue_.size());
    if (!failures_.isEmpty()) {
        QStringList names;
        for (auto it = failures_.constBegin(); it != failures_.constEnd(); ++it)
            names << it.key();
        QDateTime last;
        for (int r = 0; r < model_->rowCount(); ++r)
            if (const bk::PositionResult* res = model_->result(r); res && res->as_of.isValid() &&
                                                                   (!last.isValid() || res->as_of > last))
                last = res->as_of;
        parts << (last.isValid() ? tr("Could not refresh marks for %1 - showing the last values from %2")
                                       .arg(names.join(QStringLiteral(", ")),
                                            last.toLocalTime().toString(QStringLiteral("d MMM HH:mm")))
                                 : tr("Could not refresh marks for %1 - carried at entry prices")
                                       .arg(names.join(QStringLiteral(", "))));
    }
    if (parts.isEmpty() && valued_at_.isValid())
        parts << tr("Valued %1 · delayed data").arg(valued_at_.toString(QStringLiteral("HH:mm:ss")));
    status_->setText(parts.join(QStringLiteral(" · ")));
    if (!failures_.isEmpty()) {
        QStringList lines;
        for (auto it = failures_.constBegin(); it != failures_.constEnd(); ++it)
            lines << it.key() + QStringLiteral(": ") + it.value();
        status_->setToolTip(lines.join(QLatin1Char('\n')));
    } else {
        status_->setToolTip(QString());
    }
}

// ── Row actions / hand-offs ──────────────────────────────────────────────────

void DerivBookPage::row_menu(const QPoint& pos) {
    const QModelIndex index = table_->indexAt(pos);
    QMenu menu(this);
    const int row = index.isValid() ? proxy_->mapToSource(index).row() : -1;
    if (const bk::BookPosition* pp = model_->position(row)) {
        const bk::BookPosition p = *pp;
        bool options = false, single_option = p.legs.size() == 1 && p.legs.first().is_option();
        for (const bk::BookLeg& l : p.legs)
            options = options || l.is_option();
        menu.addAction(tr("Edit…"), this, [this, p] { edit_position(p, false); });
        menu.addAction(tr("Duplicate"), this, [this, p] {
            bk::BookPosition c = p;
            c.id.clear();
            c.created_at = 0;
            c.name = (p.name.isEmpty() ? DerivBookModel::detail_text(p) : p.name) + tr(" (copy)");
            save_position(c);
        });
        menu.addAction(tr("Delete…"), this, [this, p] {
            const QString name = p.name.isEmpty() ? DerivBookModel::detail_text(p) : p.name;
            if (QMessageBox::question(this, tr("Delete position"), tr("Delete “%1” from the book?").arg(name)) ==
                QMessageBox::Yes)
                delete_position(p.id);
        });
        menu.actions().constLast()->setProperty("fincept_agent_risk", "delete"); // agents: deletes the position from the book
        menu.addSeparator();
        if (options) {
            menu.addAction(tr("Open in STRATEGY"), this, [this, p] { open_in_strategy(p); });
            menu.addAction(tr("Run SCENARIOS on this position"), this,
                           [this, p] { open_in_scenarios({p}, p.name.isEmpty() ? DerivBookModel::detail_text(p) : p.name); });
            const QString key = bk::underlying_key(p.underlying);
            QVector<bk::BookPosition> same;
            for (const bk::BookPosition& x : model_->positions())
                if (bk::underlying_key(x.underlying) == key && x.kind != QLatin1String("swap") &&
                    x.kind != QLatin1String("future"))
                    same << x;
            if (same.size() > 1)
                menu.addAction(tr("Run SCENARIOS on every %1 position (%2)").arg(p.underlying).arg(same.size()), this,
                               [this, same, p] { open_in_scenarios(same, tr("Book: every %1 position").arg(p.underlying)); });
        }
        if (single_option)
            menu.addAction(tr("Price in PRICER"), this, [this, p] { price_in_pricer(p); });
        menu.addSeparator();
    }
    ui::ChartExport::MenuSpec spec;
    spec.suggested_name = QStringLiteral("derivatives_book");
    spec.csv = [this] { return export_table(); };
    spec.offer_image = false;
    ui::ChartExport::populate_menu(&menu, table_, spec);
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

void DerivBookPage::open_in_strategy(const bk::BookPosition& p) {
    if (ctx_)
        emit ctx_->strategy_load_requested(p.underlying, p.name.isEmpty() ? DerivBookModel::detail_text(p) : p.name, p.legs);
}

void DerivBookPage::open_in_scenarios(const QVector<bk::BookPosition>& positions, const QString& label) {
    if (!ctx_ || positions.isEmpty())
        return;
    namespace st = services::options::strategy;
    DerivContext::Position pos;
    pos.name = label;
    pos.symbol = positions.first().underlying;
    pos.currency = positions.first().currency;
    pos.multiplier = 100;
    const QDate today = QDate::currentDate();
    const bk::UnderlyingQuote q = market_.underlyings.value(bk::underlying_key(pos.symbol));
    double iv_sum = 0;
    int iv_n = 0;
    int skipped = 0;
    double spot = std::isfinite(q.spot) ? q.spot : (q.chain ? q.chain->spot : std::nan(""));
    for (const bk::BookPosition& p : positions) {
        const int row = model_->row_of(p.id);
        const bk::PositionResult* r = model_->result(row);
        int skip = 0;
        const std::vector<st::Leg> legs = bk::strategy_legs(p, today, &skip);
        skipped += skip;
        int li = 0;
        for (int i = 0; i < p.legs.size(); ++i) {
            const bk::BookLeg& b = p.legs[i];
            if (!b.is_option() && b.type != bk::LegType::Stock)
                continue;
            if (li >= static_cast<int>(legs.size()))
                break;
            LegView v;
            v.leg = legs[static_cast<size_t>(li++)];
            if (b.is_option()) {
                v.leg.quantity *= p.multiplier / pos.multiplier; // one spec multiplier for every position
                v.expiry_label = b.expiry.toString(Qt::ISODate);
                if (r && i < r->legs.size() && std::isfinite(r->legs[i].iv) && r->legs[i].iv > 0)
                    v.leg.iv = r->legs[i].iv; // the marking IV
                if (v.leg.expiry_years <= 0)
                    continue; // expired: nothing left to stress
                iv_sum += v.leg.iv;
                ++iv_n;
            }
            pos.legs.append(v);
        }
        if (!std::isfinite(spot) && r && std::isfinite(r->total.spot))
            spot = r->total.spot;
    }
    if (pos.legs.isEmpty() || !(spot > 0)) {
        status_->setText(!(spot > 0) ? tr("No spot price for %1 yet - refresh the marks first.").arg(pos.symbol)
                                     : tr("Nothing left to stress: the option legs have expired."));
        return;
    }
    pos.market.spot = spot;
    pos.market.r = CurveDataService::instance().rate_for(0.25, 0.04);
    pos.market.q = 0;
    if (q.chain && !q.chain->expiries.isEmpty()) {
        const mk::FreeExpiry& x = q.chain->expiries.first();
        pos.market.r = x.rate;
        pos.market.q = x.carry_q;
        pos.atm_iv = x.smile.atm_iv > 0 ? x.smile.atm_iv : (iv_n > 0 ? iv_sum / iv_n : 0.2);
    } else {
        pos.atm_iv = iv_n > 0 ? iv_sum / iv_n : 0.2;
    }
    pos.valid = true;
    ctx_->set_book_position(pos);
    if (skipped > 0)
        status_->setText(tr("%1 futures / swap leg(s) are not part of the option scenarios.").arg(skipped));
    emit ctx_->book_scenarios_requested();
}

void DerivBookPage::price_in_pricer(const bk::BookPosition& p) {
    if (!ctx_ || p.legs.size() != 1 || !p.legs.first().is_option())
        return;
    const bk::BookLeg& l = p.legs.first();
    const bk::PositionResult* r = model_->result(model_->row_of(p.id));
    const bk::UnderlyingQuote q = market_.underlyings.value(bk::underlying_key(p.underlying));
    ContractRef c;
    c.symbol = p.underlying;
    c.expiry_label = l.expiry.toString(Qt::ISODate);
    c.strike = l.strike;
    c.is_call = l.type == bk::LegType::Call;
    c.spot = r && std::isfinite(r->total.spot) ? r->total.spot : (q.chain ? q.chain->spot : l.strike);
    c.years = r && r->total.years > 0 ? r->total.years : std::max(bk::years_to(QDate::currentDate(), l.expiry), 1.0 / 365.0);
    c.iv = r && std::isfinite(r->total.iv) ? r->total.iv : (l.entry_iv > 0 ? l.entry_iv : 0.2);
    c.mid = r && std::isfinite(r->total.mark) ? r->total.mark : l.entry_price;
    c.rate = CurveDataService::instance().rate_for(c.years, 0.04);
    if (q.chain)
        for (const mk::FreeExpiry& x : q.chain->expiries)
            if (x.date == l.expiry) {
                c.rate = x.rate;
                c.q = x.carry_q;
            }
    emit ctx_->price_contract_requested(c);
}

// ── Export ───────────────────────────────────────────────────────────────────

ui::ChartExport::Table DerivBookPage::export_table() const {
    ui::ChartExport::Table t;
    for (int c = 0; c < DerivBookModel::kCols; ++c)
        t.headers << model_->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    for (int r = 0; r < proxy_->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < DerivBookModel::kCols; ++c)
            row << proxy_->data(proxy_->index(r, c), Qt::DisplayRole).toString();
        t.rows.append(row);
    }
    return t;
}

QPixmap DerivBookPage::export_image() const {
    if (detail_ && detail_->currentIndex() == 1 && ladder_chart_)
        return ui::ChartExport::grab(ladder_chart_);
    if (exposure_chart_)
        return ui::ChartExport::grab(exposure_chart_);
    return ui::ChartExport::grab(table_);
}

report::ReportPayload DerivBookPage::report_payload() const {
    report::ReportPayload p;
    p.source = QStringLiteral("Derivatives");
    const QString book = book_combo_->currentText();
    p.title = tr("Derivatives position book: %1").arg(book);
    p.as_of = valued_at_.isValid() ? valued_at_ : QDateTime::currentDateTime();
    p.kpi_title = tr("Book at a glance");
    for (int i = 0; i < kpis_->count(); ++i) {
        report::PayloadKpi k;
        k.label = kpis_->tile(i)->label();
        k.value = kpis_->tile(i)->value_text();
        p.kpis.append(k);
    }
    static const int kCols[] = {DerivBookModel::kName,  DerivBookModel::kUnderlying,  DerivBookModel::kDetail,
                                DerivBookModel::kMark,  DerivBookModel::kValue,       DerivBookModel::kPnl,
                                DerivBookModel::kDollarDelta, DerivBookModel::kVega, DerivBookModel::kTheta,
                                DerivBookModel::kExpiry, DerivBookModel::kMarked};
    report::PayloadTable pos;
    pos.title = tr("Positions");
    for (int c : kCols)
        pos.columns << model_->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    for (int r = 0; r < proxy_->rowCount(); ++r) {
        QStringList row;
        for (int c : kCols)
            row << proxy_->data(proxy_->index(r, c), Qt::DisplayRole).toString();
        pos.rows.append(row);
    }
    p.tables.append(pos);
    report::PayloadTable ex;
    ex.title = tr("Exposure by underlying");
    ex.columns = {tr("Underlying"), tr("Positions"), tr("Market value"), tr("P&L"), tr("$ delta"), tr("Vega"), tr("Theta/day")};
    for (const bk::Exposure& e : summary_.by_underlying)
        ex.rows.append({e.label, QString::number(e.positions), fmt::money(e.value, e.currency),
                        fmt::signed_money(e.pnl, e.currency), fmt::signed_money(e.dollar_delta, e.currency, fmt::Compact::None, 0),
                        fmt::signed_money(e.vega, e.currency), fmt::signed_money(e.theta, e.currency)});
    p.tables.append(ex);
    if (exposure_chart_) {
        report::PayloadChart ch;
        ch.title = tr("Exposure by underlying");
        ch.image = png_bytes(ui::ChartExport::grab(exposure_chart_));
        if (!ch.image.isEmpty())
            p.charts.append(ch);
    }
    p.inputs = {{tr("Book"), book}, {tr("Data"), tr("Free delayed quotes (Cboe / Yahoo Finance), U.S. Treasury curve")}};
    p.notes = note_->text();
    return p;
}

// ── Language ─────────────────────────────────────────────────────────────────

void DerivBookPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivBookPage::retranslate() {
    book_.apply();
    if (book_combo_->count() > 0)
        book_combo_->setItemText(0, tr("All books"));
    if (add_btn_->menu()) {
        add_btn_->menu()->clear();
        add_menu(add_btn_->menu());
    }
    skeleton_->set_message(tr("Opening the position book…"));
    emit model_->headerDataChanged(Qt::Horizontal, 0, DerivBookModel::kCols - 1);
    if (loaded_) {
        show_state();
        update_kpis();
        update_detail();
        update_status();
    }
}

} // namespace fincept::screens::deriv
