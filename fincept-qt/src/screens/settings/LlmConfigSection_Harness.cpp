// src/screens/settings/LlmConfigSection_Harness.cpp
//
// Settings › LLM Configuration › TERMINAL AI — agent harnesses (Claude Code,
// Codex, …) as the terminal's model provider:
//   HARNESSES   one row per harness: state, eligibility (API-key authentication
//               only — a personal subscription sign-in stays in the Agent
//               Workbench), "Use for terminal AI" (needs the one-time consent
//               dialog naming the vendor), Default, model and effort, help links.
//   WHEN A HARNESS IS UNAVAILABLE   fall back automatically | ask | fail.
//   FEATURES    what answers each AI feature now, and whether the default
//               harness may answer it automatically (features that send the
//               user's financial data are off until allowed here).
//
// Built by build_llm_terminal_ai_tab() (declared in LlmConfigSection.cpp). Part
// of the partial-class split of LlmConfigSection; no Q_OBJECT here — plain
// widgets with functor connections. One page stylesheet keyed by objectName,
// set before any child exists; rows are rebuilt (never restyled) when the
// router or the registry reports a change, coalesced and only while visible.

#include "screens/settings/LlmConfigSection.h"
#include "services/harness/HarnessEligibility.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/llm/ModelRouter.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/ModelBadge.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace llm_terminal_ai_detail {

namespace lta_ai = fincept::ai_chat;
namespace lta_h = fincept::harness;

QString lta_tr(const char* text) {
    return QCoreApplication::translate("fincept::screens::LlmConfigSection", text);
}

/// Button text is plain text: a '&' must not become a mnemonic.
QString lta_btn(const QString& text) {
    QString t = text;
    t.replace(QLatin1Char('&'), QStringLiteral("&&"));
    return t;
}

/// The objectName that colours a state (dot and label) through the page stylesheet.
QString lta_tone(lta_h::TerminalState st) {
    switch (st) {
        case lta_h::TerminalState::Ready:
        case lta_h::TerminalState::EligibleNotEnabled:
            return QStringLiteral("Ok");
        case lta_h::TerminalState::ConsentNeeded:
        case lta_h::TerminalState::CoolingDown:
        case lta_h::TerminalState::Detecting:
            return QStringLiteral("Warn");
        case lta_h::TerminalState::SubscriptionOnly:
        case lta_h::TerminalState::KeyStripped:
        case lta_h::TerminalState::SignedOut:
            return QStringLiteral("Bad");
        case lta_h::TerminalState::NotInstalled:
        case lta_h::TerminalState::ChatUnsupported:
        case lta_h::TerminalState::ReadOnlyUnverified:
            break;
    }
    return QStringLiteral("Off");
}

QString lta_basis_text(const QString& basis) {
    if (basis == QLatin1String("api_key"))
        return lta_tr("API key");
    if (basis == QLatin1String("cloud"))
        return lta_tr("cloud key");
    if (basis == QLatin1String("env_key"))
        return lta_tr("API key from environment");
    return {};
}

bool lta_is_workbench_only(lta_h::TerminalState st) {
    return st == lta_h::TerminalState::SubscriptionOnly || st == lta_h::TerminalState::KeyStripped ||
           st == lta_h::TerminalState::SignedOut || st == lta_h::TerminalState::ChatUnsupported ||
           st == lta_h::TerminalState::ReadOnlyUnverified;
}

/// The page stylesheet: built from the live tokens once, before any child
/// exists; the theme manager's re-style pass keeps it current.
QString lta_page_style() {
    using namespace ui::colors;
    const QString body = ui::fonts::px(-2);
    const QString small = ui::fonts::px(-3);
    const QString tiny = ui::fonts::px(-4);
    return QStringLiteral(
               "QWidget#ltaPage{background:%1;}"
               "QLabel{background:transparent;color:%2;font-size:%11px;}"
               "QLabel#ltaSection{color:%3;font-weight:700;letter-spacing:1px;font-size:%12px;}"
               "QLabel#ltaIntro,QLabel#ltaHint,QLabel#ltaReason{color:%4;font-size:%12px;}"
               "QLabel#ltaMuted,QLabel#ltaColHead{color:%5;font-size:%13px;}"
               "QLabel#ltaColHead{font-weight:700;letter-spacing:1px;}"
               "QLabel#ltaName{color:%2;font-weight:700;}"
               "QLabel#ltaDotOk,QLabel#ltaStateOk{color:%6;}"
               "QLabel#ltaDotWarn,QLabel#ltaStateWarn,QLabel#ltaWarnNote{color:%7;}"
               "QLabel#ltaDotBad,QLabel#ltaStateBad{color:%8;}"
               "QLabel#ltaDotOff,QLabel#ltaStateOff{color:%5;}"
               "QLabel#ltaStateOk,QLabel#ltaStateWarn,QLabel#ltaStateBad,QLabel#ltaStateOff{font-weight:600;}"
               "QFrame#ltaRow{background:%9;border:1px solid %10;}"
               "QFrame#ltaSep{background:%10;max-height:1px;min-height:1px;border:none;}"
               "QCheckBox,QRadioButton{color:%2;background:transparent;font-size:%12px;spacing:6px;}"
               "QCheckBox:disabled,QRadioButton:disabled{color:%5;}"
               "QComboBox{background:%1;color:%2;border:1px solid %10;padding:2px 6px;font-size:%12px;}"
               "QComboBox::drop-down{border:none;}"
               "QPushButton#ltaBtn{background:transparent;color:%3;border:1px solid %3;padding:3px 10px;"
               "font-size:%13px;font-weight:600;}"
               "QPushButton#ltaBtn:hover{background:%9;}"
               "QPushButton#ltaQuietBtn{background:transparent;color:%4;border:1px solid %10;padding:3px 10px;"
               "font-size:%13px;}"
               "QPushButton#ltaQuietBtn:hover{color:%2;border-color:%4;}")
        .arg(BG_BASE(), TEXT_PRIMARY(), AMBER(), TEXT_SECONDARY(), TEXT_TERTIARY(), SUCCESS(), WARNING(), DANGER(),
             BG_SURFACE())
        .arg(BORDER_DIM(), body, small, tiny);
}

QLabel* lta_label(const QString& text, const char* object_name, QWidget* parent, bool wrap = false) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QString::fromLatin1(object_name));
    l->setWordWrap(wrap);
    l->setTextFormat(Qt::PlainText);
    return l;
}

QPushButton* lta_button(const QString& text, const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(lta_btn(text), parent);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

void lta_clear_layout(QLayout* layout) {
    if (!layout)
        return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        } else if (QLayout* child = item->layout()) {
            // A nested layout IS its layout item: delete it once.
            lta_clear_layout(child);
            delete child;
            continue;
        }
        delete item;
    }
}

// ── The tab ──────────────────────────────────────────────────────────────────

class LtaTerminalAiTab : public QWidget {
  public:
    explicit LtaTerminalAiTab(QWidget* parent) : QWidget(parent) {
        ui::install_harness_consent_handler();

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);
        auto* scroll = new QScrollArea(this);
        scroll->setObjectName(QStringLiteral("ltaScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        outer->addWidget(scroll);

        page_ = new QWidget;
        page_->setObjectName(QStringLiteral("ltaPage"));
        page_->setStyleSheet(lta_page_style()); // before any child exists
        auto* vl = new QVBoxLayout(page_);
        vl->setContentsMargins(20, 16, 20, 16);
        vl->setSpacing(8);

        // ── Intro ──
        intro_ = lta_label({}, "ltaIntro", page_, true);
        vl->addWidget(intro_);

        // ── Harnesses ──
        harness_title_ = lta_label({}, "ltaSection", page_);
        vl->addSpacing(6);
        vl->addWidget(harness_title_);
        harness_box_ = new QVBoxLayout;
        harness_box_->setSpacing(6);
        vl->addLayout(harness_box_);

        // ── When a harness is unavailable ──
        vl->addSpacing(10);
        behaviour_title_ = lta_label({}, "ltaSection", page_);
        vl->addWidget(behaviour_title_);
        behaviour_group_ = new QButtonGroup(this);
        auto* brow = new QHBoxLayout;
        brow->setSpacing(18);
        rb_fallback_ = new QRadioButton(page_);
        rb_ask_ = new QRadioButton(page_);
        rb_fail_ = new QRadioButton(page_);
        behaviour_group_->addButton(rb_fallback_, static_cast<int>(lta_ai::UnavailableBehaviour::Fallback));
        behaviour_group_->addButton(rb_ask_, static_cast<int>(lta_ai::UnavailableBehaviour::Ask));
        behaviour_group_->addButton(rb_fail_, static_cast<int>(lta_ai::UnavailableBehaviour::Fail));
        brow->addWidget(rb_fallback_);
        brow->addWidget(rb_ask_);
        brow->addWidget(rb_fail_);
        brow->addStretch();
        vl->addLayout(brow);
        behaviour_hint_ = lta_label({}, "ltaHint", page_, true);
        vl->addWidget(behaviour_hint_);
        connect(behaviour_group_, &QButtonGroup::idClicked, this, [](int id) {
            lta_ai::ModelRouter::instance().set_unavailable_behaviour(static_cast<lta_ai::UnavailableBehaviour>(id));
        });

        // ── Features ──
        vl->addSpacing(10);
        features_title_ = lta_label({}, "ltaSection", page_);
        vl->addWidget(features_title_);
        features_hint_ = lta_label({}, "ltaHint", page_, true);
        vl->addWidget(features_hint_);
        feature_grid_ = new QGridLayout;
        feature_grid_->setHorizontalSpacing(14);
        feature_grid_->setVerticalSpacing(6);
        vl->addLayout(feature_grid_);
        unrouted_title_ = lta_label({}, "ltaColHead", page_);
        vl->addSpacing(6);
        vl->addWidget(unrouted_title_);
        unrouted_box_ = new QVBoxLayout;
        unrouted_box_->setSpacing(3);
        vl->addLayout(unrouted_box_);
        vl->addStretch();
        scroll->setWidget(page_);

        retranslate();

        auto& reg = lta_h::HarnessRegistry::instance();
        connect(&reg, &lta_h::HarnessRegistry::status_changed, this, [this](const QString&) { schedule_rebuild(); });
        connect(&reg, &lta_h::HarnessRegistry::defaults_changed, this, [this]() { schedule_rebuild(); });
        connect(&lta_ai::ModelRouter::instance(), &lta_ai::ModelRouter::routes_changed, this,
                [this]() { schedule_rebuild(); });
        // Widths come from font metrics: re-fit once the appearance settled.
        connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
                [this]() { schedule_rebuild(); });
        rebuild();
    }

  protected:
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        // Sign-in / model state older than its TTL is re-checked (asynchronous).
        lta_h::HarnessRegistry::instance().revalidate();
        if (dirty_)
            rebuild();
    }

    void changeEvent(QEvent* e) override {
        if (e->type() == QEvent::LanguageChange) {
            retranslate();
            schedule_rebuild();
        }
        QWidget::changeEvent(e);
    }

  private:
    void retranslate() {
        intro_->setText(lta_tr("Claude Code and Codex, authenticated with an API key, can answer AI Chat, Quick Chat, "
                               "Dashboard Quick Ask and the workflow tool picker directly — billed to your own API key, "
                               "no Fincept credits. A personal subscription sign-in is never used for terminal AI (the "
                               "vendors do not allow other apps to route requests through it); it stays available in "
                               "the Agent Workbench."));
        harness_title_->setText(lta_tr("HARNESSES"));
        behaviour_title_->setText(lta_tr("WHEN A HARNESS IS UNAVAILABLE"));
        rb_fallback_->setText(lta_tr("Fall back automatically"));
        rb_ask_->setText(lta_tr("Ask me"));
        rb_fail_->setText(lta_tr("Fail"));
        behaviour_hint_->setText(lta_tr("Signed out, rate limited, not installed or no answer in time — before anything "
                                        "was shown, the next model of the feature's route answers once. A fallback to "
                                        "Fincept LLM spends credits and is always announced."));
        features_title_->setText(lta_tr("FEATURES"));
        features_hint_->setText(lta_tr("What answers each AI feature now (an explicit choice made with a feature's model "
                                       "badge wins). Features that send your financial data use a harness "
                                       "automatically only when you allow it here."));
        unrouted_title_->setText(lta_tr("NOT ROUTED YET"));
    }

    void schedule_rebuild() {
        dirty_ = true;
        if (pending_ || !isVisible())
            return; // rebuilt on the next show
        pending_ = true;
        QTimer::singleShot(0, this, [this]() {
            pending_ = false;
            if (isVisible())
                rebuild();
        });
    }

    void rebuild() {
        dirty_ = false;
        rebuild_harnesses();
        sync_behaviour();
        rebuild_features();
    }

    // ── HARNESSES ──────────────────────────────────────────────────────────────

    void rebuild_harnesses() {
        lta_clear_layout(harness_box_);
        auto& reg = lta_h::HarnessRegistry::instance();
        QVector<lta_ai::RouterHarness> rows = lta_ai::ModelRouter::instance().harness_states();
        // Installed first, registration order otherwise.
        std::stable_sort(rows.begin(), rows.end(), [&reg](const lta_ai::RouterHarness& a, const lta_ai::RouterHarness& b) {
            return reg.status(a.id).installed() && !reg.status(b.id).installed();
        });
        bool any_installed = false;
        bool detecting = false;
        for (const auto& h : rows) {
            const lta_h::HarnessStatus st = reg.status(h.id);
            any_installed = any_installed || st.installed();
            detecting = detecting || h.eligibility.state == lta_h::TerminalState::Detecting;
        }
        if (!any_installed && !detecting && !rows.isEmpty()) {
            harness_box_->addWidget(make_empty_state(rows));
            return;
        }
        for (const auto& h : rows)
            harness_box_->addWidget(make_harness_row(h));
    }

    QWidget* make_empty_state(const QVector<lta_ai::RouterHarness>& rows) {
        auto& reg = lta_h::HarnessRegistry::instance();
        ui::EmptyState::Spec spec;
        spec.glyph = QStringLiteral("⌁");
        spec.glyph_tone = ui::EmptyState::Tone::Info;
        spec.title = lta_tr("No agent harness on this computer");
        spec.message = lta_tr("Install Claude Code or Codex and give it an API key: it then answers AI Chat, Quick Chat "
                              "and Quick Ask directly, billed to your own key. Until then Fincept LLM answers.");
        for (const auto& h : rows) {
            const lta_h::IHarnessAdapter* a = reg.adapter(h.id);
            if (!a || !a->descriptor().caps.chat)
                continue;
            const lta_h::HarnessDescriptor& d = a->descriptor();
            spec.details.append({d.display_name, d.install_command.isEmpty() ? d.install_url : d.install_command});
        }
        spec.details_heading = lta_tr("SUPPORTED FOR TERMINAL AI");
        spec.primary = lta_tr("INSTALL GUIDE");
        spec.secondary = lta_tr("DETECT AGAIN");
        auto* es = ui::EmptyState::create(spec, page_);
        es->set_compact(true);
        QString url;
        if (const lta_h::IHarnessAdapter* a = reg.adapter(QStringLiteral("claude")))
            url = a->descriptor().install_url;
        connect(es, &ui::EmptyState::primary_clicked, this, [url]() {
            if (!url.isEmpty())
                QDesktopServices::openUrl(QUrl(url));
        });
        connect(es, &ui::EmptyState::secondary_clicked, this,
                []() { lta_h::HarnessRegistry::instance().refresh({}, lta_h::RefreshScope::All); });
        return es;
    }

    QWidget* make_harness_row(const lta_ai::RouterHarness& h) {
        auto& reg = lta_h::HarnessRegistry::instance();
        const lta_h::IHarnessAdapter* adapter = reg.adapter(h.id);
        const lta_h::HarnessStatus st = reg.status(h.id);
        const lta_h::TerminalEligibility& el = h.eligibility;
        const QString tone = lta_tone(el.state);
        const QString id = h.id;

        auto* row = new QFrame(page_);
        row->setObjectName(QStringLiteral("ltaRow"));
        auto* v = new QVBoxLayout(row);
        v->setContentsMargins(10, 8, 10, 8);
        v->setSpacing(4);

        // Line 1: dot, name + version, state, basis … switches.
        auto* top = new QHBoxLayout;
        top->setSpacing(8);
        auto* dot = lta_label(QStringLiteral("●"), "", row);
        dot->setObjectName(QStringLiteral("ltaDot") + tone);
        top->addWidget(dot);
        QString name = h.display_name;
        if (st.installation && !st.installation->version.isEmpty())
            name += QStringLiteral("  ") + st.installation->version;
        if (adapter && adapter->descriptor().beta)
            name += QStringLiteral("  · BETA");
        top->addWidget(lta_label(name, "ltaName", row));
        QString state_text = lta_is_workbench_only(el.state) ? lta_tr("Workbench only") : el.label;
        if (el.state == lta_h::TerminalState::SignedOut)
            state_text = el.label + QStringLiteral(" · ") + lta_tr("Workbench only");
        auto* state = lta_label(state_text, "", row);
        state->setObjectName(QStringLiteral("ltaState") + tone);
        top->addWidget(state);
        const QString basis = lta_basis_text(el.basis);
        if (!basis.isEmpty())
            top->addWidget(lta_label(QStringLiteral("· ") + basis, "ltaMuted", row));
        top->addStretch();

        auto* use = new QCheckBox(lta_btn(lta_tr("Use for terminal AI")), row);
        use->setProperty("fincept_agent_risk", "account"); // agents: lets a coding-agent harness answer for the terminal AI
        use->setChecked(reg.enabled_for_terminal(id));
        use->setEnabled(el.eligible);
        use->setToolTip(el.eligible ? lta_tr("Let this harness answer terminal AI by default (the first time, a dialog "
                                             "names the vendor that receives the data).")
                                    : el.reason);
        QPointer<LtaTerminalAiTab> self(this);
        connect(use, &QCheckBox::clicked, this, [self, id](bool on) {
            auto& r = lta_h::HarnessRegistry::instance();
            if (on && !r.terminal_consent(id)) {
                ui::request_harness_terminal_consent(self ? self->window() : nullptr, id, true, [self](bool) {
                    if (self)
                        self->schedule_rebuild(); // the checkbox shows what was decided
                });
                return;
            }
            r.set_enabled_for_terminal(id, on);
        });
        top->addWidget(use);

        auto* def = new QRadioButton(lta_tr("Default"), row);
        def->setAutoExclusive(false);
        def->setChecked(reg.default_harness() == id);
        def->setToolTip(lta_tr("The harness terminal AI uses first when several are switched on (and the one new "
                               "Agent Workbench runs start with)."));
        connect(def, &QRadioButton::clicked, this,
                [id](bool on) { lta_h::HarnessRegistry::instance().set_default_harness(on ? id : QString()); });
        top->addWidget(def);
        v->addLayout(top);

        // Line 2: why (or what it does).
        QString reason = el.reason;
        if (lta_is_workbench_only(el.state) && el.state != lta_h::TerminalState::ChatUnsupported &&
            el.state != lta_h::TerminalState::ReadOnlyUnverified)
            reason = lta_tr("Workbench only — sign in with an API key to use it for terminal AI.") +
                     QLatin1Char(' ') + el.reason;
        if (!reason.isEmpty())
            v->addWidget(lta_label(reason, "ltaReason", row, true));

        // Line 3: model / effort for a usable harness; help links otherwise.
        auto* bottom = new QHBoxLayout;
        bottom->setSpacing(8);
        bool any = false;
        if (el.usable && adapter) {
            const QFontMetrics fm(font());
            const int char_w = fm.horizontalAdvance(QLatin1Char('M'));
            bottom->addWidget(lta_label(lta_tr("Model"), "ltaMuted", row));
            auto* model = new QComboBox(row);
            model->setMinimumWidth(char_w * 14);
            model->addItem(lta_tr("CLI default"), QString());
            for (const lta_h::ModelInfo& m : st.models)
                model->addItem(m.label.isEmpty() || m.label == m.id ? m.id : QStringLiteral("%1 (%2)").arg(m.label, m.id),
                               m.id);
            const QString cur_model = reg.default_model(id);
            int mi = model->findData(cur_model);
            if (mi < 0 && !cur_model.isEmpty()) {
                model->addItem(cur_model, cur_model);
                mi = model->count() - 1;
            }
            model->setCurrentIndex(qMax(0, mi));
            connect(model, &QComboBox::activated, this, [model, id](int) {
                lta_h::HarnessRegistry::instance().set_default_model(id, model->currentData().toString());
            });
            bottom->addWidget(model);
            if (adapter->descriptor().caps.effort) {
                bottom->addWidget(lta_label(lta_tr("Effort"), "ltaMuted", row));
                auto* effort = new QComboBox(row);
                effort->setMinimumWidth(char_w * 9);
                effort->addItem(lta_tr("default"), QString());
                for (const QString& e : reg.efforts(id, cur_model))
                    effort->addItem(e, e);
                const QString cur_effort = reg.default_effort(id);
                int ei = effort->findData(cur_effort);
                if (ei < 0 && !cur_effort.isEmpty()) {
                    effort->addItem(cur_effort, cur_effort);
                    ei = effort->count() - 1;
                }
                effort->setCurrentIndex(qMax(0, ei));
                connect(effort, &QComboBox::activated, this, [effort, id](int) {
                    lta_h::HarnessRegistry::instance().set_default_effort(id, effort->currentData().toString());
                });
                bottom->addWidget(effort);
            }
            any = true;
        }
        bottom->addStretch();
        if (el.state == lta_h::TerminalState::ConsentNeeded) {
            auto* ok = lta_button(lta_tr("REVIEW AND ALLOW…"), "ltaBtn", row);
            connect(ok, &QPushButton::clicked, this, [self, id]() {
                ui::request_harness_terminal_consent(self ? self->window() : nullptr, id, true, [self](bool) {
                    if (self)
                        self->schedule_rebuild();
                });
            });
            bottom->addWidget(ok);
            any = true;
        }
        if ((el.state == lta_h::TerminalState::SubscriptionOnly || el.state == lta_h::TerminalState::KeyStripped ||
             el.state == lta_h::TerminalState::SignedOut) &&
            !el.docs_url.isEmpty()) {
            auto* help = lta_button(lta_tr("API-KEY HELP"), "ltaBtn", row);
            const QString url = el.docs_url;
            help->setToolTip(url);
            connect(help, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
            bottom->addWidget(help);
            any = true;
        }
        if (el.state == lta_h::TerminalState::NotInstalled && adapter && !adapter->descriptor().install_url.isEmpty()) {
            auto* install = lta_button(lta_tr("INSTALL GUIDE"), "ltaQuietBtn", row);
            const QString url = adapter->descriptor().install_url;
            install->setToolTip(url);
            connect(install, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
            bottom->addWidget(install);
            any = true;
        }
        if (reg.terminal_consent(id)) {
            auto* withdraw = lta_button(lta_tr("WITHDRAW CONSENT"), "ltaQuietBtn", row);
            withdraw->setToolTip(lta_tr("Stop using this harness for terminal AI; the consent dialog appears again "
                                        "before it is used next time."));
            connect(withdraw, &QPushButton::clicked, this,
                    [id]() { lta_h::HarnessRegistry::instance().revoke_terminal_consent(id); });
            bottom->addWidget(withdraw);
            any = true;
        }
        if (any)
            v->addLayout(bottom);
        else
            delete bottom;
        return row;
    }

    // ── WHEN A HARNESS IS UNAVAILABLE ──────────────────────────────────────────

    void sync_behaviour() {
        const auto b = lta_ai::ModelRouter::instance().unavailable_behaviour();
        QAbstractButton* btn = behaviour_group_->button(static_cast<int>(b));
        if (btn && !btn->isChecked()) {
            const QSignalBlocker block(behaviour_group_);
            btn->setChecked(true);
        }
    }

    // ── FEATURES ───────────────────────────────────────────────────────────────

    void rebuild_features() {
        lta_clear_layout(feature_grid_);
        lta_clear_layout(unrouted_box_);
        auto& router = lta_ai::ModelRouter::instance();
        int r = 0;
        feature_grid_->addWidget(lta_label(lta_tr("FEATURE"), "ltaColHead", page_), r, 0);
        feature_grid_->addWidget(lta_label(lta_tr("NOW ANSWERED BY"), "ltaColHead", page_), r, 1);
        feature_grid_->addWidget(lta_label(lta_tr("HARNESS BY DEFAULT"), "ltaColHead", page_), r, 2);
        feature_grid_->addWidget(lta_label(lta_tr("NOTE"), "ltaColHead", page_), r, 3);
        ++r;
        for (const lta_ai::FeatureSpec& f : lta_ai::feature_specs()) {
            const QString key = QString::fromLatin1(f.key);
            if (!f.routed) {
                auto* line = new QHBoxLayout;
                line->setSpacing(10);
                auto* name = lta_label(lta_tr(f.label), "ltaMuted", page_);
                name->setMinimumWidth(QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')) * 16);
                line->addWidget(name);
                line->addWidget(lta_label(lta_tr(f.not_routed_reason), "ltaMuted", page_, true), 1);
                unrouted_box_->addLayout(line);
                continue;
            }
            const lta_ai::RouteResult route = router.route(key);
            QString answered;
            if (key == QLatin1String(lta_ai::kFeatureChatModeHarness)) {
                // That engine is a harness or nothing.
                for (const lta_ai::ModelChoice& c : route.chain) {
                    if (c.is_harness()) {
                        answered = c.label;
                        break;
                    }
                }
                if (answered.isEmpty())
                    answered = lta_tr("— no harness (allow one below)");
            } else if (!route.chain.isEmpty()) {
                answered = route.chain.constFirst().label;
                if (route.chain.constFirst().reason == QLatin1String("explicit"))
                    answered += QStringLiteral("  ") + lta_tr("(your choice)");
            }
            feature_grid_->addWidget(lta_label(lta_tr(f.label), "ltaName", page_), r, 0);
            feature_grid_->addWidget(lta_label(answered, "", page_), r, 1);
            auto* allow = new QCheckBox(lta_btn(lta_tr("Default harness may answer")), page_);
            allow->setProperty("fincept_agent_risk", "account"); // agents: lets the default harness answer for the terminal AI
            allow->setChecked(router.harness_default_allowed(key));
            allow->setEnabled(f.harness_fit != lta_ai::HarnessFit::No && !f.explicit_harness_only);
            connect(allow, &QCheckBox::clicked, this,
                    [key](bool on) { lta_ai::ModelRouter::instance().set_harness_default_allowed(key, on); });
            feature_grid_->addWidget(allow, r, 2);
            auto* notes = new QVBoxLayout;
            notes->setSpacing(1);
            if (f.sends_user_financial_data)
                notes->addWidget(lta_label(lta_tr("Sends your financial data (holdings, positions) to the vendor."),
                                           "ltaWarnNote", page_, true));
            if (!route.skipped.isEmpty()) {
                const lta_ai::SkippedChoice& s = route.skipped.constFirst();
                notes->addWidget(lta_label(s.what + QStringLiteral(": ") + s.reason, "ltaMuted", page_, true));
            }
            if (notes->count() == 0)
                notes->addWidget(lta_label(QStringLiteral("—"), "ltaMuted", page_));
            feature_grid_->addLayout(notes, r, 3);
            ++r;
        }
        feature_grid_->setColumnStretch(3, 1);
        const QFontMetrics fm(font());
        feature_grid_->setColumnMinimumWidth(0, fm.horizontalAdvance(QLatin1Char('M')) * 14);
        feature_grid_->setColumnMinimumWidth(1, fm.horizontalAdvance(QLatin1Char('M')) * 16);
    }

    QWidget* page_ = nullptr;
    QLabel* intro_ = nullptr;
    QLabel* harness_title_ = nullptr;
    QVBoxLayout* harness_box_ = nullptr;
    QLabel* behaviour_title_ = nullptr;
    QButtonGroup* behaviour_group_ = nullptr;
    QRadioButton* rb_fallback_ = nullptr;
    QRadioButton* rb_ask_ = nullptr;
    QRadioButton* rb_fail_ = nullptr;
    QLabel* behaviour_hint_ = nullptr;
    QLabel* features_title_ = nullptr;
    QLabel* features_hint_ = nullptr;
    QGridLayout* feature_grid_ = nullptr;
    QLabel* unrouted_title_ = nullptr;
    QVBoxLayout* unrouted_box_ = nullptr;
    bool dirty_ = false;
    bool pending_ = false;
};

} // namespace llm_terminal_ai_detail

/// Settings › LLM Configuration › TERMINAL AI (added by LlmConfigSection::build_ui).
QWidget* build_llm_terminal_ai_tab(QWidget* parent) {
    return new llm_terminal_ai_detail::LtaTerminalAiTab(parent);
}

} // namespace fincept::screens
