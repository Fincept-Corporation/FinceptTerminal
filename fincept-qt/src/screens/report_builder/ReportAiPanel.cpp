#include "screens/report_builder/ReportAiPanel.h"

#include "auth/AuthManager.h"
#include "services/llm/ModelRouter.h"
#include "services/report_builder/ReportAiLlmBackend.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/ModelBadge.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QListView>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace report_ai_panel_detail {

namespace ai = report::ai;

/// Left to right, wrapping to a new line when the width runs out: the buttons stay whole
/// (and readable) at every font size in a narrow column. Hidden widgets take no space.
class RapFlowLayout : public QLayout {
  public:
    explicit RapFlowLayout(QWidget* parent, int spacing) : QLayout(parent), spacing_(spacing) {
        setContentsMargins(0, 0, 0, 0);
    }
    ~RapFlowLayout() override {
        while (QLayoutItem* item = takeAt(0))
            delete item;
    }
    void addItem(QLayoutItem* item) override { items_.append(item); }
    int count() const override { return static_cast<int>(items_.size()); }
    QLayoutItem* itemAt(int index) const override { return items_.value(index); }
    QLayoutItem* takeAt(int index) override {
        return (index >= 0 && index < items_.size()) ? items_.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect& rect) override {
        QLayout::setGeometry(rect);
        arrange(rect, false);
    }
    QSize sizeHint() const override {
        int w = 0, h = 0;
        for (QLayoutItem* item : items_) {
            if (item->isEmpty())
                continue;
            const QSize s = item->sizeHint();
            w += s.width() + (w > 0 ? spacing_ : 0);
            h = std::max(h, s.height());
        }
        return QSize(w, h);
    }
    QSize minimumSize() const override {
        QSize s;
        for (QLayoutItem* item : items_)
            if (!item->isEmpty())
                s = s.expandedTo(item->minimumSize());
        return s;
    }

  private:
    int arrange(const QRect& rect, bool test_only) const {
        int x = rect.x();
        int y = rect.y();
        int line_h = 0;
        for (QLayoutItem* item : items_) {
            if (item->isEmpty())
                continue;
            const QSize s = item->sizeHint();
            if (x > rect.x() && x + s.width() > rect.right() + 1) {
                x = rect.x();
                y += line_h + spacing_;
                line_h = 0;
            }
            if (!test_only)
                item->setGeometry(QRect(QPoint(x, y), QSize(std::min(s.width(), std::max(1, rect.width())), s.height())));
            x += s.width() + spacing_;
            line_h = std::max(line_h, s.height());
        }
        return y + line_h - rect.y();
    }

    QList<QLayoutItem*> items_;
    int spacing_ = 6;
};

QString rap_c(const ui::ColorToken& token) {
    return QString::fromLatin1(token.get());
}

QString rap_span(const QString& style, const QString& html) {
    return QStringLiteral("<span style=\"") + style + QStringLiteral("\">") + html + QStringLiteral("</span>");
}

/// `text` (found at offset `abs_start` of the proposal) as HTML, with the flagged numbers
/// that fall inside it marked.
QString rap_marked(const QString& text, int abs_start, const QVector<ai::NumberFlag>& flags, const QString& flag_style) {
    QString out;
    int pos = 0;
    const int len = static_cast<int>(text.size());
    if (abs_start >= 0) {
        for (const ai::NumberFlag& f : flags) {
            const int s = f.start - abs_start;
            const int e = s + f.length;
            if (e <= pos || s >= len)
                continue;
            const int cs = std::max(s, pos);
            const int ce = std::min(e, len);
            out += text.mid(pos, cs - pos).toHtmlEscaped();
            out += rap_span(flag_style, text.mid(cs, ce - cs).toHtmlEscaped());
            pos = ce;
        }
    }
    out += text.mid(pos).toHtmlEscaped();
    return out;
}

QString rap_short(const QString& text, int max_chars = 48) {
    const QString t = text.simplified();
    return t.size() <= max_chars ? t : t.left(max_chars - 1).trimmed() + QStringLiteral("…");
}

using RapState = services::ReportAiSession::State;

} // namespace report_ai_panel_detail

using namespace report_ai_panel_detail;

// ── Construction ─────────────────────────────────────────────────────────────

ReportAiPanel::ReportAiPanel(QWidget* parent, services::ReportAiBackend* backend) : QWidget(parent) {
    setObjectName(QStringLiteral("rbAiPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    // One selector-scoped sheet, set before any child exists; the theme manager's re-style
    // pass keeps it in step with theme and font changes.
    const QString qss =
        QStringLiteral("#rbAiPanel { background: ") + rap_c(ui::colors::PANEL) + QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QWidget#rbAiHeader { background: ") + rap_c(ui::colors::BG_RAISED) +
        QStringLiteral("; border-bottom: 1px solid ") + rap_c(ui::colors::BORDER_DIM) + QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QWidget#rbAiBody { background: transparent; }") +
        QStringLiteral("#rbAiPanel QScrollArea { background: transparent; border: none; }") +
        QStringLiteral("#rbAiPanel QLabel { color: ") + rap_c(ui::colors::TEXT_SECONDARY) +
        QStringLiteral("; background: transparent; font-size: ") + ui::fonts::px(-2) + QStringLiteral("px; }") +
        QStringLiteral("#rbAiPanel QLabel[rbAiRole=\"title\"] { color: ") + rap_c(ui::colors::TEXT_PRIMARY) +
        QStringLiteral("; font-weight: bold; font-size: ") + ui::fonts::px(-1) + QStringLiteral("px; }") +
        QStringLiteral("#rbAiPanel QLabel[rbAiRole=\"note\"] { color: ") + rap_c(ui::colors::TEXT_TERTIARY) +
        QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QLabel[rbAiRole=\"notice\"] { color: ") + rap_c(ui::colors::TEXT_PRIMARY) +
        QStringLiteral("; background: ") + rap_c(ui::colors::BG_BASE) + QStringLiteral("; border: 1px solid ") +
        rap_c(ui::colors::BORDER_DIM) + QStringLiteral("; border-left: 3px solid ") + rap_c(ui::colors::AMBER) +
        QStringLiteral("; padding: 6px 8px; }") +
        QStringLiteral("#rbAiPanel QLabel[rbAiRole=\"warn\"] { color: ") + rap_c(ui::colors::WARNING) +
        QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QLabel[rbAiRole=\"ok\"] { color: ") + rap_c(ui::colors::SUCCESS) +
        QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QLabel[rbAiRole=\"choice\"] { color: ") + rap_c(ui::colors::TEXT_PRIMARY) +
        QStringLiteral("; font-size: ") + ui::fonts::px(-1) + QStringLiteral("px; }") +
        QStringLiteral("#rbAiPanel QPlainTextEdit, #rbAiPanel QTextBrowser, #rbAiPanel QListView { background: ") +
        rap_c(ui::colors::BG_BASE) + QStringLiteral("; color: ") + rap_c(ui::colors::TEXT_PRIMARY) +
        QStringLiteral("; border: 1px solid ") + rap_c(ui::colors::BORDER_DIM) + QStringLiteral("; font-size: ") +
        ui::fonts::px(-1) + QStringLiteral("px; selection-background-color: ") + rap_c(ui::colors::AMBER) +
        QStringLiteral("; selection-color: ") + rap_c(ui::colors::TEXT_ON_ACCENT) + QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QPushButton { background: ") + rap_c(ui::colors::BG_RAISED) +
        QStringLiteral("; color: ") + rap_c(ui::colors::TEXT_PRIMARY) + QStringLiteral("; border: 1px solid ") +
        rap_c(ui::colors::BORDER_MED) + QStringLiteral("; padding: 4px 10px; font-size: ") + ui::fonts::px(-2) +
        QStringLiteral("px; }") +
        QStringLiteral("#rbAiPanel QPushButton:hover { background: ") + rap_c(ui::colors::BG_HOVER) +
        QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QPushButton:disabled { color: ") + rap_c(ui::colors::TEXT_DIM) +
        QStringLiteral("; border-color: ") + rap_c(ui::colors::BORDER_DIM) + QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QPushButton[rbAiRole=\"primary\"] { background: ") + rap_c(ui::colors::AMBER) +
        QStringLiteral("; color: ") + rap_c(ui::colors::TEXT_ON_ACCENT) + QStringLiteral("; border-color: ") +
        rap_c(ui::colors::AMBER) + QStringLiteral("; font-weight: bold; }") +
        QStringLiteral("#rbAiPanel QPushButton[rbAiRole=\"primary\"]:disabled { background: ") +
        rap_c(ui::colors::BG_RAISED) + QStringLiteral("; color: ") + rap_c(ui::colors::TEXT_DIM) +
        QStringLiteral("; border-color: ") + rap_c(ui::colors::BORDER_DIM) + QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QPushButton[rbAiRole=\"link\"] { background: transparent; border: none; color: ") +
        rap_c(ui::colors::AMBER) + QStringLiteral("; padding: 2px 0px; text-align: left; }") +
        QStringLiteral("#rbAiPanel QPushButton[rbAiRole=\"link\"]:hover { color: ") + rap_c(ui::colors::TEXT_PRIMARY) +
        QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QPushButton[rbAiRole=\"close\"] { background: transparent; border: none; color: ") +
        rap_c(ui::colors::TEXT_SECONDARY) + QStringLiteral("; padding: 0px 6px; font-size: ") + ui::fonts::px(0) +
        QStringLiteral("px; }") +
        QStringLiteral("#rbAiPanel QPushButton[rbAiRole=\"close\"]:hover { color: ") + rap_c(ui::colors::TEXT_PRIMARY) +
        QStringLiteral("; background: ") + rap_c(ui::colors::BG_HOVER) + QStringLiteral("; }") +
        QStringLiteral("#rbAiPanel QRadioButton { background: transparent; }");
    setStyleSheet(qss);

    if (!backend)
        backend = new services::ReportAiLlmBackend(this); // a QObject child: deleted after the session (see ~)
    backend_ = backend;
    session_ = new services::ReportAiSession(backend_, this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("rbAiHeader"));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 6, 4, 6);
    hl->setSpacing(6);
    title_label_ = make_label(header, "title");
    title_label_->setText(tr("AI drafting"));
    hl->addWidget(title_label_, 1);
    close_btn_ = make_button(header, QStringLiteral("✕"), "close");
    close_btn_->setToolTip(tr("Close — nothing in the report changes"));
    close_btn_->setAccessibleName(tr("Close"));
    hl->addWidget(close_btn_, 0, Qt::AlignTop);
    root->addWidget(header);

    stack_ = new QStackedWidget(this);
    root->addWidget(stack_, 1);
    build_ready_page();
    build_stream_page();
    build_proposal_page();
    build_titles_page();
    message_ = new ui::EmptyState(stack_);
    message_->set_compact(true);
    message_->set_alignment(Qt::AlignTop);
    stack_->addWidget(message_);

    tick_ = new QTimer(this);
    tick_->setInterval(1000);
    stream_flush_ = new QTimer(this);
    stream_flush_->setSingleShot(true);
    stream_flush_->setInterval(40); // a burst of tokens becomes one append

    // ── Wiring ──
    connect(close_btn_, &QPushButton::clicked, this, &ReportAiPanel::close_panel);
    connect(send_btn_, &QPushButton::clicked, this, &ReportAiPanel::on_send);
    connect(cancel_btn_, &QPushButton::clicked, this, &ReportAiPanel::close_panel);
    connect(facts_toggle_, &QPushButton::toggled, this, [this](bool on) {
        facts_view_->setVisible(on);
        render_ready(session_->model_info());
    });
    connect(badge_, &ui::ModelBadge::choice_changed, this, &ReportAiPanel::refresh_ready);
    connect(stop_btn_, &QPushButton::clicked, session_, &services::ReportAiSession::cancel);
    connect(accept_all_btn_, &QPushButton::clicked, this, [this]() { session_->accept_all(); });
    connect(accept_sel_btn_, &QPushButton::clicked, this, &ReportAiPanel::on_accept_selected);
    auto retry = [this]() {
        if (!session_->retry() && session_->model_info().needs_setup)
            show_message(Message::NeedsSetup);
    };
    connect(retry_btn_, &QPushButton::clicked, this, retry);
    connect(title_retry_btn_, &QPushButton::clicked, this, retry);
    connect(discard_btn_, &QPushButton::clicked, this, &ReportAiPanel::close_panel);
    connect(title_discard_btn_, &QPushButton::clicked, this, &ReportAiPanel::close_panel);
    connect(use_title_btn_, &QPushButton::clicked, this, [this]() {
        const int i = title_group_->checkedId();
        const QStringList titles = session_->titles();
        if (i >= 0 && i < titles.size())
            session_->use_title(titles[i]);
    });
    connect(hunk_model_, &QStandardItemModel::itemChanged, this, &ReportAiPanel::update_accept_selected);
    connect(message_, &ui::EmptyState::primary_clicked, this, &ReportAiPanel::on_message_primary);
    connect(message_, &ui::EmptyState::secondary_clicked, this, &ReportAiPanel::on_message_secondary);
    connect(tick_, &QTimer::timeout, this, &ReportAiPanel::render_stream_status);
    connect(stream_flush_, &QTimer::timeout, this, &ReportAiPanel::flush_stream);

    connect(session_, &services::ReportAiSession::state_changed, this, &ReportAiPanel::sync_to_state);
    connect(session_, &services::ReportAiSession::stream_updated, this, [this](const QString& text) {
        stream_pending_ = text;
        schedule_stream_flush();
    });
    connect(session_, &services::ReportAiSession::apply_requested, this,
            [this](const services::ApplyResult& r) {
                emit apply_requested(r);
                emit closed();
            });

    // Who answers / whether the user is signed in can change while the panel waits.
    connect(&ai_chat::ModelRouter::instance(), &ai_chat::ModelRouter::routes_changed, this,
            &ReportAiPanel::refresh_ready);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &ReportAiPanel::refresh_ready);

    // Appearance: the diff HTML carries colours, so a theme change re-renders it (nothing
    // else here); sizes follow the font once every widget has its new style.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) {
                if (stack_->currentWidget() == proposal_page_)
                    render_diff_html();
            });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, &ReportAiPanel::refit);

    show_message(Message::Idle);
    refit();
}

ReportAiPanel::~ReportAiPanel() {
    // The session first: it cancels a request in flight through the backend, which (when
    // the panel owns it) is a child deleted after this destructor.
    delete session_;
    session_ = nullptr;
}

QLabel* ReportAiPanel::make_label(QWidget* parent, const char* role, bool wrap) {
    auto* l = new QLabel(parent);
    l->setTextFormat(Qt::PlainText); // model and report text is never interpreted as HTML
    l->setWordWrap(wrap);
    if (role)
        l->setProperty("rbAiRole", QLatin1String(role));
    return l;
}

QPushButton* ReportAiPanel::make_button(QWidget* parent, const QString& text, const char* role) {
    auto* b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoDefault(false);
    if (role)
        b->setProperty("rbAiRole", QLatin1String(role));
    return b;
}

QWidget* ReportAiPanel::button_row(QWidget* parent, const QList<QPushButton*>& buttons) {
    auto* row = new QWidget(parent);
    auto* flow = new RapFlowLayout(row, 6);
    for (QPushButton* b : buttons)
        flow->addWidget(b);
    return row;
}

void ReportAiPanel::build_ready_page() {
    auto* scroll = new QScrollArea(stack_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("rbAiBody"));
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    badge_ = new ui::ModelBadge(QString::fromLatin1(services::kReportDraftFeature), body);
    vl->addWidget(badge_, 0, Qt::AlignLeft);
    cost_label_ = make_label(body, "note");
    vl->addWidget(cost_label_);
    notice_label_ = make_label(body, "notice");
    vl->addWidget(notice_label_);
    facts_toggle_ = make_button(body, QString(), "link");
    facts_toggle_->setCheckable(true);
    vl->addWidget(facts_toggle_, 0, Qt::AlignLeft);
    facts_model_ = new QStandardItemModel(this);
    facts_view_ = new QListView(body);
    facts_view_->setModel(facts_model_);
    facts_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    facts_view_->setSelectionMode(QAbstractItemView::NoSelection);
    facts_view_->setWordWrap(true);
    facts_view_->setUniformItemSizes(false);
    facts_view_->setResizeMode(QListView::Adjust);
    facts_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    facts_view_->setVisible(false);
    vl->addWidget(facts_view_);
    auto* hint = make_label(body, "note");
    hint->setText(tr("Nothing is sent until you click Send. The answer comes back as a proposal: the report changes "
                     "only when you accept it."));
    vl->addWidget(hint);
    send_btn_ = make_button(body, tr("Send"), "primary");
    send_btn_->setProperty("fincept_agent_risk", "spend"); // agents: sends a billed model request
    cancel_btn_ = make_button(body, tr("Cancel"));
    vl->addWidget(button_row(body, {send_btn_, cancel_btn_}));
    vl->addStretch(1);
    scroll->setWidget(body);
    ready_page_ = scroll;
    stack_->addWidget(ready_page_);
}

void ReportAiPanel::build_stream_page() {
    stream_page_ = new QWidget(stack_);
    auto* vl = new QVBoxLayout(stream_page_);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);
    stream_status_ = make_label(stream_page_, nullptr);
    vl->addWidget(stream_status_);
    stream_view_ = new QPlainTextEdit(stream_page_);
    stream_view_->setReadOnly(true);
    stream_view_->setPlaceholderText(tr("Waiting for the first words…"));
    vl->addWidget(stream_view_, 1);
    auto* note = make_label(stream_page_, "note");
    note->setText(tr("This is a draft: the report does not change until you accept it."));
    vl->addWidget(note);
    stop_btn_ = make_button(stream_page_, tr("Stop"));
    vl->addWidget(button_row(stream_page_, {stop_btn_}));
    stack_->addWidget(stream_page_);
}

void ReportAiPanel::build_proposal_page() {
    proposal_page_ = new QWidget(stack_);
    auto* vl = new QVBoxLayout(proposal_page_);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(6);
    meta_label_ = make_label(proposal_page_, "note");
    vl->addWidget(meta_label_);
    fallback_label_ = make_label(proposal_page_, "warn");
    vl->addWidget(fallback_label_);
    diff_view_ = new QTextBrowser(proposal_page_);
    diff_view_->setOpenLinks(false); // model output: never navigate
    diff_view_->setOpenExternalLinks(false);
    diff_view_->document()->setDocumentMargin(6);
    vl->addWidget(diff_view_, 1);
    flags_label_ = make_label(proposal_page_, "warn");
    vl->addWidget(flags_label_);
    nochange_label_ = make_label(proposal_page_, "ok");
    nochange_label_->setText(tr("The model suggests no changes."));
    vl->addWidget(nochange_label_);
    hunks_label_ = make_label(proposal_page_, nullptr);
    hunks_label_->setText(tr("Changes to keep:"));
    vl->addWidget(hunks_label_);
    hunk_model_ = new QStandardItemModel(this);
    hunk_view_ = new QListView(proposal_page_);
    hunk_view_->setModel(hunk_model_);
    hunk_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hunk_view_->setSelectionMode(QAbstractItemView::NoSelection);
    hunk_view_->setWordWrap(true);
    hunk_view_->setUniformItemSizes(false);
    hunk_view_->setResizeMode(QListView::Adjust);
    hunk_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    vl->addWidget(hunk_view_);
    accept_all_btn_ = make_button(proposal_page_, tr("Accept all"), "primary");
    accept_sel_btn_ = make_button(proposal_page_, tr("Accept selected"));
    retry_btn_ = make_button(proposal_page_, tr("Retry"));
    retry_btn_->setProperty("fincept_agent_risk", "spend"); // agents: re-sends a billed model request
    discard_btn_ = make_button(proposal_page_, tr("Discard"));
    vl->addWidget(button_row(proposal_page_, {accept_all_btn_, accept_sel_btn_, retry_btn_, discard_btn_}));
    stack_->addWidget(proposal_page_);
}

void ReportAiPanel::build_titles_page() {
    auto* scroll = new QScrollArea(stack_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("rbAiBody"));
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);
    auto* lead = make_label(body, nullptr);
    lead->setText(tr("Pick a title for the report:"));
    vl->addWidget(lead);
    title_group_ = new QButtonGroup(this);
    title_group_->setExclusive(true);
    for (int i = 0; i < 3; ++i) {
        auto* row = new QWidget(body);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        auto* radio = new QRadioButton(row);
        radio->setCursor(Qt::PointingHandCursor);
        // The title is a word-wrapped label beside the radio (a radio button's own text
        // cannot wrap in a narrow column); a click on it picks the radio.
        auto* label = make_label(row, "choice");
        label->setCursor(Qt::PointingHandCursor);
        label->installEventFilter(this);
        hl->addWidget(radio, 0, Qt::AlignTop);
        hl->addWidget(label, 1);
        title_group_->addButton(radio, i);
        title_radios_ << radio;
        title_labels_ << label;
        vl->addWidget(row);
    }
    title_flags_label_ = make_label(body, "warn");
    vl->addWidget(title_flags_label_);
    use_title_btn_ = make_button(body, tr("Use title"), "primary");
    title_retry_btn_ = make_button(body, tr("Retry"));
    title_retry_btn_->setProperty("fincept_agent_risk", "spend"); // agents: re-sends a billed model request
    title_discard_btn_ = make_button(body, tr("Discard"));
    vl->addWidget(button_row(body, {use_title_btn_, title_retry_btn_, title_discard_btn_}));
    vl->addStretch(1);
    scroll->setWidget(body);
    titles_page_ = scroll;
    stack_->addWidget(titles_page_);
}

// ── Public ───────────────────────────────────────────────────────────────────

void ReportAiPanel::open(const services::AiRequest& request) {
    title_label_->setText(tr("AI · %1").arg(action_title(request.action, request.tone)));
    facts_toggle_->setChecked(false);
    session_->prepare(request); // -> state_changed -> sync_to_state
}

bool ReportAiPanel::busy() const {
    return session_ && session_->state() == RapState::Streaming;
}

int ReportAiPanel::preferred_width() const {
    const QFontMetrics fm(ui::fonts::font(-2));
    return std::clamp(fm.averageCharWidth() * 40, 300, 480);
}

QSize ReportAiPanel::sizeHint() const {
    return QSize(preferred_width(), QWidget::sizeHint().height());
}

QString ReportAiPanel::action_title(report::ai::Action action, const QString& tone) {
    switch (action) {
        case ai::Action::DraftFromData:
            return tr("Draft from data");
        case ai::Action::Rewrite:
            return tr("Rewrite");
        case ai::Action::Shorten:
            return tr("Shorten");
        case ai::Action::Expand:
            return tr("Expand");
        case ai::Action::FixGrammar:
            return tr("Fix grammar");
        case ai::Action::ChangeTone:
            return tone.trimmed().isEmpty() ? tr("Change tone") : tr("Change tone: %1").arg(tone.trimmed());
        case ai::Action::SummariseReport:
            return tr("Executive summary");
        case ai::Action::SuggestTitle:
            return tr("Suggest a title");
    }
    return tr("AI drafting");
}

void ReportAiPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh_ready(); // the model or the sign-in may have changed while hidden
}

bool ReportAiPanel::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        for (int i = 0; i < title_labels_.size(); ++i)
            if (watched == title_labels_[i]) {
                title_radios_[i]->setChecked(true);
                break;
            }
    }
    return QWidget::eventFilter(watched, event);
}

// ── States ───────────────────────────────────────────────────────────────────

void ReportAiPanel::sync_to_state() {
    const RapState st = session_->state();
    if (st != RapState::Streaming) {
        tick_->stop();
        if (stream_flush_->isActive()) {
            stream_flush_->stop();
            flush_stream();
        }
    }
    switch (st) {
        case RapState::Idle:
            show_message(Message::Idle);
            break;
        case RapState::Ready: {
            const services::AiModelInfo info = session_->model_info();
            if (info.needs_setup) {
                show_message(Message::NeedsSetup);
            } else {
                render_ready(info);
                stack_->setCurrentWidget(ready_page_);
            }
            break;
        }
        case RapState::Streaming:
            stream_model_ = session_->model_info().label;
            stream_pending_.clear();
            stream_rendered_ = 0;
            stream_view_->clear();
            render_stream_status();
            tick_->start();
            stack_->setCurrentWidget(stream_page_);
            break;
        case RapState::Proposal:
            if (session_->request().action == ai::Action::SuggestTitle)
                render_titles();
            else
                render_proposal();
            break;
        case RapState::Failed:
            show_message(Message::Failed);
            break;
        case RapState::Cancelled:
            show_message(Message::Cancelled);
            break;
    }
}

void ReportAiPanel::refresh_ready() {
    if (!session_)
        return;
    const bool setup_shown = stack_->currentWidget() == message_ && message_mode_ == Message::NeedsSetup;
    if (session_->state() == RapState::Ready || setup_shown)
        sync_to_state();
}

void ReportAiPanel::render_ready(const services::AiModelInfo& info) {
    const ai::Prompt& p = session_->prompt();
    const bool report_action = ai::is_report_action(session_->request().action);
    cost_label_->setText(info.cost_note);
    cost_label_->setVisible(!info.cost_note.isEmpty());

    const QString chars = QLocale().toString(p.text_chars);
    QString what;
    if (report_action)
        what = tr("the report's title, headings and text excerpts (%1 characters)").arg(chars);
    else if (session_->has_selection())
        what = tr("the selected passage (%1 characters)").arg(chars);
    else
        what = tr("this block's text (%1 characters)").arg(chars);
    if (!report_action && !p.heading.isEmpty())
        what += tr(", its section heading");
    const QString figures = p.fact_count == 0   ? tr("no figures")
                            : p.fact_count == 1 ? tr("1 figure")
                                                : tr("%1 figures").arg(p.fact_count);
    const QString model = info.label.isEmpty() ? tr("the model") : info.label;
    notice_label_->setText(tr("Sent to %1: %2 and %3 from this report's data blocks — nothing else (no credentials, "
                              "no other reports).")
                               .arg(model, what, figures));

    const QVector<ai::Fact>& facts = session_->facts();
    if (facts_model_->rowCount() != facts.size() ||
        (!facts.isEmpty() && facts_model_->item(0)->toolTip() != ai::fact_line(facts.first()))) {
        facts_model_->clear();
        for (const ai::Fact& f : facts) {
            auto* item = new QStandardItem(f.id + QStringLiteral("  ") + f.label + QStringLiteral(": ") + f.value);
            item->setToolTip(ai::fact_line(f));
            item->setEditable(false);
            facts_model_->appendRow(item);
        }
    }
    const bool shown = facts_toggle_->isChecked() && !facts.isEmpty();
    facts_toggle_->setEnabled(!facts.isEmpty());
    facts_toggle_->setText(shown ? tr("Hide the figures")
                           : facts.isEmpty()    ? tr("No figures would be sent")
                           : facts.size() == 1  ? tr("Show the figure that would be sent")
                                                : tr("Show the %1 figures that would be sent").arg(facts.size()));
    if (facts_view_->isHidden() == shown)
        facts_view_->setVisible(shown);
    send_btn_->setEnabled(!p.user.isEmpty());
    send_btn_->setToolTip(tr("Sends the text and figures above to %1").arg(model));
}

void ReportAiPanel::render_stream_status() {
    const qint64 secs = session_->elapsed_ms() / 1000;
    stream_status_->setText(tr("Writing with %1 · %2 s").arg(stream_model_.isEmpty() ? tr("the model") : stream_model_)
                                .arg(secs));
}

void ReportAiPanel::schedule_stream_flush() {
    if (!stream_flush_->isActive())
        stream_flush_->start();
}

void ReportAiPanel::flush_stream() {
    const QString& t = stream_pending_;
    if (t.size() < stream_rendered_) {
        stream_view_->setPlainText(t); // the model restarted its answer
    } else if (t.size() > stream_rendered_) {
        QTextCursor cursor(stream_view_->document());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(t.mid(stream_rendered_));
    }
    stream_rendered_ = static_cast<int>(t.size());
    stream_view_->verticalScrollBar()->setValue(stream_view_->verticalScrollBar()->maximum());
}

void ReportAiPanel::render_proposal() {
    const services::AiModelInfo info = session_->model_info();
    const QString who = !session_->route_label().isEmpty() ? session_->route_label() : info.label;
    meta_label_->setText(tr("Proposal from %1 — the report changes only when you accept.").arg(who));
    fallback_label_->setText(session_->fallback_notice());
    fallback_label_->setVisible(!session_->fallback_notice().isEmpty());
    render_diff_html();
    render_flags(flags_label_);

    const bool summary = session_->request().action == ai::Action::SummariseReport;
    const bool nothing = session_->no_change();
    nochange_label_->setVisible(nothing && !summary);
    const QVector<ai::Hunk>& hunks = session_->hunks();
    const QVector<int> changes = ai::change_hunks(hunks);
    const bool part = !summary && changes.size() > 1;

    {
        const QSignalBlocker block(hunk_model_);
        hunk_model_->clear();
        if (part) {
            int n = 0;
            for (int idx : changes) {
                const ai::Hunk& h = hunks[idx];
                ++n;
                QString text;
                if (h.kind == ai::Hunk::Kind::Replace)
                    text = tr("%1. Replace “%2” with “%3”").arg(n).arg(rap_short(h.old_text, 32), rap_short(h.new_text, 32));
                else if (h.kind == ai::Hunk::Kind::Insert)
                    text = tr("%1. Add “%2”").arg(n).arg(rap_short(h.new_text));
                else
                    text = tr("%1. Remove “%2”").arg(n).arg(rap_short(h.old_text));
                auto* item = new QStandardItem(text);
                item->setCheckable(true);
                item->setCheckState(Qt::Checked);
                item->setEditable(false);
                item->setData(idx, Qt::UserRole + 1);
                item->setToolTip(h.kind == ai::Hunk::Kind::Delete ? h.old_text : h.new_text);
                hunk_model_->appendRow(item);
            }
        }
    }
    hunks_label_->setVisible(part);
    hunk_view_->setVisible(part);
    accept_sel_btn_->setVisible(part);
    accept_all_btn_->setText(summary ? tr("Insert summary") : part ? tr("Accept all") : tr("Accept"));
    accept_all_btn_->setEnabled(!nothing);
    update_accept_selected();
    refit();
    stack_->setCurrentWidget(proposal_page_);
}

void ReportAiPanel::render_diff_html() {
    const QVector<ai::Hunk>& hunks = session_->hunks();
    const QVector<ai::NumberFlag>& flags = session_->flags();
    const bool summary = session_->request().action == ai::Action::SummariseReport;
    // State colours (insert / delete / check), never the price-direction tokens.
    const QString ins_style = QStringLiteral("background-color:%1;color:%2;")
                                  .arg(ui::colors::tint(QColor(ui::colors::SUCCESS()), 0.24), rap_c(ui::colors::TEXT_PRIMARY));
    const QString del_style =
        QStringLiteral("color:%1;text-decoration:line-through;").arg(rap_c(ui::colors::DANGER));
    const QString flag_style = QStringLiteral("background-color:%1;color:%2;font-weight:bold;")
                                   .arg(ui::colors::tint(QColor(ui::colors::WARNING()), 0.42), rap_c(ui::colors::TEXT_PRIMARY));
    const QString same_style = QStringLiteral("color:%1;").arg(rap_c(ui::colors::TEXT_SECONDARY));
    const QString mark_style = QStringLiteral("color:%1;font-weight:bold;").arg(rap_c(ui::colors::AMBER));
    const bool numbered = !summary && ai::change_hunks(hunks).size() > 1;

    QString html;
    int change_no = 0;
    for (const ai::Hunk& h : hunks) {
        QString body;
        switch (h.kind) {
            case ai::Hunk::Kind::Same:
                body = rap_span(same_style, h.old_text.toHtmlEscaped());
                break;
            case ai::Hunk::Kind::Insert:
                body = summary ? rap_marked(h.new_text, h.new_start, flags, flag_style)
                               : rap_span(ins_style, rap_marked(h.new_text, h.new_start, flags, flag_style));
                break;
            case ai::Hunk::Kind::Delete:
                body = rap_span(del_style, h.old_text.toHtmlEscaped());
                break;
            case ai::Hunk::Kind::Replace: {
                const QVector<ai::WordOp> ops = ai::diff_words(h.old_text, h.new_text);
                if (ops.isEmpty()) { // too long to align word by word: both paragraphs whole
                    body = rap_span(del_style, h.old_text.toHtmlEscaped()) + QStringLiteral("<br/>") +
                           rap_span(ins_style, rap_marked(h.new_text, h.new_start, flags, flag_style));
                    break;
                }
                int pos = h.new_start;
                for (const ai::WordOp& op : ops) {
                    if (op.kind == ai::WordOp::Kind::Delete) {
                        body += rap_span(del_style, op.text.toHtmlEscaped());
                        continue;
                    }
                    const QString marked = rap_marked(op.text, pos, flags, flag_style);
                    body += op.kind == ai::WordOp::Kind::Insert ? rap_span(ins_style, marked) : marked;
                    pos += static_cast<int>(op.text.size());
                }
                break;
            }
        }
        if (numbered && h.kind != ai::Hunk::Kind::Same)
            body = rap_span(mark_style, QStringLiteral("[%1] ").arg(++change_no)) + body;
        html += QStringLiteral("<p style=\"margin:0px 0px 10px 0px;white-space:pre-wrap;\">") + body +
                QStringLiteral("</p>");
    }
    if (hunks.isEmpty())
        html = QStringLiteral("<p>") + tr("(empty)").toHtmlEscaped() + QStringLiteral("</p>");
    diff_view_->setHtml(html);
}

void ReportAiPanel::render_flags(QLabel* label) {
    const QVector<ai::NumberFlag>& flags = session_->flags();
    if (flags.isEmpty()) {
        label->clear();
        label->setVisible(false);
        return;
    }
    QStringList texts;
    for (int i = 0; i < flags.size() && i < 8; ++i)
        texts << flags[i].text;
    if (flags.size() > 8)
        texts << QStringLiteral("…");
    const QString list = texts.join(QStringLiteral(", "));
    label->setText(flags.size() == 1
                       ? tr("1 number is not in the report's data: %1. Check it before you accept.").arg(list)
                       : tr("%1 numbers are not in the report's data: %2. Check them before you accept.")
                             .arg(flags.size())
                             .arg(list));
    label->setVisible(true);
}

void ReportAiPanel::render_titles() {
    const QStringList titles = session_->titles();
    for (int i = 0; i < title_radios_.size(); ++i) {
        const bool used = i < titles.size();
        title_radios_[i]->setVisible(used);
        title_labels_[i]->setVisible(used);
        title_labels_[i]->setText(used ? titles[i] : QString());
        title_radios_[i]->setAccessibleName(used ? titles[i] : QString());
    }
    if (!title_radios_.isEmpty() && !titles.isEmpty())
        title_radios_.first()->setChecked(true);
    use_title_btn_->setEnabled(!titles.isEmpty());
    render_flags(title_flags_label_);
    stack_->setCurrentWidget(titles_page_);
}

void ReportAiPanel::show_message(Message m) {
    message_mode_ = m;
    ui::EmptyState::Spec s;
    switch (m) {
        case Message::Idle:
            s.glyph = QStringLiteral("✎");
            s.glyph_tone = ui::EmptyState::Tone::Accent;
            s.title = tr("AI drafting");
            s.message = tr("Pick an AI action for a text block, or summarise the report or suggest its title. "
                           "Nothing is sent until you click Send.");
            break;
        case Message::NeedsSetup:
            s.glyph = QStringLiteral("⚙");
            s.glyph_tone = ui::EmptyState::Tone::Warning;
            s.title = tr("Choose a model first");
            s.message = session_->model_info().setup_reason;
            if (s.message.isEmpty())
                s.message = tr("Set up a model in Settings › LLM Configuration.");
            s.note = tr("Nothing has been sent.");
            s.note_tone = ui::EmptyState::Tone::Info;
            s.primary = tr("OPEN SETTINGS");
            s.secondary = tr("CLOSE");
            break;
        case Message::Failed: {
            const bool can_retry = !session_->prompt().user.isEmpty();
            s.glyph = QStringLiteral("!");
            s.glyph_tone = ui::EmptyState::Tone::Danger;
            s.title = tr("No draft");
            s.message = session_->error();
            s.primary = can_retry ? tr("RETRY") : tr("CLOSE");
            s.secondary = session_->settings_hint() ? tr("OPEN SETTINGS") : can_retry ? tr("CLOSE") : QString();
            break;
        }
        case Message::Cancelled:
            s.glyph = QStringLiteral("■");
            s.glyph_tone = ui::EmptyState::Tone::Neutral;
            s.title = tr("Stopped");
            s.message = session_->error();
            s.primary = tr("SEND AGAIN");
            s.secondary = tr("CLOSE");
            break;
    }
    message_->apply(s);
    stack_->setCurrentWidget(message_);
}

void ReportAiPanel::on_message_primary() {
    switch (message_mode_) {
        case Message::Idle:
            break;
        case Message::NeedsSetup:
            ui::open_llm_settings();
            break;
        case Message::Failed:
            if (session_->prompt().user.isEmpty())
                close_panel();
            else if (!session_->retry() && session_->model_info().needs_setup)
                show_message(Message::NeedsSetup);
            break;
        case Message::Cancelled:
            if (!session_->retry() && session_->model_info().needs_setup)
                show_message(Message::NeedsSetup);
            break;
    }
}

void ReportAiPanel::on_message_secondary() {
    if (message_mode_ == Message::Failed && session_->settings_hint())
        ui::open_llm_settings();
    else
        close_panel();
}

void ReportAiPanel::on_send() {
    if (!session_->send() && session_->model_info().needs_setup)
        show_message(Message::NeedsSetup);
}

void ReportAiPanel::on_accept_selected() {
    QSet<int> chosen;
    for (int r = 0; r < hunk_model_->rowCount(); ++r) {
        const QStandardItem* item = hunk_model_->item(r);
        if (item && item->checkState() == Qt::Checked)
            chosen.insert(item->data(Qt::UserRole + 1).toInt());
    }
    session_->accept_hunks(chosen);
}

void ReportAiPanel::update_accept_selected() {
    int checked = 0;
    for (int r = 0; r < hunk_model_->rowCount(); ++r) {
        const QStandardItem* item = hunk_model_->item(r);
        if (item && item->checkState() == Qt::Checked)
            ++checked;
    }
    accept_sel_btn_->setEnabled(checked > 0);
    accept_sel_btn_->setText(checked > 0 && checked < hunk_model_->rowCount()
                                 ? tr("Accept %1 of %2").arg(checked).arg(hunk_model_->rowCount())
                                 : tr("Accept selected"));
}

void ReportAiPanel::close_panel() {
    session_->discard(); // cancels a request in flight; nothing is applied
    emit closed();
}

void ReportAiPanel::refit() {
    // Sizes from the font the views use now (stylesheet font-size included).
    const QFontMetrics fm(facts_view_->font());
    const int line = fm.lineSpacing();
    facts_view_->setMaximumHeight(line * 10 + 8);
    const int rows = std::clamp(hunk_model_->rowCount(), 1, 4);
    hunk_view_->setMaximumHeight(line * rows * 2 + 8);
    diff_view_->setMinimumHeight(line * 5);
    stream_view_->setMinimumHeight(line * 5);
    updateGeometry();
}

} // namespace fincept::screens
