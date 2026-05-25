#pragma once

#include "core/result/Result.h"

#include <QByteArray>
#include <QMap>
#include <QString>

namespace fincept::multiuser {

struct PhaseOneAuthenticatedCallerContext {
    bool authenticated = false;
    QString bearer_session_token;
};

class PhaseOneHttpRequestContext {
  public:
    static fincept::Result<PhaseOneHttpRequestContext> parse(const QByteArray& raw_request);

    QByteArray method() const;
    QString path() const;
    QString header(const QString& name) const;
    const QMap<QString, QString>& headers() const;
    QByteArray body() const;
    QString bearer_session_token() const;
    PhaseOneAuthenticatedCallerContext authenticated_caller_context() const;

  private:
    QByteArray method_;
    QString path_;
    QMap<QString, QString> headers_;
    QByteArray body_;
};

} // namespace fincept::multiuser
