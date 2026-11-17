// src/screens/report_builder/ReportBuilderScreen_Components.cpp
//
// Wiring, block edits (by stable id), the coalesced service -> view refresh, live data
// (Refresh Data, per-block state) and the notice strip.
//
// Part of the partial-class split of ReportBuilderScreen.cpp.

#include "core/report/ReportBinding.h"
#include "core/session/ScreenStateManager.h"
#include "screens/report_builder/ReportAiPanel.h"
#include "screens/report_builder/ReportBuilderScreen.h"
#include "screens/report_builder/ReportLibraryPage.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportAiSession.h"
#include "services/report_builder/ReportInbox.h"
#include "ui/theme/ThemeManager.h"

#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QUrl>

namespace fincept::screens {

namespace rep = ::fincept::report;
using Service = ::fincept::services::ReportBuilderService;
using Binder = ::fincept::services::ReportDataBinder;

void ReportBuilderScreen::wire_services() {
    auto& svc = Service::instance();
    auto& binder = Binder::instance();

    // ── Left panel ───────────────────────────────────────────────────────────
    connect(comp_toolbar_, &ComponentToolbar::new_report_requested, this, &ReportBuilderScreen::on_new);
    connect(comp_toolbar_, &ComponentToolbar::open_report_requested, this, &ReportBuilderScreen::on_open);
    connect(comp_toolbar_, &ComponentToolbar::library_requested, this, &ReportBuilderScreen::show_library);
    connect(comp_toolbar_, &ComponentToolbar::recent_reports_requested, this, &ReportBuilderScreen::show_recent_dialog);
    connect(comp_toolbar_, &ComponentToolbar::templates_requested, this, &ReportBuilderScreen::show_library);
    connect(comp_toolbar_, &ComponentToolbar::save_as_template_requested, this,
            &ReportBuilderScreen::show_save_template_dialog);
    connect(comp_toolbar_, &ComponentToolbar::theme_requested, this, &ReportBuilderScreen::show_theme_dialog);
    connect(comp_toolbar_, &ComponentToolbar::metadata_requested, this, &ReportBuilderScreen::show_metadata_dialog);
    connect(comp_toolbar_, &ComponentToolbar::page_setup_requested, this, &ReportBuilderScreen::show_page_setup_dialog);
    connect(comp_toolbar_, &ComponentToolbar::variables_requested, this, &ReportBuilderScreen::show_variables_dialog);
    connect(comp_toolbar_, &ComponentToolbar::schedule_requested, this, &ReportBuilderScreen::show_schedule_dialog);
    connect(comp_toolbar_, &ComponentToolbar::ai_document_requested, this,
            [this](const QString& action) { open_ai_panel(0, action, QString(), -1, -1); });
    // Typeface / size are a document setting (saved, undoable, used by the page and exports).
    connect(comp_toolbar_, &ComponentToolbar::font_changed, this, [](const QString& family, int size) {
        auto& s = Service::instance();
        rep::ReportMetadata meta = s.metadata();
        if (meta.font_family == family && meta.font_size == size)
            return;
        meta.font_family = family;
        meta.font_size = size;
        s.set_metadata(meta);
    });
    connect(comp_toolbar_, &ComponentToolbar::style_changed, this, [this](bool bold, bool italic) {
        rep::ReportComponent c;
        if (selected_id_ <= 0 || !Service::instance().component(selected_id_, &c))
            return;
        QMap<QString, QString> patch{{QStringLiteral("bold"), bold ? QStringLiteral("1") : QString()},
                                     {QStringLiteral("italic"), italic ? QStringLiteral("1") : QString()}};
        Service::instance().patch_component(c.id, nullptr, patch, tr("Text style"), false, true);
    });
    connect(comp_toolbar_, &ComponentToolbar::add_component, this, &ReportBuilderScreen::add_component);
    connect(comp_toolbar_, &ComponentToolbar::structure_selected, this, [this](int id) { select_component(id, true); });
    connect(comp_toolbar_, &ComponentToolbar::move_up, this, [this](int id) { move_component_by(id, -1); });
    connect(comp_toolbar_, &ComponentToolbar::move_down, this, [this](int id) { move_component_by(id, +1); });
    connect(comp_toolbar_, &ComponentToolbar::duplicate, this, &ReportBuilderScreen::duplicate_component);
    connect(comp_toolbar_, &ComponentToolbar::delete_item, this, &ReportBuilderScreen::remove_component);

    // ── Properties panel (by stable id) ─────────────────────────────────────
    connect(properties_, &PropertiesPanel::content_changed, this, [this](int id, const QString& content) {
        rep::ReportComponent c;
        if (!Service::instance().component(id, &c))
            return;
        panel_echo_ids_.insert(id); // the panel already shows this: do not rebuild it under the caret
        Service::instance().update_component(id, content, c.config);
    });
    connect(properties_, &PropertiesPanel::config_changed, this, [this](int id, const QString& key, const QString& val) {
        rep::ReportComponent c;
        if (!Service::instance().component(id, &c))
            return;
        QMap<QString, QString> patch{{key, val}};
        // Editing a template chart's numbers makes them the user's own data.
        if (c.type == QLatin1String("chart") && (key == QLatin1String("data") || key == QLatin1String("labels")))
            patch.insert(QStringLiteral("sample"), QString());
        panel_echo_ids_.insert(id);
        Service::instance().patch_component(id, nullptr, patch, QString(), true,
                                            /*empty_removes=*/key == QLatin1String("style") || key == QLatin1String("sample"));
    });
    connect(properties_, &PropertiesPanel::binding_changed, this, &ReportBuilderScreen::apply_binding);
    connect(properties_, &PropertiesPanel::refresh_requested, this, [](int id) { Binder::instance().refresh(id); });
    connect(properties_, &PropertiesPanel::duplicate_requested, this, &ReportBuilderScreen::duplicate_component);
    connect(properties_, &PropertiesPanel::delete_requested, this, &ReportBuilderScreen::remove_component);
    connect(properties_, &PropertiesPanel::document_info_requested, this, &ReportBuilderScreen::show_metadata_dialog);
    connect(properties_, &PropertiesPanel::page_setup_requested, this, &ReportBuilderScreen::show_page_setup_dialog);
    connect(properties_, &PropertiesPanel::theme_requested, this, &ReportBuilderScreen::show_theme_dialog);
    connect(properties_, &PropertiesPanel::variables_requested, this, &ReportBuilderScreen::show_variables_dialog);
    connect(properties_, &PropertiesPanel::ai_requested, this, &ReportBuilderScreen::open_ai_panel);
    connect(ai_panel_, &ReportAiPanel::apply_requested, this, &ReportBuilderScreen::apply_ai_result);
    connect(ai_panel_, &ReportAiPanel::closed, this, [this]() { ai_panel_->setVisible(false); });

    // ── Canvas ──────────────────────────────────────────────────────────────
    connect(canvas_, &DocumentCanvas::component_clicked, this, [this](int id) { select_component(id, false); });
    connect(canvas_, &DocumentCanvas::component_activated, this, [this](int id) {
        select_component(id, false);
        if (right_collapsed_)
            apply_right_collapsed(false, true);
        properties_->focus_first_editor();
    });
    connect(canvas_, &DocumentCanvas::background_clicked, this, [this]() { select_component(0, false); });
    connect(canvas_, &DocumentCanvas::page_count_changed, this, [this](int) {
        if (selected_id_ <= 0)
            refresh_properties(true);
    });
    connect(canvas_, &DocumentCanvas::image_dropped, this, [this](const QString& path) {
        auto& s = Service::instance();
        rep::ReportComponent sel;
        if (selected_id_ > 0 && s.component(selected_id_, &sel) && sel.type == QLatin1String("image")) {
            s.patch_component(sel.id, nullptr, {{QStringLiteral("path"), path}});
            return;
        }
        rep::ReportComponent comp;
        comp.type = QStringLiteral("image");
        comp.config.insert(QStringLiteral("path"), path);
        comp.config.insert(QStringLiteral("width"), QStringLiteral("400"));
        comp.config.insert(QStringLiteral("align"), QStringLiteral("center"));
        const int at = selected_id_ > 0 ? s.index_of(selected_id_) + 1 : -1;
        select_component(s.add_component(comp, at), true);
    });

    // ── Services → view (coalesced: a burst renders once) ───────────────────
    connect(&svc, &Service::component_added, this, [this](int, int) { schedule_refresh(); });
    connect(&svc, &Service::component_updated, this, [this](int) { schedule_refresh(); });
    connect(&svc, &Service::component_removed, this, [this](int id, int) {
        if (id == selected_id_)
            selected_id_ = 0;
        schedule_refresh();
    });
    connect(&svc, &Service::component_moved, this, [this](int, int, int) { schedule_refresh(); });
    connect(&svc, &Service::metadata_changed, this, &ReportBuilderScreen::schedule_refresh);
    connect(&svc, &Service::theme_changed, this, &ReportBuilderScreen::schedule_refresh);
    connect(&svc, &Service::document_changed, this, [this]() {
        library_saving_ = true;
        schedule_refresh();
    });
    connect(&svc, &Service::document_loaded, this, [this](const QString&) { on_document_loaded(); });
    connect(&svc, &Service::current_file_changed, this, [this](const QString&) { update_title(); });
    connect(&svc, &Service::library_id_changed, this, [this](qint64) { update_title(); });
    connect(&svc, &Service::library_saved, this, [this](qint64) {
        library_saving_ = false;
        update_title();
    });
    connect(&svc, &Service::library_save_failed, this, [this](const QString& error) {
        library_saving_ = false;
        update_title();
        show_notice(NoticeKind::Error,
                    tr("Could not save to the report library: %1. Your work is kept in the recovery copy; Save As writes "
                       "a file.")
                        .arg(error),
                    QString(), "bad");
    });
    connect(&svc, &Service::inbox_changed, this, [this]() {
        schedule_refresh();
        if (isVisible())
            check_inbox_notice();
    });
    connect(&svc, &Service::open_requested, this, [this]() {
        if (isVisible())
            handle_open_request();
    });
    // The library refused a save: the report changed on another computer.
    connect(&svc, &Service::library_conflict_detected, this, [this]() {
        if (isVisible())
            show_conflict_dialog();
    });

    // ── Live data ──────────────────────────────────────────────────────────
    connect(&binder, &Binder::block_state_changed, this, [this](int id) {
        canvas_->refresh_component(id);
        refresh_structure();
    });
    connect(&binder, &Binder::progress, this, [this](int, int) { update_refresh_button(); });
    connect(&binder, &Binder::batch_finished, this, [this](int ok, int failed) {
        update_refresh_button();
        if (failed > 0)
            show_notice(NoticeKind::Info,
                        tr("%1 of %2 data blocks could not be refreshed — they keep their last data; each one says why.")
                            .arg(failed)
                            .arg(ok + failed),
                        QString(), "warn");
        else if (notice_kind_ == NoticeKind::Stale)
            clear_notice(NoticeKind::Stale);
    });

    // ── Library / start page ────────────────────────────────────────────────
    connect(library_page_, &ReportLibraryPage::continue_requested, this, &ReportBuilderScreen::show_editor);
    connect(library_page_, &ReportLibraryPage::open_requested, this, [this](qint64 id) { open_library_report(id, 0); });
    connect(library_page_, &ReportLibraryPage::new_requested, this, &ReportBuilderScreen::on_new);
    connect(library_page_, &ReportLibraryPage::import_requested, this, &ReportBuilderScreen::on_import);
    connect(library_page_, &ReportLibraryPage::template_requested, this, &ReportBuilderScreen::apply_template);
    connect(library_page_, &ReportLibraryPage::report_restored, this,
            [](qint64 id) { Service::instance().reattach_library(id); });

    // Widths follow the UI font; re-fit once a font change has settled (never inside
    // a theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &ReportBuilderScreen::refit_panels);
}

// ── Edits ───────────────────────────────────────────────────────────────────

void ReportBuilderScreen::add_component(const QString& type) {
    rep::ReportComponent comp;
    comp.type = type;
    if (type == QLatin1String("cover")) {
        // One cover, always first: it is the title page.
        auto& s = Service::instance();
        const auto comps = s.components();
        for (const auto& c : comps)
            if (c.type == QLatin1String("cover")) {
                show_editor();
                select_component(c.id, true);
                return;
            }
        properties_->flush_pending();
        const int id = s.add_component(comp, 0);
        show_editor();
        select_component(id, true);
        return;
    }
    if (type == QLatin1String("table")) {
        comp.config.insert(QStringLiteral("rows"), QStringLiteral("3"));
        comp.config.insert(QStringLiteral("cols"), QStringLiteral("3"));
    } else if (type == QLatin1String("heading")) {
        comp.config.insert(QStringLiteral("level"), QStringLiteral("1"));
    }
    properties_->flush_pending();
    auto& svc = Service::instance();
    const int at = selected_id_ > 0 ? svc.index_of(selected_id_) + 1 : -1;
    const int id = svc.add_component(comp, at);
    show_editor();
    select_component(id, true);
}

void ReportBuilderScreen::select_component(int id, bool scroll_into_view) {
    properties_->flush_pending();
    auto& svc = Service::instance();
    if (id > 0 && svc.index_of(id) < 0)
        id = 0;
    selected_id_ = id;
    canvas_->set_selected_id(canvas_->preview_mode() ? 0 : id, scroll_into_view);
    refresh_structure();
    refresh_properties(true);
    rep::ReportComponent c;
    static const QStringList kTextTypes = {"heading", "text", "table", "code", "quote", "list", "callout", "stats_block"};
    const bool styled = id > 0 && svc.component(id, &c) && kTextTypes.contains(c.type);
    comp_toolbar_->set_component_style(styled, styled && c.config.value(QStringLiteral("bold")) == QLatin1String("1"),
                                       styled && c.config.value(QStringLiteral("italic")) == QLatin1String("1"));
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::remove_component(int id) {
    auto& svc = Service::instance();
    const auto comps = svc.components();
    const int idx = svc.index_of(id);
    if (idx < 0)
        return;
    properties_->flush_pending();
    // The neighbour takes the selection (next, else previous).
    int next = 0;
    if (idx + 1 < comps.size())
        next = comps[idx + 1].id;
    else if (idx > 0)
        next = comps[idx - 1].id;
    svc.remove_component(id);
    select_component(next, false);
}

void ReportBuilderScreen::duplicate_component(int id) {
    auto& svc = Service::instance();
    rep::ReportComponent copy;
    if (!svc.component(id, &copy))
        return;
    properties_->flush_pending();
    copy.id = 0; // the service allocates a fresh id
    select_component(svc.add_component(copy, svc.index_of(id) + 1), true);
}

void ReportBuilderScreen::move_component_by(int id, int delta) {
    auto& svc = Service::instance();
    const int idx = svc.index_of(id);
    if (idx < 0)
        return;
    const int to = idx + delta;
    if (to < 0 || to >= svc.components().size())
        return;
    svc.move_component(id, to);
    selected_id_ = id;
    QTimer::singleShot(0, this, [this, id]() { canvas_->scroll_to_component(id); });
}

void ReportBuilderScreen::apply_binding(int id, const QString& binding_json) {
    auto& svc = Service::instance();
    rep::ReportComponent c;
    if (!svc.component(id, &c))
        return;
    if (binding_json.isEmpty()) {
        // "Unbind (keep numbers)": the snapshot stays and becomes the user's.
        rep::ReportComponent nc = c;
        rep::clear_binding(nc);
        QMap<QString, QString> patch;
        for (auto it = c.config.cbegin(); it != c.config.cend(); ++it)
            if (!nc.config.contains(it.key()))
                patch.insert(it.key(), QString());
        svc.patch_component(id, nullptr, patch, tr("Unbind data"), false, true);
        return;
    }
    const rep::Binding b = rep::Binding::parse(binding_json);
    if (!b.is_valid())
        return;
    rep::ReportComponent nc = c;
    rep::set_binding(nc, b);
    QMap<QString, QString> patch;
    for (auto it = nc.config.cbegin(); it != nc.config.cend(); ++it)
        if (c.config.value(it.key()) != it.value())
            patch.insert(it.key(), it.value());
    svc.patch_component(id, nullptr, patch, tr("Change data source"), false, false);
    Binder::instance().refresh(id);
}

// ── Service → view ──────────────────────────────────────────────────────────

void ReportBuilderScreen::schedule_refresh() {
    if (!refresh_timer_->isActive())
        refresh_timer_->start();
}

void ReportBuilderScreen::do_refresh() {
    refresh_timer_->stop();
    if (!isVisible()) {
        // Other tabs may add blocks while this one is hidden: lay the pages out when it
        // is shown, not for an invisible tab.
        hidden_dirty_ = true;
        return;
    }
    hidden_dirty_ = false;
    auto& svc = Service::instance();
    if (selected_id_ > 0 && svc.index_of(selected_id_) < 0)
        selected_id_ = 0;
    const rep::ReportMetadata meta = svc.metadata();
    canvas_->show_document(svc.components(), meta, svc.theme());
    canvas_->set_selected_id(canvas_->preview_mode() ? 0 : selected_id_, false);
    refresh_structure();
    refresh_properties(false);
    comp_toolbar_->set_document_font(meta.font_family, meta.font_size);
    update_title();
    update_refresh_button();
    if (pages_->currentWidget() == library_page_)
        library_page_->set_current(svc.library_id(), meta.title, static_cast<int>(svc.components().size()));
}

void ReportBuilderScreen::refresh_structure() {
    auto& svc = Service::instance();
    const auto comps = svc.components();
    const QVector<int> unseen = svc.unseen_ids();
    const QDateTime now = QDateTime::currentDateTime();
    QVector<ComponentToolbar::StructureItem> items;
    items.reserve(comps.size());
    for (const auto& c : comps) {
        QString label = c.type.toUpper();
        label.replace(QLatin1Char('_'), QLatin1Char(' '));
        if (!c.content.isEmpty())
            label += QStringLiteral(": ") + c.content.left(28).replace(QLatin1Char('\n'), QLatin1Char(' '));
        else if (c.type == QLatin1String("market_data"))
            label += QStringLiteral(": ") + c.config.value(QStringLiteral("symbol"), QStringLiteral("?"));
        else if (!c.config.value(QStringLiteral("title")).isEmpty())
            label += QStringLiteral(": ") + c.config.value(QStringLiteral("title")).left(28);
        else if (c.type == QLatin1String("callout"))
            label += QStringLiteral(" [") + c.config.value(QStringLiteral("style"), QStringLiteral("info")) + QLatin1Char(']');
        const bool bound = rep::is_bound(c);
        items.append({c.id, label, bound, bound && rep::is_stale(c, now), unseen.contains(c.id)});
    }
    comp_toolbar_->update_structure(items, selected_id_);
}

void ReportBuilderScreen::refresh_properties(bool force) {
    auto& svc = Service::instance();
    if (selected_id_ <= 0) {
        if (force || shown_component_.id != 0 || panel_echo_ids_.isEmpty()) {
            properties_->show_document(svc.metadata(), svc.theme(), static_cast<int>(svc.components().size()),
                                       canvas_->page_count(), svc.library_id(), svc.current_file());
            shown_component_ = rep::ReportComponent{};
        }
        panel_echo_ids_.clear();
        return;
    }
    rep::ReportComponent c;
    if (!svc.component(selected_id_, &c))
        return;
    const bool same = shown_component_.id == c.id;
    const bool changed = !same || c.type != shown_component_.type || c.content != shown_component_.content ||
                         c.config != shown_component_.config;
    if (!force && !changed) {
        panel_echo_ids_.clear();
        return;
    }
    // A change typed in the panel itself: it already shows it — rebuilding would destroy
    // the editor under the caret.
    if (!force && same && panel_echo_ids_.contains(c.id)) {
        shown_component_ = c;
        panel_echo_ids_.clear();
        return;
    }
    properties_->show_component(c);
    shown_component_ = c;
    panel_echo_ids_.clear();
}

void ReportBuilderScreen::on_document_loaded() {
    selected_id_ = 0;
    shown_component_ = rep::ReportComponent{};
    if (preview_btn_->isChecked())
        preview_btn_->setChecked(false);
    clear_notice();
    if (started_)
        show_editor();
    do_refresh();
    if (isVisible()) {
        check_inbox_notice();
        check_stale_notice();
    }
}

// ── Live data ──────────────────────────────────────────────────────────────

void ReportBuilderScreen::refresh_all_data() {
    auto& binder = Binder::instance();
    if (binder.batch_pending() > 0)
        return;
    const int n = binder.refresh_all();
    if (n == 0)
        show_notice(NoticeKind::Info,
                    tr("This report has no live data blocks yet. Select a table, chart, stats or market-data block and "
                       "pick a data source in the Properties panel."),
                    QString(), "warn");
    update_refresh_button();
}

void ReportBuilderScreen::update_refresh_button() {
    if (!refresh_data_btn_)
        return;
    auto& binder = Binder::instance();
    if (binder.batch_pending() > 0) {
        refresh_data_btn_->setText(
            tr("Refreshing %1/%2…").arg(binder.batch_total() - binder.batch_pending()).arg(binder.batch_total()));
        refresh_data_btn_->setEnabled(false);
    } else {
        refresh_data_btn_->setText(tr("Refresh Data"));
        refresh_data_btn_->setEnabled(true);
    }
    refresh_data_btn_->setToolTip(tr("Fetch fresh data for every live block (quotes, statistics, charts, statements, "
                                     "news, portfolio)  (F5)"));
}

// ── Notices ────────────────────────────────────────────────────────────────

void ReportBuilderScreen::show_notice(NoticeKind kind, const QString& text, const QString& action_text, const char* tone) {
    // An unread "new blocks" notice or an error is not pushed out by a softer one.
    if ((notice_kind_ == NoticeKind::Inbox || notice_kind_ == NoticeKind::Error) && kind == NoticeKind::Stale)
        return;
    notice_kind_ = kind;
    notice_lbl_->setText(text);
    notice_lbl_->setProperty("rbTone", QLatin1String(tone));
    notice_lbl_->style()->unpolish(notice_lbl_);
    notice_lbl_->style()->polish(notice_lbl_);
    QString act = action_text;
    notice_action_->setText(act.replace(QLatin1Char('&'), QStringLiteral("&&")));
    notice_action_->setVisible(!action_text.isEmpty());
    notice_bar_->show();
}

void ReportBuilderScreen::clear_notice(NoticeKind only) {
    if (only != NoticeKind::None && notice_kind_ != only)
        return;
    notice_kind_ = NoticeKind::None;
    notice_bar_->hide();
}

void ReportBuilderScreen::on_notice_action() {
    auto& svc = Service::instance();
    switch (notice_kind_) {
    case NoticeKind::Inbox: {
        const QVector<int> ids = svc.unseen_ids();
        show_editor();
        if (!ids.isEmpty())
            select_component(ids.first(), true);
        svc.mark_inbox_seen();
        clear_notice();
        break;
    }
    case NoticeKind::Stale:
        clear_notice();
        refresh_all_data();
        break;
    case NoticeKind::Export:
        if (!notice_path_.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(notice_path_));
        clear_notice();
        break;
    default:
        clear_notice();
        break;
    }
}

void ReportBuilderScreen::check_inbox_notice() {
    auto& svc = Service::instance();
    const QVector<int> ids = svc.unseen_ids();
    if (ids.isEmpty()) {
        clear_notice(NoticeKind::Inbox);
        return;
    }
    QStringList sources = svc.unseen_sources();
    show_notice(NoticeKind::Inbox,
                sources.isEmpty() ? tr("%n new block(s) were added to this report.", nullptr, static_cast<int>(ids.size()))
                                  : tr("%n new block(s) added from %1.", nullptr, static_cast<int>(ids.size()))
                                        .arg(sources.join(QStringLiteral(", "))),
                tr("SHOW"), "ok");
}

void ReportBuilderScreen::check_stale_notice() {
    const auto comps = Service::instance().components();
    const QDateTime now = QDateTime::currentDateTime();
    int stale = 0;
    QDateTime oldest;
    for (const auto& c : comps) {
        if (!rep::is_bound(c) || !rep::is_stale(c, now))
            continue;
        ++stale;
        const QDateTime t = rep::as_of_of(c);
        if (t.isValid() && (!oldest.isValid() || t < oldest))
            oldest = t;
    }
    if (stale == 0) {
        clear_notice(NoticeKind::Stale);
        return;
    }
    show_notice(NoticeKind::Stale,
                oldest.isValid()
                    ? tr("%n live data block(s) are out of date — the oldest data is from %1.", nullptr, stale)
                          .arg(rep::as_of_display(oldest))
                    : tr("%n live data block(s) have not been fetched yet.", nullptr, stale),
                tr("REFRESH DATA"), "warn");
}

} // namespace fincept::screens
