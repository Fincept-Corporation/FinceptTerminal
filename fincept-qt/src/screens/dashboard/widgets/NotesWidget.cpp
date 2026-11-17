#include "screens/dashboard/widgets/NotesWidget.h"

#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "storage/repositories/NotesRepository.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QSpinBox>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {
QString notes_format_relative(const QString& iso) {
    if (iso.isEmpty())
        return QStringLiteral("--");
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return iso.left(10);
    const qint64 secs = dt.secsTo(QDateTime::currentDateTime());
    if (secs < 60)
        return QStringLiteral("now");
    if (secs < 3600)
        return QString::number(secs / 60) + QStringLiteral("m");
    if (secs < 86400)
        return QString::number(secs / 3600) + QStringLiteral("h");
    if (secs < qint64{7} * 86400)
        return QString::number(secs / 86400) + QStringLiteral("d");
    return dt.toString("MMM d");
}

QString priority_color(const QString& p) {
    if (p == QStringLiteral("CRITICAL"))
        return ui::colors::DANGER();
    if (p == QStringLiteral("HIGH"))
        return ui::colors::WARNING();
    if (p == QStringLiteral("MEDIUM"))
        return ui::colors::AMBER();
    return ui::colors::TEXT_TERTIARY(); // LOW or empty
}

QString sentiment_glyph(const QString& s) {
    if (s == QStringLiteral("BULLISH"))
        return QStringLiteral("▲");
    if (s == QStringLiteral("BEARISH"))
        return QStringLiteral("▼");
    if (s == QStringLiteral("NEUTRAL"))
        return QStringLiteral("◆");
    return QString();
}

QString sentiment_color(const QString& s) {
    if (s == QStringLiteral("BULLISH"))
        return ui::colors::POSITIVE();
    if (s == QStringLiteral("BEARISH"))
        return ui::colors::NEGATIVE();
    return ui::colors::TEXT_SECONDARY();
}
} // namespace

NotesWidget::NotesWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("NOTES"), parent, ui::colors::AMBER()) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);

    auto* container = new QWidget(this);
    container->setStyleSheet("background:transparent;");
    list_layout_ = new QVBoxLayout(container);
    list_layout_->setContentsMargins(4, 4, 4, 4);
    list_layout_->setSpacing(3);
    list_layout_->addStretch();
    scroll_->setWidget(container);

    vl->addWidget(scroll_, 1);

    connect(this, &BaseWidget::refresh_requested, this, &NotesWidget::refresh_data);

    set_configurable(true);
    apply_styles();
    NotesWidget::apply_config(cfg); // qualified: no dynamic dispatch inside a constructor
}

QJsonObject NotesWidget::config() const {
    QJsonObject o;
    o.insert("filter", filter_);
    o.insert("max_rows", max_rows_);
    return o;
}

void NotesWidget::apply_config(const QJsonObject& cfg) {
    const QString f = cfg.value("filter").toString("recent");
    filter_ = (f == QStringLiteral("favorites")) ? QStringLiteral("favorites") : QStringLiteral("recent");
    max_rows_ = qBound(3, cfg.value("max_rows").toInt(8), 50);

    set_title(filter_ == QStringLiteral("favorites") ? tr("NOTES — FAVORITES") : tr("NOTES — RECENT"));
    refresh_data();
}

void NotesWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    subscribe_events();
    refresh_data();
}

void NotesWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    unsubscribe_events();
}

void NotesWidget::subscribe_events() {
    if (!event_subs_.isEmpty())
        return;

    QPointer<NotesWidget> self = this;
    auto on_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (self)
                    self->refresh_data();
            },
            Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    event_subs_.append(bus.subscribe(this, "notes.created", on_changed));
    event_subs_.append(bus.subscribe(this, "notes.deleted", on_changed));
    // Favourite / title / priority edits change what the tile shows too.
    event_subs_.append(bus.subscribe(this, "notes.updated", on_changed));
}

void NotesWidget::unsubscribe_events() {
    auto& bus = EventBus::instance();
    for (auto id : event_subs_)
        bus.unsubscribe(id);
    event_subs_.clear();
}

void NotesWidget::clear_rows() {
    while (QLayoutItem* item = list_layout_->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void NotesWidget::refresh_data() {
    clear_rows();

    auto& repo = fincept::NotesRepository::instance();
    auto result = repo.list_all(/*include_archived=*/false);
    if (!result.is_ok()) {
        auto* err = new QLabel(tr("Failed to load notes: %1").arg(QString::fromStdString(result.error())));
        err->setAlignment(Qt::AlignCenter);
        err->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:transparent;padding:16px;").arg(ui::colors::DANGER()));
        list_layout_->addWidget(err);
        list_layout_->addStretch();
        set_loading(false);
        return;
    }

    QVector<fincept::FinancialNote> notes = result.value();

    if (filter_ == QStringLiteral("favorites")) {
        QVector<fincept::FinancialNote> filtered;
        for (const auto& n : notes)
            if (n.is_favorite)
                filtered.append(n);
        notes = filtered;
    }

    // Sort newest-first by updated_at (fallback created_at)
    std::sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
        const QString lhs = a.updated_at.isEmpty() ? a.created_at : a.updated_at;
        const QString rhs = b.updated_at.isEmpty() ? b.created_at : b.updated_at;
        return lhs > rhs;
    });

    if (notes.size() > max_rows_)
        notes.resize(max_rows_);

    if (notes.isEmpty()) {
        const QString msg = (filter_ == QStringLiteral("favorites"))
                                ? tr("No favorite notes")
                                : tr("No notes yet — open Notes screen to add one");
        auto* empty = new QLabel(msg);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:transparent;padding:16px;").arg(ui::colors::TEXT_TERTIARY()));
        list_layout_->addWidget(empty);
        list_layout_->addStretch();
        set_loading(false);
        return;
    }

    for (const auto& note : notes) {
        auto* row = new QWidget(this);
        row->setCursor(Qt::PointingHandCursor);
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:2px;")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* rl = new QVBoxLayout(row);
        rl->setContentsMargins(8, 5, 8, 5);
        rl->setSpacing(2);

        // Top line: title + favorite star + sentiment
        auto* top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(6);

        if (note.is_favorite) {
            auto* star = new QLabel(QStringLiteral("★"));
            star->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;background:transparent;").arg(ui::colors::AMBER()));
            top->addWidget(star);
        }

        QString title_text = note.title.trimmed();
        if (title_text.isEmpty())
            title_text = tr("(untitled)");
        if (title_text.length() > 50)
            title_text = title_text.left(48) + QStringLiteral("…");
        auto* title_lbl = new QLabel(title_text);
        title_lbl->setToolTip(note.title);
        title_lbl->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
        top->addWidget(title_lbl, 1);

        const QString sgly = sentiment_glyph(note.sentiment);
        if (!sgly.isEmpty()) {
            auto* sent = new QLabel(sgly);
            sent->setToolTip(note.sentiment);
            sent->setStyleSheet(
                QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:transparent;").arg(sentiment_color(note.sentiment)));
            top->addWidget(sent);
        }
        rl->addLayout(top);

        // Bottom line: priority dot + category + tickers + age
        auto* bot = new QHBoxLayout;
        bot->setContentsMargins(0, 0, 0, 0);
        bot->setSpacing(6);

        if (!note.priority.isEmpty()) {
            auto* prio = new QLabel(QStringLiteral("●"));
            prio->setToolTip(QStringLiteral("Priority: ") + note.priority);
            prio->setStyleSheet(
                QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(priority_color(note.priority)));
            bot->addWidget(prio);
        }

        if (!note.category.isEmpty()) {
            auto* cat = new QLabel(note.category);
            cat->setStyleSheet(
                QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;background:transparent;").arg(ui::colors::CYAN()));
            bot->addWidget(cat);
        }

        if (!note.tickers.isEmpty()) {
            QString tkr = note.tickers;
            if (tkr.length() > 18)
                tkr = tkr.left(16) + QStringLiteral("…");
            auto* tlbl = new QLabel(tkr);
            tlbl->setToolTip(note.tickers);
            tlbl->setStyleSheet(
                QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
            bot->addWidget(tlbl);
        }

        bot->addStretch(1);

        const QString age = notes_format_relative(note.updated_at.isEmpty() ? note.created_at : note.updated_at);
        auto* age_lbl = new QLabel(age);
        age_lbl->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
        bot->addWidget(age_lbl);

        rl->addLayout(bot);

        // Click → navigate to notes screen. Children are transparent to mouse
        // events so clicks land on the row, where eventFilter catches them.
        const auto child_labels = row->findChildren<QLabel*>();
        for (auto* lbl : child_labels)
            lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        row->installEventFilter(this);
        row->setProperty("note_id", note.id);

        list_layout_->addWidget(row);
    }

    list_layout_->addStretch();
    set_loading(false);
}

bool NotesWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (auto* w = qobject_cast<QWidget*>(obj)) {
                const QVariant id_var = w->property("note_id");
                if (id_var.isValid()) {
                    EventBus::instance().publish("nav.switch_screen", QVariantMap{{"screen_id", "notes"}, {"exclusive", true}});
                    return true;
                }
            }
        }
    }
    return BaseWidget::eventFilter(obj, event);
}

QDialog* NotesWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Notes"));
    auto* form = new QFormLayout(dlg);

    auto* filter_box = new QComboBox(dlg);
    filter_box->addItem(tr("Recent"), "recent");
    filter_box->addItem(tr("Favorites only"), "favorites");
    const int idx = filter_box->findData(filter_);
    if (idx >= 0)
        filter_box->setCurrentIndex(idx);
    form->addRow(tr("Filter"), filter_box);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(3, 50);
    spin->setValue(max_rows_);
    form->addRow(tr("Max rows"), spin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, filter_box, spin]() {
        QJsonObject cfg;
        cfg.insert("filter", filter_box->currentData().toString());
        cfg.insert("max_rows", spin->value());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void NotesWidget::on_theme_changed() {
    apply_styles();
    if (isVisible())
        refresh_data();
}

void NotesWidget::apply_styles() {
    set_css_cached(scroll_, QString("QScrollArea{border:none;background:transparent;}"
                                   "QScrollBar:vertical{width:4px;background:transparent;}"
                                   "QScrollBar::handle:vertical{background:%1;border-radius:2px;min-height:20px;}"
                                   "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                               .arg(ui::colors::BORDER_MED()));
}

void NotesWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("NOTES"));
    refresh_data(); // re-renders rows in the new language
}

// ── DeskNoteWidget ───────────────────────────────────────────────────────────

DeskNoteWidget::DeskNoteWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("NOTE"), parent, ui::colors::AMBER()) {
    auto* vl = content_layout();

    view_ = new QTextBrowser(this);
    view_->setObjectName("deskNoteView");
    view_->setOpenLinks(false);
    view_->setFrameShape(QFrame::NoFrame);
    view_->setToolTip(tr("Double-click to edit"));
    view_->viewport()->installEventFilter(this);
    vl->addWidget(view_, 1);

    editor_ = new QPlainTextEdit(this);
    editor_->setObjectName("deskNoteEditor");
    editor_->setFrameShape(QFrame::NoFrame);
    editor_->setTabChangesFocus(true);
    editor_->setAccessibleName(tr("Note text (Markdown)"));
    editor_->installEventFilter(this); // Ctrl+Enter saves, Escape cancels
    editor_->hide();
    vl->addWidget(editor_, 1);

    auto* bar = new QWidget(this);
    bar->setObjectName("deskNoteBar");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 2, 6, 3);
    hl->setSpacing(4);
    status_ = new QLabel(bar);
    status_->setObjectName("deskNoteStatus");
    status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    hl->addWidget(status_, 1);
    const auto make_btn = [bar, hl](const QString& text) {
        auto* b = new QPushButton(text, bar);
        b->setObjectName("deskNoteBtn");
        b->setCursor(Qt::PointingHandCursor);
        hl->addWidget(b);
        return b;
    };
    restore_btn_ = make_btn(tr("RESTORE"));
    restore_btn_->setToolTip(tr("Restore the template's original note (asks first)"));
    edit_btn_ = make_btn(tr("EDIT"));
    edit_btn_->setToolTip(tr("Edit the note (Markdown) — double-click the text works too"));
    cancel_btn_ = make_btn(tr("CANCEL"));
    save_btn_ = make_btn(tr("SAVE"));
    vl->addWidget(bar);

    connect(edit_btn_, &QPushButton::clicked, this, &DeskNoteWidget::begin_edit);
    connect(save_btn_, &QPushButton::clicked, this, [this]() { finish_edit(true); });
    connect(cancel_btn_, &QPushButton::clicked, this, [this]() { finish_edit(false); });
    connect(restore_btn_, &QPushButton::clicked, this, &DeskNoteWidget::restore_seed);

    set_refreshable(false); // nothing to fetch
    apply_styles();
    DeskNoteWidget::apply_config(cfg); // qualified: no dynamic dispatch inside a constructor
}

QString DeskNoteWidget::shown_text() const {
    if (edited_)
        return text_;
    return seed_.isEmpty() ? QString() : template_note_seed(seed_);
}

QJsonObject DeskNoteWidget::config() const {
    QJsonObject o;
    if (!seed_.isEmpty())
        o.insert("seed", seed_);
    if (edited_)
        o.insert("text", text_);
    return o;
}

void DeskNoteWidget::apply_config(const QJsonObject& cfg) {
    seed_ = cfg.value("seed").toString().trimmed();
    edited_ = cfg.contains(QStringLiteral("text"));
    text_ = edited_ ? cfg.value("text").toString().left(64 * 1024) : QString();
    if (editing_) {
        editing_ = false;
        editor_->hide();
        view_->show();
    }
    update_title();
    render();
}

void DeskNoteWidget::update_title() {
    const QString name = seed_.isEmpty() ? QString() : template_display_name_for(seed_);
    set_title(name.isEmpty() ? tr("NOTE") : tr("DESK NOTE · %1").arg(name.toUpper()));
}

void DeskNoteWidget::render() {
    const QString md = shown_text();
    if (md.trimmed().isEmpty())
        view_->setMarkdown(tr("*Empty note — double-click or press EDIT to write one. It is saved with this "
                              "dashboard layout.*"));
    else
        view_->setMarkdown(md);
    // Tight, terminal-style spacing: Markdown's default paragraph margins and
    // 40 px list indents made a one-screen note scroll.
    auto* doc = view_->document();
    // Room for "10." list numbers, which are drawn inside the indent.
    doc->setIndentWidth(QFontMetrics(view_->font()).horizontalAdvance(QStringLiteral("00. ")) + 4);
    doc->setDocumentMargin(2);
    QTextCursor cur(doc);
    cur.beginEditBlock();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        QTextBlockFormat f = b.blockFormat();
        f.setTopMargin(b.textList() ? 0 : 3);
        f.setBottomMargin(b.textList() ? 1 : 2);
        QTextCursor bc(b);
        bc.setBlockFormat(f);
    }
    cur.endEditBlock();

    edit_btn_->setVisible(!editing_);
    save_btn_->setVisible(editing_);
    cancel_btn_->setVisible(editing_);
    restore_btn_->setVisible(!editing_ && !seed_.isEmpty() && edited_);
    if (editing_)
        status_->setText(tr("Editing · Ctrl+Enter saves, Esc cancels · Markdown: **bold**, - lists"));
    else if (seed_.isEmpty())
        status_->setText(tr("Saved with this layout"));
    else if (edited_)
        status_->setText(tr("Your text · kept when the template is applied again"));
    else
        status_->setText(tr("Template note · edit it, your text is kept"));
    status_->setToolTip(status_->text());
}

void DeskNoteWidget::begin_edit() {
    if (editing_)
        return;
    editing_ = true;
    editor_->setPlainText(shown_text());
    view_->hide();
    editor_->show();
    editor_->setFocus();
    render();
}

void DeskNoteWidget::finish_edit(bool save) {
    if (!editing_)
        return;
    editing_ = false;
    if (save) {
        const QString t = editor_->toPlainText().left(64 * 1024);
        const QString seed_text = seed_.isEmpty() ? QString() : template_note_seed(seed_);
        // Saving the seed unchanged keeps the note "unedited" (it then follows
        // the seed of later versions).
        if (!seed_.isEmpty() && t.trimmed() == seed_text.trimmed()) {
            edited_ = false;
            text_.clear();
        } else {
            edited_ = true;
            text_ = t;
        }
        if (!seed_.isEmpty()) {
            if (edited_)
                template_note_store(seed_, text_);
            else
                template_note_forget(seed_);
        }
    }
    editor_->hide();
    view_->show();
    render();
    if (save)
        emit config_changed(config());
}

void DeskNoteWidget::restore_seed() {
    if (seed_.isEmpty() || !edited_)
        return;
    confirm_async(QMessageBox::Question, tr("Restore template note"),
                  tr("Replace your text in this note with the template's original note?\n\n"
                     "Your edits are discarded, here and for the next time this template is applied."),
                  [this]() {
                      edited_ = false;
                      text_.clear();
                      template_note_forget(seed_);
                      render();
                      emit config_changed(config());
                  });
}

bool DeskNoteWidget::eventFilter(QObject* obj, QEvent* event) {
    if (view_ && obj == view_->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        begin_edit();
        return true;
    }
    // While editing: Ctrl+Enter saves, Escape cancels.
    if (editing_ && obj == editor_ && event->type() == QEvent::KeyPress) {
        const auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && (ke->modifiers() & Qt::ControlModifier)) {
            finish_edit(true);
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            finish_edit(false);
            return true;
        }
    }
    return BaseWidget::eventFilter(obj, event);
}

void DeskNoteWidget::apply_styles() {
    const QString fg = ui::colors::TEXT_PRIMARY();
    set_css_cached(view_, QString("QTextBrowser#deskNoteView{background:transparent;border:none;color:%1;"
                                 "font-size:%2px;padding:2px 6px;selection-background-color:%3;}"
                                 "QScrollBar:vertical{width:5px;background:transparent;}"
                                 "QScrollBar::handle:vertical{background:%4;border-radius:2px;min-height:20px;}"
                                 "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                             .arg(fg)
                             .arg(ui::fonts::px(-3))
                             .arg(ui::colors::ACCENT_BG(), ui::colors::BORDER_MED()));
    set_css_cached(editor_, QString("QPlainTextEdit#deskNoteEditor{background:%1;border:1px solid %2;color:%3;"
                                   "font-size:%4px;padding:4px 6px;}")
                               .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), fg)
                               .arg(ui::fonts::px(-2)));
    if (auto* bar = findChild<QWidget*>(QStringLiteral("deskNoteBar")))
        set_css_cached(bar, QString("QWidget#deskNoteBar{background:transparent;border-top:1px solid %1;}"
                                   "QLabel#deskNoteStatus{color:%2;font-size:%3px;background:transparent;}"
                                   "QPushButton#deskNoteBtn{background:%4;color:%5;border:1px solid %1;"
                                   "padding:1px 8px;font-size:%3px;font-weight:bold;}"
                                   "QPushButton#deskNoteBtn:hover{color:%6;border-color:%6;}")
                               .arg(ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4))
                               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    // Markdown headings and links in the theme's accent.
    view_->document()->setDefaultStyleSheet(
        QString("a{color:%1;} h1,h2,h3,h4{color:%1;}").arg(ui::colors::AMBER()));
}

void DeskNoteWidget::on_theme_changed() {
    apply_styles();
    render();
}

void DeskNoteWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    edit_btn_->setText(tr("EDIT"));
    save_btn_->setText(tr("SAVE"));
    cancel_btn_->setText(tr("CANCEL"));
    restore_btn_->setText(tr("RESTORE"));
    update_title();
    if (!editing_)
        render(); // an unedited note shows the seed in the new language
}

} // namespace fincept::screens::widgets
