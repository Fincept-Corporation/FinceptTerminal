#pragma once
// AsiaJsonView — the JSON view the Asia Markets and AkShare explorers show instead of the table: the
// first rows of the result as indented JSON in a read-only QPlainTextEdit, built off the GUI thread.
//
// toJson(Indented) of 1,000 wide rows is ~0.7 MB in ~21,000 lines (bond_zh_cov, 19 columns), and
// QPlainTextEdit::setPlainText of that many lines costs tens of milliseconds on the GUI thread (a text
// block per line, and a text layout per block on the first resize). The worker builds the whole document instead —
// QTextDocument, QTextBlock and QTextLayout are reentrant — and moves it to the GUI thread, which only
// attaches it (QPlainTextDocumentLayout + setDocument: two plain passes over the blocks).
//
// Header-only and without Q_OBJECT, so it needs no build-list entry.

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <memory>

namespace fincept::screens::asia_links {

/// A JSON view document on its way from the worker to the GUI thread. Deleted (deleteLater, so on
/// the GUI thread) unless install_json_document() takes it — e.g. when the screen went away first.
struct JsonViewDocument {
    QTextDocument* doc = nullptr;
    JsonViewDocument() = default;
    JsonViewDocument(const JsonViewDocument&) = delete;
    JsonViewDocument& operator=(const JsonViewDocument&) = delete;
    ~JsonViewDocument() {
        if (doc)
            doc->deleteLater();
    }
};
using JsonViewDocumentPtr = std::shared_ptr<JsonViewDocument>;

/// The JSON views' worker: one thread (a newer request supersedes an older one anyway). A child of
/// the application object, so a build still running is waited for when the application goes.
inline QThreadPool* json_view_pool() {
    static QThreadPool* const pool = [] {
        auto* p = new QThreadPool(QCoreApplication::instance());
        p->setObjectName(QStringLiteral("AsiaJsonView"));
        p->setMaxThreadCount(1);
        return p;
    }();
    return pool;
}

/// Worker side: `header` + the first `limit` of `rows` as indented JSON, in a no-wrap document whose
/// blocks already carry their text layouts, moved to `target` (the GUI thread). `rows` is only read.
/// Any thread.
inline JsonViewDocumentPtr build_json_document(const QJsonArray& rows, int limit, const QString& header,
                                               QThread* target) {
    QJsonArray shown;
    if (rows.size() <= limit) {
        shown = rows;
    } else {
        for (qsizetype i = 0; i < limit; ++i)
            shown.append(rows.at(i));
    }
    const QString text = header + QString::fromUtf8(QJsonDocument(shown).toJson(QJsonDocument::Indented));

    auto out = std::make_shared<JsonViewDocument>();
    auto* doc = new QTextDocument;
    doc->setUndoRedoEnabled(false);
    QTextOption option = doc->defaultTextOption();
    option.setWrapMode(QTextOption::NoWrap); // the view's mode: setDocument() then changes nothing
    doc->setDefaultTextOption(option);
    doc->setPlainText(text);
    // The per-block text layouts QPlainTextEdit would otherwise create for the whole document on the
    // GUI thread (setDocument() sets the text width, which touches every block's layout).
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
        (void)b.layout();
    doc->moveToThread(target);
    out->doc = doc;
    return out;
}

/// GUI side: show `doc` in `view` with the view's font; the document it replaces is deleted when it
/// was installed here (parented to the view). Takes ownership of `doc`.
inline void install_json_document(QPlainTextEdit* view, QTextDocument* doc) {
    QTextDocument* old = view->document();
    doc->setParent(view);
    doc->setDefaultFont(view->font());
    doc->setDocumentLayout(new QPlainTextDocumentLayout(doc));
    view->setDocument(doc);
    if (old && old != doc && old->parent() == view)
        old->deleteLater();
}

/// GUI side: a short text (a placeholder while the document is built) as the view's document.
inline void set_json_text(QPlainTextEdit* view, const QString& text) {
    auto* doc = new QTextDocument;
    doc->setUndoRedoEnabled(false);
    QTextOption option = doc->defaultTextOption();
    option.setWrapMode(QTextOption::NoWrap);
    doc->setDefaultTextOption(option);
    doc->setPlainText(text);
    install_json_document(view, doc);
}

} // namespace fincept::screens::asia_links
