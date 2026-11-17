// src/ui/widgets/InputMemory.cpp — see the header.
//
// The value read / apply rules are lifted from Economics' panel persistence
// (EconPanelBase: read_input / apply_input / kind_of, the layout-signature
// guard and the pending values of asynchronously filled combos).

#include "ui/widgets/InputMemory.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
#include <QVariantList>
#include <QWidget>

namespace fincept::ui {

namespace imd = input_memory_detail;

namespace {

constexpr int kImSaveDelayMs = 600;
constexpr int kImMaxTextChars = 64 * 1024; // a pasted document is not an "input" worth a settings row

imd::Kind im_kind_of(const QWidget* w) {
    if (qobject_cast<const SymbolSearchBox*>(w)) // the picked instrument, restored with the text inputs
        return imd::Kind::Text;
    if (qobject_cast<const QComboBox*>(w))
        return imd::Kind::Combo;
    if (qobject_cast<const QListWidget*>(w))
        return imd::Kind::List;
    if (qobject_cast<const QAbstractSpinBox*>(w)) // spin, double spin, date / time
        return imd::Kind::Toggle;
    if (auto* b = qobject_cast<const QAbstractButton*>(w))
        return b->isCheckable() ? imd::Kind::Toggle : imd::Kind::Other;
    if (qobject_cast<const QLineEdit*>(w) || qobject_cast<const QPlainTextEdit*>(w) || qobject_cast<const QTextEdit*>(w))
        return imd::Kind::Text;
    return imd::Kind::Other;
}

/// Item data worth matching on (an empty string marks "custom" entries).
bool im_usable_data(const QVariant& d) {
    return d.isValid() && !d.isNull() && !(d.typeId() == QMetaType::QString && d.toString().isEmpty());
}

bool im_multi_select(const QListWidget* lw) {
    return lw->selectionMode() == QAbstractItemView::MultiSelection ||
           lw->selectionMode() == QAbstractItemView::ExtendedSelection;
}

QVariantMap im_item_value(const QListWidgetItem* it) {
    return {{QStringLiteral("d"), it->data(Qt::UserRole).toString()}, {QStringLiteral("t"), it->text()}};
}

bool im_item_matches(const QListWidgetItem* it, const QVariantMap& m) {
    const QString d = m.value(QStringLiteral("d")).toString();
    return d.isEmpty() ? it->text() == m.value(QStringLiteral("t")).toString() : it->data(Qt::UserRole).toString() == d;
}

QVariant im_text_value(const QString& text) {
    if (text.size() > kImMaxTextChars)
        return {};
    return text;
}

/// The value of an input (invalid = nothing to remember).
QVariant im_read(QWidget* w) {
    if (auto* box = qobject_cast<SymbolSearchBox*>(w))
        return box->stored_value();
    if (auto* cb = qobject_cast<QComboBox*>(w)) {
        if (cb->currentIndex() < 0) {
            if (cb->isEditable() && !cb->currentText().isEmpty())
                return QVariantMap{{QStringLiteral("t"), cb->currentText()}};
            return {};
        }
        // The raw item data (not its string form): findData() compares QVariants.
        return QVariantMap{{QStringLiteral("d"), cb->currentData()}, {QStringLiteral("t"), cb->currentText()}};
    }
    if (auto* lw = qobject_cast<QListWidget*>(w)) {
        if (im_multi_select(lw)) {
            QVariantList sel;
            const QList<QListWidgetItem*> items = lw->selectedItems();
            for (const QListWidgetItem* it : items)
                sel << im_item_value(it);
            return QVariantMap{{QStringLiteral("sel"), sel}};
        }
        const QListWidgetItem* it = lw->currentItem();
        return it ? QVariant(im_item_value(it)) : QVariant();
    }
    if (auto* dt = qobject_cast<QDateTimeEdit*>(w))
        return dt->dateTime().toString(Qt::ISODate);
    if (auto* sb = qobject_cast<QSpinBox*>(w))
        return sb->value();
    if (auto* ds = qobject_cast<QDoubleSpinBox*>(w))
        return ds->value();
    if (auto* b = qobject_cast<QAbstractButton*>(w))
        return b->isCheckable() ? QVariant(b->isChecked()) : QVariant();
    if (auto* le = qobject_cast<QLineEdit*>(w))
        return le->echoMode() == QLineEdit::Normal ? im_text_value(le->text()) : QVariant();
    if (auto* pe = qobject_cast<QPlainTextEdit*>(w))
        return im_text_value(pe->toPlainText());
    if (auto* te = qobject_cast<QTextEdit*>(w))
        return im_text_value(te->toPlainText());
    return {};
}

/// Applies a stored value. False when the target is a combo / list with no items yet (filled
/// asynchronously): the caller keeps the value pending. A value that no longer exists in a populated
/// combo / list is dropped (true) rather than retried forever.
bool im_apply(QWidget* w, const QVariant& v) {
    if (auto* box = qobject_cast<SymbolSearchBox*>(w)) {
        box->restore_stored_value(v); // stored Instrument JSON, or a local-index lookup: no network
        return true;
    }
    if (auto* cb = qobject_cast<QComboBox*>(w)) {
        const QVariantMap m = v.toMap();
        const QVariant d = m.value(QStringLiteral("d"));
        const QString t = m.value(QStringLiteral("t")).toString();
        int idx = im_usable_data(d) ? cb->findData(d) : -1;
        if (idx < 0 && im_usable_data(d)) {
            // Data that went through JSON can come back as another numeric type ("10" vs 10).
            const QString ds = d.toString();
            for (int i = 0; i < cb->count() && idx < 0 && !ds.isEmpty(); ++i)
                if (cb->itemData(i).toString() == ds)
                    idx = i;
        }
        if (idx < 0 && !t.isEmpty())
            idx = cb->findText(t);
        if (idx >= 0) {
            cb->setCurrentIndex(idx);
            return true;
        }
        if (cb->isEditable() && !t.isEmpty()) {
            cb->setEditText(t);
            return true;
        }
        return cb->count() > 0;
    }
    if (auto* lw = qobject_cast<QListWidget*>(w)) {
        if (lw->count() == 0)
            return false;
        const QVariantMap m = v.toMap();
        if (m.contains(QStringLiteral("sel"))) {
            const QVariantList sel = m.value(QStringLiteral("sel")).toList();
            if (!im_multi_select(lw))
                return true; // the list is single-selection now: nothing sensible to apply
            lw->clearSelection();
            for (int i = 0; i < lw->count(); ++i) {
                QListWidgetItem* it = lw->item(i);
                for (const QVariant& s : sel)
                    if (im_item_matches(it, s.toMap())) {
                        it->setSelected(true);
                        break;
                    }
            }
            return true;
        }
        for (int i = 0; i < lw->count(); ++i) {
            QListWidgetItem* it = lw->item(i);
            if (im_item_matches(it, m)) {
                lw->setCurrentItem(it);
                lw->scrollToItem(it);
                return true;
            }
        }
        return true;
    }
    if (auto* dt = qobject_cast<QDateTimeEdit*>(w)) {
        const QDateTime when = QDateTime::fromString(v.toString(), Qt::ISODate);
        if (when.isValid())
            dt->setDateTime(when);
        return true;
    }
    if (auto* sb = qobject_cast<QSpinBox*>(w)) {
        bool ok = false;
        const int i = v.toInt(&ok);
        if (ok)
            sb->setValue(i);
        return true;
    }
    if (auto* ds = qobject_cast<QDoubleSpinBox*>(w)) {
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (ok)
            ds->setValue(d);
        return true;
    }
    if (auto* b = qobject_cast<QAbstractButton*>(w)) {
        if (b->isCheckable())
            b->setChecked(v.toBool());
        return true;
    }
    if (auto* le = qobject_cast<QLineEdit*>(w)) {
        le->setText(v.toString());
        return true;
    }
    if (auto* pe = qobject_cast<QPlainTextEdit*>(w)) {
        pe->setPlainText(v.toString());
        return true;
    }
    if (auto* te = qobject_cast<QTextEdit*>(w)) {
        te->setPlainText(v.toString());
        return true;
    }
    return true;
}

/// Whether `w` (under `root`) is an input track_all() remembers.
bool im_positional_candidate(QWidget* w, QWidget* root) {
    if (w->property("inputMemoryIgnore").toBool())
        return false;
    // A symbol box is remembered by name only (track): by position its inner line edit
    // stays the input, so the layout signature of existing forms does not change.
    if (qobject_cast<SymbolSearchBox*>(w))
        return false;
    const imd::Kind kind = im_kind_of(w);
    if (kind == imd::Kind::Other)
        return false;
    // Toggles by position: check boxes and radio buttons only (a checkable push / tool button is
    // usually navigation — a tab or a mode switch — not an input).
    if (qobject_cast<QAbstractButton*>(w) && !qobject_cast<QCheckBox*>(w) && !qobject_cast<QRadioButton*>(w))
        return false;
    if (auto* le = qobject_cast<QLineEdit*>(w)) {
        if (le->echoMode() != QLineEdit::Normal || le->isReadOnly())
            return false; // never secrets; a read-only field shows a result
        QWidget* p = le->parentWidget();
        if (qobject_cast<QComboBox*>(p) || qobject_cast<QAbstractSpinBox*>(p))
            return false; // the internal editor of a combo / spin box
    }
    if (auto* pe = qobject_cast<QPlainTextEdit*>(w); pe && pe->isReadOnly())
        return false;
    if (auto* te = qobject_cast<QTextEdit*>(w); te && te->isReadOnly())
        return false;
    if (w->isWindow())
        return false; // a popup, not part of the form
    // Parts of other inputs (a combo's popup list, a date edit's calendar with its own spin box),
    // cell editors of tables / lists (they come and go with editing) and popup content.
    for (QWidget* p = w->parentWidget(); p && p != root; p = p->parentWidget())
        if (p->isWindow() || qobject_cast<QAbstractItemView*>(p) || qobject_cast<QComboBox*>(p) ||
            qobject_cast<QAbstractSpinBox*>(p))
            return false;
    return true;
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

InputMemory::InputMemory(const QString& screen_id, const QString& view_id, QObject* parent)
    : QObject(parent), screen_id_(screen_id), view_id_(view_id) {
    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(kImSaveDelayMs);
    connect(save_timer_, &QTimer::timeout, this, &InputMemory::save_now);
    // The host view hiding (another tab, another screen) flushes an edit at once.
    if (auto* host = qobject_cast<QWidget*>(parent))
        watch_host(host);
}

InputMemory::~InputMemory() {
    // Only remembered values are written (current_), never the widgets: they may be gone already.
    if (dirty_)
        save_now();
}

QString InputMemory::storage_key() const {
    return imd::storage_key(screen_id_, view_id_);
}

void InputMemory::watch_host(QWidget* w) {
    if (w)
        w->installEventFilter(this);
}

bool InputMemory::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Hide && dirty_)
        save_now();
    return QObject::eventFilter(watched, event);
}

// ── Tracking ─────────────────────────────────────────────────────────────────

void InputMemory::track(const QString& name, QWidget* input) {
    if (!input || name.isEmpty())
        return;
    if (imd::is_positional_key(name)) {
        LOG_WARN("InputMemory", QStringLiteral("%1: input names must not start with '@' (%2)").arg(storage_key(), name));
        return;
    }
    if (im_kind_of(input) == imd::Kind::Other) {
        LOG_WARN("InputMemory", QStringLiteral("%1: %2 is not a supported input (%3)")
                                    .arg(storage_key(), name, QString::fromLatin1(input->metaObject()->className())));
        return;
    }
    for (int i = 0; i < entries_.size(); ++i) {
        if (entries_.at(i).key == name) { // re-tracked (a rebuilt form): the new widget takes the name
            entries_.removeAt(i);
            break;
        }
    }
    add_entry(name, input, nullptr, QString());
    // Tracked after restore() (a lazily built sub-tab): its stored value goes back at once.
    if (restored_ && stored_.contains(name)) {
        const Entry e = entries_.last(); // a copy: applying may run handlers that track more inputs
        restoring_ = true;
        apply_entry(e, stored_.value(name));
        restoring_ = false;
    }
}

void InputMemory::track_all(QWidget* root) {
    if (!root)
        return;
    // Calling it again for the same root replaces that root's inputs.
    for (int i = entries_.size() - 1; i >= 0; --i) {
        if (entries_.at(i).root == root) {
            current_.remove(entries_.at(i).key);
            entries_.removeAt(i);
        }
    }
    // Numbering continues after other roots' positional inputs of the same class.
    QHash<QString, int> counters;
    QHash<const QWidget*, bool> named;
    for (const Entry& e : std::as_const(entries_)) {
        if (imd::is_positional_key(e.key))
            ++counters[e.class_name];
        else if (e.widget)
            named.insert(e.widget.data(), true);
    }
    const int first_new = int(entries_.size());
    // The parts of a symbol box tracked by name (its line edit) are that box's input.
    const auto inside_named_box = [&named, root](const QWidget* w) {
        for (const QWidget* p = w->parentWidget(); p && p != root; p = p->parentWidget())
            if (named.contains(p) && qobject_cast<const SymbolSearchBox*>(p))
                return true;
        return false;
    };
    const QList<QWidget*> all = root->findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (named.contains(w) || !im_positional_candidate(w, root) || inside_named_box(w))
            continue;
        const QString cls = QString::fromLatin1(w->metaObject()->className());
        add_entry(imd::positional_key(cls, counters[cls]++), w, root, cls);
    }
    watch_host(root);
    // Tracked after restore(): the new inputs get their stored values, if the layout still matches.
    if (restored_ && positional_ok()) {
        restoring_ = true;
        for (const imd::Kind pass : imd::kRestoreOrder)
            for (int i = first_new; i < entries_.size(); ++i) {
                const Entry e = entries_.at(i);
                if (e.widget && im_kind_of(e.widget) == pass && stored_.contains(e.key))
                    apply_entry(e, stored_.value(e.key));
            }
        restoring_ = false;
    }
}

// ── Custom inputs ────────────────────────────────────────────────────────────

int InputMemory::custom_index(const QString& name) const {
    for (int i = 0; i < customs_.size(); ++i)
        if (customs_.at(i).key == name)
            return i;
    return -1;
}

void InputMemory::track(const QString& name, CustomGetter get, CustomSetter set, CustomHookup hookup,
                        QObject* guard) {
    if (name.isEmpty() || imd::is_positional_key(name) || !get) {
        LOG_WARN("InputMemory", QStringLiteral("%1: a custom input needs a name not starting with '@' and a getter (%2)")
                                    .arg(storage_key(), name));
        return;
    }
    for (const Entry& e : std::as_const(entries_)) {
        if (e.key == name) {
            LOG_WARN("InputMemory", QStringLiteral("%1: %2 is already a tracked widget").arg(storage_key(), name));
            return;
        }
    }
    if (value_keys_.contains(name)) {
        LOG_WARN("InputMemory", QStringLiteral("%1: %2 is already a set_value() value").arg(storage_key(), name));
        return;
    }
    if (const int existing = custom_index(name); existing >= 0) // re-tracked (a rebuilt builder)
        customs_.removeAt(existing);
    CustomEntry c;
    c.key = name;
    c.get = std::move(get);
    c.set = std::move(set);
    c.guard = guard;
    c.guarded = guard != nullptr;
    customs_.append(c);
    read_custom(c); // its value now: part of values() / signature(); tracking is not an edit
    if (hookup) {
        QPointer<InputMemory> self = this;
        hookup([self, name]() {
            if (self)
                self->notify_changed(name);
        });
    }
    // Tracked after restore() (a lazily built builder): its stored value goes back at once.
    if (restored_ && stored_.contains(name)) {
        const int i = custom_index(name);
        if (i >= 0) {
            const CustomEntry copy = customs_.at(i); // the setter may track more
            restoring_ = true;
            apply_custom(copy, stored_.value(name));
            restoring_ = false;
        }
    }
}

bool InputMemory::read_custom(const CustomEntry& c) {
    if (!c.alive() || !c.get)
        return false;
    bool ok = false;
    const QVariant v = imd::custom_to_stored(c.get(), &ok);
    if (!ok) { // nothing to remember (Undefined, or a value JSON cannot hold)
        if (!current_.contains(c.key))
            return false;
        current_.remove(c.key);
        return true;
    }
    const auto cur = current_.constFind(c.key);
    if (cur != current_.constEnd() && cur.value() == v)
        return false;
    current_.insert(c.key, v);
    return true;
}

void InputMemory::apply_custom(const CustomEntry& c, const QVariant& stored) {
    if (!c.alive() || !c.set)
        return;
    c.set(imd::custom_from_stored(stored));
    read_custom(c); // what the input holds now (it may have dropped parts it no longer knows)
}

void InputMemory::notify_changed(const QString& name) {
    const int i = custom_index(name);
    if (i < 0)
        return;
    const CustomEntry c = customs_.at(i); // a copy: the getter may track more
    if (!read_custom(c))
        return;
    if (restoring_)
        return; // putting a stored value back is not an edit
    dirty_ = true;
    save_timer_->start();
    emit changed();
}

void InputMemory::add_entry(const QString& key, QWidget* w, QWidget* root, const QString& class_name) {
    entries_.append(Entry{key, w, root, class_name});
    const QVariant v = im_read(w);
    if (v.isValid())
        current_.insert(key, v);
    else
        current_.remove(key);
    connect_input(w);
}

void InputMemory::connect_input(QWidget* w) {
    if (connected_.contains(w))
        return;
    connected_.insert(w, 1);
    connect(w, &QObject::destroyed, this, [this, w]() { connected_.remove(w); });
    QPointer<QWidget> guard = w;
    auto changed_fn = [this, guard]() {
        if (guard)
            on_input_changed(guard.data());
    };
    if (auto* box = qobject_cast<SymbolSearchBox*>(w)) {
        connect(box, &SymbolSearchBox::instrument_picked, this, changed_fn);
        connect(box->line_edit(), &QLineEdit::textChanged, this, changed_fn); // typed / cleared
    } else if (auto* cb = qobject_cast<QComboBox*>(w)) {
        connect(cb, qOverload<int>(&QComboBox::currentIndexChanged), this, changed_fn);
        connect(cb, &QComboBox::editTextChanged, this, changed_fn);
        if (cb->model()) // items filled asynchronously: a pending value can be applied now
            connect(cb->model(), &QAbstractItemModel::rowsInserted, this, [this]() { schedule_pending(); });
    } else if (auto* lw = qobject_cast<QListWidget*>(w)) {
        connect(lw, &QListWidget::currentItemChanged, this, changed_fn);
        connect(lw, &QListWidget::itemSelectionChanged, this, changed_fn);
        if (lw->model())
            connect(lw->model(), &QAbstractItemModel::rowsInserted, this, [this]() { schedule_pending(); });
    } else if (auto* dt = qobject_cast<QDateTimeEdit*>(w)) {
        connect(dt, &QDateTimeEdit::dateTimeChanged, this, changed_fn);
    } else if (auto* sb = qobject_cast<QSpinBox*>(w)) {
        connect(sb, qOverload<int>(&QSpinBox::valueChanged), this, changed_fn);
    } else if (auto* ds = qobject_cast<QDoubleSpinBox*>(w)) {
        connect(ds, qOverload<double>(&QDoubleSpinBox::valueChanged), this, changed_fn);
    } else if (auto* b = qobject_cast<QAbstractButton*>(w)) {
        connect(b, &QAbstractButton::toggled, this, changed_fn);
    } else if (auto* le = qobject_cast<QLineEdit*>(w)) {
        connect(le, &QLineEdit::textChanged, this, changed_fn);
    } else if (auto* pe = qobject_cast<QPlainTextEdit*>(w)) {
        connect(pe, &QPlainTextEdit::textChanged, this, changed_fn);
    } else if (auto* te = qobject_cast<QTextEdit*>(w)) {
        connect(te, &QTextEdit::textChanged, this, changed_fn);
    }
}

const InputMemory::Entry* InputMemory::entry_for(const QWidget* w) const {
    for (const Entry& e : entries_)
        if (e.widget.data() == w)
            return &e;
    return nullptr;
}

void InputMemory::on_input_changed(QWidget* w) {
    const Entry* e = entry_for(w);
    if (!e)
        return;
    const QString key = e->key;
    const QVariant v = im_read(w);
    const imd::Kind kind = im_kind_of(w);
    // A combo / list still waiting for its stored value is being filled, not edited by the user:
    // its first item must neither replace the pending value nor count as an edit.
    if (pending_.contains(key) && (kind == imd::Kind::Combo || kind == imd::Kind::List))
        return;
    if (v.isValid())
        current_.insert(key, v);
    else
        current_.remove(key);
    if (restoring_)
        return; // putting stored values back is not an edit
    dirty_ = true;
    save_timer_->start();
    emit changed();
}

// ── Restore ──────────────────────────────────────────────────────────────────

bool InputMemory::ensure_loaded() {
    if (loaded_ && !load_failed_)
        return true;
    loaded_ = true;
    const Result<QString> r = SettingsRepository::instance().get(storage_key());
    if (r.is_err()) {
        // The stored value is unknown, not absent: never write over it from this state.
        load_failed_ = true;
        LOG_WARN("InputMemory", QStringLiteral("%1: stored inputs could not be read: %2")
                                    .arg(storage_key(), QString::fromStdString(r.error())));
        return false;
    }
    load_failed_ = false;
    const imd::Decoded d = imd::decode(r.value());
    stored_ = d.ok ? d.inputs : QVariantMap();
    stored_sig_ = d.ok ? d.layout_sig : QString();
    return true;
}

QString InputMemory::positional_signature() const {
    QStringList classes;
    for (const Entry& e : entries_)
        if (imd::is_positional_key(e.key))
            classes << e.class_name;
    return imd::layout_signature(classes);
}

bool InputMemory::positional_ok() const {
    return imd::positional_restore_allowed(stored_sig_, positional_signature());
}

bool InputMemory::apply_entry(const Entry& e, const QVariant& value) {
    if (!e.widget)
        return false;
    if (!im_apply(e.widget, value)) {
        pending_.insert(e.key, value); // applied when its items arrive
        return false;
    }
    pending_.remove(e.key);
    const QVariant v = im_read(e.widget);
    if (v.isValid())
        current_.insert(e.key, v);
    return true;
}

bool InputMemory::restore() {
    restored_ = true;
    if (!ensure_loaded())
        return false;
    pending_.clear();
    const bool pos_ok = positional_ok();
    bool any_positional_stored = false;
    for (auto it = stored_.cbegin(); it != stored_.cend() && !any_positional_stored; ++it)
        any_positional_stored = imd::is_positional_key(it.key());
    if (!pos_ok && any_positional_stored)
        LOG_INFO("InputMemory", QStringLiteral("%1: the form's inputs changed since they were saved — the "
                                               "positional ones start from their defaults")
                                    .arg(storage_key()));

    bool applied = false;
    restoring_ = true;
    // Combos first (their handlers may repopulate dependent inputs or overwrite text with presets),
    // then lists, spin / date / check boxes, and text last. Indexed loop: a handler may track more.
    for (const imd::Kind pass : imd::kRestoreOrder) {
        for (int i = 0; i < entries_.size(); ++i) {
            const Entry e = entries_.at(i);
            if (!e.widget || im_kind_of(e.widget) != pass || !stored_.contains(e.key))
                continue;
            if (imd::is_positional_key(e.key) && !pos_ok)
                continue;
            if (apply_entry(e, stored_.value(e.key)))
                applied = true;
        }
    }
    // Custom inputs last: a condition builder / chip list may depend on a combo restored above.
    for (int i = 0; i < customs_.size(); ++i) {
        const CustomEntry c = customs_.at(i); // a copy: the setter may track more
        if (!c.alive() || !stored_.contains(c.key))
            continue;
        apply_custom(c, stored_.value(c.key));
        applied = true;
    }
    restoring_ = false;
    return applied;
}

void InputMemory::schedule_pending() {
    if (pending_.isEmpty() || pending_scheduled_)
        return;
    pending_scheduled_ = true;
    // After the current turn: a combo filled with several addItem() calls is complete by then.
    QTimer::singleShot(0, this, [this]() {
        pending_scheduled_ = false;
        apply_pending();
    });
}

void InputMemory::apply_pending() {
    bool any = false;
    restoring_ = true;
    const QStringList keys = pending_.keys();
    for (const QString& key : keys) {
        for (int i = 0; i < entries_.size(); ++i) {
            const Entry e = entries_.at(i);
            if (e.key != key || !e.widget)
                continue;
            if (apply_entry(e, pending_.value(key)))
                any = true;
            break;
        }
    }
    restoring_ = false;
    if (any)
        emit changed(); // the inputs differ from what a run started before their items arrived used
}

// ── Values, saving ───────────────────────────────────────────────────────────

QVariantMap InputMemory::values() const {
    QVariantMap out = current_;
    for (auto it = pending_.cbegin(); it != pending_.cend(); ++it)
        out.insert(it.key(), it.value());
    return out;
}

QString InputMemory::signature() const {
    return imd::signature(values());
}

void InputMemory::set_value(const QString& name, const QVariant& value) {
    if (name.isEmpty() || imd::is_positional_key(name)) {
        LOG_WARN("InputMemory", QStringLiteral("%1: value names must not be empty or start with '@' (%2)")
                                    .arg(storage_key(), name));
        return;
    }
    if (!value.isValid()) { // remove
        const bool had = current_.contains(name) || (ensure_loaded() && stored_.contains(name));
        current_.remove(name);
        value_keys_.insert(name);
        if (!had || removed_values_.contains(name))
            return;
        removed_values_.insert(name);
    } else {
        bool ok = false;
        const QVariant v = imd::normalise_value(value, &ok);
        if (!ok) {
            LOG_WARN("InputMemory", QStringLiteral("%1: value %2 not stored — %3 cannot be kept as JSON")
                                        .arg(storage_key(), name, QString::fromLatin1(value.typeName())));
            return;
        }
        value_keys_.insert(name);
        removed_values_.remove(name);
        if (current_.contains(name) && current_.value(name) == v)
            return;
        current_.insert(name, v);
    }
    dirty_ = true;
    save_timer_->start();
    emit changed();
}

QVariant InputMemory::value(const QString& name, const QVariant& fallback) {
    const auto cur = current_.constFind(name);
    if (cur != current_.constEnd())
        return cur.value();
    const auto pend = pending_.constFind(name);
    if (pend != pending_.constEnd())
        return pend.value();
    if (name.isEmpty() || imd::is_positional_key(name) || removed_values_.contains(name))
        return fallback;
    if (!ensure_loaded())
        return fallback; // unknown, not absent: the caller's default for now
    const auto st = stored_.constFind(name);
    if (st == stored_.constEnd())
        return fallback;
    value_keys_.insert(name);
    current_.insert(name, st.value()); // read: part of values() / signature() from now on
    return st.value();
}

void InputMemory::save_now() {
    save_timer_->stop();
    if (!dirty_)
        return;
    if (!ensure_loaded()) {
        // Unknown stored value: writing now could replace it with a partial set. Kept dirty, retried
        // on the next edit / hide.
        LOG_WARN("InputMemory", QStringLiteral("%1: not saved — the stored inputs could not be read").arg(storage_key()));
        return;
    }
    const QString sig = positional_signature();
    QVariantMap merged = imd::merge_for_save(stored_, positional_ok(), current_, pending_);
    for (const QString& gone : std::as_const(removed_values_))
        merged.remove(gone); // set_value(name, QVariant()): out of the document too
    const Result<void> r =
        SettingsRepository::instance().set(storage_key(), imd::encode(merged, sig), imd::settings_category());
    if (r.is_err()) {
        LOG_WARN("InputMemory", QStringLiteral("%1: inputs not saved: %2").arg(storage_key(), QString::fromStdString(r.error())));
        return;
    }
    stored_ = merged;
    stored_sig_ = sig;
    removed_values_.clear();
    dirty_ = false;
}

void InputMemory::forget_values() {
    // Non-widget values belong to their view: none carries over to another one.
    for (const QString& k : std::as_const(value_keys_))
        current_.remove(k);
    value_keys_.clear();
    removed_values_.clear();
}

void InputMemory::set_view(const QString& view_id) {
    if (view_id == view_id_)
        return;
    save_now();
    forget_values();
    view_id_ = view_id;
    stored_.clear();
    stored_sig_.clear();
    pending_.clear();
    loaded_ = false;
    load_failed_ = false;
    restored_ = false;
    dirty_ = false;
}

void InputMemory::clear() {
    save_timer_->stop();
    const Result<void> r = SettingsRepository::instance().remove(storage_key());
    if (r.is_err())
        LOG_WARN("InputMemory", QStringLiteral("%1: not cleared: %2").arg(storage_key(), QString::fromStdString(r.error())));
    forget_values();
    stored_.clear();
    stored_sig_.clear();
    pending_.clear();
    dirty_ = false;
    loaded_ = true;
    load_failed_ = false;
}

} // namespace fincept::ui
