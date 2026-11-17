#pragma once
// InputMemory — the one way a view remembers its inputs across sessions.
//
// Per view (screen id + sub-tab / view id) it keeps the values of the inputs a
// host tracks — combo boxes, list selections, line edits, plain-text edits,
// spin / double-spin / date-time boxes, check boxes and radio buttons — and
// puts them back on the next visit, so a tab reopens on the user's last inputs
// (and the default run of ui::AutoLoadPane uses them).
//
//   auto* mem = new ui::InputMemory(QStringLiteral("derivatives"), QStringLiteral("bond"), this);
//   mem->track(QStringLiteral("coupon"), coupon_spin);      // stable names: survive layout changes
//   mem->track(QStringLiteral("frequency"), freq_combo);
//   mem->restore();                                         // before the first default run
//   pane->set_signature(mem->signature());                  // what the result was computed from
//   connect(mem, &ui::InputMemory::changed, this, [=] { pane->set_signature(mem->signature()); });
//
// or, for a form whose inputs have no names, everything under a root by position:
//   mem->track_all(form);   // keys "@<class>#<n>", guarded by the layout signature
//
// and for state that is not a widget (a picked series, a sort column, a range):
//   const QString series = mem->value(QStringLiteral("series"), default_id).toString();
//   mem->set_value(QStringLiteral("series"), picked_id);    // same document, part of signature()
//
// Storage: SettingsRepository, key "inputs/<screen>/<view>", category "inputs",
// value = compact JSON {"v":1,"sig":"<positional layout signature>","inputs":{...}}.
// Crash-safe: each write is a single settings row; writes are debounced
// (~600 ms after the last edit) and flushed when the host widget hides and when
// the memory is destroyed. Nothing is written until an input is actually edited
// (a read failure therefore never leads to defaults overwriting stored values).
//
// Restore order: combos first (their handlers may repopulate dependent inputs),
// then lists, then spin / date / check boxes, text last. A combo or list that is
// still empty (filled asynchronously) keeps its value pending; it is applied when
// its items arrive (rowsInserted) and is carried over by saves meanwhile. A value
// that no longer exists in a populated combo / list is dropped.
//
// Positional inputs (track_all) are restored only when the saved layout
// signature — the classes of the positional inputs, in order — equals the
// current one: after an app update that adds or removes an input they start
// from the defaults instead of landing in the wrong fields. Named inputs
// (track) are always restored.
//
// Custom inputs (a condition builder, a symbol-chip list): track(name, getter, setter,
// sender, &Sender::changed) keeps a QJsonValue under `name` in the same document —
// restored after the widget inputs, part of values() / signature().
//
// Symbol boxes: track(name, symbol_box) remembers a ui::SymbolSearchBox — the
// picked instrument (canonical symbol, the text shown, its Instrument JSON) — and
// puts it back through SymbolSearchBox::set_instrument without a network call (a
// value holding only a symbol is looked up in the local index). Restored with the
// text inputs. track_all() leaves symbol boxes alone (by position they keep their
// inner line edit, as before); the inner line edit of a box tracked by name is skipped.
//
// Never stored: password / no-echo line edits, the internal editor of a combo or
// spin box, read-only editors, and any widget with the dynamic property
// "inputMemoryIgnore" set to true. GUI thread only.

#include "ui/widgets/AutoLoadPolicy.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <functional>

class QEvent;
class QTimer;
class QWidget;

namespace fincept::ui {

// ── Pure rules (QtCore only; unit-tested in tests/tst_input_memory.cpp) ──────
namespace input_memory_detail {

/// Version of the stored document.
constexpr int kFormatVersion = 1;
/// Settings category of every stored input set.
inline QString settings_category() {
    return QStringLiteral("inputs");
}

/// "inputs/<screen>/<view>" ("inputs/<screen>" when the view id is empty).
inline QString storage_key(const QString& screen_id, const QString& view_id) {
    QString key = QStringLiteral("inputs/") + screen_id;
    if (!view_id.isEmpty())
        key += QLatin1Char('/') + view_id;
    return key;
}

/// Key of the n-th positional input of a class (track_all): "@<class>#<n>".
/// Named keys (track) never start with '@'.
inline QString positional_key(const QString& class_name, int n) {
    return QLatin1Char('@') + class_name + QLatin1Char('#') + QString::number(n);
}

inline bool is_positional_key(const QString& key) {
    return key.startsWith(QLatin1Char('@'));
}

/// The layout signature of the positional inputs: their classes, in order.
inline QString layout_signature(const QStringList& class_names) {
    return class_names.join(QLatin1Char(','));
}

/// Positional values are restored only into the same layout they were saved from.
inline bool positional_restore_allowed(const QString& saved_signature, const QString& current_signature) {
    return !current_signature.isEmpty() && saved_signature == current_signature;
}

/// The stored document for `inputs` (compact JSON text).
inline QString encode(const QVariantMap& inputs, const QString& layout_sig) {
    QJsonObject doc;
    doc.insert(QStringLiteral("v"), kFormatVersion);
    doc.insert(QStringLiteral("sig"), layout_sig);
    doc.insert(QStringLiteral("inputs"), QJsonObject::fromVariantMap(inputs));
    return QString::fromUtf8(QJsonDocument(doc).toJson(QJsonDocument::Compact));
}

struct Decoded {
    bool ok = false;        ///< a readable document of a known version
    QVariantMap inputs;     ///< stored values by key
    QString layout_sig;     ///< layout signature of the positional inputs when saved
};

/// Parses a stored document. Empty, unreadable or newer-version text → ok = false.
inline Decoded decode(const QString& text) {
    Decoded out;
    if (text.trimmed().isEmpty())
        return out;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return out;
    const QJsonObject o = doc.object();
    const int version = o.value(QStringLiteral("v")).toInt(0);
    if (version < 1 || version > kFormatVersion || !o.value(QStringLiteral("inputs")).isObject())
        return out;
    out.ok = true;
    out.inputs = o.value(QStringLiteral("inputs")).toObject().toVariantMap();
    out.layout_sig = o.value(QStringLiteral("sig")).toString();
    return out;
}

/// What a save writes: the stored values (positional ones only while the layout still matches,
/// so values of inputs not tracked this session survive), overlaid by the current values, overlaid
/// by values still pending (their combo / list has not been filled yet).
inline QVariantMap merge_for_save(const QVariantMap& stored, bool positional_ok, const QVariantMap& current,
                                  const QVariantMap& pending) {
    QVariantMap out;
    for (auto it = stored.cbegin(); it != stored.cend(); ++it)
        if (positional_ok || !is_positional_key(it.key()))
            out.insert(it.key(), it.value());
    for (auto it = current.cbegin(); it != current.cend(); ++it)
        out.insert(it.key(), it.value());
    for (auto it = pending.cbegin(); it != pending.cend(); ++it)
        out.insert(it.key(), it.value());
    return out;
}

/// Kinds of input, in restore order.
enum class Kind { Combo = 0, List = 1, Toggle = 2, Text = 3, Other = 4 };
constexpr Kind kRestoreOrder[] = {Kind::Combo, Kind::List, Kind::Toggle, Kind::Text};

/// Signature of a set of input values (= ui::autoload::signature_of): stable, key-order independent.
inline QString signature(const QVariantMap& values) {
    return autoload::signature_of(values);
}

/// A symbol box (ui::SymbolSearchBox, tracked by name) as stored: the picked
/// instrument's canonical symbol, the text the box showed and the instrument's JSON
/// (core::instrument::Instrument::to_json), so it comes back without a network call.
/// Text the user typed without picking keeps only `text`.
struct SymbolValue {
    QString symbol;         ///< canonical market-data symbol ("RELIANCE.NS"); empty = not a pick
    QString text;           ///< what the box showed
    QJsonObject instrument; ///< Instrument::to_json(); empty = not known
    bool is_pick() const { return !symbol.isEmpty() || !instrument.isEmpty(); }
};

/// {"t": text[, "sym": symbol][, "inst": {...}]}
inline QVariant encode_symbol(const SymbolValue& v) {
    QVariantMap m{{QStringLiteral("t"), v.text}};
    if (!v.symbol.isEmpty())
        m.insert(QStringLiteral("sym"), v.symbol);
    if (!v.instrument.isEmpty())
        m.insert(QStringLiteral("inst"), v.instrument.toVariantMap());
    return m;
}

/// A value as the stored document gives it back — a JSON text round trip, exactly
/// what encode() / decode() do — so InputMemory::set_value() keeps the same value
/// before and after a restart (QDateTime → ISO text, whole doubles → integers,
/// lists / maps element by element). `ok` = false (and an invalid result) for an
/// invalid QVariant or a value JSON cannot hold (a custom type, NaN / infinity).
inline QVariant normalise_value(const QVariant& value, bool* ok) {
    if (ok)
        *ok = false;
    if (!value.isValid())
        return {};
    const bool null_in = value.typeId() == QMetaType::Nullptr;
    const QJsonValue j = QJsonValue::fromVariant(value);
    if (j.isUndefined() || (j.isNull() && !null_in))
        return {};
    QJsonArray wrap;
    wrap.append(j);
    const QJsonDocument back = QJsonDocument::fromJson(QJsonDocument(wrap).toJson(QJsonDocument::Compact));
    const QJsonValue out = back.array().at(0);
    if (out.isUndefined() || (out.isNull() && !null_in))
        return {};
    if (ok)
        *ok = true;
    return out.toVariant();
}

/// A custom input's value (InputMemory::track with a getter) as it is kept and stored:
/// the same JSON text round trip as normalise_value, so the value — and signature() —
/// is identical before and after a restart. Undefined / not representable → ok = false.
inline QVariant custom_to_stored(const QJsonValue& value, bool* ok) {
    if (ok)
        *ok = false;
    if (value.isUndefined())
        return {};
    return normalise_value(value.toVariant(), ok);
}

/// What a custom input's setter receives for a stored value.
inline QJsonValue custom_from_stored(const QVariant& stored) {
    return QJsonValue::fromVariant(stored);
}

/// Reads encode_symbol()'s form. A bare string (a line edit that held the symbol
/// before the box was tracked itself) reads as that symbol and text.
inline SymbolValue decode_symbol(const QVariant& stored) {
    SymbolValue v;
    if (stored.typeId() == QMetaType::QString) {
        v.text = stored.toString();
        v.symbol = v.text.trimmed();
        return v;
    }
    const QVariantMap m = stored.toMap();
    v.text = m.value(QStringLiteral("t")).toString();
    v.symbol = m.value(QStringLiteral("sym")).toString().trimmed();
    v.instrument = QJsonObject::fromVariantMap(m.value(QStringLiteral("inst")).toMap());
    return v;
}

} // namespace input_memory_detail

class InputMemory : public QObject {
    Q_OBJECT
  public:
    explicit InputMemory(const QString& screen_id, const QString& view_id, QObject* parent = nullptr);
    /// Flushes an unsaved edit.
    ~InputMemory() override;

    /// Remember `input` under a stable `name` (must not start with '@'). Tracking an input after
    /// restore() puts its stored value back at once (lazily built sub-tabs).
    void track(const QString& name, QWidget* input);
    /// Remember every supported input under `root` by position ("@<class>#<n>"), guarded by the
    /// layout signature. Calling it again for the same root replaces that root's inputs.
    void track_all(QWidget* root);

    // ── Custom inputs (condition builders, symbol-chip lists, anything not a Qt input) ──
    /// The input's current value as JSON (QJsonValue::Undefined = nothing to remember).
    using CustomGetter = std::function<QJsonValue()>;
    /// Puts a stored value back (called by restore(), or at once when tracked after it).
    using CustomSetter = std::function<void(const QJsonValue&)>;
    /// Connects the input's own change signal to `notify` (called once by track()).
    using CustomHookup = std::function<void(std::function<void()> notify)>;

    /// Remember a custom input under a stable `name` — same document, format version, debounced
    /// write and values() / signature() participation as a tracked widget. The value is kept as
    /// storage gives it back (input_memory_detail::custom_to_stored), so the signature does not
    /// change across a restart. Custom inputs are restored after every widget input (a builder
    /// may depend on a combo). `guard` (optional): once it is destroyed the getter / setter are
    /// never called again and the last value is kept, like a destroyed widget's. Tracking the
    /// same name again replaces the earlier custom input; a name used by a tracked widget or a
    /// set_value() value is refused (warning). The setter runs inside restore(): edits it causes
    /// are not counted as user edits.
    void track(const QString& name, CustomGetter get, CustomSetter set, CustomHookup hookup = {},
               QObject* guard = nullptr);
    /// Same, hooked to `sender`'s `signal` (any arguments are ignored); `sender` is the guard.
    ///   mem->track(QStringLiteral("rules"), [=] { return builder->to_json(); },
    ///              [=](const QJsonValue& v) { builder->load_json(v); }, builder, &ConditionBuilder::changed);
    template <typename Sender, typename Signal>
    void track(const QString& name, CustomGetter get, CustomSetter set, Sender* sender, Signal signal) {
        QObject* guard = sender;
        track(name, std::move(get), std::move(set),
              [this, sender, signal](std::function<void()> notify) {
                  QObject::connect(sender, signal, this, [notify]() { notify(); });
              },
              guard);
    }
    /// The custom input `name` changed (for hosts without a single change signal): re-reads its
    /// getter; changed() + a debounced write when the value differs.
    void notify_changed(const QString& name);

    /// Puts the stored values back (see the restore order above). True when something was applied.
    /// Emits no changed() for what it applies.
    bool restore();
    /// Write now (normally debounced). A no-op when nothing was edited since the last write.
    void save_now();
    /// The current values of every tracked input, plus values still pending, plus
    /// the non-widget values set or read this session (set_value / value).
    QVariantMap values() const;
    /// ui::autoload::signature_of(values()) — feed it to AutoLoadPane::set_signature.
    QString signature() const;

    /// Non-widget state of the view (a picked series id, a chart range, a sort
    /// column) in the same stored document and format version as the tracked
    /// inputs, so it counts in values() / signature(). `name` as for track() (not
    /// starting with '@'), and not the name of a tracked input. Kept as storage
    /// gives it back (input_memory_detail::normalise_value); a value JSON cannot
    /// hold is not stored (a warning). An invalid QVariant removes the value.
    /// When it changes: changed() at once, written like an edit (debounced).
    void set_value(const QString& name, const QVariant& value);
    /// This session's value, else the stored one (read on first use — restore() is
    /// not needed), else `fallback`. A stored value read here counts in values() /
    /// signature() from then on (stored values never read stay out of them).
    QVariant value(const QString& name, const QVariant& fallback = QVariant());

    /// Switch to another view of the same screen (another sub-tab): writes the old view's edits;
    /// does not restore (call restore() once the view's inputs are tracked).
    void set_view(const QString& view_id);
    QString view() const { return view_id_; }
    QString screen() const { return screen_id_; }
    QString storage_key() const;

    /// Forget this view's stored inputs (storage and pending values); the widgets keep their values.
    void clear();

  signals:
    /// A tracked input was edited (immediately, not debounced) or a pending value arrived.
    void changed();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    struct Entry {
        QString key;
        QPointer<QWidget> widget;
        QPointer<QWidget> root; ///< track_all root (null for named inputs)
        QString class_name;     ///< positional inputs: for the layout signature
    };

    void add_entry(const QString& key, QWidget* w, QWidget* root, const QString& class_name);
    void connect_input(QWidget* w);
    void on_input_changed(QWidget* w);
    void watch_host(QWidget* w);
    void schedule_pending();
    void apply_pending();
    /// Read the stored document once (and again after a read failure). False when the read failed.
    bool ensure_loaded();
    /// Apply `value` to the entry's widget; a combo / list without items keeps it pending.
    bool apply_entry(const Entry& e, const QVariant& value);
    const Entry* entry_for(const QWidget* w) const;
    /// Drop this view's non-widget values from the session (view switch, clear()).
    void forget_values();
    QString positional_signature() const;
    bool positional_ok() const;

    struct CustomEntry {
        QString key;
        CustomGetter get;
        CustomSetter set;
        QPointer<QObject> guard;
        bool guarded = false; ///< a guard was given (a null guard then means "gone")
        bool alive() const { return !guarded || !guard.isNull(); }
    };
    int custom_index(const QString& name) const;
    /// Re-read a custom input; true when its kept value changed.
    bool read_custom(const CustomEntry& c);
    void apply_custom(const CustomEntry& c, const QVariant& stored);

    QString screen_id_;
    QString view_id_;
    QList<Entry> entries_;
    QList<CustomEntry> customs_;
    QHash<const QWidget*, int> connected_;  ///< widgets whose change signals are connected
    QVariantMap current_;                   ///< last known value per key (survives widget destruction)
    QVariantMap pending_;                   ///< stored values waiting for their combo / list items
    QVariantMap stored_;                    ///< the stored document's values (merged into every write)
    QString stored_sig_;                    ///< its positional layout signature
    QSet<QString> value_keys_;              ///< set_value / value names of this view (not widgets)
    QSet<QString> removed_values_;          ///< values removed since the last write
    QTimer* save_timer_ = nullptr;
    bool loaded_ = false;
    bool load_failed_ = false;
    bool restored_ = false;
    bool restoring_ = false;
    bool dirty_ = false;
    bool pending_scheduled_ = false;
};

} // namespace fincept::ui
