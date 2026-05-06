#include "trading/exchanges/hyperliquid/HyperliquidSigner.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariantList>

namespace fincept::trading::hyperliquid {

namespace {

// Canonical JSON: sorted keys, no whitespace. Recursive over arrays/objects.
QString canonical_serialize(const QJsonValue& v);

QString serialize_object(const QJsonObject& obj) {
    QStringList keys = obj.keys();
    keys.sort();
    QStringList parts;
    for (const auto& k : keys) {
        parts << QStringLiteral("\"%1\":%2")
                     .arg(k, canonical_serialize(obj.value(k)));
    }
    return QStringLiteral("{") + parts.join(QStringLiteral(",")) + QStringLiteral("}");
}

QString serialize_array(const QJsonArray& arr) {
    QStringList parts;
    for (const auto& v : arr) parts << canonical_serialize(v);
    return QStringLiteral("[") + parts.join(QStringLiteral(",")) + QStringLiteral("]");
}

QString canonical_serialize(const QJsonValue& v) {
    if (v.isObject()) return serialize_object(v.toObject());
    if (v.isArray())  return serialize_array(v.toArray());
    if (v.isString()) {
        return QStringLiteral("\"") + v.toString() + QStringLiteral("\"");
    }
    if (v.isBool())   return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isNull())   return QStringLiteral("null");
    if (v.isDouble()) return QString::number(v.toDouble(), 'g', 17);
    return QStringLiteral("null");
}

} // namespace

QByteArray HyperliquidSigner::action_struct_hash(const QJsonObject& action) {
    // Stand-in: SHA-256 of the canonical JSON. The real signer must use
    // keccak256 of the EIP-712 typed-data encoding. Tests compare against
    // a published HL vector — they will fail until the real keccak lands.
    const QString canon = canonical_serialize(action);
    return QCryptographicHash::hash(canon.toUtf8(), QCryptographicHash::Sha256);
}

bool HyperliquidSigner::is_wired() {
    // Flip to true once libsecp256k1 + keccak are FetchContent'd and linked.
    return false;
}

Result<SignedAction> HyperliquidSigner::sign_action(const QJsonObject& action,
                                                      const QString& /*agent_priv_key_hex*/,
                                                      bool /*is_testnet*/) {
    if (!is_wired()) {
        return Result<SignedAction>::err("hl_signer_not_wired");
    }
    // Real path (to land):
    //   const auto digest = eip712_digest(domain, action_struct_hash(action));
    //   const auto sig = secp256k1_sign_rfc6979(priv, digest);
    //   sig.v += 27;
    //   const auto addr = recover_address(sig, digest);
    //   return Ok({action, now_ms, "0x"+hex(sig), addr});
    return Result<SignedAction>::err("hl_signer_unreachable");
}

} // namespace fincept::trading::hyperliquid
