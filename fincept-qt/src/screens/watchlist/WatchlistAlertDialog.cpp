#include "screens/watchlist/WatchlistAlertDialog.h"

#include "services/markets/WatchlistAlertService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
using services::WatchlistAlertService;

namespace {
constexpr int kWlaIdRole = Qt::UserRole + 1;

/// Sets a stylesheet only when its text changed (each set re-polishes the subtree).
void wlad_set_css(QWidget* w, const QString& css) {
    if (w->styleSheet() != css)
        w->setStyleSheet(css);
}
} // namespace

WatchlistAlertDialog::WatchlistAlertDialog(const QString& symbol, const QString& watchlist_id,
                                           const services::QuoteData* quote, QWidget* parent)
    : QDialog(parent), symbol_(symbol), watchlist_id_(watchlist_id) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Alerts — %1").arg(symbol));
    if (quote && quote->price > 0) {
        last_price_ = quote->price;
        fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol, quote->currency);
        if (quote->price_digits >= 0)
            inst.decimals = quote->price_digits;
        price_digits_ = fmt::price_decimals(quote->price, inst);
    }
    build_ui();
    reload();
    refresh_theme();
    connect(&WatchlistAlertService::instance(), &WatchlistAlertService::alerts_changed, this,
            &WatchlistAlertDialog::reload);
    // Restyled once an appearance change has settled (never inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
    resize(560, 420);
}

void WatchlistAlertDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    title_ = new QLabel(this);
    title_->setText(last_price_ > 0 ? tr("%1 — last %2").arg(symbol_, fmt::number(last_price_, price_digits_))
                                    : symbol_);
    root->addWidget(title_);

    status_ = new QLabel(this);
    status_->setWordWrap(true);
    root->addWidget(status_);

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({tr("ON"), tr("CONDITION"), tr("REPEAT"), tr("LAST FIRED")});
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const qint64 id = selected_id();
        for (const auto& a : WatchlistAlertService::instance().alerts_for(symbol_))
            if (a.id == id)
                start_edit(a);
    });
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!item || item->column() != 0)
            return;
        const qint64 id = item->data(kWlaIdRole).toLongLong();
        const bool on = item->checkState() == Qt::Checked;
        for (const auto& a : WatchlistAlertService::instance().alerts_for(symbol_)) {
            if (a.id == id && a.enabled != on) {
                auto r = WatchlistAlertService::instance().set_enabled(id, on);
                if (r.is_err())
                    status_->setText(tr("Could not update the alert: %1").arg(QString::fromStdString(r.error())));
            }
        }
    });
    root->addWidget(table_, 1);

    auto* buttons = new QHBoxLayout;
    add_btn_ = new QPushButton(tr("New alert"), this);
    edit_btn_ = new QPushButton(tr("Edit"), this);
    delete_btn_ = new QPushButton(tr("Delete"), this);
    delete_btn_->setProperty("fincept_agent_risk", "delete");
    buttons->addWidget(add_btn_);
    buttons->addWidget(edit_btn_);
    buttons->addWidget(delete_btn_);
    buttons->addStretch();
    root->addLayout(buttons);
    connect(add_btn_, &QPushButton::clicked, this, [this]() {
        WatchlistAlert a;
        a.symbol = symbol_;
        a.watchlist_id = watchlist_id_;
        a.kind = QStringLiteral("price_above");
        a.threshold = last_price_ > 0 ? last_price_ * 1.05 : 0;
        a.ma_period = 50;
        start_edit(a);
    });
    connect(edit_btn_, &QPushButton::clicked, this, [this]() {
        const qint64 id = selected_id();
        for (const auto& a : WatchlistAlertService::instance().alerts_for(symbol_))
            if (a.id == id)
                start_edit(a);
    });
    connect(delete_btn_, &QPushButton::clicked, this, [this]() {
        const qint64 id = selected_id();
        if (id <= 0)
            return;
        auto r = WatchlistAlertService::instance().remove(id);
        if (r.is_err())
            status_->setText(tr("Could not delete the alert: %1").arg(QString::fromStdString(r.error())));
    });

    editor_ = new QWidget(this);
    auto* form = new QFormLayout(editor_);
    form->setContentsMargins(0, 6, 0, 0);
    kind_ = new QComboBox(editor_);
    for (const QString& k : WatchlistAlertService::kinds())
        kind_->addItem(WatchlistAlertService::kind_label(k), k);
    form->addRow(tr("When"), kind_);
    threshold_label_ = new QLabel(editor_);
    threshold_ = new QDoubleSpinBox(editor_);
    threshold_->setRange(0, 1e12);
    threshold_->setGroupSeparatorShown(true);
    form->addRow(threshold_label_, threshold_);
    ma_label_ = new QLabel(tr("Average"), editor_);
    ma_period_ = new QComboBox(editor_);
    ma_period_->addItem(tr("50-day"), 50);
    ma_period_->addItem(tr("200-day"), 200);
    form->addRow(ma_label_, ma_period_);
    repeat_ = new QCheckBox(tr("Repeat (re-arm after the condition clears)"), editor_);
    form->addRow(QString(), repeat_);
    enabled_ = new QCheckBox(tr("Enabled"), editor_);
    form->addRow(QString(), enabled_);
    note_ = new QLineEdit(editor_);
    note_->setPlaceholderText(tr("Optional note shown with the notification"));
    form->addRow(tr("Note"), note_);
    auto* eb = new QHBoxLayout;
    save_btn_ = new QPushButton(tr("Save alert"), editor_);
    cancel_btn_ = new QPushButton(tr("Cancel"), editor_);
    eb->addStretch();
    eb->addWidget(cancel_btn_);
    eb->addWidget(save_btn_);
    form->addRow(eb);
    root->addWidget(editor_);
    editor_->setVisible(false);
    connect(kind_, &QComboBox::currentIndexChanged, this, &WatchlistAlertDialog::on_kind_changed);
    connect(save_btn_, &QPushButton::clicked, this, &WatchlistAlertDialog::save_editor);
    connect(cancel_btn_, &QPushButton::clicked, this, [this]() { editor_->setVisible(false); });
}

void WatchlistAlertDialog::refresh_theme() {
    wlad_set_css(this, QStringLiteral("QDialog { background:%1; } QLabel { color:%2; } QCheckBox { color:%2; }")
                          .arg(colors::BG_BASE(), colors::TEXT_SECONDARY()));
    wlad_set_css(title_, QStringLiteral("color:%1; font-size:%2px; font-weight:700;")
                            .arg(colors::AMBER())
                            .arg(fonts::px(1)));
    wlad_set_css(status_,
                QStringLiteral("color:%1; font-size:%2px;").arg(colors::TEXT_TERTIARY()).arg(fonts::px(-2)));
}

void WatchlistAlertDialog::reload() {
    auto& svc = WatchlistAlertService::instance();
    if (!svc.available()) {
        status_->setText(tr("Alerts need the database upgrade that comes with this version (migration v054); it "
                            "runs at the next start of the application."));
        add_btn_->setEnabled(false);
        return;
    }
    const auto alerts = svc.alerts_for(symbol_);
    status_->setText(alerts.isEmpty() ? tr("No alerts yet. Alerts are checked on every quote, also while this "
                                           "screen is closed.")
                                      : QString());
    const qint64 keep = selected_id();
    QSignalBlocker block(table_);
    table_->setRowCount(0);
    for (const auto& a : alerts) {
        const int r = table_->rowCount();
        table_->insertRow(r);
        auto* on = new QTableWidgetItem;
        on->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        on->setCheckState(a.enabled ? Qt::Checked : Qt::Unchecked);
        on->setData(kWlaIdRole, a.id);
        table_->setItem(r, 0, on);
        auto* cond = new QTableWidgetItem(WatchlistAlertService::describe(a));
        cond->setData(kWlaIdRole, a.id);
        cond->setToolTip(a.note);
        table_->setItem(r, 1, cond);
        table_->setItem(r, 2, new QTableWidgetItem(a.repeat ? tr("Yes") : tr("Once")));
        QString fired = fmt::placeholder();
        if (!a.last_triggered_at.isEmpty()) {
            const QDateTime dt = QDateTime::fromString(a.last_triggered_at, Qt::ISODate);
            fired = dt.isValid() ? fmt::date_time(dt, fmt::DateStyle::Short) : a.last_triggered_at;
            if (a.trigger_count > 1)
                fired += QStringLiteral(" (%1×)").arg(a.trigger_count);
        }
        table_->setItem(r, 3, new QTableWidgetItem(fired));
        if (a.id == keep)
            table_->selectRow(r);
    }
    table_->resizeColumnToContents(0);
    table_->resizeColumnToContents(2);
    table_->resizeColumnToContents(3);
}

qint64 WatchlistAlertDialog::selected_id() const {
    const int r = table_->currentRow();
    if (r < 0 || !table_->item(r, 1))
        return 0;
    return table_->item(r, 1)->data(kWlaIdRole).toLongLong();
}

void WatchlistAlertDialog::start_edit(const WatchlistAlert& a) {
    editing_ = a;
    QSignalBlocker block(kind_);
    const int ki = kind_->findData(a.kind);
    kind_->setCurrentIndex(ki >= 0 ? ki : 0);
    on_kind_changed();
    threshold_->setValue(std::abs(a.threshold));
    const int mi = ma_period_->findData(a.ma_period == 200 ? 200 : 50);
    ma_period_->setCurrentIndex(mi >= 0 ? mi : 0);
    repeat_->setChecked(a.repeat);
    enabled_->setChecked(a.enabled);
    note_->setText(a.note);
    save_btn_->setText(a.id == 0 ? tr("Create alert") : tr("Save alert"));
    editor_->setVisible(true);
    threshold_->setFocus();
}

void WatchlistAlertDialog::on_kind_changed() {
    const QString kind = kind_->currentData().toString();
    const bool has_threshold = WatchlistAlertService::kind_has_threshold(kind);
    threshold_label_->setVisible(has_threshold);
    threshold_->setVisible(has_threshold);
    threshold_label_->setText(WatchlistAlertService::threshold_label(kind));
    const bool ma = WatchlistAlertService::kind_is_ma_cross(kind);
    ma_label_->setVisible(ma);
    ma_period_->setVisible(ma);
    if (kind == QLatin1String("price_above") || kind == QLatin1String("price_below")) {
        threshold_->setDecimals(std::clamp(price_digits_, 0, 8));
        threshold_->setSingleStep(last_price_ > 0 ? std::pow(10.0, -std::clamp(price_digits_, 0, 8)) * 10 : 0.1);
        threshold_->setSuffix(QString());
        if (editing_.kind != kind && last_price_ > 0)
            threshold_->setValue(kind == QLatin1String("price_above") ? last_price_ * 1.05 : last_price_ * 0.95);
    } else if (kind == QLatin1String("change_up") || kind == QLatin1String("change_down")) {
        threshold_->setDecimals(2);
        threshold_->setSingleStep(0.5);
        threshold_->setSuffix(QStringLiteral(" %"));
        if (editing_.kind != kind)
            threshold_->setValue(3.0);
    } else if (kind == QLatin1String("volume_spike")) {
        threshold_->setDecimals(1);
        threshold_->setSingleStep(0.5);
        threshold_->setSuffix(QStringLiteral(" ×"));
        if (editing_.kind != kind)
            threshold_->setValue(2.0);
    }
    editing_.kind = kind;
}

void WatchlistAlertDialog::save_editor() {
    WatchlistAlert a = editing_;
    a.symbol = symbol_;
    if (a.watchlist_id.isEmpty())
        a.watchlist_id = watchlist_id_;
    a.kind = kind_->currentData().toString();
    a.threshold = WatchlistAlertService::kind_has_threshold(a.kind) ? threshold_->value() : 0.0;
    a.ma_period = WatchlistAlertService::kind_is_ma_cross(a.kind) ? ma_period_->currentData().toInt() : 0;
    a.repeat = repeat_->isChecked();
    a.enabled = enabled_->isChecked();
    a.note = note_->text().trimmed();
    if (WatchlistAlertService::kind_has_threshold(a.kind) && !(a.threshold > 0)) {
        status_->setText(tr("Enter a threshold above zero."));
        return;
    }
    auto r = WatchlistAlertService::instance().save(a);
    if (r.is_err()) {
        status_->setText(tr("Could not save the alert: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    editor_->setVisible(false);
}

} // namespace fincept::screens
