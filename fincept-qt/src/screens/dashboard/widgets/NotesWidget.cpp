#include "screens/dashboard/widgets/NotesWidget.h"

#include "storage/repositories/NotesRepository.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
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
QString format_relative(const QString& iso) {
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
    if (secs < 7 * 86400)
        return QString::number(secs / 86400) + QStringLiteral("d");
    return dt.toString("MMM d");
}

QString priority_color(const QString& p) {
    if (p == QStringLiteral("CRITICAL")) return ui::colors::NEGATIVE();
    if (p == QStringLiteral("HIGH"))     return ui::colors::WARNING();
    if (p == QStringLiteral("MEDIUM"))   return ui::colors::AMBER();
    return ui::colors::TEXT_TERTIARY(); // LOW or empty
}

QString sentiment_glyph(const QString& s) {
    if (s == QStringLiteral("BULLISH")) return QStringLiteral("▲");
    if (s == QStringLiteral("BEARISH")) return QStringLiteral("▼");
    if (s == QStringLiteral("NEUTRAL")) return QStringLiteral("◆");
    return QString();
}

QString sentiment_color(const QString& s) {
    if (s == QStringLiteral("BULLISH")) return ui::colors::POSITIVE();
    if (s == QStringLiteral("BEARISH")) return ui::colors::NEGATIVE();
    return ui::colors::TEXT_SECONDARY();
}
} // namespace

NotesWidget::NotesWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("NOTES", parent, ui::colors::AMBER()) {
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
    apply_config(cfg);
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

    set_title(filter_ == QStringLiteral("favorites") ? QStringLiteral("NOTES — FAVORITES")
                                                     : QStringLiteral("NOTES — RECENT"));
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
            self.data(), [self]() {
                if (self)
                    self->refresh_data();
            },
            Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    event_subs_.append(bus.subscribe("notes.created", on_changed));
    event_subs_.append(bus.subscribe("notes.deleted", on_changed));
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
        auto* err = new QLabel(QString("Failed to load notes: %1").arg(QString::fromStdString(result.error())));
        err->setAlignment(Qt::AlignCenter);
        err->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;padding:16px;")
                               .arg(ui::colors::NEGATIVE()));
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
                                ? QStringLiteral("No favorite notes")
                                : QStringLiteral("No notes yet — open Notes screen to add one");
        auto* empty = new QLabel(msg);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;padding:16px;")
                                 .arg(ui::colors::TEXT_TERTIARY()));
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
            star->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;")
                                    .arg(ui::colors::AMBER()));
            top->addWidget(star);
        }

        QString title_text = note.title.trimmed();
        if (title_text.isEmpty())
            title_text = QStringLiteral("(untitled)");
        if (title_text.length() > 50)
            title_text = title_text.left(48) + QStringLiteral("…");
        auto* title_lbl = new QLabel(title_text);
        title_lbl->setToolTip(note.title);
        title_lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY()));
        top->addWidget(title_lbl, 1);

        const QString sgly = sentiment_glyph(note.sentiment);
        if (!sgly.isEmpty()) {
            auto* sent = new QLabel(sgly);
            sent->setToolTip(note.sentiment);
            sent->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;")
                                    .arg(sentiment_color(note.sentiment)));
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
            prio->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                    .arg(priority_color(note.priority)));
            bot->addWidget(prio);
        }

        if (!note.category.isEmpty()) {
            auto* cat = new QLabel(note.category);
            cat->setStyleSheet(QString("color:%1;font-size:9px;font-weight:600;background:transparent;")
                                   .arg(ui::colors::CYAN()));
            bot->addWidget(cat);
        }

        if (!note.tickers.isEmpty()) {
            QString tkr = note.tickers;
            if (tkr.length() > 18)
                tkr = tkr.left(16) + QStringLiteral("…");
            auto* tlbl = new QLabel(tkr);
            tlbl->setToolTip(note.tickers);
            tlbl->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
            bot->addWidget(tlbl);
        }

        bot->addStretch(1);

        const QString age = format_relative(note.updated_at.isEmpty() ? note.created_at : note.updated_at);
        auto* age_lbl = new QLabel(age);
        age_lbl->setStyleSheet(QString("color:%1;font-size:9px;background:transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
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
                    EventBus::instance().publish("nav.switch_screen",
                                                 QVariantMap{{"screen_id", "notes"}});
                    return true;
                }
            }
        }
    }
    return BaseWidget::eventFilter(obj, event);
}

QDialog* NotesWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Notes");
    auto* form = new QFormLayout(dlg);

    auto* filter_box = new QComboBox(dlg);
    filter_box->addItem("Recent", "recent");
    filter_box->addItem("Favorites only", "favorites");
    const int idx = filter_box->findData(filter_);
    if (idx >= 0)
        filter_box->setCurrentIndex(idx);
    form->addRow("Filter", filter_box);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(3, 50);
    spin->setValue(max_rows_);
    form->addRow("Max rows", spin);

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
    scroll_->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;}"
                "QScrollBar:vertical{width:4px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%1;border-radius:2px;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BORDER_MED()));
}

} // namespace fincept::screens::widgets
