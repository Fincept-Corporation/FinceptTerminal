// src/screens/alt_investments/AltDueDiligencePanel.cpp — A10: private credit analytics and
// due-diligence checklists. See AltDueDiligencePanel.h.

#include "screens/alt_investments/AltDueDiligencePanel.h"

#include "core/logging/Logger.h"
#include "core/report/ReportBlocks.h"
#include "screens/alt_investments/AltCreditMath.h"
#include "screens/alt_investments/AltDdTemplates.h"
#include "screens/alt_investments/AltLiveMath.h"
#include "screens/alt_investments/AltPanels.h"
#include "services/options/CurveDataService.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/ResultView.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QRegularExpression>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QUuid>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::alt {

using fincept::services::alt::AltDdStore;
using fincept::services::alt::DdChecklist;

namespace {

constexpr int kAltDdStatusRole = Qt::UserRole + 1; ///< status value on a status cell
constexpr int kAltDdIdRole = Qt::UserRole + 2;     ///< item id on the item cell

QString alt_dd_style() {
    using namespace fincept::ui;
    return QString("#altDdPanel QTabWidget::pane { border: none; border-top: 1px solid %1; background: %2; }"
                   "#altDdPanel QTabBar::tab { background: transparent; color: %3; padding: 6px 14px; border: none;"
                   "  font-size: " +
                   fonts::px(-2) +
                   "px; font-weight: 700; }"
                   "#altDdPanel QTabBar::tab:selected { color: %4; border-bottom: 2px solid %4; }"
                   "#altDdPanel QTreeView { background: %2; color: %5; border: 1px solid %1; font-size: " +
                   fonts::px(-1) +
                   "px; alternate-background-color: %6; }"
                   "#altDdPanel QHeaderView::section { background: %7; color: %3; border: none;"
                   "  border-bottom: 1px solid %1; padding: 4px 6px; font-size: " +
                   fonts::px(-3) + "px; font-weight: 700; }")
        .arg(colors::BORDER_DIM(), colors::BG_SURFACE(), colors::TEXT_SECONDARY(), colors::AMBER(),
             colors::TEXT_PRIMARY(), colors::ROW_ALT(), colors::BG_RAISED());
}

QString alt_dd_status_label(const QString& s) {
    if (s == QLatin1String("done"))
        return QCoreApplication::translate("AltDueDiligence", "Done");
    if (s == QLatin1String("issue"))
        return QCoreApplication::translate("AltDueDiligence", "Issue");
    if (s == QLatin1String("na"))
        return QCoreApplication::translate("AltDueDiligence", "N/A");
    return QCoreApplication::translate("AltDueDiligence", "Open");
}

/// Status column: a combo to edit, and the status in its state colour read from the live theme
/// at paint time (nothing cached, so a theme switch needs no refresh).
class AltDdStatusDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& idx) const override {
        if (!idx.data(kAltDdStatusRole).isValid())
            return nullptr;
        auto* cb = new QComboBox(parent);
        for (const QString& s : alt_dd::statuses())
            cb->addItem(alt_dd_status_label(s), s);
        return cb;
    }
    void setEditorData(QWidget* editor, const QModelIndex& idx) const override {
        auto* cb = qobject_cast<QComboBox*>(editor);
        if (cb)
            cb->setCurrentIndex(std::max(0, cb->findData(idx.data(kAltDdStatusRole).toString())));
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& idx) const override {
        auto* cb = qobject_cast<QComboBox*>(editor);
        if (!cb)
            return;
        const QString s = cb->currentData().toString();
        model->setData(idx, s, kAltDdStatusRole);
        model->setData(idx, alt_dd_status_label(s), Qt::DisplayRole);
    }

  protected:
    void initStyleOption(QStyleOptionViewItem* opt, const QModelIndex& idx) const override {
        QStyledItemDelegate::initStyleOption(opt, idx);
        const QString s = idx.data(kAltDdStatusRole).toString();
        if (s.isEmpty())
            return;
        QColor c(fincept::ui::colors::TEXT_SECONDARY());
        if (s == QLatin1String("done"))
            c = QColor(fincept::ui::colors::SUCCESS());
        else if (s == QLatin1String("issue"))
            c = QColor(fincept::ui::colors::DANGER());
        else if (s == QLatin1String("na"))
            c = QColor(fincept::ui::colors::TEXT_TERTIARY());
        opt->palette.setColor(QPalette::Text, c);
        opt->font.setBold(s == QLatin1String("issue") || s == QLatin1String("done"));
    }
};

double alt_cr_value(const QHash<QString, QDoubleSpinBox*>& in, const char* key) {
    auto* sp = in.value(QString::fromLatin1(key));
    return sp ? sp->value() : 0.0;
}

QString alt_money(double v) {
    namespace fmt = fincept::ui::formatting;
    return fmt::money(v, fmt::display_currency(), fincept::ui::formatting::Compact::FromSettings);
}

} // namespace

QWidget* make_due_diligence_panel(QWidget* parent) {
    return new AltDueDiligencePanel(parent);
}

AltDueDiligencePanel::AltDueDiligencePanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("altDdPanel"));
    setStyleSheet(alt_dd_style()); // before any child exists
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    tabs_ = new QTabWidget(this);
    tabs_->addTab(build_credit_tab(tabs_), tr("PRIVATE CREDIT"));
    tabs_->addTab(build_dd_tab(tabs_), tr("DUE DILIGENCE"));
    root->addWidget(tabs_);
    load_checklists();
}

AltDueDiligencePanel::~AltDueDiligencePanel() {
    if (base_fetch_)
        services::options::CurveDataService::instance().cancel(base_fetch_);
    if (save_timer_ && save_timer_->isActive())
        save_current(); // flush an unsaved edit (the write itself runs on a worker)
    if (credit_pane_)
        credit_pane_->set_cancel({});
}

// ═══════════════════════════════════════════════════════════════════════════
// PRIVATE CREDIT
// ═══════════════════════════════════════════════════════════════════════════

QWidget* AltDueDiligencePanel::build_credit_tab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* hl = new QHBoxLayout(page);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    auto* split = new QSplitter(Qt::Horizontal, page);
    split->setChildrenCollapsible(false);
    hl->addWidget(split);

    // ── Inputs ──────────────────────────────────────────────────────────────
    auto* scroll = new QScrollArea(split);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* form_w = new QWidget(scroll);
    auto* fl = new QFormLayout(form_w);
    fl->setContentsMargins(12, 12, 12, 12);
    fl->setSpacing(6);
    fl->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    const alt_credit::LoanInputs def;
    auto add_spin = [this, form_w, fl](const char* key, const QString& label, double v, double lo, double hi, int dec,
                                       const QString& suffix, const QString& tip) {
        auto* sp = new QDoubleSpinBox(form_w);
        sp->setRange(lo, hi);
        sp->setDecimals(dec);
        sp->setValue(v);
        sp->setSuffix(suffix);
        sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sp->setToolTip(tip);
        sp->setAccessibleName(label);
        auto* lbl = new QLabel(label, form_w);
        lbl->setObjectName(QStringLiteral("altFieldLabel"));
        lbl->setToolTip(tip);
        fl->addRow(lbl, sp);
        credit_inputs_.insert(QString::fromLatin1(key), sp);
        return sp;
    };
    add_spin("face", tr("FACE AMOUNT"), def.face, 1, 1e12, 0, QString(), tr("Principal you hold"));
    add_spin("price_pct", tr("PRICE (% OF PAR)"), def.price_pct, 1, 200, 3, QStringLiteral("%"), tr("What you pay, % of face"));
    add_spin("upfront_fee_pct", tr("UPFRONT FEE / OID"), def.upfront_fee_pct, 0, 20, 2, QStringLiteral("%"),
             tr("Fee received at close, % of face (raises the yield)"));
    coupon_type_ = new QComboBox(form_w);
    coupon_type_->addItem(tr("Floating (base + spread)"), QStringLiteral("floating"));
    coupon_type_->addItem(tr("Fixed"), QStringLiteral("fixed"));
    coupon_type_->setAccessibleName(tr("Coupon type"));
    auto* ct_lbl = new QLabel(tr("COUPON"), form_w);
    ct_lbl->setObjectName(QStringLiteral("altFieldLabel"));
    fl->addRow(ct_lbl, coupon_type_);
    add_spin("base_pct", tr("BASE RATE (TODAY)"), def.base_pct, -5, 30, 3, QStringLiteral("%"),
             tr("The loan's reference rate today (e.g. 3-month SOFR); held flat over the life"));
    // The 3-month T-bill from the shared Treasury curve as a starting point (on request).
    auto* base_row = new QWidget(form_w);
    auto* brl = new QHBoxLayout(base_row);
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(6);
    auto* base_btn = new QPushButton(tr("3-MONTH T-BILL"), base_row);
    base_btn->setObjectName(QStringLiteral("altSecondaryBtn"));
    base_btn->setCursor(Qt::PointingHandCursor);
    base_btn->setToolTip(tr("Fill the base rate with the 3-month Treasury bill yield (U.S. Treasury daily curve) - close "
                            "to, not the same as, SOFR"));
    base_note_ = new QLabel(base_row);
    base_note_->setObjectName(QStringLiteral("altFieldLabel"));
    base_note_->setWordWrap(true);
    brl->addWidget(base_btn);
    brl->addWidget(base_note_, 1);
    fl->addRow(QString(), base_row);
    connect(base_btn, &QPushButton::clicked, this, [this, base_btn]() {
        auto& curves = services::options::CurveDataService::instance();
        if (base_fetch_)
            curves.cancel(base_fetch_);
        base_btn->setEnabled(false);
        base_note_->setText(tr("Fetching…"));
        QPointer<QPushButton> btn = base_btn;
        base_fetch_ = curves.treasury_curve(false, this, [this, btn](const services::options::CurveDataService::TreasuryReply& r) {
            base_fetch_ = 0;
            if (btn)
                btn->setEnabled(true);
            if (!r.curve || !r.curve->ok()) {
                base_note_->setText(r.message.isEmpty() ? tr("The Treasury curve could not be loaded.") : r.message);
                return;
            }
            const double y = alt_live::interpolate(r.curve->tenors, r.curve->yields, 0.25);
            if (!std::isfinite(y)) {
                base_note_->setText(tr("No 3-month yield on the curve."));
                return;
            }
            credit_inputs_.value(QStringLiteral("base_pct"))->setValue(y);
            base_note_->setText(tr("3-month T-bill %1% · %2 · %3")
                                    .arg(QLocale().toString(y, 'f', 2), r.curve->source_label,
                                         QLocale().toString(r.curve->date, QLocale::ShortFormat)) +
                                (r.stale ? tr(" · saved copy") : QString()));
        });
    });
    add_spin("spread_bps", tr("SPREAD"), def.spread_bps, -500, 5000, 0, tr(" bp"), tr("Margin over the base rate"));
    add_spin("floor_pct", tr("BASE RATE FLOOR"), def.floor_pct, -5, 20, 2, QStringLiteral("%"), tr("Minimum base rate"));
    add_spin("coupon_pct", tr("FIXED COUPON"), def.coupon_pct, 0, 50, 3, QStringLiteral("%"), tr("Annual coupon of a fixed-rate loan"));
    add_spin("reference_yield_pct", tr("REFERENCE YIELD (FIXED)"), 0, 0, 50, 3, QStringLiteral("%"),
             tr("Treasury / swap yield at the same maturity, for the spread of a fixed-rate loan (0 = no spread shown)"));
    add_spin("maturity_years", tr("MATURITY"), def.maturity_years, 0.25, 40, 2, tr(" yrs"), tr("Years to final repayment"));
    frequency_ = new QComboBox(form_w);
    frequency_->addItem(tr("Quarterly"), 4);
    frequency_->addItem(tr("Monthly"), 12);
    frequency_->addItem(tr("Semi-annual"), 2);
    frequency_->addItem(tr("Annual"), 1);
    frequency_->setAccessibleName(tr("Payment frequency"));
    auto* fq_lbl = new QLabel(tr("PAYMENTS"), form_w);
    fq_lbl->setObjectName(QStringLiteral("altFieldLabel"));
    fl->addRow(fq_lbl, frequency_);
    add_spin("amort_pct", tr("AMORTISATION A YEAR"), def.amort_pct, 0, 100, 2, QStringLiteral("%"),
             tr("Straight-line repayment, % of the original face each year; the rest at maturity"));
    add_spin("call_years", tr("EXPECTED REPAYMENT (TAKEOUT)"), 0, 0, 40, 2, tr(" yrs"),
             tr("Years until you expect the loan to be refinanced at par (0 = held to maturity)"));
    add_spin("pd_pct", tr("DEFAULT PROBABILITY A YEAR"), def.pd_pct, 0, 100, 2, QStringLiteral("%"),
             tr("Your estimate of the annual probability of default"));
    add_spin("lgd_pct", tr("LOSS GIVEN DEFAULT"), def.lgd_pct, 0, 100, 1, QStringLiteral("%"),
             tr("Share of the exposure lost in a default (1 - recovery)"));
    auto* run = new QPushButton(tr("CALCULATE"), form_w);
    run->setObjectName(QStringLiteral("altAnalyzeBtn"));
    run->setCursor(Qt::PointingHandCursor);
    fl->addRow(QString(), run);
    auto* note = new QLabel(tr("Floating coupons hold today's base rate flat (no forward curve is assumed). Expected loss "
                               "is PD x LGD a year; the loss-adjusted yield subtracts it from the yield."),
                            form_w);
    note->setObjectName(QStringLiteral("altMethodNote"));
    note->setWordWrap(true);
    fl->addRow(note);
    scroll->setWidget(form_w);
    split->addWidget(scroll);

    // ── Result ──────────────────────────────────────────────────────────────
    auto* right = new QWidget(split);
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(0, 0, 0, 0);
    rvl->setSpacing(0);
    auto* bar = new QWidget(right);
    bar->setObjectName(QStringLiteral("altRightBar"));
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(12, 6, 8, 6);
    bl->setSpacing(6);
    auto* title = new QLabel(tr("LOAN ANALYTICS"), bar);
    title->setObjectName(QStringLiteral("altRightTitle"));
    auto* status = new QLabel(bar);
    status->setObjectName(QStringLiteral("altStatusText"));
    bl->addWidget(title);
    bl->addStretch(1);
    bl->addWidget(status);
    auto* copy = new QPushButton(tr("COPY"), bar);
    auto* csv = new QPushButton(tr("CSV"), bar);
    for (auto* b : {copy, csv}) {
        b->setObjectName(QStringLiteral("altSecondaryBtn"));
        b->setCursor(Qt::PointingHandCursor);
        bl->addWidget(b);
    }
    auto* report = new ui::AddToReportButton(QStringLiteral("Alternative Investments"), bar);
    report->set_payload_provider([this]() { return credit_report(); });
    report->set_show_result(false);
    bl->addWidget(report);
    rvl->addWidget(bar);

    credit_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, right);
    credit_view_ = new ui::ResultView(credit_pane_->content_parent());
    credit_pane_->set_content(credit_view_);
    credit_pane_->set_cache_key(QStringLiteral("alt_investments/private_credit"));
    credit_pane_->set_max_age(ui::autoload::kNeverRefresh); // deterministic: same inputs, same answer
    credit_pane_->set_status_label(status);
    credit_pane_->set_start([this](quint64 id) {
        const QJsonObject p = credit_payload(); // microseconds: delivered at once
        if (p.contains(QStringLiteral("problem")))
            credit_pane_->fail(id, p.value(QStringLiteral("problem")).toString(), tr("Check the inputs"));
        else
            credit_pane_->deliver(id, p);
    });
    credit_pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) {
        credit_last_ = payload.toObject();
        ui::result::Options o;
        o.kpi_paths = {QStringLiteral("yield_pct"), QStringLiteral("loss_adjusted_yield_pct"), QStringLiteral("spread_bps"),
                       QStringLiteral("modified_duration"), QStringLiteral("expected_loss_pct"),
                       QStringLiteral("cumulative_pd_pct")};
        o.money_format = [](double v) { return alt_money(v); };
        o.unit_for = [](const QString&, const QString& key, double) {
            if (key.endsWith(QLatin1String("_pct")))
                return ui::result::Unit::Percent;
            if (key == QLatin1String("spread_bps") || key == QLatin1String("shock_bps"))
                return ui::result::Unit::Bps;
            if (key == QLatin1String("period"))
                return ui::result::Unit::Integer;
            if (key.contains(QLatin1String("duration")) || key.contains(QLatin1String("years")) ||
                key == QLatin1String("year") || key.contains(QLatin1String("life")))
                return ui::result::Unit::Plain;
            if (key == QLatin1String("interest") || key == QLatin1String("principal") || key == QLatin1String("balance") ||
                key.contains(QLatin1String("change")) || key == QLatin1String("cost"))
                return ui::result::Unit::Money;
            return ui::result::Unit::Auto;
        };
        o.x_keys << QStringLiteral("year");
        credit_view_->set_options(o);
        credit_view_->show_result(credit_last_);
    });
    rvl->addWidget(credit_pane_, 1);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    const int form_w_px = std::max(320, QFontMetrics(ui::fonts::font(-1)).averageCharWidth() * 46);
    scroll->setMinimumWidth(form_w_px);
    split->setSizes({form_w_px, 4000});

    // Remembered inputs: put back before anything runs; the pane matches its result to them.
    credit_memory_ = new ui::InputMemory(QStringLiteral("alt_investments"), QStringLiteral("private_credit"), page);
    for (auto it = credit_inputs_.cbegin(); it != credit_inputs_.cend(); ++it)
        credit_memory_->track(it.key(), it.value());
    credit_memory_->track(QStringLiteral("coupon_type"), coupon_type_);
    credit_memory_->track(QStringLiteral("frequency"), frequency_);
    credit_memory_->restore();
    credit_pane_->set_signature(credit_memory_->signature());
    connect(credit_memory_, &ui::InputMemory::changed, this, [this]() {
        credit_pane_->set_signature(credit_memory_->signature());
    });
    connect(coupon_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { update_credit_mode(); });
    update_credit_mode();
    connect(run, &QPushButton::clicked, credit_pane_, &ui::AutoLoadPane::run);
    // Amounts are in the display currency (a calculator the user denominates; never converted).
    ui::formatting::bind_currency_affix(credit_inputs_.value(QStringLiteral("face")));
    ui::formatting::on_format_changed(this, [this]() {
        if (!credit_last_.isEmpty())
            credit_view_->show_result(credit_last_);
    });

    auto table_text = [this]() {
        QVector<QStringList> rows;
        rows << QStringList{tr("Item"), tr("Value")};
        for (auto it = credit_last_.constBegin(); it != credit_last_.constEnd(); ++it)
            if (it.value().isDouble())
                rows << QStringList{ui::result::humanize(it.key()), QString::number(it.value().toDouble(), 'g', 10)};
        rows << QStringList{};
        rows << QStringList{tr("Period"), tr("Year"), tr("Interest"), tr("Principal"), tr("Balance")};
        for (const QJsonValue& v : credit_last_.value(QStringLiteral("schedule")).toArray()) {
            const QJsonObject o = v.toObject();
            rows << QStringList{QString::number(o.value(QStringLiteral("period")).toInt()),
                                QString::number(o.value(QStringLiteral("year")).toDouble(), 'f', 2),
                                QString::number(o.value(QStringLiteral("interest")).toDouble(), 'f', 2),
                                QString::number(o.value(QStringLiteral("principal")).toDouble(), 'f', 2),
                                QString::number(o.value(QStringLiteral("balance")).toDouble(), 'f', 2)};
        }
        return rows;
    };
    connect(copy, &QPushButton::clicked, this, [table_text]() {
        QStringList lines;
        for (const QStringList& r : table_text())
            lines << r.join(QLatin1Char('\t'));
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });
    connect(csv, &QPushButton::clicked, this, [this, table_text]() {
        QVector<QStringList> rows = table_text();
        const QStringList header = rows.takeFirst();
        ui::ChartExport::save_csv(header, rows, QStringLiteral("private_credit_loan"), this);
    });
    return page;
}

void AltDueDiligencePanel::update_credit_mode() {
    const bool floating = coupon_type_->currentData().toString() == QLatin1String("floating");
    for (const char* k : {"base_pct", "spread_bps", "floor_pct"})
        credit_inputs_.value(QString::fromLatin1(k))->setEnabled(floating);
    for (const char* k : {"coupon_pct", "reference_yield_pct"})
        credit_inputs_.value(QString::fromLatin1(k))->setEnabled(!floating);
}

QJsonObject AltDueDiligencePanel::credit_payload() const {
    alt_credit::LoanInputs in;
    in.face = alt_cr_value(credit_inputs_, "face");
    in.price_pct = alt_cr_value(credit_inputs_, "price_pct");
    in.upfront_fee_pct = alt_cr_value(credit_inputs_, "upfront_fee_pct");
    in.floating = coupon_type_->currentData().toString() == QLatin1String("floating");
    in.base_pct = alt_cr_value(credit_inputs_, "base_pct");
    in.spread_bps = alt_cr_value(credit_inputs_, "spread_bps");
    in.floor_pct = alt_cr_value(credit_inputs_, "floor_pct");
    in.coupon_pct = alt_cr_value(credit_inputs_, "coupon_pct");
    const double ref = alt_cr_value(credit_inputs_, "reference_yield_pct");
    in.reference_yield_pct = ref > 0 ? ref : alt_credit::kNaN;
    in.maturity_years = alt_cr_value(credit_inputs_, "maturity_years");
    in.frequency = frequency_->currentData().toInt();
    in.amort_pct = alt_cr_value(credit_inputs_, "amort_pct");
    in.call_years = alt_cr_value(credit_inputs_, "call_years");
    in.pd_pct = alt_cr_value(credit_inputs_, "pd_pct");
    in.lgd_pct = alt_cr_value(credit_inputs_, "lgd_pct");
    const alt_credit::LoanResult r = alt_credit::analyse(in);
    QJsonObject o;
    if (!r.problem.isEmpty()) {
        o.insert(QStringLiteral("problem"), tr("%1").arg(r.problem));
        return o;
    }
    auto put = [&o](const char* key, double v) {
        if (std::isfinite(v))
            o.insert(QString::fromLatin1(key), v);
    };
    put("yield_pct", r.yield_pct);
    put("loss_adjusted_yield_pct", r.loss_adjusted_yield_pct);
    put("spread_bps", r.spread_bps);
    put("modified_duration", r.modified_duration);
    put("expected_loss_pct", r.expected_loss_pct);
    put("cumulative_pd_pct", r.cumulative_pd_pct);
    put("effective_yield_pct", r.effective_yield_pct);
    put("yield_to_takeout_pct", r.yield_to_call_pct);
    put("coupon_pct", r.coupon_pct);
    put("macaulay_duration_years", r.macaulay_years);
    put("rate_duration_years", r.rate_duration);
    put("weighted_average_life_years", r.weighted_average_life);
    put("cost", r.cost);
    QJsonArray sched;
    for (const auto& p : r.periods)
        sched.append(QJsonObject{{QStringLiteral("period"), p.index},
                                 {QStringLiteral("year"), p.years},
                                 {QStringLiteral("interest"), p.interest},
                                 {QStringLiteral("principal"), p.principal},
                                 {QStringLiteral("balance"), p.balance_after}});
    o.insert(QStringLiteral("schedule"), sched);
    QJsonArray shocks;
    for (const auto& s : r.shocks) {
        QJsonObject so{{QStringLiteral("shock_bps"), s.bps}};
        if (std::isfinite(s.income_change))
            so.insert(QStringLiteral("annual_income_change"), s.income_change);
        if (std::isfinite(s.value_change))
            so.insert(QStringLiteral("value_change"), s.value_change);
        if (std::isfinite(s.value_change_pct))
            so.insert(QStringLiteral("value_change_pct"), s.value_change_pct);
        shocks.append(so);
    }
    o.insert(QStringLiteral("rate_shocks"), shocks);
    return o;
}

fincept::report::ReportPayload AltDueDiligencePanel::credit_report() const {
    fincept::report::ReportPayload p;
    if (credit_last_.isEmpty())
        return p;
    p.title = tr("Private credit: loan analytics");
    p.source = QStringLiteral("Alternative Investments");
    p.as_of = QDateTime::currentDateTime();
    auto pct = [](double v) { return QLocale().toString(v, 'f', 2); };
    auto kpi = [&p, &pct, this](const char* key, const QString& label, const QString& unit) {
        const QJsonValue v = credit_last_.value(QLatin1String(key));
        if (v.isDouble())
            p.kpis.append({label, pct(v.toDouble()), unit, QString()});
    };
    kpi("yield_pct", tr("Yield"), QStringLiteral("%"));
    kpi("loss_adjusted_yield_pct", tr("Loss-adjusted yield"), QStringLiteral("%"));
    kpi("spread_bps", tr("Spread"), QStringLiteral("bp"));
    kpi("modified_duration", tr("Modified duration"), tr("yrs"));
    kpi("expected_loss_pct", tr("Expected loss a year"), QStringLiteral("%"));
    kpi("cumulative_pd_pct", tr("Cumulative default probability"), QStringLiteral("%"));
    for (auto it = credit_inputs_.cbegin(); it != credit_inputs_.cend(); ++it)
        if (it.value()->isEnabled())
            p.inputs.append({ui::result::humanize(it.key()), it.value()->text()});
    p.inputs.append({tr("Coupon"), coupon_type_->currentText()});
    p.inputs.append({tr("Payments"), frequency_->currentText()});
    fincept::report::PayloadTable t;
    t.title = tr("Cash-flow schedule");
    t.columns = {tr("Period"), tr("Year"), tr("Interest"), tr("Principal"), tr("Balance")};
    for (const QJsonValue& v : credit_last_.value(QStringLiteral("schedule")).toArray()) {
        const QJsonObject o = v.toObject();
        t.rows << QStringList{QString::number(o.value(QStringLiteral("period")).toInt()),
                              QLocale().toString(o.value(QStringLiteral("year")).toDouble(), 'f', 2),
                              alt_money(o.value(QStringLiteral("interest")).toDouble()),
                              alt_money(o.value(QStringLiteral("principal")).toDouble()),
                              alt_money(o.value(QStringLiteral("balance")).toDouble())};
    }
    p.tables << t;
    p.notes = tr("Floating coupons hold today's base rate flat. Expected loss = PD x LGD a year; the loss-adjusted yield "
                 "subtracts it from the yield.");
    return p;
}

// ═══════════════════════════════════════════════════════════════════════════
// DUE DILIGENCE
// ═══════════════════════════════════════════════════════════════════════════

QWidget* AltDueDiligencePanel::build_dd_tab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(12, 10, 12, 10);
    vl->setSpacing(8);

    auto* bar = new QWidget(page);
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(6);
    auto* lbl = new QLabel(tr("CHECKLIST"), bar);
    lbl->setObjectName(QStringLiteral("altFieldLabel"));
    list_combo_ = new QComboBox(bar);
    list_combo_->setMinimumContentsLength(24);
    list_combo_->setAccessibleName(tr("Saved checklist"));
    connect(list_combo_, QOverload<int>::of(&QComboBox::activated), this, [this](int i) { show_checklist(i); });
    auto make_btn = [bar](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, bar);
        b->setObjectName(QStringLiteral("altSecondaryBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        return b;
    };
    auto* new_btn = make_btn(tr("NEW"), tr("Start a checklist from a template"));
    auto* menu = new QMenu(new_btn);
    for (const alt_dd::Template& t : alt_dd::templates()) {
        const QString id = QString::fromLatin1(t.id);
        menu->addAction(alt_dd::tr_dd(t.name).replace(QLatin1Char('&'), QLatin1String("&&")), this,
                        [this, id]() { new_checklist(id); });
    }
    new_btn->setMenu(menu);
    auto* rename_btn = make_btn(tr("RENAME"), tr("Rename this checklist"));
    auto* delete_btn = make_btn(tr("DELETE"), tr("Delete this checklist"));
    delete_btn->setProperty("fincept_agent_risk", "delete"); // agents: deletes the checklist
    auto* copy_btn = make_btn(tr("COPY"), tr("Copy as a Markdown checklist"));
    auto* csv_btn = make_btn(tr("CSV"), tr("Save as a CSV file (section, item, status, note)"));
    connect(rename_btn, &QPushButton::clicked, this, &AltDueDiligencePanel::rename_checklist);
    connect(delete_btn, &QPushButton::clicked, this, &AltDueDiligencePanel::delete_checklist);
    connect(copy_btn, &QPushButton::clicked, this, [this]() { QApplication::clipboard()->setText(dd_markdown()); });
    connect(csv_btn, &QPushButton::clicked, this, [this]() {
        if (current_ < 0)
            return;
        QString name = lists_.at(current_).name;
        name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
        ui::ChartExport::save_csv({tr("Section"), tr("Item"), tr("Status"), tr("Note")}, dd_rows(),
                                  QStringLiteral("due_diligence_") + name, this);
    });
    auto* report = new ui::AddToReportButton(QStringLiteral("Alternative Investments"), bar);
    report->set_payload_provider([this]() { return dd_report(); });
    report->set_show_result(false);
    bl->addWidget(lbl);
    bl->addWidget(list_combo_, 1);
    bl->addWidget(new_btn);
    bl->addWidget(rename_btn);
    bl->addWidget(delete_btn);
    bl->addSpacing(8);
    bl->addWidget(copy_btn);
    bl->addWidget(csv_btn);
    bl->addWidget(report);
    vl->addWidget(bar);
    dd_actions_ = {rename_btn, delete_btn, copy_btn, csv_btn};

    auto* info = new QWidget(page);
    auto* il = new QHBoxLayout(info);
    il->setContentsMargins(0, 0, 0, 0);
    progress_ = new QLabel(info);
    progress_->setObjectName(QStringLiteral("altResultTitle"));
    save_note_ = new QLabel(info);
    save_note_->setObjectName(QStringLiteral("altStatusText"));
    il->addWidget(progress_, 1);
    il->addWidget(save_note_);
    vl->addWidget(info);

    dd_stack_ = new QStackedWidget(page);
    dd_empty_ = new ui::EmptyState(dd_stack_);
    {
        QList<QPair<QString, QString>> rows;
        for (const alt_dd::Template& t : alt_dd::templates())
            rows.append({alt_dd::tr_dd(t.name), tr("%1 checks").arg(alt_dd::item_count(t))});
        dd_empty_->set_glyph(QStringLiteral("☑"), ui::EmptyState::Tone::Accent); // ☑
        dd_empty_->set_title(tr("No due-diligence checklist yet"));
        dd_empty_->set_message(tr("Start one per fund or deal you are reviewing: mark each check done, n/a or an issue "
                                  "and keep notes. Saved on this computer only."));
        dd_empty_->set_details(rows, tr("TEMPLATES"));
        dd_empty_->set_primary_action(tr("NEW PRIVATE EQUITY CHECKLIST"));
        dd_empty_->set_secondary_action(tr("NEW PRIVATE CREDIT CHECKLIST"));
        dd_empty_->set_note(tr("A working list drawn from public due-diligence questionnaires, not legal advice"),
                            ui::EmptyState::Tone::Info);
        connect(dd_empty_, &ui::EmptyState::primary_clicked, this, [this]() { new_checklist(QStringLiteral("pe")); });
        connect(dd_empty_, &ui::EmptyState::secondary_clicked, this,
                [this]() { new_checklist(QStringLiteral("private_credit")); });
    }
    dd_stack_->addWidget(dd_empty_);

    tree_ = new QTreeView(dd_stack_);
    model_ = new QStandardItemModel(0, 3, tree_);
    model_->setHorizontalHeaderLabels({tr("CHECK"), tr("STATUS"), tr("NOTE")});
    tree_->setModel(model_);
    tree_->setItemDelegateForColumn(1, new AltDdStatusDelegate(tree_));
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(true);
    tree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked |
                           QAbstractItemView::EditKeyPressed);
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    const QFontMetrics fm(ui::fonts::font(-1));
    tree_->header()->resizeSection(0, fm.averageCharWidth() * 64);
    tree_->header()->resizeSection(1, fm.horizontalAdvance(QStringLiteral("STATUS  Issue ▾")) + 24);
    connect(model_, &QStandardItemModel::itemChanged, this, [this](QStandardItem*) { on_item_edited(); });
    dd_stack_->addWidget(tree_);
    vl->addWidget(dd_stack_, 1);

    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(600);
    connect(save_timer_, &QTimer::timeout, this, &AltDueDiligencePanel::save_current);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        const QFontMetrics f(ui::fonts::font(-1));
        tree_->header()->resizeSection(1, f.horizontalAdvance(QStringLiteral("STATUS  Issue ▾")) + 24);
    });
    for (auto* b : dd_actions_)
        b->setEnabled(false);
    progress_->setText(tr("Loading checklists…"));
    return page;
}

void AltDueDiligencePanel::load_checklists() {
    AltDdStore::list(this, [this](const QVector<DdChecklist>& lists, const QString& error) {
        lists_ = lists;
        list_combo_->clear();
        for (const DdChecklist& c : lists_) {
            const alt_dd::Template* t = alt_dd::find_template(c.template_id);
            list_combo_->addItem(c.name + QStringLiteral("  ·  ") + (t ? alt_dd::tr_dd(t->name) : c.template_id));
        }
        if (!error.isEmpty())
            save_note_->setText(tr("Saved checklists could not be read."));
        show_checklist(lists_.isEmpty() ? -1 : 0);
    });
}

void AltDueDiligencePanel::show_checklist(int index) {
    if (save_timer_->isActive()) {
        save_timer_->stop();
        save_current();
    }
    current_ = (index >= 0 && index < lists_.size()) ? index : -1;
    for (auto* b : dd_actions_)
        b->setEnabled(current_ >= 0);
    if (current_ < 0) {
        dd_stack_->setCurrentIndex(0);
        progress_->setText(QString());
        return;
    }
    if (list_combo_->currentIndex() != current_)
        list_combo_->setCurrentIndex(current_);
    const DdChecklist& c = lists_.at(current_);
    QHash<QString, QJsonObject> saved;
    for (const QJsonValue& v : c.items)
        saved.insert(v.toObject().value(QStringLiteral("id")).toString(), v.toObject());

    filling_ = true;
    model_->removeRows(0, model_->rowCount());
    const alt_dd::Template* t = alt_dd::find_template(c.template_id);
    if (t) {
        for (const alt_dd::Section& s : t->sections) {
            auto* sec = new QStandardItem(alt_dd::tr_dd(s.title).toUpper());
            sec->setEditable(false);
            QFont f = sec->font();
            f.setBold(true);
            sec->setFont(f);
            auto* sec_status = new QStandardItem;
            sec_status->setEditable(false);
            auto* sec_note = new QStandardItem;
            sec_note->setEditable(false);
            for (const alt_dd::Item& it : s.items) {
                const QString id = QString::fromLatin1(it.id);
                const QJsonObject st = saved.value(id);
                auto* text = new QStandardItem(alt_dd::tr_dd(it.text));
                text->setEditable(false);
                text->setData(id, kAltDdIdRole);
                text->setToolTip(alt_dd::tr_dd(it.text));
                const QString status = st.value(QStringLiteral("status")).toString(QStringLiteral("open"));
                auto* stat = new QStandardItem(alt_dd_status_label(status));
                stat->setData(status, kAltDdStatusRole);
                auto* note = new QStandardItem(st.value(QStringLiteral("note")).toString());
                note->setToolTip(tr("Double-click to write a note"));
                sec->appendRow({text, stat, note});
            }
            model_->appendRow({sec, sec_status, sec_note});
        }
    }
    filling_ = false;
    tree_->expandAll();
    dd_stack_->setCurrentIndex(1);
    save_note_->setText(tr("Saved %1").arg(QLocale().toString(QDateTime::fromMSecsSinceEpoch(c.updated_at), QLocale::ShortFormat)));
    update_progress();
}

void AltDueDiligencePanel::new_checklist(const QString& template_id) {
    const alt_dd::Template* t = alt_dd::find_template(template_id);
    if (!t)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New checklist"), tr("Fund or deal name:"), QLineEdit::Normal,
                                               alt_dd::tr_dd(t->name), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    DdChecklist c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.name = name;
    c.template_id = template_id;
    c.created_at = QDateTime::currentMSecsSinceEpoch();
    c.updated_at = c.created_at;
    lists_.prepend(c);
    list_combo_->insertItem(0, name + QStringLiteral("  ·  ") + alt_dd::tr_dd(t->name));
    show_checklist(0);
    AltDdStore::save(c, this, [this](bool ok2, const QString&) {
        save_note_->setText(ok2 ? tr("Saved") : tr("Could not save - your edits stay on screen"));
    });
}

void AltDueDiligencePanel::rename_checklist() {
    if (current_ < 0)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename checklist"), tr("Fund or deal name:"), QLineEdit::Normal,
                                               lists_.at(current_).name, &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    lists_[current_].name = name;
    const alt_dd::Template* t = alt_dd::find_template(lists_.at(current_).template_id);
    list_combo_->setItemText(current_, name + QStringLiteral("  ·  ") + (t ? alt_dd::tr_dd(t->name) : QString()));
    save_current();
}

void AltDueDiligencePanel::delete_checklist() {
    if (current_ < 0)
        return;
    const DdChecklist c = lists_.at(current_);
    if (QMessageBox::question(this, tr("Delete checklist"), tr("Delete the checklist \"%1\" and its notes?").arg(c.name)) !=
        QMessageBox::Yes)
        return;
    save_timer_->stop();
    lists_.removeAt(current_);
    list_combo_->removeItem(current_);
    AltDdStore::remove(c.id, this);
    show_checklist(lists_.isEmpty() ? -1 : 0);
}

QJsonArray AltDueDiligencePanel::items_from_model() const {
    QJsonArray out;
    for (int s = 0; s < model_->rowCount(); ++s) {
        QStandardItem* sec = model_->item(s, 0);
        for (int r = 0; r < sec->rowCount(); ++r) {
            const QString id = sec->child(r, 0)->data(kAltDdIdRole).toString();
            const QString status = sec->child(r, 1)->data(kAltDdStatusRole).toString();
            const QString note = sec->child(r, 2)->text().trimmed();
            if (status == QLatin1String("open") && note.isEmpty())
                continue; // untouched items are not stored
            out.append(QJsonObject{{QStringLiteral("id"), id},
                                   {QStringLiteral("status"), status},
                                   {QStringLiteral("note"), note}});
        }
    }
    return out;
}

void AltDueDiligencePanel::on_item_edited() {
    if (filling_ || current_ < 0)
        return;
    update_progress();
    save_note_->setText(tr("Saving…"));
    save_timer_->start();
}

void AltDueDiligencePanel::save_current() {
    if (current_ < 0)
        return;
    lists_[current_].items = items_from_model();
    lists_[current_].updated_at = QDateTime::currentMSecsSinceEpoch();
    QPointer<QLabel> note = save_note_;
    AltDdStore::save(lists_.at(current_), this, [note](bool ok, const QString&) {
        if (note)
            note->setText(ok ? AltDueDiligencePanel::tr("Saved") : AltDueDiligencePanel::tr("Could not save - try again"));
    });
}

void AltDueDiligencePanel::update_progress() {
    int total = 0, done = 0, issues = 0, na = 0;
    for (int s = 0; s < model_->rowCount(); ++s) {
        QStandardItem* sec = model_->item(s, 0);
        int sec_done = 0;
        for (int r = 0; r < sec->rowCount(); ++r) {
            const QString st = sec->child(r, 1)->data(kAltDdStatusRole).toString();
            ++total;
            if (st == QLatin1String("done")) {
                ++done;
                ++sec_done;
            } else if (st == QLatin1String("issue")) {
                ++issues;
            } else if (st == QLatin1String("na")) {
                ++na;
                ++sec_done;
            }
        }
        filling_ = true;
        model_->item(s, 1)->setText(tr("%1 / %2").arg(sec_done).arg(sec->rowCount()));
        filling_ = false;
    }
    const int applicable = total - na;
    progress_->setText(tr("%1 of %2 applicable checks done (%3%)  ·  %4 issue(s)  ·  %5 not applicable")
                           .arg(done)
                           .arg(applicable)
                           .arg(applicable > 0 ? QString::number(100.0 * done / applicable, 'f', 0) : QStringLiteral("0"))
                           .arg(issues)
                           .arg(na));
}

QVector<QStringList> AltDueDiligencePanel::dd_rows() const {
    QVector<QStringList> rows;
    for (int s = 0; s < model_->rowCount(); ++s) {
        QStandardItem* sec = model_->item(s, 0);
        for (int r = 0; r < sec->rowCount(); ++r)
            rows << QStringList{sec->text(), sec->child(r, 0)->text(), sec->child(r, 1)->text(), sec->child(r, 2)->text()};
    }
    return rows;
}

QString AltDueDiligencePanel::dd_markdown() const {
    if (current_ < 0)
        return QString();
    QStringList lines;
    lines << QStringLiteral("# ") + tr("Due diligence: %1").arg(lists_.at(current_).name) << progress_->text() << QString();
    for (int s = 0; s < model_->rowCount(); ++s) {
        QStandardItem* sec = model_->item(s, 0);
        lines << QStringLiteral("## ") + sec->text();
        for (int r = 0; r < sec->rowCount(); ++r) {
            const QString st = sec->child(r, 1)->data(kAltDdStatusRole).toString();
            const QString box = st == QLatin1String("done") ? QStringLiteral("[x]")
                                : st == QLatin1String("na") ? QStringLiteral("[-]")
                                                            : QStringLiteral("[ ]");
            QString line = QStringLiteral("- ") + box + QLatin1Char(' ') + sec->child(r, 0)->text();
            if (st == QLatin1String("issue"))
                line += QStringLiteral(" **(") + tr("issue") + QStringLiteral(")**");
            const QString note = sec->child(r, 2)->text().trimmed();
            if (!note.isEmpty())
                line += QStringLiteral(" - ") + note;
            lines << line;
        }
        lines << QString();
    }
    return lines.join(QLatin1Char('\n'));
}

fincept::report::ReportPayload AltDueDiligencePanel::dd_report() const {
    fincept::report::ReportPayload p;
    if (current_ < 0)
        return p;
    p.title = tr("Due diligence: %1").arg(lists_.at(current_).name);
    p.source = QStringLiteral("Alternative Investments");
    p.as_of = QDateTime::fromMSecsSinceEpoch(lists_.at(current_).updated_at);
    p.markdown = progress_->text();
    fincept::report::PayloadTable t;
    t.title = tr("Checklist");
    t.columns = {tr("Section"), tr("Check"), tr("Status"), tr("Note")};
    t.rows = dd_rows();
    p.tables << t;
    p.notes = tr("A working list drawn from public due-diligence questionnaires, not legal advice.");
    return p;
}

} // namespace fincept::screens::alt
