// ModelBadge.cpp — the shared "which model answers" badge, its picker and the
// terminal-AI consent dialog (see ModelBadge.h).

#include "ui/widgets/ModelBadge.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/harness/HarnessEligibility.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/llm/ModelRouter.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/MnemonicText.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDesktopServices>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace fincept::ui {

namespace model_badge_detail {

namespace mb_ai = fincept::ai_chat;
namespace mb_h = fincept::harness;

constexpr int kMbFontOffset = -4;
constexpr int kMbMaxTextChars = 44;
constexpr int kMbCompactMaxTextChars = 26;

QFont mb_font() {
    return ui::fonts::font(kMbFontOffset);
}

int mb_pad(const QFontMetrics& fm) {
    return std::max(6, fm.averageCharWidth());
}

int mb_dot(const QFontMetrics& fm) {
    return std::max(6, fm.height() / 2 - 1);
}

QString mb_chevron() {
    return QStringLiteral("▾"); // ▾
}

/// The harness part of a label: "Claude Code · sonnet" (no effort) for compact badges.
QString mb_compact_label(const mb_ai::ModelChoice& c) {
    if (!c.is_harness())
        return c.label;
    const QStringList parts = c.label.split(QStringLiteral(" · "));
    return parts.mid(0, 2).join(QStringLiteral(" · "));
}

QString mb_reason_text(const QString& reason) {
    if (reason == QLatin1String("explicit"))
        return ModelBadge::tr("your choice for this feature");
    if (reason == QLatin1String("harness_default"))
        return ModelBadge::tr("your connected harness (Auto)");
    if (reason == QLatin1String("profile"))
        return ModelBadge::tr("your default profile (Auto)");
    if (reason == QLatin1String("active_provider"))
        return ModelBadge::tr("your active API provider (Auto)");
    return ModelBadge::tr("nothing else is configured (Auto)");
}

QString mb_choice_key(const QString& provider, const QString& model, const QString& effort) {
    return provider.toLower() + QLatin1Char('|') + model + QLatin1Char('|') + effort;
}

QSet<QString>& mb_open_consents() {
    static QSet<QString> open;
    return open;
}

} // namespace model_badge_detail

using namespace model_badge_detail;

// ── ModelBadge ───────────────────────────────────────────────────────────────

ModelBadge::ModelBadge(const QString& feature, QWidget* parent, const QString& context_id)
    : QWidget(parent), feature_(feature), context_id_(context_id) {
    setObjectName(QStringLiteral("modelBadge"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    install_harness_consent_handler();
    connect(&mb_ai::ModelRouter::instance(), &mb_ai::ModelRouter::routes_changed, this, &ModelBadge::refresh);
    // Painted from the live tokens: a theme change only needs a repaint; a
    // font change re-fits once the appearance has settled.
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) { update(); });
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        updateGeometry();
        update();
    });
    refresh();
}

void ModelBadge::set_compact(bool compact) {
    if (compact_ == compact)
        return;
    compact_ = compact;
    refresh();
}

void ModelBadge::refresh() {
    const mb_ai::RouteResult r = mb_ai::ModelRouter::instance().route(feature_, context_id_);
    const mb_ai::ModelChoice c = r.chain.isEmpty() ? mb_ai::ModelChoice{} : r.chain.constFirst();
    const QString label = c.label.isEmpty() ? tr("Fincept LLM") : c.label;
    const QString shown = compact_ ? mb_compact_label(c) : label;

    // A harness the user meant to use was passed over: WARNING, with the reason.
    QString skipped;
    for (const mb_ai::SkippedChoice& s : r.skipped) {
        if (!s.harness_id.isEmpty()) {
            skipped = s.what + QStringLiteral(": ") + s.reason;
            break;
        }
    }
    const Tone tone = c.is_harness() ? Tone::Harness : (skipped.isEmpty() ? Tone::Neutral : Tone::Warning);
    const mb_ai::FeatureSpec* spec = mb_ai::feature_spec(feature_);
    QStringList tip;
    tip << tr("%1 is answered by %2 — %3.")
               .arg(spec ? QString::fromUtf8(spec->label) : feature_, label, mb_reason_text(c.reason));
    const QString billing = mb_ai::ModelRouter::billing_note(c);
    if (!billing.isEmpty())
        tip << billing;
    else if (c.is_fincept())
        tip << tr("Fincept LLM uses your Fincept credits.");
    if (!skipped.isEmpty())
        tip << tr("Not used: %1").arg(skipped);
    tip << tr("Click to choose the model for this feature.");
    const QString tooltip = tip.join(QStringLiteral("\n"));

    const bool changed = shown != text_ || tone != tone_;
    text_ = shown;
    tone_ = tone;
    if (tooltip != tooltip_) {
        tooltip_ = tooltip;
        setToolTip(tooltip_);
    }
    setAccessibleName(tr("Model: %1").arg(label));
    if (changed) {
        updateGeometry();
        update();
    }
}

QString ModelBadge::elided_text(int width) const {
    return QFontMetrics(mb_font()).elidedText(text_, Qt::ElideRight, std::max(0, width));
}

QSize ModelBadge::sizeHint() const {
    const QFontMetrics fm(mb_font());
    const int pad = mb_pad(fm);
    const int max_text = fm.averageCharWidth() * (compact_ ? kMbCompactMaxTextChars : kMbMaxTextChars);
    const int text_w = std::min(fm.horizontalAdvance(text_), max_text);
    const int w = pad + mb_dot(fm) + pad / 2 + text_w + pad / 2 + fm.horizontalAdvance(mb_chevron()) + pad;
    return {w, fm.height() + 8};
}

QSize ModelBadge::minimumSizeHint() const {
    const QFontMetrics fm(mb_font());
    const int pad = mb_pad(fm);
    return {pad * 2 + mb_dot(fm) + fm.averageCharWidth() * 8 + fm.horizontalAdvance(mb_chevron()), fm.height() + 8};
}

void ModelBadge::paintEvent(QPaintEvent* e) {
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QFont f = mb_font();
    const QFontMetrics fm(f);
    p.setFont(f);
    const bool active = hover_ || hasFocus();
    p.setPen(QPen(QColor(active ? colors::AMBER() : colors::BORDER_MED()), 1));
    p.setBrush(QColor(colors::BG_BASE()));
    p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

    const int pad = mb_pad(fm);
    const int dot = mb_dot(fm);
    QColor dot_color(colors::TEXT_TERTIARY());
    if (tone_ == Tone::Harness)
        dot_color = QColor(colors::SUCCESS());
    else if (tone_ == Tone::Warning)
        dot_color = QColor(colors::WARNING());
    else
        dot_color = QColor(colors::AMBER());
    p.setPen(Qt::NoPen);
    p.setBrush(dot_color);
    int x = pad;
    p.drawEllipse(QRectF(x, (height() - dot) / 2.0, dot, dot));
    x += dot + pad / 2;

    const int chev_w = fm.horizontalAdvance(mb_chevron());
    const int text_w = width() - x - pad / 2 - chev_w - pad;
    p.setPen(QColor(active ? colors::TEXT_PRIMARY() : colors::TEXT_SECONDARY()));
    p.drawText(QRect(x, 0, std::max(0, text_w), height()), Qt::AlignVCenter | Qt::AlignLeft, elided_text(text_w));
    p.setPen(QColor(colors::TEXT_TERTIARY()));
    p.drawText(QRect(width() - pad - chev_w, 0, chev_w, height()), Qt::AlignVCenter | Qt::AlignLeft, mb_chevron());
}

void ModelBadge::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        open_picker();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void ModelBadge::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) {
        open_picker();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void ModelBadge::enterEvent(QEnterEvent* e) {
    hover_ = true;
    update();
    QWidget::enterEvent(e);
}

void ModelBadge::leaveEvent(QEvent* e) {
    hover_ = false;
    update();
    QWidget::leaveEvent(e);
}

void ModelBadge::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        refresh();
    QWidget::changeEvent(e);
}

void ModelBadge::open_picker() {
    auto& router = mb_ai::ModelRouter::instance();
    auto& reg = mb_h::HarnessRegistry::instance();
    reg.revalidate(); // stale sign-in states are re-checked in the background
    const mb_ai::RouterState state = router.state();
    const mb_ai::FeatureSpec* spec = mb_ai::feature_spec(feature_);
    const QString current_pid = router.choice_profile_id(feature_, context_id_);
    QString current_key;
    if (!current_pid.isEmpty() && state.profiles.contains(current_pid)) {
        const mb_ai::RouterProfile& p = state.profiles[current_pid];
        current_key = mb_choice_key(p.provider, p.model, p.effort);
    }

    // What "Auto" resolves to: the rule without this feature's explicit choice.
    mb_ai::RouterState auto_state = state;
    auto_state.assignments.remove(mb_ai::ModelRouter::assignment_key(feature_, context_id_));
    if (!context_id_.isEmpty())
        auto_state.assignments.remove(mb_ai::ModelRouter::assignment_key(feature_, {}));
    const mb_ai::RouteResult auto_route = mb_ai::ModelRouter::resolve_with(auto_state, feature_, context_id_);
    const QString auto_label = auto_route.chain.isEmpty() ? tr("Fincept LLM") : auto_route.chain.constFirst().label;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);
    QPointer<ModelBadge> self(this);
    const QString feature = feature_;
    const QString context = context_id_;
    const auto pick = [self, feature, context](const mb_ai::ModelChoice& c) {
        QString error;
        if (!mb_ai::ModelRouter::instance().set_choice(feature, context, c, &error)) {
            LOG_WARN("ModelBadge", QStringLiteral("Model choice not stored: %1").arg(error));
            return;
        }
        if (self)
            emit self->choice_changed();
    };

    QAction* auto_action = menu->addAction(literal_ampersands(tr("Auto — %1").arg(auto_label)));
    auto_action->setCheckable(true);
    auto_action->setChecked(current_pid.isEmpty());
    auto_action->setToolTip(tr("The terminal's rule: your choice → your connected harness → your API provider → "
                               "Fincept LLM."));
    connect(auto_action, &QAction::triggered, this, [self, feature, context]() {
        mb_ai::ModelRouter::instance().clear_choice(feature, context);
        if (self)
            emit self->choice_changed();
    });

    // ── Harnesses ──
    menu->addSection(tr("Agent harnesses"));
    const bool fit = !spec || spec->harness_fit != mb_ai::HarnessFit::No;
    bool any_harness = false;
    for (const mb_ai::RouterHarness& h : state.harnesses) {
        const mb_h::HarnessStatus st = reg.status(h.id);
        if (!st.installed())
            continue;
        any_harness = true;
        const mb_h::TerminalEligibility& el = h.eligibility;
        if (!fit) {
            QAction* a = menu->addAction(literal_ampersands(tr("%1 — not available for this feature").arg(h.display_name)));
            a->setEnabled(false);
            continue;
        }
        if (el.usable) {
            QMenu* sub = menu->addMenu(literal_ampersands(
                el.state == mb_h::TerminalState::Ready ? h.display_name
                                                       : tr("%1 · %2").arg(h.display_name, el.label)));
            sub->setToolTipsVisible(true);
            QStringList models{QString()};
            for (const mb_h::ModelInfo& m : st.models)
                models << m.id;
            for (const QString& model : std::as_const(models)) {
                const QString model_label = model.isEmpty() ? tr("CLI default") : model;
                const QStringList efforts = model.isEmpty() ? QStringList() : reg.efforts(h.id, model);
                const mb_h::IHarnessAdapter* ad = reg.adapter(h.id);
                const bool effort_cap = ad && ad->descriptor().caps.effort;
                const auto make_choice = [&h](const QString& m, const QString& e) {
                    mb_ai::ModelChoice c;
                    c.kind = mb_ai::ModelChoice::Kind::Harness;
                    c.harness_id = h.id;
                    c.provider = QStringLiteral("harness:") + h.id;
                    c.model = m;
                    c.effort = e;
                    c.vendor = h.vendor;
                    c.label = mb_ai::ModelRouter::harness_label(h.display_name, m, e);
                    return c;
                };
                if (!effort_cap || efforts.isEmpty()) {
                    QAction* a = sub->addAction(literal_ampersands(model_label));
                    a->setCheckable(true);
                    a->setChecked(current_key == mb_choice_key(QStringLiteral("harness:") + h.id, model, QString()));
                    const mb_ai::ModelChoice c = make_choice(model, QString());
                    connect(a, &QAction::triggered, this, [pick, c]() { pick(c); });
                    continue;
                }
                QMenu* em = sub->addMenu(literal_ampersands(model_label));
                for (const QString& effort : QStringList{QString()} + efforts) {
                    QAction* a = em->addAction(literal_ampersands(effort.isEmpty() ? tr("default effort") : effort));
                    a->setCheckable(true);
                    a->setChecked(current_key == mb_choice_key(QStringLiteral("harness:") + h.id, model, effort));
                    const mb_ai::ModelChoice c = make_choice(model, effort);
                    connect(a, &QAction::triggered, this, [pick, c]() { pick(c); });
                }
            }
            continue;
        }
        if (el.state == mb_h::TerminalState::ConsentNeeded) {
            QAction* a = menu->addAction(literal_ampersands(tr("%1 — needs your OK…").arg(h.display_name)));
            a->setToolTip(el.reason);
            const mb_ai::RouterHarness hh = h;
            connect(a, &QAction::triggered, this, [this, hh, pick]() {
                request_harness_terminal_consent(this, hh.id, /*enable_default=*/false, [hh, pick](bool agreed) {
                    if (!agreed)
                        return;
                    mb_ai::ModelChoice c;
                    c.kind = mb_ai::ModelChoice::Kind::Harness;
                    c.harness_id = hh.id;
                    c.provider = QStringLiteral("harness:") + hh.id;
                    c.model = hh.default_model;
                    c.effort = hh.default_effort;
                    c.vendor = hh.vendor;
                    c.label = mb_ai::ModelRouter::harness_label(hh.display_name, hh.default_model, hh.default_effort);
                    pick(c);
                });
            });
            continue;
        }
        QAction* a = menu->addAction(literal_ampersands(tr("%1 — %2").arg(h.display_name, el.label)));
        a->setEnabled(false);
        a->setToolTip(el.reason);
        if (!el.docs_url.isEmpty() &&
            (el.state == mb_h::TerminalState::SubscriptionOnly || el.state == mb_h::TerminalState::KeyStripped ||
             el.state == mb_h::TerminalState::SignedOut)) {
            QAction* help = menu->addAction(literal_ampersands(tr("    Use an API key with %1…").arg(h.display_name)));
            help->setToolTip(el.reason);
            const QString url = el.docs_url;
            connect(help, &QAction::triggered, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
        }
    }
    if (!any_harness) {
        QAction* a = menu->addAction(tr("No harness installed (Claude Code, Codex)"));
        a->setEnabled(false);
    }

    // ── API providers ──
    menu->addSection(tr("API providers"));
    QVector<mb_ai::RouterProfile> profiles;
    for (auto it = state.profiles.cbegin(); it != state.profiles.cend(); ++it) {
        const mb_ai::RouterProfile& p = it.value();
        if (p.provider.startsWith(QLatin1String("harness:"), Qt::CaseInsensitive) ||
            p.provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0)
            continue;
        profiles.append(p);
    }
    std::sort(profiles.begin(), profiles.end(), [](const mb_ai::RouterProfile& a, const mb_ai::RouterProfile& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    for (const mb_ai::RouterProfile& p : std::as_const(profiles)) {
        const QString provider_name = state.provider_names.value(p.provider.toLower(), p.provider);
        QAction* a = menu->addAction(literal_ampersands(tr("%1 · %2 / %3").arg(p.name, provider_name, p.model)));
        a->setCheckable(true);
        a->setChecked(p.id == current_pid);
        const bool ready = !p.key_required || p.has_key;
        a->setEnabled(ready);
        if (!ready)
            a->setToolTip(tr("This profile has no API key."));
        mb_ai::ModelChoice c;
        c.kind = mb_ai::ModelChoice::Kind::Api;
        c.provider = p.provider.toLower();
        c.model = p.model;
        c.profile_id = p.id;
        c.label = p.name;
        connect(a, &QAction::triggered, this, [pick, c]() { pick(c); });
    }
    const mb_ai::RouterActiveRow& row = state.active_row;
    if (row.present && row.provider != QLatin1String("fincept")) {
        const QString provider_name = state.provider_names.value(row.provider, row.provider);
        QAction* a = menu->addAction(literal_ampersands(
            tr("%1 · %2 (active provider)").arg(provider_name, row.model.isEmpty() ? tr("default model") : row.model)));
        const bool ready = !row.key_required || row.has_key;
        a->setEnabled(ready);
        if (!ready)
            a->setToolTip(tr("The active provider has no API key."));
        a->setCheckable(true);
        a->setChecked(current_key == mb_choice_key(row.provider, row.model, QString()));
        mb_ai::ModelChoice c;
        c.kind = mb_ai::ModelChoice::Kind::Api;
        c.provider = row.provider;
        c.model = row.model;
        c.vendor = provider_name;
        c.label = row.model.isEmpty() ? provider_name : provider_name + QStringLiteral(" · ") + row.model;
        connect(a, &QAction::triggered, this, [pick, c]() { pick(c); });
    }
    {
        QAction* a = menu->addAction(tr("Fincept LLM (uses credits)"));
        a->setCheckable(true);
        a->setChecked(current_key.startsWith(QLatin1String("fincept|")));
        mb_ai::ModelChoice c;
        c.kind = mb_ai::ModelChoice::Kind::Fincept;
        c.provider = QStringLiteral("fincept");
        c.label = QStringLiteral("Fincept LLM");
        c.vendor = QStringLiteral("Fincept");
        connect(a, &QAction::triggered, this, [pick, c]() { pick(c); });
    }
    menu->addSeparator();
    connect(menu->addAction(tr("Model settings…")), &QAction::triggered, this, []() { open_llm_settings(); });
    menu->popup(mapToGlobal(QPoint(0, height())));
}

// ── Consent ──────────────────────────────────────────────────────────────────

void request_harness_terminal_consent(QWidget* parent, const QString& harness_id, bool enable_default,
                                      std::function<void(bool agreed)> done) {
    auto& reg = harness::HarnessRegistry::instance();
    const harness::IHarnessAdapter* a = reg.adapter(harness_id);
    if (!a) {
        if (done)
            done(false);
        return;
    }
    if (reg.terminal_consent(harness_id)) {
        if (enable_default)
            reg.set_enabled_for_terminal(harness_id, true);
        if (done)
            done(true);
        return;
    }
    if (mb_open_consents().contains(harness_id)) {
        if (done)
            done(false); // already being asked
        return;
    }
    const harness::HarnessDescriptor& d = a->descriptor();
    const QString vendor = d.vendor.isEmpty() ? d.display_name : d.vendor;
    QWidget* host = parent ? parent->window() : QApplication::activeWindow();
    auto* box = new QMessageBox(host);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setIcon(QMessageBox::Question);
    box->setWindowTitle(ModelBadge::tr("Use %1 for terminal AI?").arg(d.display_name));
    box->setText(ModelBadge::tr("Terminal AI answered by %1 sends your data to %2.").arg(d.display_name, vendor));
    box->setInformativeText(
        ModelBadge::tr("Your messages, the conversation so far and whatever %1 reads through the terminal's tools — "
                       "the screens, tables, watchlists, portfolio and order details you have open — go to %2 and are "
                       "billed to your %2 API key. Fincept does not receive them.\n\n"
                       "Terminal AI uses %1 only while it signs in with an API key, never with a personal "
                       "subscription. Features that send your financial data (Chat Mode tasks, Alpha Arena) still "
                       "need their own switch.\n\n"
                       "You can withdraw this any time in Settings › LLM Configuration › Terminal AI.")
            .arg(d.display_name, vendor));
    QPushButton* allow =
        box->addButton(literal_ampersands(ModelBadge::tr("Allow — send to %1").arg(vendor)), QMessageBox::AcceptRole);
    QPushButton* cancel = box->addButton(QMessageBox::Cancel);
    box->setDefaultButton(cancel);
    box->setEscapeButton(cancel);
    box->setWindowModality(host ? Qt::WindowModal : Qt::ApplicationModal);
    mb_open_consents().insert(harness_id);
    QObject::connect(box, &QMessageBox::finished, box, [box, allow, harness_id, enable_default, done](int) {
        mb_open_consents().remove(harness_id);
        const bool agreed = box->clickedButton() == allow;
        if (agreed)
            harness::HarnessRegistry::instance().grant_terminal_consent(harness_id, enable_default);
        LOG_INFO("ModelBadge", QStringLiteral("Terminal AI consent for %1: %2")
                                   .arg(harness_id, agreed ? QStringLiteral("given") : QStringLiteral("declined")));
        if (done)
            done(agreed);
    });
    box->open();
}

void install_harness_consent_handler() {
    static bool installed = false;
    if (installed || !qApp)
        return;
    installed = true;
    QObject::connect(&harness::HarnessRegistry::instance(), &harness::HarnessRegistry::terminal_consent_requested, qApp,
                     [](const QString& id) {
                         request_harness_terminal_consent(QApplication::activeWindow(), id, /*enable_default=*/true);
                     });
}

void open_llm_settings() {
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                 QVariantMap{{QStringLiteral("screen_id"), QStringLiteral("settings")},
                                             {QStringLiteral("section"), QStringLiteral("llm_config")}});
}

} // namespace fincept::ui
