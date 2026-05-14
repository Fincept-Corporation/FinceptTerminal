// src/ui/navigation/CommandBar_Input.cpp
//
// Input-event handlers: on_text_changed (the typing-time router that picks
// among slash, dock, or asset search modes), on_return_pressed (the commit
// path), on_item_clicked / on_item_entered (mouse interaction with the
// dropdown).
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

void CommandBar::on_text_changed(const QString& text) {
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty()) {
        mode_ = Mode::Screen;
        active_asset_type_.clear();
        search_debounce_->stop();
        hide_dropdown();
        return;
    }

    // ── Check if user is in asset search mode (e.g. "/stock AAPL") ──────────
    if (mode_ == Mode::AssetSearch && !active_asset_type_.isEmpty()) {
        // Find the space after the slash-type prefix
        const int space_idx = trimmed.indexOf(' ');
        if (space_idx < 0) {
            // User deleted back to just "/stock" — revert to slash picker
            if (trimmed.startsWith("/")) {
                mode_ = Mode::SlashPicker;
                show_slash_suggestions(trimmed);
            } else {
                mode_ = Mode::Screen;
                active_asset_type_.clear();
            }
            return;
        }
        const QString query = trimmed.mid(space_idx + 1).trimmed();
        if (query.isEmpty()) {
            // Just "/stock " with nothing after — show hint
            list_->clear();
            auto* item = new QListWidgetItem(list_);
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            auto* row = new QWidget;
            row->setStyleSheet("background:transparent;");
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(10, 6, 10, 6);
            auto* hint = new QLabel(QString("Type a symbol or name to search %1s...").arg(active_asset_type_));
            hint->setStyleSheet(
                QString("color:%1;font-size:11px;font-family:'Consolas',monospace;background:transparent;")
                    .arg(colors::TEXT_TERTIARY.get()));
            rl->addWidget(hint);
            item->setSizeHint(QSize(0, 30));
            list_->setItemWidget(item, row);
            show_dropdown();
            return;
        }
        schedule_asset_search(query);
        return;
    }

    // ── Slash prefix detection ──────────────────────────────────────────────
    if (trimmed.startsWith("/")) {
        // Check if the user has completed a type and started typing a query
        // e.g. "/stock AAPL" — the space separates type from query
        const int space_idx = trimmed.indexOf(' ');
        if (space_idx > 0) {
            const QString slash_part = trimmed.left(space_idx).toLower();
            for (const auto& at : asset_types_) {
                if (at.slash == slash_part) {
                    activate_asset_mode(at.api_type);
                    const QString query = trimmed.mid(space_idx + 1).trimmed();
                    if (!query.isEmpty())
                        schedule_asset_search(query);
                    return;
                }
            }
        }
        // Still picking the slash type
        mode_ = Mode::SlashPicker;
        show_slash_suggestions(trimmed);
        return;
    }

    // ── DockSecondary: user picked a verb and is typing the second screen ──────
    if (mode_ == Mode::DockSecondary && !dock_primary_id_.isEmpty() && !dock_verb_.isEmpty()) {
        // Text looks like: "<primary> <verb> <partial>"
        // Find the verb position and extract everything after it
        const QString lower = trimmed.toLower();
        for (const QString& v : {QString("add"), QString("replace")}) {
            const int vi = lower.indexOf(" " + v + " ");
            if (vi >= 0) {
                const QString partial = trimmed.mid(vi + v.length() + 2);
                show_dock_secondary_suggestions(v, partial);
                return;
            }
            // verb at end with no second token yet
            if (lower.endsWith(" " + v)) {
                show_dock_secondary_suggestions(v, {});
                return;
            }
        }
    }

    // ── DockCommand: user already resolved primary, picking verb ────────────
    if (mode_ == Mode::DockCommand && !dock_primary_id_.isEmpty()) {
        // Stay in DockCommand until they type something non-verb-like
        const QString after_space = trimmed.mid(trimmed.indexOf(' ') + 1).toLower().trimmed();
        // If they've typed "add " or "replace " transition to DockSecondary
        if (after_space.startsWith("add ") || after_space.startsWith("replace ")) {
            dock_verb_ = after_space.startsWith("add") ? "add" : "replace";
            mode_ = Mode::DockSecondary;
            const QString partial = after_space.mid(after_space.indexOf(' ') + 1);
            show_dock_secondary_suggestions(dock_verb_, partial);
            return;
        }
        // Still picking verb — re-show verb suggestions
        show_dock_verb_suggestions(dock_primary_id_);
        return;
    }

    // ── Detect "<valid_screen> " (space after a known screen) ───────────────
    if (trimmed.endsWith(' ') || trimmed.contains(' ')) {
        const int space = trimmed.indexOf(' ');
        if (space > 0) {
            const QString first_token = trimmed.left(space);
            const QString resolved = resolve_screen_id(first_token);
            if (!resolved.isEmpty()) {
                // Check if the second token is "remove" — that's a complete command
                const QString rest = trimmed.mid(space + 1).trimmed().toLower();
                if (rest == "remove") {
                    // Show hint that Enter will execute
                    list_->clear();
                    auto* item = new QListWidgetItem(list_);
                    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
                    auto* row = new QWidget;
                    row->setStyleSheet("background:transparent;");
                    auto* hl2 = new QHBoxLayout(row);
                    hl2->setContentsMargins(10, 6, 10, 6);
                    auto* lbl = new QLabel(QString("Press Enter to close all except %1")
                                               .arg(resolve_screen_id(first_token).toUpper().replace("_", " ")));
                    lbl->setStyleSheet(QString("color:%1;font-size:11px;"
                                               "font-family:'Consolas',monospace;background:transparent;")
                                           .arg(colors::AMBER.get()));
                    hl2->addWidget(lbl);
                    item->setSizeHint(QSize(0, 30));
                    list_->setItemWidget(item, row);
                    show_dropdown();
                    dock_primary_id_ = resolved;
                    mode_ = Mode::DockCommand;
                    return;
                }
                // Transition to DockCommand — show verb suggestions
                if (rest.isEmpty() || rest == "a" || rest == "ad" || rest == "r" || rest == "re" || rest == "rep") {
                    dock_primary_id_ = resolved;
                    dock_verb_.clear();
                    mode_ = Mode::DockCommand;
                    show_dock_verb_suggestions(resolved);
                    return;
                }
                // They're typing "add X" or "replace X"
                if (rest.startsWith("add") || rest.startsWith("replace")) {
                    dock_primary_id_ = resolved;
                    const QString verb = rest.startsWith("add") ? "add" : "replace";
                    dock_verb_ = verb;
                    mode_ = Mode::DockSecondary;
                    const int vi = rest.indexOf(' ');
                    const QString partial = vi >= 0 ? rest.mid(vi + 1) : QString();
                    show_dock_secondary_suggestions(verb, partial);
                    return;
                }
            }
        }
    }

    // ── Normal screen command search ────────────────────────────────────────
    mode_ = Mode::Screen;
    dock_primary_id_.clear();
    dock_verb_.clear();
    active_asset_type_.clear();

    const auto results = search(text);
    if (results.isEmpty()) {
        hide_dropdown();
        return;
    }

    list_->clear();
    for (const auto& cmd : results) {
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

        auto* sep_lbl = new QLabel(QStringLiteral("\u203A"));
        sep_lbl->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;").arg(colors::TEXT_TERTIARY.get()));

        auto* name_lbl = new QLabel(cmd.name);
        name_lbl->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;"
                                        "font-family:'Consolas',monospace;")
                                    .arg(colors::TEXT_SECONDARY.get()));

        hl->addWidget(alias_lbl);
        hl->addWidget(sep_lbl);
        hl->addWidget(name_lbl, 1);

        if (!cmd.shortcut.isEmpty()) {
            auto* sc_lbl = new QLabel(cmd.shortcut);
            sc_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-family:'Consolas',monospace;"
                                          "background:transparent;")
                                      .arg(colors::TEXT_DIM.get()));
            hl->addWidget(sc_lbl);
        }

        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
    }

    list_->setCurrentRow(0);
    show_dropdown();
}

void CommandBar::on_return_pressed() {
    // In asset-search or slash-picker modes, Enter must never fall through to
    // screen navigation — the user has to explicitly pick an asset from the list.
    if (mode_ == Mode::AssetSearch) {
        auto* item = list_->currentItem();
        if (!item)
            return;
        const QString symbol = item->data(Qt::UserRole).toString();
        const QString type = item->data(Qt::UserRole + 2).toString();
        // Only navigate if this item is an actual asset result (has a symbol)
        if (!symbol.isEmpty())
            select_asset(symbol, type);
        // else: hint or "no results" row — do nothing, keep waiting for input
        return;
    }

    // ── Try compound dock command from typed text in any dock-related mode ──
    if (!dropdown_->isVisible() || !list_->currentItem()) {
        const QString text = input_->text().trimmed();
        // Try compound dock command: "X add Y", "X replace Y", "X remove"
        // Works from Screen, DockCommand, or DockSecondary modes
        if (mode_ == Mode::Screen || mode_ == Mode::DockCommand || mode_ == Mode::DockSecondary) {
            if (try_parse_dock_command(text)) {
                mode_ = Mode::Screen;
                dock_primary_id_.clear();
                dock_verb_.clear();
                return;
            }
        }
        if (mode_ == Mode::Screen) {
            // Fall back to simple screen navigation
            const auto results = search(text);
            if (!results.isEmpty()) {
                emit navigate_to(results.first().id);
                input_->clear();
                mode_ = Mode::Screen;
                hide_dropdown();
            }
        }
        return;
    }

    auto* item = list_->currentItem();

    if (mode_ == Mode::SlashPicker) {
        // User pressed Enter on a slash-type suggestion — activate that asset mode
        const QString slash = item->data(Qt::UserRole + 1).toString();
        for (const auto& at : asset_types_) {
            if (at.slash == slash) {
                input_->setText(slash + " ");
                input_->setCursorPosition(input_->text().length());
                activate_asset_mode(at.api_type);
                return;
            }
        }
        return;
    }

    // ── DockCommand: user pressed Enter on a verb suggestion ────────────
    if (mode_ == Mode::DockCommand) {
        const QString verb = item->data(Qt::UserRole).toString();
        const QString primary = item->data(Qt::UserRole + 1).toString();
        if (verb == "remove") {
            emit dock_command("remove", primary, {});
            input_->clear();
            mode_ = Mode::Screen;
            dock_primary_id_.clear();
            dock_verb_.clear();
            hide_dropdown();
            input_->clearFocus();
        } else {
            // Transition to secondary — append verb to input
            dock_verb_ = verb;
            mode_ = Mode::DockSecondary;
            const QString current = input_->text().trimmed();
            input_->setText(current.endsWith(' ') ? current + verb + " " : current + " " + verb + " ");
            input_->setCursorPosition(input_->text().length());
            input_->setFocus();
        }
        return;
    }

    // ── DockSecondary: user pressed Enter on a secondary screen ─────────
    if (mode_ == Mode::DockSecondary) {
        const QString secondary_id = item->data(Qt::UserRole).toString();
        if (secondary_id.isEmpty())
            return; // header row
        emit dock_command(dock_verb_, dock_primary_id_, secondary_id);
        input_->clear();
        mode_ = Mode::Screen;
        dock_primary_id_.clear();
        dock_verb_.clear();
        hide_dropdown();
        input_->clearFocus();
        return;
    }

    // Screen mode — navigate to the selected screen
    execute_index(list_->currentRow());
}

void CommandBar::on_item_clicked(QListWidgetItem* item) {
    if (!item)
        return;

    if (mode_ == Mode::SlashPicker) {
        const QString slash = item->data(Qt::UserRole + 1).toString();
        for (const auto& at : asset_types_) {
            if (at.slash == slash) {
                input_->setText(slash + " ");
                input_->setCursorPosition(input_->text().length());
                activate_asset_mode(at.api_type);
                input_->setFocus();
                return;
            }
        }
        return;
    }

    if (mode_ == Mode::AssetSearch) {
        const QString symbol = item->data(Qt::UserRole).toString();
        const QString type = item->data(Qt::UserRole + 2).toString();
        if (!symbol.isEmpty())
            select_asset(symbol, type);
        return;
    }

    if (mode_ == Mode::DockCommand) {
        // User clicked a verb (add/replace/remove)
        const QString verb = item->data(Qt::UserRole).toString();
        const QString primary = item->data(Qt::UserRole + 1).toString();
        if (verb == "remove") {
            emit dock_command("remove", primary, {});
            input_->clear();
            mode_ = Mode::Screen;
            dock_primary_id_.clear();
            dock_verb_.clear();
            hide_dropdown();
            input_->clearFocus();
        } else {
            // Transition to secondary — append verb to input
            dock_verb_ = verb;
            mode_ = Mode::DockSecondary;
            const QString current = input_->text().trimmed();
            input_->setText(current.endsWith(' ') ? current + verb + " " : current + " " + verb + " ");
            input_->setCursorPosition(input_->text().length());
            input_->setFocus();
            show_dock_secondary_suggestions(verb, {});
        }
        return;
    }

    if (mode_ == Mode::DockSecondary) {
        const QString secondary_id = item->data(Qt::UserRole).toString();
        if (secondary_id.isEmpty())
            return; // header row
        emit dock_command(dock_verb_, dock_primary_id_, secondary_id);
        input_->clear();
        mode_ = Mode::Screen;
        dock_primary_id_.clear();
        dock_verb_.clear();
        hide_dropdown();
        input_->clearFocus();
        return;
    }

    // Screen mode
    emit navigate_to(item->data(Qt::UserRole).toString());
    input_->clear();
    mode_ = Mode::Screen;
    hide_dropdown();
}

void CommandBar::on_item_entered(QListWidgetItem* item) {
    list_->setCurrentItem(item);
}

// ── slash-command helpers ────────────────────────────────────────────────────

} // namespace fincept::ui
