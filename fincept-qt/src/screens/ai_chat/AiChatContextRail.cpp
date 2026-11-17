// AiChatContextRail.cpp — see AiChatContextRail.h.
//
// Full mode: a scrolling column of four sections, each under a PanelHeader strip
// (CONTEXT · TOOLS · SESSION · SAVED PROMPTS). Icons mode: one glyph button per section with
// a small count, asking the host to show the full rail (peek_requested).
//
// Rows that change (context items, tool groups, saved prompts) are rebuilt only when their
// data changed; old rows are hidden and deleted later (one of them may be the button whose
// click is being handled).

#include "screens/ai_chat/AiChatContextRail.h"

#include "screens/ai_chat/AiChatStyle.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/PanelHeader.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <initializer_list>

namespace fincept::screens::aichat {

namespace {

constexpr int kCrStMessages = 0;
constexpr int kCrStTokens = 1;
constexpr int kCrStCost = 2;
constexpr int kCrStLast = 3;
constexpr int kCrStModels = 4;
constexpr int kCrStAsOf = 5;
constexpr int kCrStRows = 6;

constexpr const char* kCrCategoryProperty = "aicCrCategory";

/// One line that elides instead of clipping (names, titles); the caller sets the tooltip.
class AicCtxElideLabel : public QLabel {
  public:
    explicit AicCtxElideLabel(QWidget* parent) : QLabel(parent) {
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

QLabel* cr_label(const char* object_name, QWidget* parent, bool wrap = false) {
    auto* l = new QLabel(parent);
    l->setObjectName(object_name);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(wrap);
    return l;
}

QPushButton* cr_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(object_name);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

void cr_set_text(QLabel* l, const QString& text) {
    if (l->text() != text)
        l->setText(text);
}

/// Outside text in a tooltip: escaped rich text (markup in a name is never interpreted).
QString cr_tip(const QString& plain) {
    QString esc = plain.toHtmlEscaped();
    esc.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return QStringLiteral("<p>") + esc + QStringLiteral("</p>");
}

QString cr_preview(const QString& text) {
    constexpr int kMax = 400;
    return text.size() > kMax ? text.left(kMax).trimmed() + QStringLiteral("…") : text;
}

QVBoxLayout* cr_body_layout(QWidget* body) {
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(style::u(3), style::u(2), style::u(3), style::u(3));
    vl->setSpacing(style::u(1));
    return vl;
}

/// Empties a row container; widgets are hidden now and deleted later.
void cr_clear(QLayout* layout) {
    while (QLayoutItem* it = layout->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete it;
    }
}

QString cr_glyph(Attachment::Kind kind) {
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

QString cr_size(qint64 bytes) {
    const QLocale loc;
    if (bytes < 1024)
        return AicContextRail::tr("%1 B").arg(loc.toString(bytes));
    if (bytes < 1024 * 1024)
        return AicContextRail::tr("%1 KB").arg(loc.toString(double(bytes) / 1024.0, 'f', 1));
    return AicContextRail::tr("%1 MB").arg(loc.toString(double(bytes) / (1024.0 * 1024.0), 'f', 1));
}

/// The dim line under an item's name: what it is and what it costs.
QString cr_detail(const Attachment& a) {
    switch (a.kind) {
        case Attachment::Kind::Symbol:
            return a.detail.isEmpty() ? AicContextRail::tr("mentioned")
                                      : a.detail + QStringLiteral(" · ") + AicContextRail::tr("mentioned");
        case Attachment::Kind::Portfolio:
            return AicContextRail::tr("portfolio · a reference the model looks up");
        case Attachment::Kind::Watchlist:
            return AicContextRail::tr("watchlist · a reference the model looks up");
        case Attachment::Kind::Image:
            return AicContextRail::tr("image · not sent (text-only chat)");
        case Attachment::Kind::File:
            break;
    }
    if (a.bytes < 0)
        return AicContextRail::tr("file · read when you send");
    QString d = cr_size(a.bytes) + QStringLiteral(" · ") + AicContextRail::tr("≈ %1 tok").arg(format_count(a.bytes / 4));
    if (a.truncated || a.bytes > kAttachMaxChars)
        d += QStringLiteral(" · ") +
             AicContextRail::tr("first %1 characters sent").arg(QLocale().toString(qint64(kAttachMaxChars)));
    return d;
}

/// Items the list shows (the linked symbol has its own row).
int cr_item_count(const QVector<Attachment>& context, const QVector<Attachment>& files) {
    int n = int(files.size());
    for (const Attachment& a : context)
        if (!(a.kind == Attachment::Kind::Symbol && a.linked))
            ++n;
    return n;
}

bool cr_same_groups(const QVector<ToolGroup>& a, const QVector<ToolGroup>& b) {
    if (a.size() != b.size())
        return false;
    for (qsizetype i = 0; i < a.size(); ++i)
        if (a.at(i).category != b.at(i).category || a.at(i).tools != b.at(i).tools)
            return false;
    return true;
}

bool cr_same_prompts(const QVector<PromptTemplate>& a, const QVector<PromptTemplate>& b) {
    if (a.size() != b.size())
        return false;
    for (qsizetype i = 0; i < a.size(); ++i) {
        const PromptTemplate& x = a.at(i);
        const PromptTemplate& y = b.at(i);
        if (x.id != y.id || x.group != y.group || x.title != y.title || x.body != y.body || x.saved != y.saved)
            return false;
    }
    return true;
}

} // namespace

struct AicContextRail::Parts {
    // ── state
    QString linked_symbol;
    QString linked_class;
    bool linked_attached = false;
    QVector<Attachment> context;
    QVector<Attachment> files;
    QString wire;
    bool tools_on = true;
    bool nav_on = true;
    QVector<ToolGroup> groups;
    bool loading = true; ///< until the host's first set_tool_groups()
    QSet<QString> expanded; ///< tool categories showing their tool names
    SessionStats stats;
    QVector<PromptTemplate> prompts;

    // ── CONTEXT
    ui::PanelHeader* ctx_head = nullptr;
    QPushButton* add_btn = nullptr;
    QWidget* linked_row = nullptr;
    QLabel* linked_tag = nullptr;
    QLabel* linked_sym = nullptr;
    QLabel* linked_cls = nullptr;
    QPushButton* linked_btn = nullptr;
    QWidget* items_box = nullptr;
    QVBoxLayout* items_layout = nullptr;
    QLabel* ctx_empty = nullptr;
    QLabel* sent_title = nullptr;
    QLabel* sent_text = nullptr;

    // ── TOOLS
    ui::PanelHeader* tools_head = nullptr;
    QPushButton* tools_btn = nullptr;
    QPushButton* nav_btn = nullptr;
    QLabel* tools_status = nullptr; ///< "Loading the tool catalogue…" / "No tools…"
    QWidget* groups_box = nullptr;
    QVBoxLayout* groups_layout = nullptr;
    QLabel* tools_note = nullptr;
    QPushButton* manage_btn = nullptr;

    // ── SESSION
    ui::PanelHeader* session_head = nullptr;
    QLabel* st_key[kCrStRows] = {};
    QLabel* st_val[kCrStRows] = {};
    QLabel* st_run = nullptr;

    // ── SAVED PROMPTS
    ui::PanelHeader* prompts_head = nullptr;
    QPushButton* save_btn = nullptr;
    QWidget* prompts_box = nullptr;
    QVBoxLayout* prompts_layout = nullptr;
    QLabel* prompts_empty = nullptr;

    // ── Icons mode
    QToolButton* icon_btns[4] = {};
};

// ── Construction ─────────────────────────────────────────────────────────────

AicContextRail::AicContextRail(QWidget* parent) : QWidget(parent), p_(std::make_unique<Parts>()) {
    setObjectName(style::kRailRight);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    build_full();
    build_icons();
    root->addWidget(full_, 1);
    root->addWidget(icons_, 1);
    icons_->hide();

    retranslate();
    fit_metrics();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_metrics(); });
}

AicContextRail::~AicContextRail() = default;

void AicContextRail::build_full() {
    Parts& p = *p_;
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    full_ = scroll;

    auto* column = new QWidget(scroll);
    auto* vl = new QVBoxLayout(column);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── CONTEXT ──────────────────────────────────────────────────────────────
    p.ctx_head = new ui::PanelHeader(tr("CONTEXT"), column);
    p.add_btn = cr_button(style::kBtn, p.ctx_head);
    p.ctx_head->add_control(p.add_btn);
    connect(p.add_btn, &QPushButton::clicked, this, [this]() { open_add_menu(); });
    vl->addWidget(p.ctx_head);

    auto* ctx = new QWidget(column);
    auto* cl = cr_body_layout(ctx);

    p.linked_row = new QWidget(ctx);
    auto* lr = new QHBoxLayout(p.linked_row);
    lr->setContentsMargins(0, 0, 0, style::u(1));
    lr->setSpacing(style::u(2));
    p.linked_tag = cr_label(style::kSectionMeta, p.linked_row);
    p.linked_sym = cr_label(style::kBody, p.linked_row);
    p.linked_cls = new AicCtxElideLabel(p.linked_row);
    p.linked_cls->setObjectName(style::kDim);
    p.linked_btn = cr_button(style::kBtn, p.linked_row);
    p.linked_btn->setCheckable(true);
    lr->addWidget(p.linked_tag);
    lr->addWidget(p.linked_sym);
    lr->addWidget(p.linked_cls, 1);
    lr->addWidget(p.linked_btn);
    cl->addWidget(p.linked_row);
    connect(p.linked_btn, &QPushButton::clicked, this, [this](bool checked) {
        p_->linked_attached = checked;
        update_linked();
        update_icons();
        emit linked_attach_toggled(checked);
    });

    p.items_box = new QWidget(ctx);
    p.items_layout = new QVBoxLayout(p.items_box);
    p.items_layout->setContentsMargins(0, 0, 0, 0);
    p.items_layout->setSpacing(0);
    cl->addWidget(p.items_box);

    p.ctx_empty = cr_label(style::kDim, ctx, true);
    cl->addWidget(p.ctx_empty);

    p.sent_title = cr_label(style::kSectionMeta, ctx);
    p.sent_text = cr_label(style::kDim, ctx, true);
    p.sent_text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    cl->addSpacing(style::u(1));
    cl->addWidget(p.sent_title);
    cl->addWidget(p.sent_text);
    vl->addWidget(ctx);

    // ── TOOLS ────────────────────────────────────────────────────────────────
    p.tools_head = new ui::PanelHeader(tr("TOOLS"), column);
    p.tools_btn = cr_button(style::kBtn, p.tools_head);
    p.tools_btn->setCheckable(true);
    p.nav_btn = cr_button(style::kBtn, p.tools_head);
    p.nav_btn->setCheckable(true);
    p.tools_head->add_control(p.tools_btn);
    p.tools_head->add_control(p.nav_btn);
    connect(p.tools_btn, &QPushButton::clicked, this, [this](bool on) {
        p_->tools_on = on;
        update_tool_toggles();
        update_icons();
        emit tool_policy_changed(on, p_->nav_on);
    });
    connect(p.nav_btn, &QPushButton::clicked, this, [this](bool on) {
        p_->nav_on = on;
        update_tool_toggles();
        emit tool_policy_changed(p_->tools_on, on);
    });
    vl->addWidget(p.tools_head);

    auto* tools = new QWidget(column);
    auto* tl = cr_body_layout(tools);
    p.tools_status = cr_label(style::kDim, tools, true);
    tl->addWidget(p.tools_status);
    p.groups_box = new QWidget(tools);
    p.groups_layout = new QVBoxLayout(p.groups_box);
    p.groups_layout->setContentsMargins(0, 0, 0, 0);
    p.groups_layout->setSpacing(0);
    tl->addWidget(p.groups_box);
    auto* note_row = new QWidget(tools);
    auto* nl = new QHBoxLayout(note_row);
    nl->setContentsMargins(0, style::u(1), 0, 0);
    nl->setSpacing(style::u(2));
    p.tools_note = cr_label(style::kDim, note_row, true);
    p.manage_btn = cr_button(style::kLinkBtn, note_row);
    nl->addWidget(p.tools_note, 1);
    nl->addWidget(p.manage_btn, 0, Qt::AlignTop);
    tl->addWidget(note_row);
    connect(p.manage_btn, &QPushButton::clicked, this, &AicContextRail::manage_tools_requested);
    vl->addWidget(tools);

    // ── SESSION ──────────────────────────────────────────────────────────────
    p.session_head = new ui::PanelHeader(tr("SESSION"), column);
    vl->addWidget(p.session_head);
    auto* ses = new QWidget(column);
    auto* grid = new QGridLayout(ses);
    grid->setContentsMargins(style::u(3), style::u(2), style::u(3), style::u(3));
    grid->setHorizontalSpacing(style::u(2));
    grid->setVerticalSpacing(style::u(1));
    for (int i = 0; i < kCrStRows; ++i) {
        // Rows: MESSAGES, TOKENS, (in / out this run), COST, LAST ANSWER, MODELS, AS OF.
        const int grid_row = i <= kCrStTokens ? i : i + 1;
        p.st_key[i] = cr_label(style::kSectionMeta, ses);
        p.st_val[i] = cr_label(style::kBody, ses, i == kCrStModels);
        p.st_val[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        p.st_val[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(p.st_key[i], grid_row, 0, Qt::AlignLeft | Qt::AlignTop);
        grid->addWidget(p.st_val[i], grid_row, 1);
    }
    p.st_run = cr_label(style::kDim, ses);
    p.st_run->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(p.st_run, kCrStTokens + 1, 0, 1, 2);
    grid->setColumnStretch(1, 1);
    vl->addWidget(ses);

    // ── SAVED PROMPTS ────────────────────────────────────────────────────────
    p.prompts_head = new ui::PanelHeader(tr("SAVED PROMPTS"), column);
    p.save_btn = cr_button(style::kBtn, p.prompts_head);
    p.prompts_head->add_control(p.save_btn);
    connect(p.save_btn, &QPushButton::clicked, this, &AicContextRail::save_draft_requested);
    vl->addWidget(p.prompts_head);
    auto* prompts = new QWidget(column);
    auto* pl = cr_body_layout(prompts);
    p.prompts_box = new QWidget(prompts);
    p.prompts_layout = new QVBoxLayout(p.prompts_box);
    p.prompts_layout->setContentsMargins(0, 0, 0, 0);
    p.prompts_layout->setSpacing(0);
    pl->addWidget(p.prompts_box);
    p.prompts_empty = cr_label(style::kDim, prompts, true);
    pl->addWidget(p.prompts_empty);
    vl->addWidget(prompts);

    vl->addStretch(1);
    scroll->setWidget(column);
    column->setAutoFillBackground(false); // setWidget() turned it on: the rail's surface shows through
}

void AicContextRail::build_icons() {
    icons_ = new QWidget(this);
    auto* vl = new QVBoxLayout(icons_);
    vl->setContentsMargins(0, style::u(2), 0, style::u(2));
    vl->setSpacing(style::u(1));
    for (int i = 0; i < 4; ++i) {
        auto* b = new QToolButton(icons_);
        b->setObjectName(style::kIconBtn);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::StrongFocus);
        connect(b, &QToolButton::clicked, this, [this, i]() { emit peek_requested(i); });
        vl->addWidget(b, 0, Qt::AlignHCenter);
        p_->icon_btns[i] = b;
    }
    vl->addStretch(1);
}

// ── Public API ───────────────────────────────────────────────────────────────

void AicContextRail::set_mode(Mode mode) {
    mode_ = mode;
    full_->setVisible(mode == Mode::Full);
    icons_->setVisible(mode == Mode::Icons);
}

int AicContextRail::preferred_width(Mode mode) const {
    if (mode == Mode::Icons)
        return std::max(style::u(8), style::text_h(0) * 2);
    return std::max(260, 30 * style::avg_char(0) + style::u(6));
}

void AicContextRail::set_linked_symbol(const QString& symbol, const QString& asset_class, bool attached) {
    Parts& p = *p_;
    if (p.linked_symbol == symbol && p.linked_class == asset_class && p.linked_attached == attached)
        return;
    p.linked_symbol = symbol;
    p.linked_class = asset_class;
    p.linked_attached = attached;
    update_linked();
    update_icons();
}

void AicContextRail::set_context(const QVector<Attachment>& context, const QVector<Attachment>& files) {
    Parts& p = *p_;
    if (p.context == context && p.files == files)
        return;
    p.context = context;
    p.files = files;
    rebuild_context();
}

void AicContextRail::set_wire_preview(const QString& text) {
    if (p_->wire == text)
        return;
    p_->wire = text;
    update_wire();
}

void AicContextRail::set_tool_policy(bool tools_on, bool navigation_on) {
    p_->tools_on = tools_on;
    p_->nav_on = navigation_on;
    update_tool_toggles();
    update_icons();
}

void AicContextRail::set_tool_groups(const QVector<ToolGroup>& groups, bool loading) {
    Parts& p = *p_;
    if (p.loading == loading && cr_same_groups(p.groups, groups))
        return;
    p.groups = groups;
    p.loading = loading;
    rebuild_tools();
}

void AicContextRail::set_stats(const SessionStats& stats) {
    p_->stats = stats;
    update_stats();
}

void AicContextRail::set_saved_prompts(const QVector<PromptTemplate>& prompts) {
    if (cr_same_prompts(p_->prompts, prompts))
        return;
    p_->prompts = prompts;
    rebuild_prompts();
}

void AicContextRail::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

// ── Sections ─────────────────────────────────────────────────────────────────

void AicContextRail::retranslate() {
    Parts& p = *p_;
    full_->setAccessibleName(tr("Context, tools and session"));
    icons_->setAccessibleName(tr("Context rail"));

    p.ctx_head->set_title(tr("CONTEXT"));
    p.add_btn->setText(tr("+ ADD ▾"));
    p.add_btn->setToolTip(tr("Attach a symbol, a portfolio, a watchlist or a file to your next message"));
    p.add_btn->setAccessibleName(tr("Add context"));
    p.linked_tag->setText(tr("LINKED"));
    p.linked_tag->setToolTip(tr("The symbol of this tab's symbol group"));
    cr_set_text(p.ctx_empty, tr("Nothing attached — the model sees only the conversation."));
    p.sent_title->setText(tr("SENT AS"));
    p.sent_text->setToolTip(
        tr("The exact lines your next message starts with (attached files follow their headers when you send)"));
    p.sent_text->setAccessibleName(tr("What the context adds to your next message"));

    p.tools_head->set_title(tr("TOOLS"));
    p.manage_btn->setText(tr("MANAGE ›"));
    p.manage_btn->setToolTip(tr("Open MCP Servers to turn single tools on or off"));
    p.manage_btn->setAccessibleName(tr("Manage tools"));
    cr_set_text(p.tools_note, tr("Per-tool on / off lives in MCP Servers."));

    p.session_head->set_title(tr("SESSION"));
    const QString keys[kCrStRows] = {tr("MESSAGES"),    tr("TOKENS"), tr("COST"),
                                     tr("LAST ANSWER"), tr("MODELS"), tr("AS OF")};
    for (int i = 0; i < kCrStRows; ++i)
        cr_set_text(p.st_key[i], keys[i]);

    p.prompts_head->set_title(tr("SAVED PROMPTS"));
    p.save_btn->setText(tr("+ SAVE DRAFT"));
    p.save_btn->setToolTip(tr("Save the composer's text as a prompt"));
    p.save_btn->setAccessibleName(tr("Save the draft as a prompt"));
    cr_set_text(p.prompts_empty, tr("No saved prompts — save the composer's text with SAVE DRAFT."));

    rebuild_context();
    update_wire();
    update_tool_toggles();
    rebuild_tools();
    update_stats();
    rebuild_prompts();
}

void AicContextRail::rebuild_context() {
    Parts& p = *p_;
    cr_clear(p.items_layout);
    const int bh = style::button_h();
    const int glyph_w = style::avg_char(0) * 2;
    auto add_row = [this, &p, bh, glyph_w](const Attachment& a) {
        auto* row = new QWidget(p.items_box);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, style::u(1), 0, style::u(1));
        hl->setSpacing(style::u(2));

        auto* glyph = cr_label(style::kDim, row);
        glyph->setText(cr_glyph(a.kind));
        glyph->setFixedWidth(glyph_w);
        glyph->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        hl->addWidget(glyph, 0, Qt::AlignTop);

        auto* text_col = new QWidget(row);
        auto* tl = new QVBoxLayout(text_col);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(0);
        auto* name = new AicCtxElideLabel(text_col);
        name->setObjectName(style::kBody);
        name->setText(a.name);
        name->setToolTip(cr_tip(a.kind == Attachment::Kind::File && !a.ref.isEmpty() ? a.ref : a.name));
        const QString detail_text = cr_detail(a);
        auto* detail = new AicCtxElideLabel(text_col);
        detail->setObjectName(style::kDim);
        detail->setText(detail_text);
        detail->setToolTip(cr_tip(detail_text));
        tl->addWidget(name);
        tl->addWidget(detail);
        hl->addWidget(text_col, 1);

        auto* x = cr_button(style::kIconBtn, row);
        x->setText(QStringLiteral("×"));
        x->setFixedSize(bh, bh);
        x->setToolTip(cr_tip(tr("Remove %1 from the next message").arg(a.name)));
        x->setAccessibleName(tr("Remove %1").arg(a.name));
        connect(x, &QPushButton::clicked, this, [this, a]() { emit detach_requested(a); });
        hl->addWidget(x, 0, Qt::AlignTop);

        p.items_layout->addWidget(row);
    };
    for (const Attachment& a : p.context)
        if (!(a.kind == Attachment::Kind::Symbol && a.linked))
            add_row(a);
    for (const Attachment& a : p.files)
        add_row(a);
    update_linked();
    update_icons();
}

void AicContextRail::update_linked() {
    Parts& p = *p_;
    const bool has = !p.linked_symbol.isEmpty();
    p.linked_row->setVisible(has);
    if (has) {
        cr_set_text(p.linked_sym, p.linked_symbol);
        p.linked_cls->setText(p.linked_class);
        p.linked_cls->setToolTip(cr_tip(p.linked_class));
        p.linked_btn->setChecked(p.linked_attached);
        p.linked_btn->setText(p.linked_attached ? tr("ATTACHED") : tr("ATTACH"));
        p.linked_btn->setToolTip(p.linked_attached
                                     ? tr("The linked symbol is sent with your next message — click to stop sending it")
                                     : tr("Send the linked symbol with your next message"));
        p.linked_btn->setAccessibleName(tr("Send the linked symbol"));
        p.linked_btn->setAccessibleDescription(p.linked_attached ? tr("attached") : tr("not attached"));
    }
    const int items = cr_item_count(p.context, p.files);
    const int sent = items + (has && p.linked_attached ? 1 : 0);
    p.items_box->setVisible(items > 0);
    p.ctx_empty->setVisible(sent == 0);
    p.ctx_head->set_badge(sent > 0 ? QString::number(sent) : QString());
}

void AicContextRail::update_wire() {
    Parts& p = *p_;
    const bool show = !p.wire.trimmed().isEmpty();
    p.sent_title->setVisible(show);
    p.sent_text->setVisible(show);
    cr_set_text(p.sent_text, p.wire.trimmed());
}

void AicContextRail::update_tool_toggles() {
    Parts& p = *p_;
    p.tools_btn->setChecked(p.tools_on);
    p.tools_btn->setText(p.tools_on ? tr("TOOLS ON") : tr("TOOLS OFF"));
    p.tools_btn->setToolTip(p.tools_on ? tr("The model may call the terminal's tools (quotes, news, portfolios …) "
                                            "in this chat — click to turn them off")
                                       : tr("Tools are off: the model answers from the conversation only — click "
                                            "to turn them on"));
    p.tools_btn->setAccessibleName(tr("Tools in this chat"));
    p.tools_btn->setAccessibleDescription(p.tools_on ? tr("on") : tr("off"));

    p.nav_btn->setText(tr("NAV"));
    p.nav_btn->setEnabled(p.tools_on);
    p.nav_btn->setChecked(p.tools_on && p.nav_on);
    p.nav_btn->setToolTip(!p.tools_on ? tr("Turn tools on first — switching tabs is a tool")
                          : p.nav_on  ? tr("The model may also switch the terminal's tabs — click to keep it in place")
                                      : tr("The model cannot switch tabs — click to allow it"));
    p.nav_btn->setAccessibleName(tr("Navigation tools"));
    p.nav_btn->setAccessibleDescription(p.tools_on && p.nav_on ? tr("on") : tr("off"));
}

void AicContextRail::rebuild_tools() {
    Parts& p = *p_;
    // Keyboard focus on a group row survives the rebuild (it is a new button afterwards).
    QString focus_cat;
    if (QWidget* f = QApplication::focusWidget(); f && p.groups_box->isAncestorOf(f))
        focus_cat = f->property(kCrCategoryProperty).toString();
    cr_clear(p.groups_layout);

    const int row_h = style::text_h(0) + style::u(1);
    int total = 0;
    for (const ToolGroup& g : p.groups) {
        const int n = int(g.tools.size());
        total += n;
        const QString cat = g.category.isEmpty() ? tr("external") : g.category;
        const bool open = p.expanded.contains(cat);

        auto* btn = cr_button(style::kLinkBtn, p.groups_box);
        btn->setProperty(kCrCategoryProperty, cat);
        btn->setAutoDefault(true); // Enter toggles, like Space
        btn->setMinimumHeight(row_h);
        auto* hl = new QHBoxLayout(btn);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(style::u(2));
        auto* name = new AicCtxElideLabel(btn);
        name->setObjectName(style::kBody);
        name->setText((open ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) + cat);
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* count = cr_label(style::kDim, btn);
        count->setText(QString::number(n));
        count->setFont(style::mono_font(-2));
        count->setAttribute(Qt::WA_TransparentForMouseEvents);
        hl->addWidget(name, 1);
        hl->addWidget(count);
        btn->setAccessibleName(tr("%1 tools: %2").arg(cat, QString::number(n)));
        btn->setAccessibleDescription(open ? tr("expanded") : tr("collapsed"));
        btn->setToolTip(open ? tr("Hide the tool names") : tr("Show the tool names"));
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            if (p_->expanded.contains(cat))
                p_->expanded.remove(cat);
            else
                p_->expanded.insert(cat);
            rebuild_tools();
        });
        p.groups_layout->addWidget(btn);
        if (!focus_cat.isEmpty() && focus_cat == cat)
            QTimer::singleShot(0, btn, [btn]() { btn->setFocus(Qt::OtherFocusReason); });

        if (open) {
            auto* names = cr_label(style::kDim, p.groups_box, true);
            names->setText(g.tools.join(QStringLiteral(" · ")));
            names->setContentsMargins(style::avg_char(0) * 2, 0, 0, style::u(1));
            names->setTextInteractionFlags(Qt::TextSelectableByMouse);
            p.groups_layout->addWidget(names);
        }
    }
    p.groups_box->setVisible(!p.groups.isEmpty());
    p.tools_status->setVisible(p.loading || p.groups.isEmpty());
    cr_set_text(p.tools_status,
                p.loading ? tr("loading the tool catalogue…") : tr("No tools are available right now."));
    p.tools_head->set_badge(p.loading || total == 0 ? QString() : QString::number(total));
    update_icons();
}

void AicContextRail::update_stats() {
    Parts& p = *p_;
    const SessionStats& s = p.stats;
    const QLocale loc;
    auto set_row = [&p](int i, bool known, const QString& value, const QString& tip) {
        p.st_key[i]->setVisible(known);
        p.st_val[i]->setVisible(known);
        if (!known)
            return;
        cr_set_text(p.st_val[i], value);
        p.st_val[i]->setToolTip(tip);
    };
    set_row(kCrStMessages, true, loc.toString(s.messages), tr("Messages in this session"));
    set_row(kCrStTokens, s.tokens > 0, format_count(s.tokens),
            tr("%1 tokens in this session, as the services reported them").arg(loc.toString(s.tokens)));
    const bool run = s.prompt_tokens >= 0 || s.completion_tokens >= 0;
    p.st_run->setVisible(run);
    if (run)
        cr_set_text(p.st_run, tr("in %1 / out %2 this run")
                                  .arg(s.prompt_tokens >= 0 ? format_count(s.prompt_tokens) : QStringLiteral("—"),
                                       s.completion_tokens >= 0 ? format_count(s.completion_tokens)
                                                                : QStringLiteral("—")));
    set_row(kCrStCost, s.cost_usd >= 0.0,
            QStringLiteral("$") + loc.toString(s.cost_usd, 'f', s.cost_usd < 1.0 ? 4 : 2),
            tr("The sum of the costs the services reported"));
    set_row(kCrStLast, s.last_ms >= 0, format_ms(s.last_ms), tr("From sending to the final answer"));
    set_row(kCrStModels, !s.models.isEmpty(), s.models.join(QLatin1Char('\n')), tr("Models that answered"));
    set_row(kCrStAsOf, s.as_of.isValid(), s.as_of.toString(QStringLiteral("HH:mm:ss")),
            s.as_of.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    update_icons();
}

void AicContextRail::rebuild_prompts() {
    Parts& p = *p_;
    cr_clear(p.prompts_layout);
    const int bh = style::button_h();
    const int row_h = std::max(bh, style::text_h(0) + style::u(1));
    for (int i = 0; i < int(p.prompts.size()); ++i) {
        const PromptTemplate t = p.prompts.at(i);
        auto* row = new QWidget(p.prompts_box);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(style::u(1));

        auto* use = cr_button(style::kLinkBtn, row);
        use->setAutoDefault(true);
        use->setMinimumHeight(row_h);
        auto* ul = new QHBoxLayout(use);
        ul->setContentsMargins(0, 0, 0, 0);
        ul->setSpacing(0);
        auto* title = new AicCtxElideLabel(use);
        title->setObjectName(style::kBody);
        title->setText(t.title);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        ul->addWidget(title);
        use->setToolTip(cr_tip(t.title + QStringLiteral("\n\n") + cr_preview(t.body)));
        use->setAccessibleName(tr("Use the saved prompt %1").arg(t.title));
        use->setAccessibleDescription(tr("Puts the prompt in the composer — nothing is sent"));
        connect(use, &QPushButton::clicked, this, [this, t]() { emit prompt_activated(t); });

        auto* del = cr_button(style::kIconBtn, row);
        del->setText(QStringLiteral("×"));
        del->setFixedSize(bh, bh);
        del->setToolTip(tr("Delete this saved prompt"));
        del->setAccessibleName(tr("Delete the saved prompt %1").arg(t.title));
        connect(del, &QPushButton::clicked, this, [this, i]() { emit prompt_delete_requested(i); });

        hl->addWidget(use, 1);
        hl->addWidget(del);
        p.prompts_layout->addWidget(row);
    }
    p.prompts_box->setVisible(!p.prompts.isEmpty());
    p.prompts_empty->setVisible(p.prompts.isEmpty());
    p.prompts_head->set_badge(p.prompts.isEmpty() ? QString() : QString::number(p.prompts.size()));
    update_icons();
}

void AicContextRail::update_icons() {
    Parts& p = *p_;
    if (!p.icon_btns[0])
        return;
    const int ctx_n = cr_item_count(p.context, p.files) + (!p.linked_symbol.isEmpty() && p.linked_attached ? 1 : 0);
    int tool_n = 0;
    for (const ToolGroup& g : p.groups)
        tool_n += int(g.tools.size());
    const int prompt_n = int(p.prompts.size());

    const QString glyphs[4] = {QStringLiteral("⊕"), QStringLiteral("⚙"), QStringLiteral("Σ"), QStringLiteral("¶")};
    const QString badges[4] = {
        ctx_n > 0 ? QString::number(ctx_n) : QString(),
        !p.tools_on ? tr("OFF") : (tool_n > 0 ? format_count(tool_n) : QString()),
        p.stats.messages > 0 ? format_count(p.stats.messages) : QString(),
        prompt_n > 0 ? QString::number(prompt_n) : QString(),
    };
    const QString tips[4] = {
        ctx_n > 0 ? tr("Context — %n item(s) sent with your next message", "", ctx_n)
                  : tr("Context — nothing attached"),
        !p.tools_on ? tr("Tools — off in this chat") : tr("Tools — %n tool(s) the model may call", "", tool_n),
        tr("Session — %n message(s)", "", p.stats.messages),
        tr("Saved prompts — %n saved", "", prompt_n),
    };
    const QString names[4] = {tr("Show the context section"), tr("Show the tools section"),
                              tr("Show the session figures"), tr("Show the saved prompts")};
    for (int i = 0; i < 4; ++i) {
        QToolButton* b = p.icon_btns[i];
        const QString text = badges[i].isEmpty() ? glyphs[i] : glyphs[i] + QLatin1Char('\n') + badges[i];
        if (b->text() != text)
            b->setText(text);
        b->setToolTip(tips[i]);
        b->setAccessibleName(names[i]);
        b->setAccessibleDescription(tips[i]);
    }
}

void AicContextRail::fit_metrics() {
    Parts& p = *p_;
    const int bh = style::button_h();
    for (QPushButton* b : {p.add_btn, p.linked_btn, p.tools_btn, p.nav_btn, p.save_btn, p.manage_btn})
        b->setFixedHeight(bh);
    // Figures in tabular digits (the sheet sets the size, the family and features come from here).
    p.sent_text->setFont(style::mono_font(-2));
    for (int i = 0; i < kCrStRows; ++i)
        p.st_val[i]->setFont(style::mono_font(0));
    p.st_run->setFont(style::mono_font(-2));
    p.linked_sym->setFont(style::mono_font(0, QFont::DemiBold));
    const int iw = preferred_width(Mode::Icons) - style::u(1);
    const int ih = style::text_h(0) * 2 + style::u(2);
    for (QToolButton* b : p.icon_btns)
        b->setFixedSize(iw, ih);
    rebuild_context();
    rebuild_tools();
    rebuild_prompts();
}

// ── + ADD ▾ ──────────────────────────────────────────────────────────────────

void AicContextRail::open_add_menu() {
    Parts& p = *p_;
    auto* menu = new QMenu(p.add_btn);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(style::menu_sheet());
    connect(menu->addAction(tr("Symbol…")), &QAction::triggered, this, &AicContextRail::add_symbol_requested);
    QMenu* portfolios = menu->addMenu(tr("Portfolio"));
    portfolios->addAction(tr("…"))->setEnabled(false);
    connect(portfolios, &QMenu::aboutToShow, this, [this, portfolios]() { fill_reference_menu(portfolios, true); });
    QMenu* watchlists = menu->addMenu(tr("Watchlist"));
    watchlists->addAction(tr("…"))->setEnabled(false);
    connect(watchlists, &QMenu::aboutToShow, this, [this, watchlists]() { fill_reference_menu(watchlists, false); });
    connect(menu->addAction(tr("File…")), &QAction::triggered, this, &AicContextRail::add_file_requested);
    menu->popup(p.add_btn->mapToGlobal(QPoint(0, p.add_btn->height())));
}

void AicContextRail::fill_reference_menu(QMenu* menu, bool portfolios) {
    // Small reads on the GUI thread, only while the menu opens.
    menu->clear();
    if (portfolios) {
        const auto r = PortfolioRepository::instance().list_portfolios();
        if (r.is_err()) {
            menu->addAction(tr("Could not read the portfolios"))->setEnabled(false);
            return;
        }
        if (r.value().isEmpty()) {
            menu->addAction(tr("No portfolios yet"))->setEnabled(false);
            return;
        }
        for (const auto& pf : r.value()) {
            Attachment a;
            a.kind = Attachment::Kind::Portfolio;
            a.name = pf.name;
            a.ref = pf.id;
            connect(menu->addAction(ui::literal_ampersands(pf.name)), &QAction::triggered, this,
                    [this, a]() { emit add_reference_requested(a); });
        }
        return;
    }
    const auto r = WatchlistRepository::instance().list_all();
    if (r.is_err()) {
        menu->addAction(tr("Could not read the watchlists"))->setEnabled(false);
        return;
    }
    if (r.value().isEmpty()) {
        menu->addAction(tr("No watchlists yet"))->setEnabled(false);
        return;
    }
    for (const auto& wl : r.value()) {
        Attachment a;
        a.kind = Attachment::Kind::Watchlist;
        a.name = wl.name;
        a.ref = wl.id;
        connect(menu->addAction(ui::literal_ampersands(wl.name)), &QAction::triggered, this,
                [this, a]() { emit add_reference_requested(a); });
    }
}

} // namespace fincept::screens::aichat
