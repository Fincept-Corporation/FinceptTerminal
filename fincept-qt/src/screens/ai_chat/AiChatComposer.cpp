// AiChatComposer.cpp — see AiChatComposer.h.
//
//   AicComposerEdit   the editor: Enter / Shift+Enter / Esc / Ctrl+R, "/" and "@" triggers,
//                     drag-and-drop and paste of files (metadata only — contents are read by
//                     the host when the message is sent)
//   AicComposer       chips, the editor (1 → 8 lines), hint line, token meter, ATTACH, SEND / STOP
//
// The "@" picker is a frameless Tool window, not a Qt::Popup: ui::SymbolSearchBox shows its
// result list in a window of its own, and a click there would close a popup first.

#include "screens/ai_chat/AiChatComposer.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/config/AppConfig.h"
#include "core/instrument/Instrument.h"
#include "screens/ai_chat/AiChatStyle.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QStyle>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>

namespace fincept::screens::aichat {

namespace {

constexpr int kCpMaxLines = 8;

/// The hint line: one line that elides (never wraps, never clips); full text in the tooltip.
class AicComposerElide : public QLabel {
  public:
    explicit AicComposerElide(QWidget* parent) : QLabel(parent) {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    QSize minimumSizeHint() const override { return QSize(0, QLabel::minimumSizeHint().height()); }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect r = contentsRect();
        p.setFont(font());
        p.setPen(palette().color(foregroundRole()));
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, r.width()));
    }
};

/// The "@" picker window: frameless, closes on Esc (and when the focus leaves it).
class AicMentionPopup : public QFrame {
  public:
    explicit AicMentionPopup(QWidget* parent) : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint) {}
    std::function<void()> on_escape;

  protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) {
            e->accept();
            if (on_escape)
                on_escape();
            else
                close();
            return;
        }
        QFrame::keyPressEvent(e);
    }
};

/// `w` is `root` or below it — across windows (menus and lists opened from inside count).
bool cp_inside(const QWidget* root, const QWidget* w) {
    for (; w; w = w->parentWidget())
        if (w == root)
            return true;
    return false;
}

bool cp_is_image(const QString& suffix) {
    static const QStringList kImages = {
        QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("gif"),
        QStringLiteral("bmp"),  QStringLiteral("webp"), QStringLiteral("tif"),  QStringLiteral("tiff"),
        QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("ico"),  QStringLiteral("svg"),
        QStringLiteral("avif")};
    return kImages.contains(suffix.toLower());
}

QString cp_glyph(Attachment::Kind kind) {
    switch (kind) {
        case Attachment::Kind::Symbol:
            return QStringLiteral("$");
        case Attachment::Kind::Portfolio:
            return QStringLiteral("▦");
        case Attachment::Kind::Watchlist:
            return QStringLiteral("≡");
        case Attachment::Kind::Image:
            return QStringLiteral("▣");
        case Attachment::Kind::File:
            break;
    }
    return QStringLiteral("▤");
}

QString cp_size(qint64 bytes) {
    const QLocale loc;
    if (bytes < 1024)
        return AicComposer::tr("%1 B").arg(loc.toString(bytes));
    if (bytes < 1024 * 1024)
        return AicComposer::tr("%1 KB").arg(loc.toString(double(bytes) / 1024.0, 'f', 0));
    return AicComposer::tr("%1 MB").arg(loc.toString(double(bytes) / (1024.0 * 1024.0), 'f', 1));
}

QString cp_short(const QString& text, int max_chars) {
    return text.size() > max_chars ? text.left(max_chars - 1) + QStringLiteral("…") : text;
}

/// Outside text in a tooltip: escaped rich text (markup in a name is never interpreted).
QString cp_tip(const QString& plain) {
    QString esc = plain.toHtmlEscaped();
    esc.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return QStringLiteral("<p>") + esc + QStringLiteral("</p>");
}

QString cp_preview(const QString& text) {
    constexpr int kMax = 400;
    return text.size() > kMax ? text.left(kMax).trimmed() + QStringLiteral("…") : text;
}

/// A menu of the user's portfolios or watchlists under `anchor` (small reads, only when asked).
void cp_reference_menu(QWidget* anchor, bool portfolios, const std::function<void(const Attachment&)>& chosen) {
    auto* menu = new QMenu(anchor);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(style::menu_sheet());
    auto add_ref = [menu, chosen](Attachment::Kind kind, const QString& name, const QString& id) {
        Attachment a;
        a.kind = kind;
        a.name = name;
        a.ref = id;
        QObject::connect(menu->addAction(ui::literal_ampersands(name)), &QAction::triggered, menu,
                         [chosen, a]() { chosen(a); });
    };
    if (portfolios) {
        const auto r = PortfolioRepository::instance().list_portfolios();
        if (r.is_err())
            menu->addAction(AicComposer::tr("Could not read the portfolios"))->setEnabled(false);
        else if (r.value().isEmpty())
            menu->addAction(AicComposer::tr("No portfolios yet"))->setEnabled(false);
        else
            for (const auto& pf : r.value())
                add_ref(Attachment::Kind::Portfolio, pf.name, pf.id);
    } else {
        const auto r = WatchlistRepository::instance().list_all();
        if (r.is_err())
            menu->addAction(AicComposer::tr("Could not read the watchlists"))->setEnabled(false);
        else if (r.value().isEmpty())
            menu->addAction(AicComposer::tr("No watchlists yet"))->setEnabled(false);
        else
            for (const auto& wl : r.value())
                add_ref(Attachment::Kind::Watchlist, wl.name, wl.id);
    }
    menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

} // namespace

// ── Editor ───────────────────────────────────────────────────────────────────

class AicComposerEdit : public QPlainTextEdit {
  public:
    explicit AicComposerEdit(AicComposer* owner) : QPlainTextEdit(owner), owner_(owner) {}

  protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::ShortcutOverride) {
            // These keys belong to the composer, not to a window shortcut.
            const auto* ke = static_cast<QKeyEvent*>(e);
            const Qt::KeyboardModifiers mods = ke->modifiers() & ~Qt::KeypadModifier;
            const int key = ke->key();
            const bool ours =
                ((key == Qt::Key_Return || key == Qt::Key_Enter) &&
                 (mods == Qt::NoModifier || mods == Qt::ShiftModifier || mods == Qt::ControlModifier)) ||
                (key == Qt::Key_R && mods == Qt::ControlModifier) ||
                (key == Qt::Key_Escape && mods == Qt::NoModifier && owner_->busy_);
            if (ours) {
                e->accept();
                return true;
            }
        }
        return QPlainTextEdit::event(e);
    }

    void keyPressEvent(QKeyEvent* e) override {
        const int key = e->key();
        const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (mods == Qt::ShiftModifier) {
                insertPlainText(QStringLiteral("\n"));
                e->accept();
                return;
            }
            if (mods == Qt::NoModifier || mods == Qt::ControlModifier) {
                e->accept();
                // Enter (or Ctrl+Enter) sends; it never stops a running answer (Esc and STOP do).
                if (!owner_->busy_ && owner_->configured_)
                    emit owner_->send_requested();
                return;
            }
        }
        if (key == Qt::Key_Escape && mods == Qt::NoModifier && owner_->busy_) {
            e->accept();
            if (!owner_->stopping_)
                emit owner_->stop_requested();
            return;
        }
        if (key == Qt::Key_R && mods == Qt::ControlModifier) {
            e->accept();
            if (!owner_->busy_)
                emit owner_->regenerate_requested();
            return;
        }

        QPlainTextEdit::keyPressEvent(e);

        // "/" or "@" typed at the start or after white space opens its menu.
        const QString typed = e->text();
        if (typed.size() != 1 || (typed.at(0) != QLatin1Char('/') && typed.at(0) != QLatin1Char('@')))
            return;
        const int pos = textCursor().position() - 1;
        if (pos < 0 || document()->characterAt(pos) != typed.at(0))
            return;
        if (pos > 0 && !document()->characterAt(pos - 1).isSpace())
            return; // "and/or", "name@host": plain text
        owner_->trigger_pos_ = pos;
        const bool slash = typed.at(0) == QLatin1Char('/');
        QPointer<AicComposer> o(owner_);
        QTimer::singleShot(0, owner_, [o, slash]() {
            if (!o)
                return;
            if (slash)
                o->open_prompt_menu();
            else
                o->open_mention_menu();
        });
    }

    bool canInsertFromMimeData(const QMimeData* source) const override {
        return source->hasUrls() || source->hasImage() || QPlainTextEdit::canInsertFromMimeData(source);
    }

    void insertFromMimeData(const QMimeData* source) override {
        if (source->hasUrls()) {
            QStringList files;
            bool local = false;
            for (const QUrl& url : source->urls()) {
                if (!url.isLocalFile())
                    continue;
                local = true;
                const QFileInfo fi(url.toLocalFile()); // metadata only
                if (!fi.isFile())
                    continue; // folders are not attached
                if (cp_is_image(fi.suffix()))
                    emit owner_->image_rejected(fi.fileName());
                else
                    files.append(fi.absoluteFilePath());
            }
            if (local) {
                if (!files.isEmpty())
                    emit owner_->files_dropped(files);
                return;
            }
        }
        if (!source->hasText() && source->hasImage()) {
            emit owner_->image_rejected(AicComposer::tr("pasted image"));
            return;
        }
        if (!source->hasText() && source->hasUrls()) {
            // Web links: their addresses, as text.
            QStringList links;
            for (const QUrl& url : source->urls())
                links.append(url.toString());
            insertPlainText(links.join(QLatin1Char(' ')));
            return;
        }
        QPlainTextEdit::insertFromMimeData(source);
    }

    void focusInEvent(QFocusEvent* e) override {
        QPlainTextEdit::focusInEvent(e);
        owner_->set_focus_frame(true);
    }

    void focusOutEvent(QFocusEvent* e) override {
        QPlainTextEdit::focusOutEvent(e);
        owner_->set_focus_frame(false);
    }

    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        owner_->fit_height(); // a new width re-wraps the text
    }

    void changeEvent(QEvent* e) override {
        QPlainTextEdit::changeEvent(e);
        if (e->type() == QEvent::StyleChange || e->type() == QEvent::FontChange) {
            QPointer<AicComposer> o(owner_);
            QTimer::singleShot(0, owner_, [o]() {
                if (o)
                    o->fit_height();
            });
        }
    }

  private:
    AicComposer* owner_;
};

// ── Composer ─────────────────────────────────────────────────────────────────

AicComposer::AicComposer(QWidget* parent) : QFrame(parent) {
    setObjectName(style::kComposer);
    setFrameShape(QFrame::NoFrame);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(style::u(3), style::u(2), style::u(3), style::u(2));
    vl->setSpacing(style::u(2));

    chips_ = new QWidget(this);
    new ui::FlowLayout(chips_, style::u(1), style::u(1));
    chips_->hide();
    vl->addWidget(chips_);

    edit_ = new AicComposerEdit(this);
    edit_->setObjectName(style::kComposerEdit);
    edit_->setTabChangesFocus(true);
    edit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    edit_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Enter sends a billed request; Ctrl+R (keyPressEvent) regenerates one.
    edit_->setProperty(core::agent::kAgentRiskProperty, QStringLiteral("spend"));
    edit_->setProperty(core::agent::kAgentRiskKeysProperty, QStringLiteral("Ctrl+R=spend"));
    vl->addWidget(edit_);
    connect(edit_, &QPlainTextEdit::textChanged, this, [this]() {
        fit_height();
        update_meter();
        emit text_changed();
    });
    if (QAbstractTextDocumentLayout* lay = edit_->document()->documentLayout())
        connect(lay, &QAbstractTextDocumentLayout::documentSizeChanged, this, [this]() { fit_height(); });

    auto* bottom = new QHBoxLayout;
    bottom->setContentsMargins(0, 0, 0, 0);
    bottom->setSpacing(style::u(2));
    hint_ = new AicComposerElide(this);
    hint_->setObjectName(style::kHint);
    meter_ = new QLabel(this);
    meter_->setObjectName(style::kHint);
    meter_->setTextFormat(Qt::PlainText);
    attach_btn_ = new QPushButton(this);
    attach_btn_->setObjectName(style::kBtn);
    attach_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_ = new QPushButton(this);
    send_btn_->setObjectName(style::kBtnPrimary);
    send_btn_->setCursor(Qt::PointingHandCursor);
    effort_btn_ = new QPushButton(this);
    effort_btn_->setObjectName(style::kBtn);
    effort_btn_->setCursor(Qt::PointingHandCursor);
    {
        const QString saved = AppConfig::instance().get(QStringLiteral("ai_chat/effort")).toString().toLower();
        static const QStringList kLevels = {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"),
                                            QStringLiteral("xhigh"), QStringLiteral("max")};
        if (kLevels.contains(saved))
            effort_ = saved;
    }
    connect(effort_btn_, &QPushButton::clicked, this, [this]() { open_effort_menu(); });
    bottom->addWidget(hint_, 1);
    bottom->addWidget(meter_);
    bottom->addWidget(effort_btn_);
    bottom->addWidget(attach_btn_);
    bottom->addWidget(send_btn_);
    vl->addLayout(bottom);

    connect(attach_btn_, &QPushButton::clicked, this, &AicComposer::attach_file_requested);
    connect(send_btn_, &QPushButton::clicked, this, [this]() {
        if (busy_) {
            if (!stopping_)
                emit stop_requested();
            return;
        }
        if (configured_)
            emit send_requested();
    });

    setTabOrder(edit_, attach_btn_);
    setTabOrder(attach_btn_, send_btn_);

    retranslate();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_metrics(); });
}

AicComposer::~AicComposer() = default;

QPlainTextEdit* AicComposer::editor() const {
    return edit_;
}

QString AicComposer::text() const {
    return edit_->toPlainText();
}

void AicComposer::set_text(const QString& text) {
    edit_->setPlainText(text);
    QTextCursor c = edit_->textCursor();
    c.movePosition(QTextCursor::End);
    edit_->setTextCursor(c);
    edit_->ensureCursorVisible();
}

void AicComposer::insert_text(const QString& text) {
    edit_->insertPlainText(text);
    edit_->ensureCursorVisible();
}

void AicComposer::clear_text() {
    edit_->clear();
}

void AicComposer::focus_editor() {
    edit_->setFocus(Qt::OtherFocusReason);
}

void AicComposer::set_busy(bool busy) {
    busy_ = busy;
    if (!busy)
        stopping_ = false;
    update_send_button();
}

void AicComposer::set_stopping() {
    if (!busy_)
        return;
    stopping_ = true;
    update_send_button();
}

void AicComposer::set_configured(bool configured, const QString& reason) {
    configured_ = configured;
    setup_reason_ = configured ? QString() : reason;
    update_hint();
    update_send_button();
}

void AicComposer::set_chips(const QVector<Attachment>& context, const QVector<Attachment>& files) {
    if (chip_context_ == context && chip_files_ == files)
        return;
    chip_context_ = context;
    chip_files_ = files;
    rebuild_chips();
}

void AicComposer::set_reply_cap(int tokens) {
    reply_cap_ = std::max(0, tokens);
    update_meter();
}

void AicComposer::set_attachment_tokens(int tokens) {
    attachment_tokens_ = std::max(0, tokens);
    update_meter();
}

void AicComposer::set_saved_prompts(const QVector<PromptTemplate>& prompts) {
    saved_prompts_ = prompts;
}

void AicComposer::set_symbol_hint(const QString& symbol) {
    symbol_hint_ = symbol.trimmed();
}

void AicComposer::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QFrame::changeEvent(e);
}

// ── private ──────────────────────────────────────────────────────────────────

void AicComposer::retranslate() {
    edit_->setPlaceholderText(tr("Ask about markets, a company, your portfolio or macro data…"));
    edit_->setAccessibleName(tr("Message Fincept AI"));
    edit_->setAccessibleDescription(
        tr("Enter sends · Shift+Enter starts a new line · / opens saved prompts and research templates · @ mentions a "
           "symbol, a portfolio, a watchlist or a file · Esc stops a streaming answer · Ctrl+R regenerates the last "
           "answer"));
    attach_btn_->setText(tr("ATTACH"));
    attach_btn_->setToolTip(tr("Attach files to this message — their contents are read only when you send"));
    attach_btn_->setAccessibleName(tr("Attach file"));
    update_effort_button();
    update_hint();
    update_meter();
    update_send_button();
    fit_metrics(); // label widths changed; chips carry translated words
}

void AicComposer::update_effort_button() {
    if (!effort_btn_)
        return;
    const bool costly = effort_ == QLatin1String("high") || effort_ == QLatin1String("xhigh") ||
                        effort_ == QLatin1String("max");
    effort_btn_->setText(tr("EFFORT: %1").arg(effort_.toUpper()) + (costly ? QStringLiteral(" +") : QString()));
    effort_btn_->setToolTip(
        costly ? tr("Reasoning effort: %1. The model thinks longer before it answers. On the Fincept model each "
                    "step of an answer costs extra credits at this level.")
                     .arg(effort_.toUpper())
               : tr("Reasoning effort: %1. Raise it for hard, multi-step questions (HIGH and above cost extra "
                    "credits on the Fincept model).")
                     .arg(effort_.toUpper()));
    effort_btn_->setAccessibleName(tr("Reasoning effort"));
}

void AicComposer::open_effort_menu() {
    struct Level {
        const char* id;
        QString label;
    };
    const Level levels[] = {
        {"low", tr("LOW  \u00B7  fastest, short answers")},
        {"medium", tr("MEDIUM  \u00B7  default")},
        {"high", tr("HIGH  \u00B7  deeper analysis  \u00B7  extra credits")},
        {"xhigh", tr("XHIGH  \u00B7  long multi-step work  \u00B7  extra credits")},
        {"max", tr("MAX  \u00B7  hardest problems, slowest  \u00B7  extra credits")},
    };
    QMenu menu(this);
    for (const Level& level : levels) {
        QAction* a = menu.addAction(level.label);
        a->setCheckable(true);
        a->setChecked(effort_ == QLatin1String(level.id));
        a->setData(QString::fromLatin1(level.id));
    }
    const QAction* chosen = menu.exec(effort_btn_->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu.sizeHint().height()));
    if (!chosen)
        return;
    effort_ = chosen->data().toString();
    AppConfig::instance().set(QStringLiteral("ai_chat/effort"), effort_);
    update_effort_button();
}

void AicComposer::fit_metrics() {
    const int bh = style::button_h();
    attach_btn_->setFixedHeight(bh);
    effort_btn_->setFixedHeight(bh);
    send_btn_->setFixedHeight(bh);
    meter_->setFont(style::mono_font(-3));
    // SEND / STOP / STOPPING… share one width, so the line never jumps when the label changes.
    const QFontMetrics fm(style::ui_font(-3, QFont::Bold)); // the sheet's button font
    int w = 0;
    for (const QString& t : {tr("SEND"), tr("STOP"), tr("STOPPING…")})
        w = std::max(w, fm.horizontalAdvance(t) + int(t.size()));
    send_btn_->setMinimumWidth(w + 2 * style::u(3) + 2);
    rebuild_chips();
    fit_height();
}

void AicComposer::fit_height() {
    if (!edit_)
        return;
    const QFontMetrics fm(edit_->font());
    int lines = 1;
    if (const QAbstractTextDocumentLayout* lay = edit_->document()->documentLayout())
        lines = std::max(1, int(std::ceil(lay->documentSize().height()))); // plain-text layout: wrapped lines
    const bool overflow = lines > kCpMaxLines;
    lines = std::clamp(lines, 1, kCpMaxLines);
    // Frame, border and the sheet's padding, as laid out now (before the first layout: an estimate).
    const int vp = edit_->viewport()->height();
    const int chrome = vp > 0 ? edit_->height() - vp : 2 * edit_->frameWidth() + style::u(2) + 2;
    const int doc_margin = int(std::ceil(edit_->document()->documentMargin()));
    const int h = lines * fm.lineSpacing() + 2 * doc_margin + std::max(0, chrome);
    if (edit_->minimumHeight() != h || edit_->maximumHeight() != h)
        edit_->setFixedHeight(h);
    const Qt::ScrollBarPolicy policy = overflow ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    if (edit_->verticalScrollBarPolicy() != policy)
        edit_->setVerticalScrollBarPolicy(policy);
}

void AicComposer::update_hint() {
    if (!hint_)
        return;
    if (!configured_) {
        const QString reason =
            setup_reason_.isEmpty() ? tr("No AI model is set up — Settings > LLM Config") : setup_reason_;
        hint_->setText(reason);
        hint_->setToolTip(cp_tip(reason));
        return;
    }
    hint_->setText(tr("ENTER send · SHIFT+ENTER new line · / prompts · @ symbol or file · ESC stop"));
    hint_->setToolTip(tr("Enter sends · Shift+Enter starts a new line · / opens prompts · @ mentions a symbol, "
                         "portfolio, watchlist or file · Esc stops a streaming answer · Ctrl+R regenerates"));
}

void AicComposer::update_meter() {
    if (!meter_)
        return;
    const QLocale loc;
    const int n = estimate_tokens(edit_->toPlainText()) + std::max(0, attachment_tokens_);
    QString text = tr("≈ %1 tok").arg(format_count(n));
    QString tip = tr("About %1 tokens: your text (about 4 characters per token) and the attached files")
                      .arg(loc.toString(n));
    if (reply_cap_ > 0) {
        text += QStringLiteral(" · ") + tr("reply ≤ %1").arg(loc.toString(reply_cap_));
        tip += QLatin1Char('\n') + tr("The answer is limited to %1 tokens").arg(loc.toString(reply_cap_));
    }
    if (meter_->text() != text)
        meter_->setText(text);
    meter_->setToolTip(tip);
    meter_->setAccessibleName(tip);
}

void AicComposer::update_send_button() {
    if (!send_btn_)
        return;
    const char* name = busy_ ? style::kBtnDanger : style::kBtnPrimary; // SEND ↔ STOP look
    if (send_btn_->objectName() != QLatin1String(name)) {
        send_btn_->setObjectName(name);
        send_btn_->style()->unpolish(send_btn_);
        send_btn_->style()->polish(send_btn_);
        send_btn_->update();
    }
    if (busy_) {
        send_btn_->setText(stopping_ ? tr("STOPPING…") : tr("STOP"));
        send_btn_->setEnabled(!stopping_);
        send_btn_->setToolTip(stopping_ ? tr("Stopping the answer…") : tr("Stop the answer (Esc)"));
        send_btn_->setAccessibleName(tr("Stop the answer"));
        // Stopping spends nothing: no marker (removed, never "").
        if (send_btn_->property(core::agent::kAgentRiskProperty).isValid())
            send_btn_->setProperty(core::agent::kAgentRiskProperty, QVariant());
        return;
    }
    send_btn_->setText(tr("SEND"));
    send_btn_->setEnabled(configured_);
    send_btn_->setToolTip(configured_ ? tr("Send (Enter)")
                                      : (setup_reason_.isEmpty() ? tr("Set up an AI model first — Settings > LLM Config")
                                                                 : setup_reason_));
    send_btn_->setAccessibleName(tr("Send message"));
    send_btn_->setProperty(core::agent::kAgentRiskProperty, QStringLiteral("spend")); // a billed LLM request
}

void AicComposer::set_focus_frame(bool focused) {
    if (property("focus").toBool() == focused)
        return;
    setProperty("focus", focused); // QFrame#aicComposer[focus="true"]: accent top rule
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void AicComposer::rebuild_chips() {
    if (!chips_)
        return;
    auto* flow = static_cast<ui::FlowLayout*>(chips_->layout());
    while (QLayoutItem* it = flow->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            w->hide();
            w->deleteLater(); // may be the chip whose click is being handled
        }
        delete it;
    }
    const int bh = style::button_h();
    int count = 0;
    auto add_chip = [this, flow, bh, &count](const Attachment& a) {
        auto* chip = new QPushButton(chips_);
        chip->setObjectName(style::kChip);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFixedHeight(bh);
        if (a.kind == Attachment::Kind::Symbol && a.linked)
            chip->setProperty("tone", QStringLiteral("accent"));
        QString label = cp_glyph(a.kind) + QLatin1Char(' ') + cp_short(a.name, 28);
        QString tip;
        if (a.kind == Attachment::Kind::File) {
            if (a.bytes >= 0)
                label += QStringLiteral(" · ") + cp_size(a.bytes);
            tip = tr("%1 — attached; its contents are read when you send. Click to remove it.")
                      .arg(a.ref.isEmpty() ? a.name : a.ref);
        } else if (a.kind == Attachment::Kind::Symbol && a.linked) {
            label += QStringLiteral(" · ") + tr("linked");
            tip = tr("%1 — the linked symbol, sent as context with your next message. Click to stop sending it.")
                      .arg(a.name);
        } else {
            tip = tr("%1 — sent as context with your next message. Click to remove it.").arg(a.name);
        }
        chip->setText(ui::literal_ampersands(label + QStringLiteral("  ×")));
        chip->setToolTip(cp_tip(tip));
        chip->setAccessibleName(tr("Remove %1 from the next message").arg(a.name));
        connect(chip, &QPushButton::clicked, this, [this, a]() { emit chip_removed(a); });
        flow->addWidget(chip);
        ++count;
    };
    for (const Attachment& a : chip_context_)
        add_chip(a);
    for (const Attachment& a : chip_files_)
        add_chip(a);
    chips_->setVisible(count > 0);
}

void AicComposer::replace_trigger(int pos, QChar trigger, const QString& with) {
    QTextDocument* doc = edit_->document();
    const int end_of_text = doc->characterCount() - 1; // the last paragraph separator
    if (pos < 0 || pos >= end_of_text || doc->characterAt(pos) != trigger) {
        edit_->insertPlainText(with);
        return;
    }
    // The trigger alone: its menu held the keyboard, nothing was typed after it.
    QTextCursor c(doc);
    c.setPosition(pos);
    c.setPosition(pos + 1, QTextCursor::KeepAnchor);
    c.insertText(with);
    edit_->setTextCursor(c);
}

void AicComposer::apply_template(int slash_pos, const PromptTemplate& prompt) {
    QString body = prompt.body;
    if (!symbol_hint_.isEmpty())
        body.replace(QStringLiteral("{SYMBOL}"), symbol_hint_);
    replace_trigger(slash_pos, QLatin1Char('/'), body);
    // A placeholder left to fill is selected: typing replaces it.
    const QTextCursor found = edit_->document()->find(QStringLiteral("{SYMBOL}"), std::max(0, slash_pos));
    if (!found.isNull())
        edit_->setTextCursor(found);
    edit_->ensureCursorVisible();
    edit_->setFocus(Qt::PopupFocusReason);
}

void AicComposer::open_prompt_menu() {
    const int slash = trigger_pos_;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(style::menu_sheet());
    menu->setToolTipsVisible(true);
    menu->setAccessibleName(tr("Prompts"));

    QAction* first = nullptr;
    auto add_header = [menu](const QString& title) { menu->addAction(title)->setEnabled(false); };
    auto add_prompt = [this, menu, slash, &first](const PromptTemplate& t) {
        QAction* a = menu->addAction(ui::literal_ampersands(t.title));
        a->setToolTip(cp_tip(cp_preview(t.body)));
        connect(a, &QAction::triggered, this, [this, slash, t]() { apply_template(slash, t); });
        if (!first)
            first = a;
    };
    bool sections = false;
    if (!saved_prompts_.isEmpty()) {
        add_header(tr("SAVED PROMPTS"));
        for (const PromptTemplate& t : saved_prompts_)
            add_prompt(t);
        sections = true;
    }
    const QVector<PromptTemplate> builtins = builtin_templates();
    QString group;
    for (qsizetype i = 0; i < builtins.size(); ++i) {
        const PromptTemplate& t = builtins.at(i);
        if (i == 0 || t.group != group) {
            group = t.group;
            if (sections)
                menu->addSeparator();
            add_header(group.toUpper());
            sections = true;
        }
        add_prompt(t);
    }
    const QRect cr = edit_->cursorRect();
    menu->popup(edit_->viewport()->mapToGlobal(cr.bottomLeft()));
    if (first)
        menu->setActiveAction(first);
}

void AicComposer::open_mention_menu() {
    const int at_pos = trigger_pos_;
    auto* pop = new AicMentionPopup(this);
    pop->setObjectName(style::kDrawer);
    pop->setAttribute(Qt::WA_DeleteOnClose);
    pop->setAttribute(Qt::WA_StyledBackground, true);
    pop->setAccessibleName(tr("Mention"));
    pop->setAccessibleDescription(tr("Pick a symbol, a portfolio, a watchlist or a file for your next message"));

    auto* vl = new QVBoxLayout(pop);
    vl->setContentsMargins(style::u(3), style::u(2), style::u(3), style::u(3));
    vl->setSpacing(style::u(2));

    auto* head = new QHBoxLayout;
    head->setSpacing(style::u(2));
    auto* title = new QLabel(tr("MENTION"), pop);
    title->setObjectName(style::kSectionTitle);
    auto* esc = new QLabel(tr("ESC closes"), pop);
    esc->setObjectName(style::kSectionMeta);
    head->addWidget(title);
    head->addStretch(1);
    head->addWidget(esc);
    vl->addLayout(head);

    auto* box = new ui::SymbolSearchBox(pop);
    box->set_compact(true);
    box->set_clear_on_pick(true);
    box->set_recent_key(QStringLiteral("ai_chat"));
    box->set_placeholder(tr("Symbol, company or ISIN"));
    box->setAccessibleName(tr("Symbol to mention"));
    vl->addWidget(box);

    auto* row = new QHBoxLayout;
    row->setSpacing(style::u(1));
    auto make_btn = [pop](const QString& text, const QString& tip, const QString& name) {
        auto* b = new QPushButton(text, pop);
        b->setObjectName(style::kBtn);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(style::button_h());
        b->setToolTip(tip);
        b->setAccessibleName(name);
        return b;
    };
    auto* pf_btn = make_btn(tr("PORTFOLIO ▸"), tr("Mention one of your portfolios — the model looks it up with its tools"),
                            tr("Mention a portfolio"));
    auto* wl_btn = make_btn(tr("WATCHLIST ▸"), tr("Mention one of your watchlists — the model looks it up with its tools"),
                            tr("Mention a watchlist"));
    auto* file_btn = make_btn(tr("ATTACH FILE…"), tr("Attach a file to this message"), tr("Attach file"));
    row->addWidget(pf_btn);
    row->addWidget(wl_btn);
    row->addStretch(1);
    row->addWidget(file_btn);
    vl->addLayout(row);

    QPointer<QPlainTextEdit> ed(edit_);
    const auto back_to_editor = [ed]() {
        if (!ed)
            return;
        ed->window()->activateWindow();
        ed->setFocus(Qt::PopupFocusReason);
    };
    // A space after the inserted name unless one follows already.
    const auto spacer = [ed, at_pos]() -> QString {
        if (!ed)
            return {};
        const QTextDocument* doc = ed->document();
        const int after = at_pos + 1;
        return (after >= doc->characterCount() - 1 || !doc->characterAt(after).isSpace()) ? QStringLiteral(" ")
                                                                                           : QString();
    };
    pop->on_escape = [pop, back_to_editor]() {
        pop->close();
        back_to_editor();
    };

    connect(box, &ui::SymbolSearchBox::instrument_picked, pop,
            [this, pop, at_pos, back_to_editor, spacer](const core::instrument::Instrument& inst) {
                const QString symbol = inst.symbol.isEmpty() ? inst.display : inst.symbol;
                const QString display = inst.display.isEmpty() ? symbol : inst.display;
                if (symbol.isEmpty())
                    return;
                const QString sp = spacer();
                pop->close();
                replace_trigger(at_pos, QLatin1Char('@'), display + sp);
                back_to_editor();
                Attachment a;
                a.kind = Attachment::Kind::Symbol;
                a.name = display;
                a.ref = symbol;
                a.detail = inst.asset_class == core::instrument::AssetClass::Unknown
                               ? QString()
                               : core::instrument::asset_class_slug(inst.asset_class);
                emit context_attach_requested(a);
            });
    auto reference = [this, pop, at_pos, back_to_editor, spacer](QPushButton* anchor, bool portfolios) {
        cp_reference_menu(anchor, portfolios, [this, pop, at_pos, back_to_editor, spacer](const Attachment& a) {
            const QString sp = spacer();
            pop->close();
            replace_trigger(at_pos, QLatin1Char('@'), a.name + sp);
            back_to_editor();
            emit context_attach_requested(a);
        });
    };
    connect(pf_btn, &QPushButton::clicked, pop, [reference, pf_btn]() { reference(pf_btn, true); });
    connect(wl_btn, &QPushButton::clicked, pop, [reference, wl_btn]() { reference(wl_btn, false); });
    connect(file_btn, &QPushButton::clicked, pop, [this, pop, at_pos, back_to_editor]() {
        pop->close();
        replace_trigger(at_pos, QLatin1Char('@'), QString());
        back_to_editor();
        emit attach_file_requested();
    });

    // Closes when the focus moves anywhere outside it (a menu or list opened from it is inside).
    connect(qApp, &QApplication::focusChanged, pop, [pop](QWidget*, QWidget* now) {
        if (now && !cp_inside(pop, now))
            pop->close();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, pop, [pop](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive)
            pop->close();
    });

    pop->setFixedWidth(std::max(style::u(80), 40 * style::avg_char(0)));
    pop->adjustSize();
    // Above the cursor (the composer sits at the bottom); below it when there is no room.
    const QRect cr = edit_->cursorRect();
    const QPoint anchor = edit_->viewport()->mapToGlobal(cr.topLeft());
    QPoint at(anchor.x() - style::u(3), anchor.y() - pop->height() - style::u(1));
    if (const QScreen* scr = edit_->screen()) {
        const QRect avail = scr->availableGeometry();
        at.setX(std::clamp(at.x(), avail.left(), std::max(avail.left(), avail.right() - pop->width() + 1)));
        if (at.y() < avail.top())
            at.setY(edit_->viewport()->mapToGlobal(cr.bottomLeft()).y() + style::u(1));
    }
    pop->move(at);
    pop->show();
    pop->raise();
    pop->activateWindow();
    box->focus_and_select();
}

} // namespace fincept::screens::aichat
