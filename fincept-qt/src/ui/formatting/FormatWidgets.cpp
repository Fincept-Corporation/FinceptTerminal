// FormatWidgets.cpp — see FormatWidgets.h.

#include "ui/formatting/FormatWidgets.h"

#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/NumberFormat.h"

#include <QAbstractItemModel>
#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QPointer>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

#include <utility>

namespace fincept::ui::formatting {

namespace fmtwidgets_detail {

/// Re-renders its owner after format changes: deferred and coalesced while
/// the owner is visible, once on the next show otherwise. A child of the
/// owner, so it dies with it.
class FmtWidgetsChangeHook : public QObject {
  public:
    FmtWidgetsChangeHook(QWidget* owner, std::function<void()> fn) : QObject(owner), owner_(owner), fn_(std::move(fn)) {
        owner->installEventFilter(this);
        connect(&FormatSettings::instance(), &FormatSettings::changed, this, &FmtWidgetsChangeHook::on_changed);
    }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == owner_ && event->type() == QEvent::Show && dirty_ && !pending_) {
            dirty_ = false;
            if (fn_)
                fn_();
        }
        return false;
    }

  private:
    void on_changed() {
        if (!owner_->isVisible()) {
            dirty_ = true;
            return;
        }
        if (pending_)
            return;
        pending_ = true;
        QTimer::singleShot(0, this, [this]() {
            pending_ = false;
            if (!owner_->isVisible()) { // hidden meanwhile: wait for the next show
                dirty_ = true;
                return;
            }
            dirty_ = false;
            if (fn_)
                fn_();
        });
    }

    QWidget* owner_;
    std::function<void()> fn_;
    bool pending_ = false;
    bool dirty_ = false;
};

/// Emits a full-range dataChanged / headerDataChanged on its model after
/// format changes, once per event-loop turn.
class FmtWidgetsModelHook : public QObject {
  public:
    explicit FmtWidgetsModelHook(QAbstractItemModel* model) : QObject(model), model_(model) {
        connect(&FormatSettings::instance(), &FormatSettings::changed, this, &FmtWidgetsModelHook::on_changed);
    }

  private:
    void on_changed() {
        if (pending_)
            return;
        pending_ = true;
        QTimer::singleShot(0, this, [this]() {
            pending_ = false;
            const int rows = model_->rowCount();
            const int cols = model_->columnCount();
            if (cols <= 0)
                return;
            if (rows > 0)
                emit model_->dataChanged(model_->index(0, 0), model_->index(rows - 1, cols - 1),
                                         {Qt::DisplayRole, Qt::ToolTipRole, Qt::AccessibleTextRole});
            emit model_->headerDataChanged(Qt::Horizontal, 0, cols - 1);
        });
    }

    QAbstractItemModel* model_;
    bool pending_ = false;
};

// Object names of the per-widget binding helpers (bind_currency_label / _affix).
constexpr char kFmtWidgetsLabelBinding[] = "fincept_fmt_currency_label_binding";
constexpr char kFmtWidgetsAffixBinding[] = "fincept_fmt_currency_affix_binding";

/// The helper that carries one widget's currency binding: a child of the widget
/// (dies with it) and the receiver of the FormatSettings::changed connection, so
/// deleting it ends the binding. A second bind on the same widget replaces the
/// first one through it — screens re-bind when their currency changes.
void fmtwidgets_drop_binding(QWidget* widget, const char* name) {
    const auto old = widget->findChildren<QObject*>(QString::fromLatin1(name), Qt::FindDirectChildrenOnly);
    for (QObject* o : old) {
        QObject::disconnect(&FormatSettings::instance(), nullptr, o, nullptr); // no more applies, now
        o->setObjectName(QString());                                           // not found again
        o->deleteLater(); // may be inside a changed() emission: delete on the next turn
    }
}

QObject* fmtwidgets_rebind(QWidget* widget, const char* name) {
    fmtwidgets_drop_binding(widget, name);
    auto* holder = new QObject(widget);
    holder->setObjectName(QString::fromLatin1(name));
    return holder;
}

/// Prefix / suffix text for a money spin box in `ccy`.
void fmtwidgets_affix(const QString& ccy, QString* prefix, QString* suffix) {
    const QString unit = currency_unit(ccy);
    // money() shows where this user's settings put the unit for this currency.
    const QString sample = money(1.0, ccy.trimmed().isEmpty() ? display_currency() : ccy);
    if (sample.startsWith(unit)) {
        *prefix = unit + QStringLiteral(" ");
        suffix->clear();
    } else {
        prefix->clear();
        *suffix = QStringLiteral("\u00A0") + unit;
    }
}

} // namespace fmtwidgets_detail

void bind_currency_label(QLabel* lbl, const QString& tmpl, const QString& ccy) {
    if (!lbl)
        return;
    auto apply = [lbl, tmpl, ccy]() {
        const QString text = tmpl.arg(currency_unit(ccy));
        if (lbl->text() != text)
            lbl->setText(text);
    };
    apply();
    QObject* holder = fmtwidgets_detail::fmtwidgets_rebind(lbl, fmtwidgets_detail::kFmtWidgetsLabelBinding);
    QObject::connect(&FormatSettings::instance(), &FormatSettings::changed, holder, apply);
}

void bind_currency_affix(QAbstractSpinBox* spin, const QString& ccy) {
    if (!spin)
        return;
    QPointer<QAbstractSpinBox> guard(spin);
    auto apply = [guard, ccy]() {
        if (!guard)
            return;
        QString prefix;
        QString suffix;
        fmtwidgets_detail::fmtwidgets_affix(ccy, &prefix, &suffix);
        if (auto* d = qobject_cast<QDoubleSpinBox*>(guard.data())) {
            if (d->prefix() != prefix)
                d->setPrefix(prefix);
            if (d->suffix() != suffix)
                d->setSuffix(suffix);
        } else if (auto* i = qobject_cast<QSpinBox*>(guard.data())) {
            if (i->prefix() != prefix)
                i->setPrefix(prefix);
            if (i->suffix() != suffix)
                i->setSuffix(suffix);
        }
    };
    apply();
    QObject* holder = fmtwidgets_detail::fmtwidgets_rebind(spin, fmtwidgets_detail::kFmtWidgetsAffixBinding);
    QObject::connect(&FormatSettings::instance(), &FormatSettings::changed, holder, apply);
}

void unbind_currency(QWidget* widget) {
    if (!widget)
        return;
    fmtwidgets_detail::fmtwidgets_drop_binding(widget, fmtwidgets_detail::kFmtWidgetsLabelBinding);
    fmtwidgets_detail::fmtwidgets_drop_binding(widget, fmtwidgets_detail::kFmtWidgetsAffixBinding);
}

void on_format_changed(QWidget* owner, std::function<void()> rerender) {
    if (!owner || !rerender)
        return;
    new fmtwidgets_detail::FmtWidgetsChangeHook(owner, std::move(rerender));
}

void refresh_model_on_format_change(QAbstractItemModel* model) {
    if (!model)
        return;
    new fmtwidgets_detail::FmtWidgetsModelHook(model);
}

} // namespace fincept::ui::formatting
