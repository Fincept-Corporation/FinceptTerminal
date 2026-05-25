#include "multiuser/server/http/PhaseOneHttpRequestContext.h"

#include <QStringList>

namespace fincept::multiuser {

fincept::Result<PhaseOneHttpRequestContext> PhaseOneHttpRequestContext::parse(const QByteArray& raw_request) {
    const int header_end = raw_request.indexOf("\r\n\r\n");
    const QByteArray header_block = header_end >= 0 ? raw_request.left(header_end) : raw_request;
    const QList<QByteArray> lines = header_block.split('\n');
    if (lines.isEmpty())
        return fincept::Result<PhaseOneHttpRequestContext>::err("missing request line");

    const QList<QByteArray> request_line_parts = lines.first().trimmed().split(' ');
    if (request_line_parts.size() < 2)
        return fincept::Result<PhaseOneHttpRequestContext>::err("invalid request line");

    PhaseOneHttpRequestContext context;
    context.method_ = request_line_parts.at(0).trimmed().toUpper();
    context.path_ = QString::fromUtf8(request_line_parts.at(1).trimmed());

    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty())
            continue;

        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;

        const QString name = QString::fromUtf8(line.left(colon).trimmed());
        const QString value = QString::fromUtf8(line.mid(colon + 1).trimmed());
        context.headers_.insert(name, value);
    }

    if (header_end >= 0)
        context.body_ = raw_request.mid(header_end + 4);

    return fincept::Result<PhaseOneHttpRequestContext>::ok(context);
}

QByteArray PhaseOneHttpRequestContext::method() const {
    return method_;
}

QString PhaseOneHttpRequestContext::path() const {
    return path_;
}

QString PhaseOneHttpRequestContext::header(const QString& name) const {
    for (auto it = headers_.cbegin(); it != headers_.cend(); ++it) {
        if (it.key().compare(name, Qt::CaseInsensitive) == 0)
            return it.value();
    }
    return {};
}

const QMap<QString, QString>& PhaseOneHttpRequestContext::headers() const {
    return headers_;
}

QByteArray PhaseOneHttpRequestContext::body() const {
    return body_;
}

QString PhaseOneHttpRequestContext::bearer_session_token() const {
    const QString authorization = header(QStringLiteral("Authorization"));
    const QString prefix = QStringLiteral("Bearer ");
    if (!authorization.startsWith(prefix, Qt::CaseInsensitive))
        return {};
    return authorization.mid(prefix.size()).trimmed();
}

PhaseOneAuthenticatedCallerContext PhaseOneHttpRequestContext::authenticated_caller_context() const {
    const QString session_token = bearer_session_token();
    return {!session_token.isEmpty(), session_token};
}

} // namespace fincept::multiuser
