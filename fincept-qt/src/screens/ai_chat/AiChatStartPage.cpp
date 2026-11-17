// AiChatStartPage.cpp — see AiChatStartPage.h.
//
// A scroll area over one centred column (at most ~140 average characters wide):
//   ▌AI RESEARCH DESK · lead line · needs-setup page (when no model) · example cards grouped
//   in columns (5 / 3 / 2 / 1 by width) · RECENT SESSIONS · the Enterprise mention line.
// Cards only fill the composer; nothing here sends.

#include "screens/ai_chat/AiChatStartPage.h"

#include "screens/ai_chat/AiChatStyle.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/ModelBadge.h"
#include "ui/widgets/Requirements.h"

#include <QDate>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::aichat {

namespace {

constexpr const char* kSpTickProperty = "aicSpTick"; ///< "title" / "section": refitted when the font changes
constexpr int kSpMaxRecent = 5;

/// The accent tick in front of a title (▌), painted from the live theme.
class AicStartTick : public QWidget {
  public:
    explicit AicStartTick(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), style::accent());
    }
};

/// One line that elides (session titles in the recent list).
class AicStartElide : public QLabel {
  public:
    explicit AicStartElide(QWidget* parent) : QLabel(parent) {
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

QFont sp_title_font() {
    QFont f = style::ui_font(4, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    return f;
}

QSize sp_tick_size(bool title) {
    const QFontMetrics fm(title ? sp_title_font() : style::ui_font(-3, QFont::Bold));
    return QSize(title ? style::u(1) : 3, std::max(10, fm.capHeight() + (title ? 6 : 4)));
}

/// "▌TITLE" — the tick and a small upper-case section title.
QWidget* sp_section_title(const QString& text, QWidget* parent, QLabel** label_out = nullptr) {
    auto* w = new QWidget(parent);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(style::u(2));
    auto* tick = new AicStartTick(w);
    tick->setProperty(kSpTickProperty, QStringLiteral("section"));
    tick->setFixedSize(sp_tick_size(false));
    auto* l = new QLabel(text.toUpper(), w);
    l->setObjectName(style::kSectionTitle);
    l->setTextFormat(Qt::PlainText);
    hl->addWidget(tick, 0, Qt::AlignVCenter);
    hl->addWidget(l, 0, Qt::AlignVCenter);
    hl->addStretch(1);
    if (label_out)
        *label_out = l;
    return w;
}

/// Empties a row container; widgets are hidden now and deleted later.
void sp_clear(QLayout* layout) {
    while (QLayoutItem* it = layout->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete it;
    }
}

bool sp_same_recent(const QVector<SessionSummary>& a, const QVector<SessionSummary>& b) {
    if (a.size() != b.size())
        return false;
    for (qsizetype i = 0; i < a.size(); ++i)
        if (a.at(i).id != b.at(i).id || a.at(i).title != b.at(i).title || a.at(i).updated != b.at(i).updated)
            return false;
    return true;
}

} // namespace

AicStartPage::AicStartPage(QWidget* parent) : QWidget(parent) {
    setObjectName(style::kStartPage);
    setAttribute(Qt::WA_StyledBackground, true);
    build();
    retranslate();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_metrics(); });
}

AicStartPage::~AicStartPage() = default;

void AicStartPage::build() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    scroll_ = new QScrollArea(this);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->viewport()->setAutoFillBackground(false);
    root->addWidget(scroll_);

    // The column is centred: the side stretches take what the column's maximum width leaves.
    content_ = new QWidget(scroll_);
    auto* cl = new QHBoxLayout(content_);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    body_ = new QWidget(content_);
    body_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    body_->setMaximumWidth(body_max_width());
    cl->addStretch(1);
    cl->addWidget(body_, 1000);
    cl->addStretch(1);

    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(style::u(6), style::u(6), style::u(6), style::u(6));
    bl->setSpacing(style::u(4));

    // ── ▌AI RESEARCH DESK
    auto* title_row = new QWidget(body_);
    auto* tl = new QHBoxLayout(title_row);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(style::u(2));
    auto* tick = new AicStartTick(title_row);
    tick->setProperty(kSpTickProperty, QStringLiteral("title"));
    tick->setFixedSize(sp_tick_size(true));
    title_ = new QLabel(title_row); // no sheet name: its own (larger) font, the palette's text colour
    title_->setTextFormat(Qt::PlainText);
    title_->setFont(sp_title_font());
    tl->addWidget(tick, 0, Qt::AlignVCenter);
    tl->addWidget(title_, 0, Qt::AlignVCenter);
    tl->addStretch(1);
    bl->addWidget(title_row);

    lead_ = new QLabel(body_);
    lead_->setObjectName(style::kBody);
    lead_->setTextFormat(Qt::PlainText);
    lead_->setWordWrap(true);
    bl->addWidget(lead_);

    // ── Needs-setup page (no AI model), filled by refresh_setup().
    setup_ = new ui::EmptyState(body_);
    setup_->set_compact(true);
    setup_->set_auto_fit(false); // the page scrolls: the full state, never a squeezed one
    setup_->set_alignment(Qt::AlignTop);
    setup_->hide();
    connect(setup_, &ui::EmptyState::primary_clicked, this, [this]() {
        const ui::RequirementStatus now = ui::check_requirements({ui::Requirement::llm()});
        if (now.link.valid())
            ui::open_setup_link(now.link, this);
        else
            ui::open_llm_settings();
    });
    bl->addWidget(setup_);

    // ── Example cards (grouped columns, re-flowed by width)
    cards_box_ = new QWidget(body_);
    cards_grid_ = new QGridLayout(cards_box_);
    cards_grid_->setContentsMargins(0, 0, 0, 0);
    cards_grid_->setHorizontalSpacing(style::u(4));
    cards_grid_->setVerticalSpacing(style::u(5));
    bl->addWidget(cards_box_);

    // ── RECENT SESSIONS
    recent_box_ = new QWidget(body_);
    auto* rl = new QVBoxLayout(recent_box_);
    rl->setContentsMargins(0, style::u(2), 0, 0);
    rl->setSpacing(style::u(1));
    rl->addWidget(sp_section_title(QString(), recent_box_, &recent_title_));
    auto* rows = new QWidget(recent_box_);
    recent_layout_ = new QVBoxLayout(rows);
    recent_layout_->setContentsMargins(0, 0, 0, 0);
    recent_layout_->setSpacing(0);
    rl->addWidget(rows);
    recent_box_->hide();
    bl->addWidget(recent_box_);

    // ── Enterprise mention (one muted line; the label styles itself)
    auto* rule = new QFrame(body_);
    rule->setObjectName(style::kHairline);
    rule->setFrameShape(QFrame::NoFrame);
    rule->setFixedHeight(1);
    bl->addWidget(rule);
    ent_ = ui::enterprise::make_mention_label(body_);
    ent_->setWordWrap(true);
    bl->addWidget(ent_);
    bl->addStretch(1);

    scroll_->setWidget(content_);
    content_->setAutoFillBackground(false); // setWidget() turned it on: the page's base shows through
}

void AicStartPage::set_recent(const QVector<SessionSummary>& recent) {
    const QVector<SessionSummary> top = recent.mid(0, kSpMaxRecent);
    if (sp_same_recent(top, recent_))
        return;
    recent_ = top;
    rebuild_recent();
}

void AicStartPage::refresh_setup() {
    const ui::RequirementStatus st = ui::check_requirements({ui::Requirement::llm()});
    const int state = st.ok ? 1 : 0;
    if (state == setup_state_)
        return; // nothing changed — no re-apply on every refresh
    setup_state_ = state;
    if (st.ok) {
        setup_->hide();
        return;
    }
    ui::EmptyState::Spec spec = ui::EmptyState::needs_setup_spec(st);
    spec.message = tr("Choose a provider and a model in Settings > LLM Config. AI Chat sends what you type to that "
                      "model — nothing is sent until you press Send.");
    setup_->apply(spec);
    setup_->show();
}

void AicStartPage::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    reflow(e->size().width());
}

void AicStartPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

// ── private ──────────────────────────────────────────────────────────────────

void AicStartPage::retranslate() {
    title_->setText(tr("AI RESEARCH DESK"));
    lead_->setText(tr("Ask about markets, companies, your portfolio or macro data. Answers can use the terminal's "
                      "tools; nothing is sent until you press Send."));
    recent_title_->setText(tr("RECENT SESSIONS"));
    ui::enterprise::set_mention_text(ent_, tr("Fincept Terminal Enterprise — multi-agent research for firms, AI credits included:"),
                                     tr("Discuss custom requirements ↗"), ui::enterprise::meeting_url());
    ent_->setToolTip(tr("Opens a page to arrange a meeting with the Fincept team about custom requirements or a partnership"));
    scroll_->setAccessibleName(tr("AI Chat start page"));
    rebuild_cards();
    rebuild_recent();
    if (setup_state_ != -1) {
        setup_state_ = -1; // re-apply the translated page
        refresh_setup();
    }
}

int AicStartPage::body_max_width() const {
    return 140 * style::avg_char(0) + 2 * style::u(6);
}

void AicStartPage::rebuild_cards() {
    for (QWidget* w : group_cols_) {
        cards_grid_->removeWidget(w);
        w->hide();
        w->deleteLater();
    }
    group_cols_.clear();
    columns_ = 0;

    const QVector<ExampleCard> cards = example_cards();
    QStringList groups;
    for (const ExampleCard& c : cards)
        if (!groups.contains(c.group))
            groups.append(c.group);

    const QString tip = tr("Puts this prompt in the composer — nothing is sent until you press Send");
    for (const QString& g : groups) {
        auto* col = new QWidget(cards_box_);
        auto* vl = new QVBoxLayout(col);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(style::u(2));
        vl->addWidget(sp_section_title(g, col));
        for (const ExampleCard& c : cards) {
            if (c.group != g)
                continue;
            auto* card = new QPushButton(col);
            card->setObjectName(style::kCard);
            card->setCursor(Qt::PointingHandCursor);
            card->setAutoDefault(true); // Enter fills the composer, like a click or Space
            card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
            card->setAccessibleName(c.title);
            card->setAccessibleDescription(c.prompt);
            card->setToolTip(tip);
            auto* cl = new QVBoxLayout(card);
            cl->setContentsMargins(style::u(3), style::u(2), style::u(3), style::u(3));
            cl->setSpacing(style::u(1));
            auto* title = new QLabel(c.title, card);
            title->setObjectName(style::kBody);
            title->setTextFormat(Qt::PlainText);
            title->setWordWrap(true);
            title->setFont(style::ui_font(0, QFont::DemiBold));
            title->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* prompt = new QLabel(c.prompt, card);
            prompt->setObjectName(style::kDim);
            prompt->setTextFormat(Qt::PlainText);
            prompt->setWordWrap(true);
            prompt->setAttribute(Qt::WA_TransparentForMouseEvents);
            cl->addWidget(title);
            cl->addWidget(prompt);
            const QString text = c.prompt;
            connect(card, &QPushButton::clicked, this, [this, text]() { emit prompt_chosen(text); });
            vl->addWidget(card);
        }
        vl->addStretch(1);
        group_cols_.append(col);
    }
    reflow(width());
}

void AicStartPage::reflow(int width) {
    if (group_cols_.isEmpty() || !cards_grid_)
        return;
    const int avail = std::min(std::max(0, width - style::u(2)), body_max_width()) - 2 * style::u(6);
    const int card_min = 24 * style::avg_char(0);
    const int gap = style::u(4);
    auto fits = [avail, card_min, gap = gap](int n) { return avail >= n * card_min + (n - 1) * gap; };
    int cols = fits(5) ? 5 : fits(3) ? 3 : fits(2) ? 2 : 1;
    cols = std::min(cols, int(group_cols_.size()));
    if (cols == columns_)
        return;
    columns_ = cols;
    for (QWidget* w : group_cols_)
        cards_grid_->removeWidget(w);
    for (int c = 0; c < 5; ++c)
        cards_grid_->setColumnStretch(c, c < cols ? 1 : 0);
    for (int i = 0; i < int(group_cols_.size()); ++i)
        cards_grid_->addWidget(group_cols_.at(i), i / cols, i % cols);
}

void AicStartPage::rebuild_recent() {
    sp_clear(recent_layout_);
    const QDate today = QDate::currentDate();
    const int row_h = style::text_h(0) + style::u(2);
    for (qsizetype i = 0; i < recent_.size(); ++i) {
        const SessionSummary& s = recent_.at(i);
        if (i > 0) {
            auto* rule = new QFrame(recent_layout_->parentWidget());
            rule->setObjectName(style::kHairline);
            rule->setFrameShape(QFrame::NoFrame);
            rule->setFixedHeight(1);
            recent_layout_->addWidget(rule);
        }
        auto* row = new QPushButton(recent_layout_->parentWidget());
        row->setObjectName(style::kLinkBtn);
        row->setCursor(Qt::PointingHandCursor);
        row->setAutoDefault(true);
        row->setMinimumHeight(row_h);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(style::u(3));
        auto* title = new AicStartElide(row);
        title->setObjectName(style::kBody);
        title->setText(s.title);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        const QString when_text = short_when(s.updated, today);
        auto* when = new QLabel(when_text, row);
        when->setObjectName(style::kDim);
        when->setTextFormat(Qt::PlainText);
        when->setFont(style::mono_font(-2));
        when->setAttribute(Qt::WA_TransparentForMouseEvents);
        hl->addWidget(title, 1);
        hl->addWidget(when);
        row->setAccessibleName(s.title);
        row->setAccessibleDescription(when_text.isEmpty() ? tr("Open this session")
                                                          : tr("Open this session · last change %1").arg(when_text));
        row->setToolTip(QStringLiteral("<p>") + s.title.toHtmlEscaped() + QStringLiteral("</p>"));
        const QString id = s.id;
        connect(row, &QPushButton::clicked, this, [this, id]() { emit session_chosen(id); });
        recent_layout_->addWidget(row);
    }
    recent_box_->setVisible(!recent_.isEmpty());
}

void AicStartPage::fit_metrics() {
    body_->setMaximumWidth(body_max_width());
    title_->setFont(sp_title_font());
    const QList<QWidget*> all = findChildren<QWidget*>();
    for (QWidget* w : all) {
        const QVariant kind = w->property(kSpTickProperty);
        if (kind.isValid())
            w->setFixedSize(sp_tick_size(kind.toString() == QLatin1String("title")));
    }
    rebuild_cards(); // card title fonts, column widths for the new character width
    rebuild_recent();
}

} // namespace fincept::screens::aichat
