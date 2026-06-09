#pragma once
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QUuid>

#include <functional>

namespace fincept {

/// Strongly-typed UUID newtype.
///
/// `QUuid` is structurally weak: a `WindowId` and a `PanelInstanceId` are the
/// same type to the compiler, so passing one where the other is expected is a
/// silent bug. The newtype below tags each kind with a phantom struct so the
/// type system catches mistakes that would otherwise need careful review.
///
/// All operations are inline / header-only; the macro at the bottom registers
/// QHash + Q_DECLARE_METATYPE so the typed ids flow through QVariant, signal/
/// slot queues, and QHash without per-id-type boilerplate.
///
/// Usage:
///   auto id = WindowId::generate();        // new random uuid
///   auto same = WindowId::from_string(s);  // null on parse failure
///   QString txt = id.to_string();          // canonical 8-4-4-4-12 form
///   if (id.is_null()) ...                  // null/zero check
///   QHash<WindowId, Foo> map;              // works (qHash overload below)
template <typename Tag>
class TypedUuid {
  public:
    constexpr TypedUuid() = default;
    explicit TypedUuid(const QUuid& uuid) : uuid_(uuid) {}

    /// Generate a fresh random uuid (v4).
    static TypedUuid generate() { return TypedUuid(QUuid::createUuid()); }

    /// Deterministic name-based uuid (v5): the same `name` always yields the
    /// same uuid, stable across process restarts. Use when an id must survive
    /// persistence WITHOUT being stored separately — e.g. a panel's instance id
    /// derived from "window+dock id" so its saved UI state still matches after
    /// a relaunch (a random generate() would mint a new id every launch and the
    /// saved state would never be found again).
    static TypedUuid from_name(const QString& name) {
        // Fixed app namespace — arbitrary but constant so names map stably.
        static const QUuid kNamespace(
            QStringLiteral("{6b3f8a1e-2c4d-5e6f-8a9b-0c1d2e3f4a5b}"));
        return TypedUuid(QUuid::createUuidV5(kNamespace, name.toUtf8()));
    }

    /// Parse from canonical 8-4-4-4-12 string. Returns a null TypedUuid on
    /// failure — call is_null() to discriminate.
    static TypedUuid from_string(const QString& s) {
        return TypedUuid(QUuid::fromString(s));
    }

    /// Canonical hyphenated form without surrounding braces:
    ///   "ee5d3c0e-9c4b-4f0a-9e0a-1c2b3a4d5e6f"
    QString to_string() const {
        return uuid_.toString(QUuid::WithoutBraces);
    }

    bool is_null() const { return uuid_.isNull(); }
    const QUuid& raw() const { return uuid_; }

    bool operator==(const TypedUuid& other) const { return uuid_ == other.uuid_; }
    bool operator!=(const TypedUuid& other) const { return uuid_ != other.uuid_; }
    bool operator<(const TypedUuid& other) const { return uuid_ < other.uuid_; }

  private:
    QUuid uuid_;
};

template <typename Tag>
inline size_t qHash(const TypedUuid<Tag>& id, size_t seed = 0) noexcept {
    return qHash(id.raw(), seed);
}

// ── Tags + concrete types ────────────────────────────────────────────────────
//
// Each tag is an empty struct; `WindowId` and `PanelInstanceId` are distinct
// types even though they share the same underlying representation.

struct WindowIdTag {};
struct PanelInstanceIdTag {};
struct LayoutIdTag {};
struct ProfileIdTag {};

using WindowId = TypedUuid<WindowIdTag>;
using PanelInstanceId = TypedUuid<PanelInstanceIdTag>;
using LayoutId = TypedUuid<LayoutIdTag>;
using ProfileId = TypedUuid<ProfileIdTag>;

} // namespace fincept

Q_DECLARE_METATYPE(fincept::WindowId)
Q_DECLARE_METATYPE(fincept::PanelInstanceId)
Q_DECLARE_METATYPE(fincept::LayoutId)
Q_DECLARE_METATYPE(fincept::ProfileId)
