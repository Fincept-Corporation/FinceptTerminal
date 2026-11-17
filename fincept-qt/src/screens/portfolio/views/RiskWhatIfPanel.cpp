// src/screens/portfolio/views/RiskWhatIfPanel.cpp
#include "screens/portfolio/views/RiskWhatIfPanel.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "screens/portfolio/PortfolioLedgerDialogs.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/BarChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <optional>

namespace fincept::screens {

namespace {

QTableWidgetItem* wif_ro(const QString& text, const QColor& fg = {}) {
    auto* it = new QTableWidgetItem(text);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

QTableWidgetItem* wif_label(const QString& text) {
    auto* it = new QTableWidgetItem(text);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}

} // namespace

RiskWhatIfPanel::RiskWhatIfPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    // Currency / number format changed: re-format NOW / the last result (no run).
    ui::formatting::on_format_changed(this, [this]() {
        if (!summary_.holdings.isEmpty())
            render();
    });
}

RiskWhatIfPanel::~RiskWhatIfPanel() {
    // A script still running for this panel is stopped with it.
    req_.cancel();
}

void RiskWhatIfPanel::build_ui() {
    // One selector stylesheet before any child exists; the theme manager's re-style
    // pass keeps it in step with theme / font changes.
    setStyleSheet(QString("#wifNote { color:%1; font-size:%2px; }"
                          // Scoped to the editor: the empty-book page and the notice style their own buttons.
                          "#wifLeft QPushButton { background:transparent; color:%3; border:1px solid %4; padding:3px 8px;"
                          "  font-size:%2px; font-weight:700; }"
                          "#wifLeft QPushButton#wifRun { color:%5; border-color:%5; }"
                          "#wifLeft QPushButton:disabled { color:%10; border-color:%4; }"
                          "QLineEdit { background:%6; color:%7; border:1px solid %4; padding:2px 6px; }"
                          "QTableWidget { background:%8; color:%7; border:none; font-size:%9px; }")
                      .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3), ui::colors::TEXT_PRIMARY(),
                           ui::colors::BORDER_DIM(), ui::colors::AMBER(), ui::colors::BG_RAISED(),
                           ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE(), ui::fonts::px(-2))
                      .arg(ui::colors::TEXT_TERTIARY()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    pages_ = new QStackedWidget(this);
    outer->addWidget(pages_);

    auto* page = new QWidget(pages_);
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(6);
    auto* intro = new QLabel(tr("Change target weights (or add a symbol), then RUN: the risk of the current book and of "
                                "the target book on the same trailing daily returns. Nothing is traded or recorded."),
                             page);
    intro->setWordWrap(true);
    intro->setObjectName("wifNote");
    root->addWidget(intro);

    auto* split = new QSplitter(Qt::Horizontal, page);
    auto* left = new QWidget(split);
    left->setObjectName("wifLeft");
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    weights_ = new QTableWidget(0, 4, left);
    weights_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("CURRENT"), tr("TARGET %"), tr("CHANGE")});
    weights_->verticalHeader()->setVisible(false);
    weights_->horizontalHeader()->setStretchLastSection(true);
    weights_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                              QAbstractItemView::AnyKeyPressed);
    ui::metrics::follow_row_height(weights_, 24);
    connect(weights_, &QTableWidget::cellChanged, this, [this](int, int c) {
        if (c == 2)
            update_total();
    });
    lv->addWidget(weights_, 1);
    auto* row = new QHBoxLayout;
    add_edit_ = new QLineEdit(left);
    add_edit_->setPlaceholderText(tr("Add symbol (Yahoo), e.g. AGG"));
    add_btn_ = new QPushButton(tr("ADD"), left);
    reset_btn_ = new QPushButton(tr("RESET"), left);
    norm_btn_ = new QPushButton(tr("NORMALISE TO 100 %"), left);
    row->addWidget(add_edit_, 1);
    row->addWidget(add_btn_);
    row->addWidget(reset_btn_);
    row->addWidget(norm_btn_);
    lv->addLayout(row);
    auto* row2 = new QHBoxLayout;
    total_label_ = new QLabel(left);
    total_label_->setObjectName("wifNote");
    total_label_->setWordWrap(true);
    row2->addWidget(total_label_, 1);
    run_btn_ = new QPushButton(tr("RUN WHAT-IF"), left);
    run_btn_->setObjectName("wifRun");
    run_btn_->setCursor(Qt::PointingHandCursor);
    row2->addWidget(run_btn_);
    lv->addLayout(row2);
    split->addWidget(left);

    right_ = new QWidget(split);
    auto* rv = new QVBoxLayout(right_);
    rv->setContentsMargins(0, 0, 0, 0);
    notice_ = new pfmt::PortfolioNoticeBar(right_);
    connect(notice_->retry_button(), &QPushButton::clicked, this, &RiskWhatIfPanel::run);
    rv->addWidget(notice_);
    status_ = new QLabel(right_);
    status_->setObjectName("wifNote");
    status_->setWordWrap(true);
    rv->addWidget(status_);
    results_ = new QTableWidget(0, 4, right_);
    results_->setHorizontalHeaderLabels({tr("MEASURE"), tr("NOW"), tr("TARGET"), tr("CHANGE")});
    results_->verticalHeader()->setVisible(false);
    results_->horizontalHeader()->setStretchLastSection(true);
    results_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui::metrics::follow_row_height(results_, 24);
    rv->addWidget(results_);
    contrib_chart_ = new ui::BarChart(right_);
    contrib_chart_->set_orientation(Qt::Horizontal);
    contrib_chart_->set_value_formatter([](double v) { return pfmt::upct(v, 1); });
    contrib_chart_->set_show_legend(true);
    rv->addWidget(contrib_chart_, 1);
    // Dims the result pane while a RUN is in flight (the NOW figures stay readable under it).
    overlay_ = new ui::LoadingOverlay(right_);
    split->addWidget(right_);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);
    pages_->addWidget(page);

    empty_ = ui::EmptyState::create(
        ui::EmptyState::no_data_spec(tr("Add a position first"),
                                     tr("What-if compares the risk of your book today with the risk of a target mix. "
                                        "Buy a holding to start."),
                                     tr("BUY")),
        pages_);
    connect(empty_, &ui::EmptyState::primary_clicked, this, &RiskWhatIfPanel::add_position_requested);
    pages_->addWidget(empty_);

    connect(add_btn_, &QPushButton::clicked, this, &RiskWhatIfPanel::add_symbol);
    connect(add_edit_, &QLineEdit::returnPressed, this, &RiskWhatIfPanel::add_symbol);
    connect(reset_btn_, &QPushButton::clicked, this, &RiskWhatIfPanel::reset_weights);
    connect(norm_btn_, &QPushButton::clicked, this, &RiskWhatIfPanel::normalise);
    connect(run_btn_, &QPushButton::clicked, this, &RiskWhatIfPanel::run);

    // Item colours (target cells, deltas, bars) are data: re-render them once an appearance
    // change has settled (never inside theme_changed). No stylesheet work here — the theme
    // manager re-styles the widgets itself.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        recolor_targets();
        if (summary_.holdings.isEmpty())
            return;
        if (!result_.isEmpty())
            render_result();
        else
            render_now();
    });
}

void RiskWhatIfPanel::set_data(const portfolio::PortfolioSummary& summary) {
    const bool new_book = loaded_for_ != summary.portfolio.id;
    summary_ = summary;
    if (new_book) {
        // A run for the previous book must not land on this one.
        cancel_run();
        result_ = {};
        result_targets_.clear();
        run_error_.clear();
        run_stopped_ = false;
        if (analytics_.portfolio_id != summary.portfolio.id)
            analytics_ = {};
        // Keep the user's edits across price refreshes of the same book.
        reset_weights();
    } else if (weights_->rowCount() == 0 && !summary.holdings.isEmpty()) {
        reset_weights(); // the first position of an empty book: start from its weights
    }
    render();
}

void RiskWhatIfPanel::set_analytics(const portfolio::MarketAnalytics& analytics) {
    // An empty `analytics` only re-evaluates the NOW column (the run ended without a result).
    if (!analytics.portfolio_id.isEmpty()) {
        if (!summary_.portfolio.id.isEmpty() && analytics.portfolio_id != summary_.portfolio.id)
            return;
        analytics_ = analytics;
    }
    if (result_.isEmpty() && !summary_.holdings.isEmpty())
        render_now();
}

void RiskWhatIfPanel::hideEvent(QHideEvent* e) {
    // A what-if is always started by the user (RUN): leaving the tab does not stop
    // it (shared rule R5) — its result is on screen when the tab is shown again. A
    // newer RUN, a portfolio switch or the panel's destruction still cancel it.
    QWidget::hideEvent(e);
}

void RiskWhatIfPanel::cancel_run() {
    if (run_seq_ == 0)
        return;
    req_.cancel();
    req_ = {};
    run_seq_ = 0;
    overlay_->hide_loading();
    run_btn_->setEnabled(true);
}

void RiskWhatIfPanel::recolor_targets() {
    QSignalBlocker block(weights_);
    for (int r = 0; r < weights_->rowCount(); ++r) {
        auto* target = weights_->item(r, 2);
        auto* change = weights_->item(r, 3);
        auto* current = weights_->item(r, 1);
        if (target)
            target->setForeground(QColor(ui::colors::AMBER()));
        if (change && target && current)
            change->setForeground(pfmt::dir_color(std::max(0.0, ledger_parse_number(target->text())) -
                                                  current->data(Qt::UserRole).toDouble()));
    }
}

void RiskWhatIfPanel::reset_weights() {
    loaded_for_ = summary_.portfolio.id;
    QSignalBlocker block(weights_);
    weights_->setRowCount(0);
    for (const auto& h : summary_.holdings) {
        if (!h.valued() || !std::isfinite(h.weight))
            continue;
        const int r = weights_->rowCount();
        weights_->insertRow(r);
        auto* sym = new QTableWidgetItem(h.symbol);
        sym->setFlags(sym->flags() & ~Qt::ItemIsEditable);
        weights_->setItem(r, 0, sym);
        weights_->setItem(r, 1, wif_ro(pfmt::upct(h.weight, 2)));
        weights_->item(r, 1)->setData(Qt::UserRole, h.weight);
        auto* target = new QTableWidgetItem(QString::number(h.weight, 'f', 2));
        target->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        target->setForeground(QColor(ui::colors::AMBER()));
        weights_->setItem(r, 2, target);
        weights_->setItem(r, 3, wif_ro(QString()));
    }
    weights_->resizeColumnsToContents();
    update_total();
}

void RiskWhatIfPanel::add_symbol() {
    const QString sym = add_edit_->text().trimmed().toUpper();
    if (sym.isEmpty())
        return;
    for (int r = 0; r < weights_->rowCount(); ++r)
        if (weights_->item(r, 0) && weights_->item(r, 0)->text() == sym)
            return;
    QSignalBlocker block(weights_);
    const int r = weights_->rowCount();
    weights_->insertRow(r);
    auto* s = new QTableWidgetItem(sym);
    s->setFlags(s->flags() & ~Qt::ItemIsEditable);
    weights_->setItem(r, 0, s);
    weights_->setItem(r, 1, wif_ro(pfmt::upct(0, 2)));
    weights_->item(r, 1)->setData(Qt::UserRole, 0.0);
    auto* t = new QTableWidgetItem(QStringLiteral("5"));
    t->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    t->setForeground(QColor(ui::colors::AMBER()));
    weights_->setItem(r, 2, t);
    weights_->setItem(r, 3, wif_ro(QString()));
    add_edit_->clear();
    update_total();
}

void RiskWhatIfPanel::normalise() {
    double total = 0;
    for (int r = 0; r < weights_->rowCount(); ++r)
        total += std::max(0.0, ledger_parse_number(weights_->item(r, 2)->text()));
    if (!(total > 0))
        return;
    QSignalBlocker block(weights_);
    for (int r = 0; r < weights_->rowCount(); ++r) {
        const double w = std::max(0.0, ledger_parse_number(weights_->item(r, 2)->text()));
        weights_->item(r, 2)->setText(QString::number(w / total * 100.0, 'f', 2));
    }
    update_total();
}

void RiskWhatIfPanel::update_total() {
    double total = 0;
    QSignalBlocker block(weights_);
    for (int r = 0; r < weights_->rowCount(); ++r) {
        const double target = std::max(0.0, ledger_parse_number(weights_->item(r, 2)->text()));
        const double cur = weights_->item(r, 1)->data(Qt::UserRole).toDouble();
        total += target;
        const double d = target - cur;
        weights_->item(r, 3)->setText(std::abs(d) < 0.005 ? QString() : pfmt::pp(d, 2));
        weights_->item(r, 3)->setForeground(pfmt::dir_color(d));
    }
    total_label_->setText(std::abs(total - 100.0) < 0.05
                              ? tr("Target weights sum to 100 %.")
                              : tr("Target weights sum to %1 — they are normalised to 100 % for the run.")
                                    .arg(pfmt::upct(total, 2)));
    // A result computed for other targets says so (only the targets are compared,
    // not prices).
    if (!result_.isEmpty() && run_seq_ == 0 && run_error_.isEmpty() && !run_stopped_) {
        if (targets_signature() != result_targets_)
            notice_->set_notice(pfmt::PortfolioNoticeBar::Kind::Info,
                                tr("Target weights changed since this result — RUN to update it."));
        else
            notice_->clear();
    }
}

QString RiskWhatIfPanel::targets_signature() const {
    QStringList parts;
    for (int r = 0; r < weights_->rowCount(); ++r) {
        const auto* s = weights_->item(r, 0);
        const auto* t = weights_->item(r, 2);
        if (!s || !t)
            continue;
        const double w = std::max(0.0, ledger_parse_number(t->text()));
        if (w > 0)
            parts << s->text() + QLatin1Char('=') + QString::number(w, 'f', 4);
    }
    parts.sort();
    return parts.join(QLatin1Char(','));
}

void RiskWhatIfPanel::run() {
    if (summary_.holdings.isEmpty())
        return;
    QHash<QString, double> target;
    for (int r = 0; r < weights_->rowCount(); ++r) {
        const double w = std::max(0.0, ledger_parse_number(weights_->item(r, 2)->text()));
        if (w > 0)
            target.insert(weights_->item(r, 0)->text(), w);
    }
    if (target.isEmpty()) {
        notice_->set_notice(pfmt::PortfolioNoticeBar::Kind::Warning,
                            tr("Give at least one symbol a target weight above 0."));
        return;
    }
    cancel_run(); // a newer RUN supersedes the one in flight
    run_error_.clear();
    run_stopped_ = false;
    notice_->clear();
    run_btn_->setEnabled(false);
    overlay_->show_loading(tr("Downloading a year of daily prices and computing the risk of both books…"));
    const QString targets = targets_signature();
    const QString pid = summary_.portfolio.id;
    const quint64 seq = ++next_seq_;
    run_seq_ = seq;
    QPointer<RiskWhatIfPanel> self(this);
    const services::AnalyticsRequest req = services::PortfolioService::instance().what_if(
        summary_, target,
        [self, targets, pid, seq](const services::AnalyticsResult& res) {
            // Superseded, cancelled or the tab was left: a late reply never draws. (The
            // reply may also come synchronously — Python missing — hence the own id.)
            if (!self || res.cancelled || seq != self->run_seq_)
                return;
            self->run_seq_ = 0;
            self->req_ = {};
            self->overlay_->hide_loading();
            self->run_btn_->setEnabled(true);
            if (self->summary_.portfolio.id != pid)
                return;
            if (!res.success) {
                self->run_error_ = res.error.isEmpty() ? tr("The what-if run failed.") : res.error;
                self->render();
                return;
            }
            if (res.data.value("after").toObject().isEmpty()) {
                self->run_error_ = tr("The target book has too little shared price history to measure.");
                self->render();
                return;
            }
            self->result_ = res.data;
            self->result_targets_ = targets;
            self->render();
        },
        opts_);
    if (run_seq_ == seq)
        req_ = req; // still in flight: keep its cancel token
}

void RiskWhatIfPanel::render() {
    const bool empty_book = summary_.holdings.isEmpty();
    pages_->setCurrentIndex(empty_book ? 1 : 0);
    if (empty_book)
        return;
    if (!result_.isEmpty())
        render_result();
    else
        render_now();

    // One notice: a failed / stopped RUN offers RETRY; a result for other targets says so.
    if (run_seq_ != 0)
        notice_->clear();
    else if (!run_error_.isEmpty())
        notice_->set_notice(pfmt::PortfolioNoticeBar::Kind::Error, tr("What-if failed: %1").arg(run_error_), true);
    else if (run_stopped_)
        notice_->set_notice(pfmt::PortfolioNoticeBar::Kind::Info,
                            tr("The last RUN was stopped before it finished."), true);
    else if (!result_.isEmpty())
        update_total(); // sets / clears the "targets changed" notice
    else
        notice_->clear();
}

void RiskWhatIfPanel::render_now() {
    // NOW from the screen's market-analytics run (same trailing daily returns, base
    // currency); TARGET / CHANGE wait for a RUN.
    const bool have = !analytics_.portfolio_id.isEmpty() && analytics_.portfolio_id == summary_.portfolio.id;
    const bool pending =
        !have && services::PortfolioService::instance().market_analytics_pending(summary_.portfolio.id);
    const QString base = pfmt::base_ccy(summary_);
    const double nav = pfmt::holdings_value(summary_);

    const portfolio::VarEstimate* var = nullptr;
    for (const auto& e : analytics_.var) {
        if (std::abs(e.confidence - analytics_.var_confidence) < 1e-9) {
            var = &e;
            break;
        }
    }
    if (!var && !analytics_.var.isEmpty())
        var = &analytics_.var.first();
    const double conf = var ? var->confidence : analytics_.var_confidence;
    const int horizon = var ? var->horizon_days : analytics_.var_horizon_days;

    struct Row {
        QString label;
        std::optional<double> value;
        int kind; // 0 = fraction %, 1 = ratio, 2 = loss fraction (money too)
    };
    const Row rows[] = {
        {tr("Volatility (annualised)"), analytics_.ann_vol, 0},
        {tr("Beta"), analytics_.beta, 1},
        {tr("Tracking error"), analytics_.tracking_error, 0},
        {tr("VaR %1 %2-day (historical)").arg(pfmt::upct(conf * 100.0, 1)).arg(horizon),
         var ? var->historical : std::optional<double>{}, 2},
        {tr("CVaR %1 %2-day").arg(pfmt::upct(conf * 100.0, 1)).arg(horizon), var ? var->cvar : std::optional<double>{},
         2},
        {tr("Diversification ratio"), analytics_.diversification_ratio, 1},
    };
    const QString hint = tr("Edit a target weight, then RUN");
    const QColor muted(ui::colors::TEXT_TERTIARY());
    results_->setUpdatesEnabled(false);
    results_->setRowCount(static_cast<int>(std::size(rows)));
    int r = 0;
    for (const auto& row : rows) {
        QString now;
        QString tip;
        if (have && row.value && std::isfinite(*row.value)) {
            const double x = *row.value;
            now = row.kind == 1   ? pfmt::num(x, 2)
                  : row.kind == 2 ? QStringLiteral("%1 (%2)").arg(
                                        pfmt::fpct(x, 2, false),
                                        ui::formatting::money(x * nav, base, ui::formatting::Compact::FromSettings))
                                  : pfmt::fpct(x, 2, false);
        } else if (pending) {
            now = QStringLiteral("…");
            tip = tr("Downloading daily prices for the risk figures…");
        } else {
            now = pfmt::dash();
            tip = have ? tr("Not enough shared history for this measure.")
                       : tr("The risk figures are not available yet — see the notice above (RETRY / RECALCULATE).");
        }
        results_->setItem(r, 0, wif_label(row.label));
        auto* now_item = wif_ro(now, pending ? muted : QColor());
        now_item->setToolTip(tip);
        results_->setItem(r, 1, now_item);
        auto* tgt = wif_ro(pfmt::dash(), muted);
        tgt->setToolTip(hint);
        results_->setItem(r, 2, tgt);
        auto* chg = wif_ro(pfmt::dash(), muted);
        chg->setToolTip(hint);
        results_->setItem(r, 3, chg);
        ++r;
    }
    results_->setUpdatesEnabled(true);
    results_->resizeColumnsToContents();

    // Risk share now, per valued holding (largest first).
    QVector<QPair<QString, double>> shares;
    for (const auto& h : summary_.holdings) {
        if (!h.valued())
            continue;
        const auto rc = analytics_.holdings.value(h.symbol).risk_contribution;
        shares.append({h.symbol, have && rc ? *rc * 100.0 : portfolio::kAbsent});
    }
    std::sort(shares.begin(), shares.end(), [](const auto& a, const auto& b) {
        const double x = std::isfinite(a.second) ? a.second : -1e300;
        const double y = std::isfinite(b.second) ? b.second : -1e300;
        return x > y;
    });
    QStringList cats;
    QVector<double> vals;
    for (const auto& s : shares) {
        cats << s.first;
        vals << s.second;
    }
    contrib_chart_->set_categories(cats);
    contrib_chart_->set_series({ui::BarSeries{tr("Risk share now"), vals, ui::colors::chart_color(0)}});
    contrib_chart_->set_state(have      ? (cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready)
                              : pending ? ui::PaintedChart::Loading
                                        : ui::PaintedChart::Empty,
                              have      ? QString()
                              : pending ? tr("Downloading daily prices…")
                                        : tr("Risk shares appear when the price history has loaded."));

    status_->setText(have ? tr("NOW = the book today (%1 observations to %2). Edit a target weight, then RUN to see the "
                               "target book next to it.")
                                .arg(analytics_.observations)
                                .arg(pfmt::date(analytics_.as_of, ui::formatting::DateStyle::Short))
                          : tr("Edit a target weight, then RUN to see the target book's risk next to today's."));
}

void RiskWhatIfPanel::render_result() {
    const QJsonObject before = result_.value("before").toObject();
    const QJsonObject after = result_.value("after").toObject();
    const QString base = pfmt::base_ccy(summary_);
    const double nav = pfmt::holdings_value(summary_);
    struct Row {
        QString label;
        const char* key;
        int kind; // 0 = fraction %, 1 = ratio, 2 = loss fraction (money too)
    };
    const double conf = after.value("var_confidence").toDouble(0.95);
    const int horizon = after.value("var_horizon_days").toInt(1);
    const Row rows[] = {
        {tr("Volatility (annualised)"), "ann_vol", 0},
        {tr("Beta"), "beta", 1},
        {tr("Tracking error"), "tracking_error", 0},
        {tr("VaR %1 %2-day (historical)").arg(pfmt::upct(conf * 100.0, 1)).arg(horizon), "var", 2},
        {tr("CVaR %1 %2-day").arg(pfmt::upct(conf * 100.0, 1)).arg(horizon), "cvar", 2},
        {tr("Diversification ratio"), "diversification_ratio", 1},
    };
    results_->setUpdatesEnabled(false);
    results_->setRowCount(static_cast<int>(std::size(rows)));
    int r = 0;
    for (const auto& row : rows) {
        const QJsonValue b = before.value(row.key);
        const QJsonValue a = after.value(row.key);
        auto fmt = [&](const QJsonValue& v) {
            if (!v.isDouble())
                return pfmt::dash();
            const double x = v.toDouble();
            if (row.kind == 1)
                return pfmt::num(x, 2);
            if (row.kind == 2)
                return QStringLiteral("%1 (%2)").arg(pfmt::fpct(x, 2, false), pfmt::money_c(x * nav, base));
            return pfmt::fpct(x, 2, false);
        };
        results_->setItem(r, 0, wif_label(row.label));
        results_->setItem(r, 1, wif_ro(fmt(b)));
        results_->setItem(r, 2, wif_ro(fmt(a)));
        QString delta = pfmt::dash();
        QColor dc;
        if (a.isDouble() && b.isDouble()) {
            const double d = a.toDouble() - b.toDouble();
            delta = row.kind == 1 ? ui::formatting::signed_number(d, 2) : pfmt::pp(d * 100.0, 2);
            // Lower risk reads as an improvement except for the diversification ratio.
            const bool better = QString::fromLatin1(row.key) == QLatin1String("diversification_ratio") ? d > 0 : d < 0;
            dc = std::abs(d) < 1e-9 ? QColor(ui::colors::TEXT_SECONDARY())
                                    : QColor(better ? ui::colors::SUCCESS() : ui::colors::DANGER());
        }
        auto* chg = wif_ro(delta, dc);
        if (dc.isValid() && a.isDouble() && b.isDouble() && std::abs(a.toDouble() - b.toDouble()) >= 1e-9) {
            const bool div = QString::fromLatin1(row.key) == QLatin1String("diversification_ratio");
            const bool better = div ? a.toDouble() > b.toDouble() : a.toDouble() < b.toDouble();
            chg->setToolTip(div ? (better ? tr("More diversified") : tr("Less diversified"))
                                : (better ? tr("Lower risk") : tr("Higher risk")));
        }
        results_->setItem(r, 3, chg);
        ++r;
    }
    results_->setUpdatesEnabled(true);
    results_->resizeColumnsToContents();

    // Risk contribution before / after.
    const QJsonObject rb = before.value("risk_contribution").toObject();
    const QJsonObject ra = after.value("risk_contribution").toObject();
    QStringList syms = ra.keys();
    for (const auto& k : rb.keys())
        if (!syms.contains(k))
            syms << k;
    std::sort(syms.begin(), syms.end(), [&ra](const QString& x, const QString& y) {
        return ra.value(x).toDouble() > ra.value(y).toDouble();
    });
    QVector<double> vb, va;
    for (const auto& s : syms) {
        vb << (rb.value(s).isDouble() ? rb.value(s).toDouble() * 100.0 : portfolio::kAbsent);
        va << (ra.value(s).isDouble() ? ra.value(s).toDouble() * 100.0 : portfolio::kAbsent);
    }
    contrib_chart_->set_categories(syms);
    contrib_chart_->set_series({ui::BarSeries{tr("Risk share now"), vb, ui::colors::chart_color(0)},
                                ui::BarSeries{tr("Risk share target"), va, ui::colors::chart_color(1)}});
    contrib_chart_->set_state(syms.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready);
    QStringList warn;
    for (const auto& w : result_.value("warnings").toArray())
        warn << w.toString();
    status_->setText(tr("%1 observations · coverage %2 of the target weight.%3")
                         .arg(after.value("observations").toInt())
                         .arg(pfmt::fpct(after.value("coverage").toDouble(1.0), 0, false))
                         .arg(warn.isEmpty() ? QString() : QLatin1Char(' ') + warn.join(QLatin1Char(' '))));
}

} // namespace fincept::screens
