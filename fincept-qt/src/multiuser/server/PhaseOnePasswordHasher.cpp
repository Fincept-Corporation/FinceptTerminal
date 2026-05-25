#include "multiuser/server/PhaseOnePasswordHasher.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStringList>

namespace fincept::multiuser {

namespace {

constexpr int kIterations = 200000;
constexpr int kSaltLength = 32;
constexpr int kHashLength = 32;

QByteArray hmac_sha256(const QByteArray& key, const QByteArray& message) {
    static constexpr int kBlockSize = 64;

    QByteArray normalized_key = key;
    if (normalized_key.size() > kBlockSize)
        normalized_key = QCryptographicHash::hash(normalized_key, QCryptographicHash::Sha256);
    while (normalized_key.size() < kBlockSize)
        normalized_key.append('\0');

    QByteArray ipad(kBlockSize, '\0');
    QByteArray opad(kBlockSize, '\0');
    for (int i = 0; i < kBlockSize; ++i) {
        ipad[i] = static_cast<char>(normalized_key[i] ^ 0x36);
        opad[i] = static_cast<char>(normalized_key[i] ^ 0x5C);
    }

    QCryptographicHash inner(QCryptographicHash::Sha256);
    inner.addData(ipad);
    inner.addData(message);
    const QByteArray inner_hash = inner.result();

    QCryptographicHash outer(QCryptographicHash::Sha256);
    outer.addData(opad);
    outer.addData(inner_hash);
    return outer.result();
}

QByteArray derive_key(const QString& password, const QByteArray& salt) {
    const QByteArray password_bytes = password.toUtf8();
    QByteArray salt_block = salt;
    salt_block.append('\0');
    salt_block.append('\0');
    salt_block.append('\0');
    salt_block.append('\x01');

    QByteArray u = hmac_sha256(password_bytes, salt_block);
    QByteArray t = u;
    for (int iter = 1; iter < kIterations; ++iter) {
        u = hmac_sha256(password_bytes, u);
        for (int i = 0; i < t.size(); ++i)
            t[i] = static_cast<char>(t[i] ^ u[i]);
    }

    return t.left(kHashLength);
}

bool constant_time_equals(const QByteArray& left, const QByteArray& right) {
    if (left.size() != right.size())
        return false;

    unsigned char diff = 0;
    for (int i = 0; i < left.size(); ++i)
        diff |= static_cast<unsigned char>(left[i] ^ right[i]);
    return diff == 0;
}

QByteArray random_salt() {
    QByteArray salt(kSaltLength, '\0');
    QRandomGenerator* rng = QRandomGenerator::system();
    for (int i = 0; i < kSaltLength; ++i)
        salt[i] = static_cast<char>(rng->bounded(256));
    return salt;
}

} // namespace

QString PhaseOnePasswordHasher::hash_password(const QString& password) {
    const QByteArray salt = random_salt();
    const QByteArray hash = derive_key(password, salt);
    return QStringLiteral("pbkdf2_sha256$%1$%2$%3")
        .arg(kIterations)
        .arg(QString::fromUtf8(salt.toBase64(QByteArray::Base64Encoding)))
        .arg(QString::fromUtf8(hash.toBase64(QByteArray::Base64Encoding)));
}

bool PhaseOnePasswordHasher::verify_password(const QString& password, const QString& encoded_hash) {
    const QStringList parts = encoded_hash.split('$');
    if (parts.size() != 4 || parts.at(0) != QStringLiteral("pbkdf2_sha256"))
        return false;

    bool iterations_ok = false;
    const int iterations = parts.at(1).toInt(&iterations_ok);
    if (!iterations_ok || iterations != kIterations)
        return false;

    const QByteArray salt = QByteArray::fromBase64(parts.at(2).toUtf8());
    const QByteArray expected_hash = QByteArray::fromBase64(parts.at(3).toUtf8());
    if (salt.isEmpty() || expected_hash.isEmpty())
        return false;

    return constant_time_equals(derive_key(password, salt), expected_hash);
}

} // namespace fincept::multiuser
