#pragma once
// ReportBuilderTemplates — the built-in template catalogue and the builder that turns
// a template name into a document. Pure data (no service state), so the library page
// can draw previews and the inbox can start a new report from a template off the
// Report Builder screen.
//
// "Live data" templates are made of bound blocks (core/report/ReportBinding.h): the
// data binder fills them on Refresh Data (and right after the template is applied).
// They take parameters: {"symbol": "AAPL"} or {"portfolio_id": "<id>"}.

#include "core/report/ReportDocument.h"

#include <QString>
#include <QVariantMap>
#include <QVector>

namespace fincept::services::report_templates {

struct BuiltinTemplate {
    QString name;        ///< stable lookup key (never translated)
    QString category;    ///< display (translated)
    QString description; ///< display (translated)
    QString needs;       ///< "symbol" | "portfolio" | ""
    bool live = false;   ///< made of self-filling data blocks
};

/// Every built-in template, in display order (live-data templates first).
QVector<BuiltinTemplate> catalogue();

bool is_builtin(const QString& name);

/// The document of template `name` in report theme `theme` (a template keeps the
/// user's look). Unknown names give an empty "Untitled Report".
report::ReportDocument build(const QString& name, const report::ReportTheme& theme, const QVariantMap& params = {});

} // namespace fincept::services::report_templates
