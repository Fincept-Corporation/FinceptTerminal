// ChatStartPage.cpp — Chat Mode's start page (2026-11-17 revamp). See ChatStartPage.h.
//
// A centred column (max width from font metrics) in a scroll area: headline,
// purpose line, example cards (painted QPushButtons in a ui::FlowLayout), recent
// runs (painted rows, updated in place), and a SET UP block listing only what is
// missing. One selector stylesheet on the root built before any child exists; the
// painted cards / rows read the live theme. refresh() is cheap (no I/O) and runs
// on show and — debounced, only while shown — when runs or requirements change.

#include "screens/chat_mode/ChatStartPage.h"

#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/llm/ModelRouter.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace chat_start_page_detail {

constexpr int kCspMaxRuns = 6;
constexpr int kCspColumnPx = 1040; ///< the column's max width at the default font size
constexpr int kCspRefreshMs = 250;

struct CspExample {
    QString category;
    QString title;
    QString blurb;
    QString prompt;
};

QVector<CspExample> csp_examples() {
    return {
        {ChatStartPage::tr("EQUITIES"), ChatStartPage::tr("Earnings prep"),
         ChatStartPage::tr("Consensus, guidance and what to watch before the next print."),
         ChatStartPage::tr("Prepare me for the next earnings report of AAPL: consensus EPS and revenue, last "
                           "quarter's guidance, the key metrics to watch, recent news, and how the stock moved after "
                           "the last four reports. End with a one-paragraph view.")},
        {ChatStartPage::tr("PORTFOLIO"), ChatStartPage::tr("Portfolio review"),
         ChatStartPage::tr("Allocation, concentration, winners and losers, rebalancing ideas."),
         ChatStartPage::tr("Review my portfolio: allocation by sector and asset class, concentration in single "
                           "names, the biggest winners and losers, key risk figures, and three concrete rebalancing "
                           "ideas with their trade-offs.")},
        {ChatStartPage::tr("MARKETS"), ChatStartPage::tr("Sector screen"),
         ChatStartPage::tr("Rank a sector's largest names on valuation, growth and momentum."),
         ChatStartPage::tr("Screen the US semiconductor sector: compare the ten largest names on P/E, EV/EBITDA, "
                           "revenue growth, operating margin and 3-month performance in one table, then rank them "
                           "and explain the top three.")},
        {ChatStartPage::tr("STRATEGY"), ChatStartPage::tr("Backtest idea"),
         ChatStartPage::tr("Turn an idea into rules, data, benchmark and metrics to test."),
         ChatStartPage::tr("Design a simple momentum strategy for large-cap US equities: state the entry and exit "
                           "rules, position sizing, the data and period to use, the benchmark, and the metrics that "
                           "would show it works. Point out the main risks of overfitting.")},
        {ChatStartPage::tr("MACRO"), ChatStartPage::tr("Macro brief"),
         ChatStartPage::tr("Rates, inflation, central banks, FX and commodities this week."),
         ChatStartPage::tr("Write a macro brief for this week: rates and the yield curve, inflation data, the "
                           "central bank calendar, FX and commodities, and what each means for equities. Keep it to "
                           "one page with a short table of the key numbers.")},
        {ChatStartPage::tr("RISK"), ChatStartPage::tr("Risk check"),
         ChatStartPage::tr("Volatility, beta, drawdown, VaR and what drives the risk."),
         ChatStartPage::tr("Run a risk check on my portfolio: volatility, beta to the market, maximum drawdown, "
                           "1-day Value at Risk, and the positions that drive most of the risk. Suggest how to cut "
                           "the largest risk without changing the overall exposure much.")},
    };
}

QString csp_style() {
    const QString base = ui::colors::BG_BASE;
    const QString surface = ui::colors::BG_SURFACE;
    const QString raised = ui::colors::BG_RAISED;
    const QString dim_border = ui::colors::BORDER_DIM;
    const QString med_border = ui::colors::BORDER_MED;
    const QString text = ui::colors::TEXT_PRIMARY;
    const QString text2 = ui::colors::TEXT_SECONDARY;
    const QString text3 = ui::colors::TEXT_TERTIARY;
    const QString accent = ui::colors::AMBER;
    const QString warning = ui::colors::WARNING;
    // Font sizes through ui::fonts::px() (concatenation: the number carries its offset).
    const auto px = [](int offset) { return ui::fonts::px(offset); };

    QString s;
    s.reserve(4000);
    s += "#chatStartPage{background:" + base + ";}";
    s += "#chatStartPage QScrollArea#cspScroll{background:" + base + ";border:none;}";
    s += "#chatStartPage QWidget#cspCanvas{background:" + base + ";}";
    s += "#chatStartPage QWidget#cspColumn{background:transparent;}";
    s += "#chatStartPage QLabel#cspEyebrow{background:transparent;color:" + accent + ";font-size:" + px(-4) +
         "px;font-weight:700;letter-spacing:2px;}";
    s += "#chatStartPage QLabel#cspHeadline{background:transparent;color:" + text + ";font-size:" + px(12) +
         "px;font-weight:700;letter-spacing:3px;}";
    s += "#chatStartPage QFrame#cspRule{background:" + accent + ";border:none;}";
    s += "#chatStartPage QLabel#cspPurpose{background:transparent;color:" + text2 + ";font-size:" + px(0) + "px;}";
    s += "#chatStartPage QLabel#cspNote{background:transparent;color:" + text3 + ";font-size:" + px(-2) + "px;}";
    s += "#chatStartPage QPushButton#cspLink{background:transparent;color:" + accent + ";border:1px solid " +
         med_border + ";padding:2px 8px;font-size:" + px(-4) + "px;font-weight:700;letter-spacing:1px;}";
    s += "#chatStartPage QPushButton#cspLink:hover,#chatStartPage QPushButton#cspLink:focus{border-color:" + accent +
         ";}";
    s += "#chatStartPage QFrame#cspSetupRow{background:" + surface + ";border:1px solid " + dim_border +
         ";border-left:2px solid " + warning + ";}";
    s += "#chatStartPage QLabel#cspSetupTitle{background:transparent;color:" + text + ";font-size:" + px(-2) +
         "px;font-weight:700;}";
    s += "#chatStartPage QLabel#cspSetupText{background:transparent;color:" + text2 + ";font-size:" + px(-3) + "px;}";
    s += "#chatStartPage QPushButton#cspSetupBtn{background:" + raised + ";color:" + text + ";border:1px solid " +
         med_border + ";padding:3px 10px;font-size:" + px(-4) + "px;font-weight:700;letter-spacing:1px;}";
    s += "#chatStartPage QPushButton#cspSetupBtn:hover,#chatStartPage QPushButton#cspSetupBtn:focus{color:" + accent +
         ";border-color:" + accent + ";}";
    s += "#chatStartPage QScrollArea#cspScroll QScrollBar:vertical{background:transparent;width:6px;margin:2px;}";
    s += "#chatStartPage QScrollArea#cspScroll QScrollBar::handle:vertical{background:" + med_border +
         ";min-height:24px;}";
    s += "#chatStartPage QScrollArea#cspScroll QScrollBar::add-line:vertical,"
         "#chatStartPage QScrollArea#cspScroll QScrollBar::sub-line:vertical{height:0;}";
    return s;
}

/// "14:32" today, "12 Nov 14:32" before.
QString csp_when(qint64 ms) {
    if (ms <= 0)
        return {};
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
    if (dt.date() == QDate::currentDate())
        return dt.toString(QStringLiteral("HH:mm"));
    return QLocale().toString(dt, QStringLiteral("dd MMM HH:mm"));
}

QString csp_engine_text(const QString& engine) {
    if (engine == QLatin1String(agt::kEngineLocal))
        return ChatStartPage::tr("LOCAL");
    if (engine == QLatin1String(rv::kHarnessEngineId))
        return ChatStartPage::tr("HARNESS");
    return ChatStartPage::tr("CLOUD");
}

/// Line 1 fits `width`; line 2 takes the rest, elided.
std::array<QString, 2> csp_two_lines(const QString& text, const QFontMetrics& fm, int width) {
    std::array<QString, 2> out;
    const QStringList words = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    int i = 0;
    QString line;
    for (; i < words.size(); ++i) {
        const QString next = line.isEmpty() ? words.at(i) : line + QLatin1Char(' ') + words.at(i);
        if (!line.isEmpty() && fm.horizontalAdvance(next) > width)
            break;
        line = next;
    }
    out[0] = fm.elidedText(line, Qt::ElideRight, width);
    QStringList rest;
    for (; i < words.size(); ++i)
        rest << words.at(i);
    out[1] = fm.elidedText(rest.join(QLatin1Char(' ')), Qt::ElideRight, width);
    return out;
}

// ── Example card ─────────────────────────────────────────────────────────────

/// A painted example card: category, title, two lines of description and a
/// "USE ▸" cue; Enter / Space / click emit clicked(). Fixed size from the font.
class CspCard : public QPushButton {
  public:
    explicit CspCard(QWidget* parent) : QPushButton(parent) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void set_content(const CspExample& e) {
        ex_ = e;
        setAccessibleName(e.title);
        setAccessibleDescription(e.blurb + QLatin1Char(' ') + ChatStartPage::tr("Fills the message box; nothing is sent."));
        setToolTip(e.prompt);
        updateGeometry();
        update();
    }
    const CspExample& example() const { return ex_; }

    QSize sizeHint() const override {
        const QFontMetrics fc(ui::fonts::font(-5, QFont::Bold));
        const QFontMetrics ft(ui::fonts::font(-1, QFont::Bold));
        const QFontMetrics fb(ui::fonts::font(-3));
        const int p = pad();
        const int w = fb.averageCharWidth() * 36 + 2 * p;
        const int h = p + fc.height() + 4 + ft.height() + 4 + 2 * fb.lineSpacing() + 6 + fc.height() + p;
        return {w, h};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect r = rect();
        const bool hot = underMouse() || isDown();
        const bool focus = hasFocus();
        p.fillRect(r, QColor(hot ? ui::colors::BG_RAISED() : ui::colors::BG_SURFACE()));
        p.setPen(QColor(hot ? ui::colors::BORDER_MED() : ui::colors::BORDER_DIM()));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(0, 0, -1, -1));
        // Accent edge: the terminal's "live" cue.
        p.fillRect(QRect(r.left(), r.top(), (hot || focus) ? 3 : 2, r.height()),
                   QColor(hot || focus ? ui::colors::AMBER() : ui::colors::BORDER_MED()));
        if (focus) {
            p.setPen(QPen(QColor(ui::colors::AMBER()), 2));
            p.drawRect(r.adjusted(1, 1, -1, -1));
        }

        const QFont fcat = ui::fonts::font(-5, QFont::Bold);
        const QFont ftitle = ui::fonts::font(-1, QFont::Bold);
        const QFont fblurb = ui::fonts::font(-3);
        const QFontMetrics fc(fcat), ft(ftitle), fb(fblurb);
        const int pd = pad();
        const int x = r.left() + pd + 2;
        const int w = std::max(10, r.width() - 2 * pd - 2);
        int y = r.top() + pd;

        p.setFont(fcat);
        p.setPen(QColor(ui::colors::TEXT_TERTIARY()));
        p.drawText(QRect(x, y, w, fc.height()), Qt::AlignLeft | Qt::AlignVCenter, ex_.category);
        y += fc.height() + 4;

        p.setFont(ftitle);
        p.setPen(QColor(ui::colors::TEXT_PRIMARY()));
        p.drawText(QRect(x, y, w, ft.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   ft.elidedText(ex_.title, Qt::ElideRight, w));
        y += ft.height() + 4;

        p.setFont(fblurb);
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        const std::array<QString, 2> lines = csp_two_lines(ex_.blurb, fb, w);
        for (const QString& l : lines) {
            p.drawText(QRect(x, y, w, fb.lineSpacing()), Qt::AlignLeft | Qt::AlignVCenter, l);
            y += fb.lineSpacing();
        }

        p.setFont(fcat);
        p.setPen(QColor(hot || focus ? ui::colors::AMBER() : ui::colors::TEXT_DIM()));
        p.drawText(QRect(x, r.bottom() - pd - fc.height(), w, fc.height()), Qt::AlignRight | Qt::AlignVCenter,
                   ChatStartPage::tr("USE ▸"));
    }

    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && e->modifiers() == Qt::NoModifier) {
            click();
            return;
        }
        QPushButton::keyPressEvent(e);
    }

  private:
    static int pad() { return std::max(8, QFontMetrics(ui::fonts::font(-3)).height() * 2 / 3); }
    CspExample ex_;
};

// ── Recent run row ───────────────────────────────────────────────────────────

/// One recent run: state pill, title, engine and time. Click / Enter opens it.
class CspRunRow : public QPushButton {
  public:
    explicit CspRunRow(QWidget* parent) : QPushButton(parent) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void set_run(const agt::TaskRun& r) {
        id_ = r.id;
        title_ = r.title.simplified().isEmpty() ? ChatStartPage::tr("Untitled run") : r.title.simplified();
        status_ = r.status;
        when_ = csp_when(agt::is_terminal(r.status) && r.finished_at_ms > 0 ? r.finished_at_ms : r.created_at_ms);
        engine_ = csp_engine_text(r.engine);
        setAccessibleName(ChatStartPage::tr("%1, %2, %3").arg(title_, agt::run_status_label(r.status), when_));
        setToolTip(title_);
        update();
    }
    QString run_id() const { return id_; }

    QSize sizeHint() const override {
        const QFontMetrics ft(ui::fonts::font(-2, QFont::DemiBold));
        const QFontMetrics fp(ui::fonts::font(-5, QFont::Bold));
        return {ft.averageCharWidth() * 40, std::max(ft.height(), rv::pill_height(fp)) + 12};
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect r = rect();
        const bool hot = underMouse() || isDown();
        if (hot)
            p.fillRect(r, QColor(ui::colors::BG_RAISED()));
        p.setPen(QColor(ui::colors::BORDER_DIM()));
        p.drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        if (hasFocus()) {
            p.setPen(QColor(ui::colors::AMBER()));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(0, 0, -1, -2));
        }
        const QFont fpill = ui::fonts::font(-5, QFont::Bold);
        const QFont ftitle = ui::fonts::font(-2, QFont::DemiBold);
        const QFont fmeta = ui::fonts::font(-3);
        const QFontMetrics fp(fpill), ft(ftitle), fm(fmeta);
        const int left = r.left() + 8;
        const int right = r.right() - 8;
        const int mid = r.top() + (r.height() - 1) / 2;
        const QString pill = rv::run_status_pill(status_);
        const int pill_w = rv::paint_pill(&p, QPoint(left, mid - rv::pill_height(fp) / 2), pill,
                                          rv::run_status_tone(status_), fpill);
        // Right side: engine · time (tabular: the terminal font).
        const QString meta = engine_ + QStringLiteral("  ") + when_;
        const int meta_w = fm.horizontalAdvance(meta);
        p.setFont(fmeta);
        p.setPen(QColor(ui::colors::TEXT_TERTIARY()));
        p.drawText(QRect(right - meta_w, r.top(), meta_w, r.height()), Qt::AlignRight | Qt::AlignVCenter, meta);
        const int tx = left + pill_w + 8;
        const int tw = std::max(10, right - meta_w - 12 - tx);
        p.setFont(ftitle);
        p.setPen(QColor(ui::colors::TEXT_PRIMARY()));
        p.drawText(QRect(tx, r.top(), tw, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   ft.elidedText(title_, Qt::ElideRight, tw));
    }

    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && e->modifiers() == Qt::NoModifier) {
            click();
            return;
        }
        QPushButton::keyPressEvent(e);
    }

  private:
    QString id_;
    QString title_;
    agt::RunStatus status_ = agt::RunStatus::Queued;
    QString when_;
    QString engine_;
};

/// One thing to set up.
struct CspSetup {
    QString key;
    QString title;
    QString message;
    QString action;      ///< button text; empty = no button (pending)
    ui::SetupLink link;
};

} // namespace chat_start_page_detail

using namespace chat_start_page_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct ChatStartPage::Impl {
    ChatStartPage* q = nullptr;
    QScrollArea* scroll = nullptr;
    QWidget* column = nullptr;
    QLabel* eyebrow = nullptr;
    QLabel* headline = nullptr;
    QFrame* rule = nullptr;
    QLabel* purpose = nullptr;
    ui::PanelHeader* examples_header = nullptr;
    QWidget* cards_host = nullptr;
    QVector<CspCard*> cards;
    ui::PanelHeader* runs_header = nullptr;
    QPushButton* templates_link = nullptr;
    QVector<CspRunRow*> rows;
    QLabel* runs_note = nullptr;
    ui::PanelHeader* setup_header = nullptr;
    QWidget* setup_host = nullptr;
    QVBoxLayout* setup_layout = nullptr;
    QString setup_signature;
    QTimer* refresh_timer = nullptr;
    bool dirty = true;

    void build();
    void wire();
    void retranslate();
    void fit();
    void refresh_runs();
    void refresh_setup();
    void schedule_refresh();
};

void ChatStartPage::Impl::build() {
    // Stylesheet first, then the children under their final parent.
    q->setObjectName(QStringLiteral("chatStartPage"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(csp_style());

    auto* outer = new QVBoxLayout(q);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    scroll = new QScrollArea(q);
    scroll->setObjectName(QStringLiteral("cspScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* canvas = new QWidget(scroll);
    canvas->setObjectName(QStringLiteral("cspCanvas"));
    auto* hl = new QHBoxLayout(canvas);
    hl->setContentsMargins(16, 24, 16, 24);
    hl->setSpacing(0);
    hl->addStretch(1);
    column = new QWidget(canvas);
    column->setObjectName(QStringLiteral("cspColumn"));
    hl->addWidget(column, 100);
    hl->addStretch(1);
    auto* vl = new QVBoxLayout(column);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(10);

    eyebrow = new QLabel(column);
    eyebrow->setObjectName(QStringLiteral("cspEyebrow"));
    vl->addWidget(eyebrow);
    headline = new QLabel(column);
    headline->setObjectName(QStringLiteral("cspHeadline"));
    headline->setAccessibleName(QStringLiteral("CHAT MODE"));
    vl->addWidget(headline);
    rule = new QFrame(column);
    rule->setObjectName(QStringLiteral("cspRule"));
    vl->addWidget(rule);
    purpose = new QLabel(column);
    purpose->setObjectName(QStringLiteral("cspPurpose"));
    purpose->setWordWrap(true);
    vl->addWidget(purpose);
    vl->addSpacing(8);

    // ── Example cards ──
    examples_header = new ui::PanelHeader(QString(), column);
    vl->addWidget(examples_header);
    cards_host = new QWidget(column);
    auto* flow = new ui::FlowLayout(cards_host);
    flow->setContentsMargins(0, 4, 0, 4);
    for (const CspExample& e : csp_examples()) {
        auto* card = new CspCard(cards_host);
        card->set_content(e);
        flow->addWidget(card);
        cards.append(card);
        QObject::connect(card, &QPushButton::clicked, q, [this, card]() { emit q->example_chosen(card->example().prompt); });
    }
    vl->addWidget(cards_host);
    vl->addSpacing(6);

    // ── Recent runs ──
    runs_header = new ui::PanelHeader(QString(), column);
    templates_link = new QPushButton(runs_header->controls());
    templates_link->setObjectName(QStringLiteral("cspLink"));
    templates_link->setCursor(Qt::PointingHandCursor);
    runs_header->add_control(templates_link);
    vl->addWidget(runs_header);
    auto* runs_box = new QWidget(column);
    auto* rl = new QVBoxLayout(runs_box);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    for (int i = 0; i < kCspMaxRuns; ++i) {
        auto* row = new CspRunRow(runs_box);
        row->setVisible(false);
        rl->addWidget(row);
        rows.append(row);
        QObject::connect(row, &QPushButton::clicked, q, [this, row]() {
            if (!row->run_id().isEmpty())
                emit q->run_open_requested(row->run_id());
        });
    }
    runs_note = new QLabel(runs_box);
    runs_note->setObjectName(QStringLiteral("cspNote"));
    runs_note->setWordWrap(true);
    rl->addWidget(runs_note);
    vl->addWidget(runs_box);
    vl->addSpacing(6);

    // ── Set up (only what is missing) ──
    setup_header = new ui::PanelHeader(QString(), column);
    setup_header->setVisible(false);
    vl->addWidget(setup_header);
    setup_host = new QWidget(column);
    setup_layout = new QVBoxLayout(setup_host);
    setup_layout->setContentsMargins(0, 4, 0, 0);
    setup_layout->setSpacing(6);
    setup_host->setVisible(false);
    vl->addWidget(setup_host);
    vl->addStretch(1);

    scroll->setWidget(canvas);

    refresh_timer = new QTimer(q);
    refresh_timer->setSingleShot(true);
    refresh_timer->setInterval(kCspRefreshMs);
}

void ChatStartPage::Impl::wire() {
    QObject::connect(refresh_timer, &QTimer::timeout, q, [this]() { q->refresh(); });
    QObject::connect(templates_link, &QPushButton::clicked, q, &ChatStartPage::templates_requested);

    auto& svc = agt::TaskRunService::instance();
    QObject::connect(&svc, &agt::TaskRunService::run_added, q, [this](const QString&) { schedule_refresh(); });
    QObject::connect(&svc, &agt::TaskRunService::run_updated, q, [this](const QString&) { schedule_refresh(); });
    QObject::connect(&svc, &agt::TaskRunService::run_finished, q, [this](const QString&) { schedule_refresh(); });
    QObject::connect(&svc, &agt::TaskRunService::runs_reset, q, [this]() { schedule_refresh(); });
    // Sign-in finishing / a harness allowed for Chat Mode: the SET UP block follows.
    ui::watch_requirement_changes(q, [this]() { schedule_refresh(); });
    QObject::connect(&ai_chat::ModelRouter::instance(), &ai_chat::ModelRouter::routes_changed, q,
                     [this]() { schedule_refresh(); });

    // Painted cards / rows read the live theme; sizes follow the font once settled.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, q,
                     [this](const ui::ThemeTokens&) {
                         for (CspCard* c : std::as_const(cards))
                             c->update();
                         for (CspRunRow* r : std::as_const(rows))
                             r->update();
                     });
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() { fit(); });

    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange) {
            retranslate();
            setup_signature.clear(); // rebuilt in the new language
            q->refresh();
        } else if (t == QEvent::Show) {
            q->refresh();
        }
    });
}

void ChatStartPage::Impl::retranslate() {
    eyebrow->setText(tr("FINCEPT AGENT"));
    headline->setText(tr("CHAT MODE"));
    purpose->setText(tr("Long-running finance tasks with an agent: it plans, uses the terminal's tools, asks before it "
                        "acts, and keeps going while you work elsewhere. Pick an example or type below."));
    examples_header->set_title(tr("Start with an example"));
    examples_header->set_badge(tr("FILLS THE MESSAGE BOX — NOTHING IS SENT"));
    runs_header->set_title(tr("Recent runs"));
    templates_link->setText(tr("BROWSE TEMPLATES"));
    templates_link->setToolTip(tr("Task templates with parameters, engine and budget (NEW RUN)"));
    templates_link->setAccessibleName(tr("Browse task templates"));
    setup_header->set_title(tr("Set up"));
    const QVector<CspExample> ex = csp_examples();
    for (int i = 0; i < cards.size() && i < ex.size(); ++i)
        cards[i]->set_content(ex.at(i));
}

void ChatStartPage::Impl::fit() {
    const int col = ui::metrics::width(kCspColumnPx);
    if (column->maximumWidth() != col)
        column->setMaximumWidth(col);
    const QSize rule_size(ui::metrics::width(48), 2);
    if (rule->size() != rule_size)
        rule->setFixedSize(rule_size);
    for (CspCard* c : std::as_const(cards))
        c->updateGeometry();
    for (CspRunRow* r : std::as_const(rows))
        r->updateGeometry();
    if (cards_host->layout())
        cards_host->layout()->invalidate();
}

void ChatStartPage::Impl::schedule_refresh() {
    dirty = true;
    if (q->isVisible() && !refresh_timer->isActive())
        refresh_timer->start();
}

void ChatStartPage::Impl::refresh_runs() {
    auto& svc = agt::TaskRunService::instance();
    QVector<const agt::TaskRun*> runs;
    const QStringList ids = svc.run_ids();
    runs.reserve(ids.size());
    for (const QString& id : ids)
        if (const agt::TaskRun* r = svc.find_run(id))
            runs.append(r);
    std::sort(runs.begin(), runs.end(), [](const agt::TaskRun* a, const agt::TaskRun* b) {
        if (a->created_at_ms != b->created_at_ms)
            return a->created_at_ms > b->created_at_ms;
        return a->id < b->id;
    });
    const int shown = std::min<int>(kCspMaxRuns, static_cast<int>(runs.size()));
    for (int i = 0; i < rows.size(); ++i) {
        CspRunRow* row = rows.at(i);
        if (i < shown)
            row->set_run(*runs.at(i));
        if (row->isVisibleTo(q) != (i < shown))
            row->setVisible(i < shown);
    }
    QString note;
    if (!svc.is_loaded() && runs.isEmpty())
        note = tr("Loading runs…");
    else if (runs.isEmpty())
        note = tr("No runs yet. Every message you send is a run: it keeps going while you work elsewhere, and you "
                  "can follow, pause or retry it from the RUNS list.");
    if (runs_note->text() != note)
        runs_note->setText(note);
    if (runs_note->isVisibleTo(q) == note.isEmpty())
        runs_note->setVisible(!note.isEmpty());
    const int attention = svc.attention_count();
    runs_header->set_badge(runs.isEmpty() ? QString()
                           : attention > 0
                               ? tr("%1 RUNS · %2 NEED YOU").arg(runs.size()).arg(attention)
                               : tr("%1 RUNS").arg(runs.size()));
}

void ChatStartPage::Impl::refresh_setup() {
    QVector<CspSetup> missing;
    const ui::RequirementStatus signin = ui::check_requirements({ui::Requirement::signed_in()});
    if (signin.pending)
        missing.append(CspSetup{QStringLiteral("signin"), signin.title.isEmpty() ? tr("Checking sign-in…") : signin.title,
                        tr("Cloud runs start once your sign-in is confirmed."), QString(), {}});
    else if (!signin.ok)
        missing.append(CspSetup{QStringLiteral("signin"), signin.title.isEmpty() ? tr("Sign in for the cloud agent") : signin.title,
                        signin.message, signin.action_text.isEmpty() ? tr("SIGN IN") : signin.action_text, signin.link});

    const ui::RequirementStatus llm = ui::check_requirements({ui::Requirement::llm()});
    if (!llm.ok && !llm.pending) {
        ui::SetupLink link = llm.link;
        if (!link.valid())
            link = ui::SetupLink{QStringLiteral("settings"), QStringLiteral("llm_config"), QString()};
        missing.append(CspSetup{QStringLiteral("llm"), tr("Set up a model (optional, for the local agent)"),
                        llm.message.isEmpty() ? tr("The local agent runs on your own LLM profile; none is configured.")
                                              : llm.message,
                        llm.action_text.isEmpty() ? tr("OPEN SETTINGS > LLM CONFIGURATION") : llm.action_text, link});
    }

    const rv::HarnessEngineOption harness = rv::harness_engine_option_now();
    if (harness.listed && !harness.enabled)
        missing.append(CspSetup{QStringLiteral("harness"), tr("Connect a coding agent (optional)"),
                        harness.reason.isEmpty() ? harness.state : harness.reason,
                        tr("OPEN SETTINGS > LLM CONFIGURATION"),
                        ui::SetupLink{QStringLiteral("settings"), QStringLiteral("llm_config"), QString()}});

    QString sig;
    for (const CspSetup& s : std::as_const(missing))
        sig += s.key + QLatin1Char('|') + s.title + QLatin1Char('|') + s.message + QLatin1Char('|') + s.action +
               QLatin1Char('\n');
    if (sig == setup_signature)
        return;
    setup_signature = sig;

    while (QLayoutItem* item = setup_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    for (const CspSetup& s : std::as_const(missing)) {
        auto* row = new QFrame(setup_host);
        row->setObjectName(QStringLiteral("cspSetupRow"));
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 8, 10, 8);
        hl->setSpacing(10);
        auto* texts = new QVBoxLayout;
        texts->setSpacing(2);
        auto* title = new QLabel(s.title, row);
        title->setObjectName(QStringLiteral("cspSetupTitle"));
        title->setTextFormat(Qt::PlainText);
        title->setWordWrap(true);
        auto* text = new QLabel(s.message, row);
        text->setObjectName(QStringLiteral("cspSetupText"));
        text->setTextFormat(Qt::PlainText);
        text->setWordWrap(true);
        texts->addWidget(title);
        texts->addWidget(text);
        hl->addLayout(texts, 1);
        if (!s.action.isEmpty() && s.link.valid()) {
            auto* btn = new QPushButton(QString(s.action).replace(QLatin1Char('&'), QStringLiteral("&&")), row);
            btn->setObjectName(QStringLiteral("cspSetupBtn"));
            btn->setCursor(Qt::PointingHandCursor);
            btn->setAccessibleName(s.action);
            btn->setToolTip(s.message);
            const ui::SetupLink link = s.link;
            QPointer<ChatStartPage> self(q);
            QObject::connect(btn, &QPushButton::clicked, q, [self, link]() {
                if (!self)
                    return;
                // Chat Mode covers the window: the screen leaves it first, then the link opens.
                emit self->leave_requested();
                ui::open_setup_link(link, self);
            });
            hl->addWidget(btn, 0, Qt::AlignVCenter);
        }
        setup_layout->addWidget(row);
    }
    const bool any = !missing.isEmpty();
    if (setup_header->isVisibleTo(q) != any)
        setup_header->setVisible(any);
    if (setup_host->isVisibleTo(q) != any)
        setup_host->setVisible(any);
    setup_header->set_badge(any ? tr("%n ITEM(S)", nullptr, static_cast<int>(missing.size())) : QString());
}

// ── ChatStartPage ────────────────────────────────────────────────────────────

ChatStartPage::ChatStartPage(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
    d_->fit();
}

ChatStartPage::~ChatStartPage() = default;

void ChatStartPage::refresh() {
    d_->refresh_timer->stop();
    d_->dirty = false;
    d_->refresh_runs();
    d_->refresh_setup();
}

void ChatStartPage::focus_first() {
    if (!d_->cards.isEmpty())
        d_->cards.first()->setFocus(Qt::TabFocusReason);
}

} // namespace fincept::chat_mode
