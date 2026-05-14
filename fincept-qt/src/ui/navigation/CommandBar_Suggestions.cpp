// src/ui/navigation/CommandBar_Suggestions.cpp
//
// Suggestion-list builders for the three completion modes —
// slash commands, dock primary-verb (e.g. ":pin "), and dock secondary
// completions — plus the small parsers (resolve_screen_id,
// try_parse_dock_command).
//
// Part of the partial-class split of CommandBar.cpp.

#include "ui/navigation/CommandBar.h"
#include "ui/navigation/CommandBar_internal.h"

#include "core/events/EventBus.h"
#include "core/keys/KeyConfigManager.h"
#include "core/session/ScreenStateManager.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QRegularExpression>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::ui {

using fincept::ui::commandbar_internal::kMaxResults;

void CommandBar::show_slash_suggestions(const QString& partial) {
    const QString q = partial.toLower();

    list_->clear();
    for (const auto& at : asset_types_) {
        if (!at.slash.startsWith(q) && q != "/")
            continue;

        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, at.api_type);
        item->setData(Qt::UserRole + 1, at.slash);

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 5, 10, 5);
        hl->setSpacing(8);

        auto* slash_lbl = new QLabel(at.slash);
        slash_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;"
                                         "font-family:'Consolas',monospace;background:transparent;")
                                     .arg(colors::AMBER.get()));
        slash_lbl->setFixedWidth(80);

        auto* sep_lbl = new QLabel(QStringLiteral("\u203A"));
        sep_lbl->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;").arg(colors::TEXT_TERTIARY.get()));

        auto* desc_lbl = new QLabel(at.description);
        desc_lbl->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;"
                                        "font-family:'Consolas',monospace;")
                                    .arg(colors::TEXT_SECONDARY.get()));

        hl->addWidget(slash_lbl);
        hl->addWidget(sep_lbl);
        hl->addWidget(desc_lbl, 1);

        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
    }

    if (list_->count() > 0) {
        list_->setCurrentRow(0);
        show_dropdown();
    } else {
        hide_dropdown();
    }
}


void CommandBar::show_dock_verb_suggestions(const QString& primary_id) {
    // Show add / replace / remove as clickable suggestions after "<screen> "
    list_->clear();

    struct Verb {
        QString verb;
        QString label;
        QString hint;
    };
    const QList<Verb> verbs = {
        {"add", "ADD", "Open a second screen alongside"},
        {"replace", "REPLACE", "Close this screen, open another"},
        {"remove", "REMOVE", "Close all other screens"},
    };

    for (const auto& v : verbs) {
        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, v.verb);         // verb
        item->setData(Qt::UserRole + 1, primary_id); // primary screen id

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 5, 10, 5);
        hl->setSpacing(8);

        auto* verb_lbl = new QLabel(v.verb.toUpper());
        verb_lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;"
                                        "font-family:'Consolas',monospace;background:transparent;")
                                    .arg(colors::AMBER.get()));
        verb_lbl->setFixedWidth(64);

        auto* sep = new QLabel(QStringLiteral("\u203A"));
        sep->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(colors::TEXT_TERTIARY.get()));

        auto* hint_lbl = new QLabel(v.hint);
        hint_lbl->setStyleSheet(
            QString("color:%1;font-size:11px;font-family:'Consolas',monospace;background:transparent;")
                .arg(colors::TEXT_SECONDARY.get()));

        hl->addWidget(verb_lbl);
        hl->addWidget(sep);
        hl->addWidget(hint_lbl, 1);
        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
    }

    list_->setCurrentRow(0);
    show_dropdown();
}

void CommandBar::show_dock_secondary_suggestions(const QString& verb, const QString& partial) {
    // Show filtered screen list as the second screen for add/replace
    const auto results = search(partial.trimmed().isEmpty() ? QString() : partial);
    list_->clear();

    // Header hint row
    {
        auto* item = new QListWidgetItem(list_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 4, 10, 4);
        auto* lbl = new QLabel(QString("%1 — pick a screen:").arg(verb.toUpper()));
        lbl->setStyleSheet(QString("color:%1;font-size:10px;font-family:'Consolas',monospace;background:transparent;")
                               .arg(colors::AMBER.get()));
        hl->addWidget(lbl);
        item->setSizeHint(QSize(0, 24));
        list_->setItemWidget(item, row);
    }

    const auto& pool = results.isEmpty() ? commands_ : results;
    for (const auto& cmd : pool) {
        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, cmd.id);
        item->setData(Qt::UserRole + 1, cmd.aliases.first());

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 5, 10, 5);
        hl->setSpacing(6);

        auto* alias_lbl = new QLabel(cmd.aliases.first().toUpper());
        alias_lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;"
                                         "font-family:'Consolas',monospace;background:transparent;")
                                     .arg(colors::TEXT_PRIMARY.get()));
        alias_lbl->setFixedWidth(72);

        auto* sep = new QLabel(QStringLiteral("\u203A"));
        sep->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(colors::TEXT_TERTIARY.get()));

        auto* name_lbl = new QLabel(cmd.name);
        name_lbl->setStyleSheet(
            QString("color:%1;font-size:11px;font-family:'Consolas',monospace;background:transparent;")
                .arg(colors::TEXT_SECONDARY.get()));

        hl->addWidget(alias_lbl);
        hl->addWidget(sep);
        hl->addWidget(name_lbl, 1);
        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
    }

    if (list_->count() > 1)
        list_->setCurrentRow(1); // skip header
    show_dropdown();
}

QString CommandBar::resolve_screen_id(const QString& token) const {
    const QString t = token.trimmed().toLower();
    // Direct id match
    for (const auto& cmd : commands_)
        if (cmd.id == t)
            return cmd.id;
    // Alias / name match (same logic as search())
    for (const auto& cmd : commands_) {
        if (cmd.name.toLower() == t)
            return cmd.id;
        for (const auto& a : cmd.aliases)
            if (a.toLower() == t)
                return cmd.id;
    }
    // Keyword prefix
    const auto results = search(t);
    if (!results.isEmpty())
        return results.first().id;
    return {};
}

bool CommandBar::try_parse_dock_command(const QString& text) {
    // Not active during asset search or slash picker
    if (mode_ == Mode::AssetSearch || mode_ == Mode::SlashPicker)
        return false;

    const QString t = text.trimmed();

    // Match: "<primary> add <secondary>"
    static const QRegularExpression re_add(R"(^(.+?)\s+add\s+(.+)$)", QRegularExpression::CaseInsensitiveOption);
    // Match: "<primary> replace <secondary>"
    static const QRegularExpression re_replace(R"(^(.+?)\s+replace\s+(.+)$)",
                                               QRegularExpression::CaseInsensitiveOption);
    // Match: "<primary> remove"
    static const QRegularExpression re_remove(R"(^(.+?)\s+remove$)", QRegularExpression::CaseInsensitiveOption);

    auto m_add = re_add.match(t);
    auto m_replace = re_replace.match(t);
    auto m_remove = re_remove.match(t);

    if (m_add.hasMatch()) {
        const QString primary = resolve_screen_id(m_add.captured(1));
        const QString secondary = resolve_screen_id(m_add.captured(2));
        if (primary.isEmpty() || secondary.isEmpty())
            return false;
        emit dock_command("add", primary, secondary);
        input_->clear();
        hide_dropdown();
        input_->clearFocus();
        return true;
    }
    if (m_replace.hasMatch()) {
        const QString primary = resolve_screen_id(m_replace.captured(1));
        const QString secondary = resolve_screen_id(m_replace.captured(2));
        if (primary.isEmpty() || secondary.isEmpty())
            return false;
        emit dock_command("replace", primary, secondary);
        input_->clear();
        hide_dropdown();
        input_->clearFocus();
        return true;
    }
    if (m_remove.hasMatch()) {
        const QString primary = resolve_screen_id(m_remove.captured(1));
        if (primary.isEmpty())
            return false;
        emit dock_command("remove", primary, {});
        input_->clear();
        hide_dropdown();
        input_->clearFocus();
        return true;
    }
    return false;
}

} // namespace fincept::ui
