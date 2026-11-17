#include "ui/widgets/PlanHint.h"

#include "auth/AuthTypes.h"
#include "services/billing/PlanAccess.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EnterprisePromo.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::ui {

namespace {

constexpr const char* kPlanHintLink = "fincept://plans";

QString plan_hint_check() {
    return QString::fromUtf8("\xe2\x9c\x93"); // U+2713
}

QString plan_hint_plus() {
    return QStringLiteral("+");
}

/// "0.35" / "412" — credit amounts as the rest of the app prints them.
QString plan_hint_credits(double credits) {
    return auth::format_credits(credits);
}

} // namespace

// ── Shared copy ──────────────────────────────────────────────────────────────

namespace plan {

namespace {

/// Translation context for the free functions below.
QString plan_tr(const char* text) {
    return QCoreApplication::translate("fincept::ui::plan", text);
}

} // namespace

QString badge_tooltip() {
    const auto& access = fincept::billing::PlanAccess::instance();
    if (!access.has_table())
        return plan_tr("Plans & pricing — Pro is $10/month");
    const QString included = access.included_labels().join(QStringLiteral(", "));
    const auto& savings = access.savings();
    QStringList lines;
    if (access.is_paid()) {
        lines << plan_tr("Pro plan — included at no credit cost: %1.").arg(included);
        if (savings.credits_saved >= 0.01)
            lines << plan_tr("Saved this month: %1 credits.").arg(plan_hint_credits(savings.credits_saved));
        lines << plan_tr("Click to see your plan, renew or add credits.");
    } else {
        lines << plan_tr("Free plan — data calls are paid from your credits.");
        lines << plan_tr("Pro ($10/month) includes at no credit cost: %1.").arg(included);
        if (savings.would_save_with_pro >= 0.01)
            lines << plan_tr("Last %1 days: Pro would have saved you %2 credits.")
                         .arg(savings.window_days)
                         .arg(plan_hint_credits(savings.would_save_with_pro));
        if (access.rollover().credits > 0)
            lines << plan_tr("%1 credits from your last pass come back when you renew.")
                         .arg(plan_hint_credits(access.rollover().credits));
        lines << plan_tr("Click for Plans & Pricing.");
    }
    return lines.join(QLatin1Char('\n'));
}

QString credits_tooltip() {
    const auto& access = fincept::billing::PlanAccess::instance();
    if (!access.has_table())
        return plan_tr("Fincept credit balance");
    if (access.is_paid())
        return plan_tr("Fincept credit balance. On Pro your data calls use no credits — the balance is kept for "
                       "AI agents, research models and news analysis.");
    return plan_tr("Fincept credit balance. On Free every data call uses credits; on Pro ($10/month) data is "
                   "included and credits are kept for AI.");
}

QString out_of_credits_note(const QString& category) {
    return fincept::billing::PlanAccess::instance().upgrade_note(category);
}

} // namespace plan

// ── PlanHintLabel ────────────────────────────────────────────────────────────

PlanHintLabel::PlanHintLabel(const QString& category, QWidget* parent) : QLabel(parent), category_(category) {
    setObjectName(QStringLiteral("planHint"));
    setTextFormat(Qt::RichText);
    setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    setOpenExternalLinks(false);
    setStyleSheet(QString("QLabel#planHint{color:%1;background:transparent;border:none;font-size:" + fonts::px(-3) +
                          "px;}")
                      .arg(colors::TEXT_TERTIARY()));
    connect(this, &QLabel::linkActivated, this, [this](const QString&) { billing::open_plans(this); });
    connect(&fincept::billing::PlanAccess::instance(), &fincept::billing::PlanAccess::changed, this,
            &PlanHintLabel::render);
    // The stylesheet follows the theme through the central re-style pass; the
    // rich text's inline colours do not (text only — no restyle in this handler).
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) { render(); });
    render();
}

void PlanHintLabel::set_category(const QString& category) {
    if (category == category_)
        return;
    category_ = category;
    render();
}

void PlanHintLabel::set_compact(bool compact) {
    if (compact == compact_)
        return;
    compact_ = compact;
    render();
}

void PlanHintLabel::changeEvent(QEvent* event) {
    QLabel::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        render();
}

void PlanHintLabel::render() {
    const auto& access = fincept::billing::PlanAccess::instance();
    const fincept::billing::PlanPrice* price = access.price(category_);
    // Nothing to say when the server sent no table, or Pro pays the same.
    if (!price || !price->cheaper_on_pro()) {
        clear();
        // Only on a real change: this also runs from the theme_changed handler,
        // where a widget must not be shown, hidden or re-styled.
        if (!isHidden())
            setVisible(false);
        return;
    }
    const QString accent = QString(colors::AMBER());
    const QString label = price->label.toHtmlEscaped();
    QString text;
    if (access.is_paid()) {
        const QString chip = QStringLiteral("<span style=\"color:%1;font-weight:700;\">PRO</span>").arg(accent);
        text = price->included_with_pro
                   ? chip + QStringLiteral("&nbsp; ") + tr("%1 included — no credits used").arg(label)
                   : chip + QStringLiteral("&nbsp; ") + tr("%1: %2").arg(label, price->pro_price.toHtmlEscaped().toLower());
        setToolTip(tr("%1\nYour Pro plan: %2. Free accounts pay %3.")
                       .arg(price->summary, price->pro_price, price->free_price));
    } else {
        const QString offer = price->included_with_pro ? tr("Included with Pro")
                                                       : tr("%1 with Pro").arg(price->pro_price.toHtmlEscaped());
        const QString link = QStringLiteral("<a href=\"%1\" style=\"color:%2;text-decoration:none;\">%3 %4</a>")
                                 .arg(QLatin1String(kPlanHintLink), accent, offer, QString::fromUtf8("\xe2\x86\x97"));
        text = compact_ ? link
                        : label + QStringLiteral(": ") + price->free_price.toHtmlEscaped() +
                              QStringLiteral(" &middot; ") + link;
        setToolTip(tr("%1\nFree: %2. Pro ($10/month): %3. Click to see Plans & Pricing.")
                       .arg(price->summary, price->free_price, price->pro_price));
    }
    setText(text);
    if (isHidden())
        setVisible(true);
}

// ── PlanBenefitsView ─────────────────────────────────────────────────────────

PlanBenefitsView::PlanBenefitsView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("planBenefits"));
    // One sheet, set before any child exists; the central re-style pass keeps
    // it on the active theme and font.
    setStyleSheet(
        QString("QWidget#planBenefits{background:transparent;}"
                "QLabel#pbHeadline{color:%1;font-size:" + fonts::px(0) + "px;font-weight:600;background:transparent;}"
                "QLabel#pbHeadline[tone=\"saved\"]{color:%2;}"
                "QLabel#pbRollover{color:%3;font-size:" + fonts::px(-1) + "px;background:transparent;}"
                "QLabel#pbSection{color:%4;font-size:" + fonts::px(-3) + "px;font-weight:700;background:transparent;}"
                "QLabel#pbMark{color:%4;font-size:" + fonts::px(-1) + "px;font-weight:700;background:transparent;}"
                "QLabel#pbMark[active=\"true\"]{color:%2;}"
                "QLabel#pbTitle{color:%1;font-size:" + fonts::px(-1) + "px;font-weight:600;background:transparent;}"
                "QLabel#pbDetail{color:%5;font-size:" + fonts::px(-2) + "px;background:transparent;}"
                "QLabel#pbHead{color:%4;font-size:" + fonts::px(-3) + "px;font-weight:700;background:transparent;}"
                "QLabel#pbCell{color:%5;font-size:" + fonts::px(-2) + "px;background:transparent;}"
                "QLabel#pbCell[role=\"name\"]{color:%1;}"
                "QLabel#pbCell[role=\"pro\"]{color:%3;font-weight:600;}"
                "QLabel#pbCell[yours=\"true\"]{font-weight:700;}"
                "QFrame#pbRule{background:%6;border:none;}")
            .arg(colors::TEXT_PRIMARY(), colors::SUCCESS(), colors::AMBER(), colors::TEXT_TERTIARY(),
                 colors::TEXT_SECONDARY(), colors::BORDER_DIM()));
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(12, 10, 12, 12);
    layout_->setSpacing(6);
    connect(&fincept::billing::PlanAccess::instance(), &fincept::billing::PlanAccess::changed, this,
            &PlanBenefitsView::rebuild);
    rebuild();
}

void PlanBenefitsView::set_show_upgrade_button(bool show) {
    if (show == show_upgrade_button_)
        return;
    show_upgrade_button_ = show;
    rebuild();
}

void PlanBenefitsView::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        rebuild();
}

void PlanBenefitsView::rebuild() {
    while (QLayoutItem* item = layout_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        if (QLayout* l = item->layout()) {
            while (QLayoutItem* sub = l->takeAt(0)) {
                if (QWidget* w = sub->widget())
                    w->deleteLater();
                delete sub;
            }
        }
        delete item;
    }
    const auto& access = fincept::billing::PlanAccess::instance();
    if (!access.has_table()) {
        setVisible(false);
        return;
    }
    setVisible(true);
    const bool paid = access.is_paid();
    const auto& savings = access.savings();
    const auto& rollover = access.rollover();

    // Headline: the plan in credits.
    auto* headline = new QLabel(this);
    headline->setObjectName(QStringLiteral("pbHeadline"));
    headline->setWordWrap(true);
    if (paid) {
        if (savings.credits_saved >= 0.01) {
            headline->setText(tr("Pro has saved you %1 credits this month — your data calls use none.")
                                  .arg(plan_hint_credits(savings.credits_saved)));
            headline->setProperty("tone", "saved");
        } else {
            headline->setText(tr("Your Pro plan is active: data calls use no credits, so your balance is kept for AI."));
        }
    } else if (savings.would_save_with_pro >= 0.01) {
        headline->setText(tr("In the last %1 days you spent %2 credits on data. Pro would have saved %3 of them.")
                              .arg(savings.window_days)
                              .arg(plan_hint_credits(savings.spent_on_included),
                                   plan_hint_credits(savings.would_save_with_pro)));
    } else {
        headline->setText(tr("On Pro, market, macro and research data use no credits — your balance is kept for AI."));
    }
    layout_->addWidget(headline);

    if (rollover.credits > 0) {
        auto* waiting = new QLabel(this);
        waiting->setObjectName(QStringLiteral("pbRollover"));
        waiting->setWordWrap(true);
        const QString until = auth::format_local_stamp(rollover.expires_at, false);
        waiting->setText(until.isEmpty()
                             ? tr("%1 credits from your last pass are waiting — renew Pro to get them back.")
                                   .arg(plan_hint_credits(rollover.credits))
                             : tr("%1 credits from your last pass are waiting — renew Pro by %2 to get them back.")
                                   .arg(plan_hint_credits(rollover.credits), until));
        layout_->addWidget(waiting);
    }

    // What Pro adds.
    if (!access.benefits().isEmpty()) {
        layout_->addSpacing(6);
        auto* section = new QLabel(paid ? tr("INCLUDED IN YOUR PLAN") : tr("WHAT PRO ADDS"), this);
        section->setObjectName(QStringLiteral("pbSection"));
        layout_->addWidget(section);
        auto* grid = new QGridLayout;
        grid->setContentsMargins(0, 2, 0, 0);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(6);
        grid->setColumnStretch(1, 1);
        int row = 0;
        for (const auto& benefit : access.benefits()) {
            auto* mark = new QLabel(benefit.active ? plan_hint_check() : plan_hint_plus(), this);
            mark->setObjectName(QStringLiteral("pbMark"));
            mark->setProperty("active", benefit.active);
            mark->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
            mark->setAccessibleName(benefit.active ? tr("Active") : tr("Available with Pro"));
            grid->addWidget(mark, row, 0, Qt::AlignTop);
            auto* cell = new QVBoxLayout;
            cell->setContentsMargins(0, 0, 0, 0);
            cell->setSpacing(1);
            auto* title = new QLabel(benefit.title, this);
            title->setObjectName(QStringLiteral("pbTitle"));
            title->setWordWrap(true);
            cell->addWidget(title);
            if (!benefit.detail.isEmpty()) {
                auto* detail = new QLabel(benefit.detail, this);
                detail->setObjectName(QStringLiteral("pbDetail"));
                detail->setWordWrap(true);
                cell->addWidget(detail);
            }
            grid->addLayout(cell, row, 1);
            ++row;
        }
        layout_->addLayout(grid);
    }

    // Price of every category on both plans.
    if (!access.prices().isEmpty()) {
        layout_->addSpacing(8);
        auto* rule = new QFrame(this);
        rule->setObjectName(QStringLiteral("pbRule"));
        rule->setFixedHeight(1);
        layout_->addWidget(rule);
        layout_->addSpacing(4);
        auto* grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(4);
        grid->setColumnStretch(0, 3);
        grid->setColumnStretch(1, 2);
        grid->setColumnStretch(2, 2);
        const QStringList heads{tr("WHAT A CALL COSTS"), tr("FREE"), tr("PRO")};
        for (int c = 0; c < heads.size(); ++c) {
            auto* head = new QLabel(heads.at(c), this);
            head->setObjectName(QStringLiteral("pbHead"));
            grid->addWidget(head, 0, c);
        }
        int row = 1;
        for (const auto& price : access.prices()) {
            auto* name = new QLabel(price.label, this);
            name->setObjectName(QStringLiteral("pbCell"));
            name->setProperty("role", "name");
            name->setToolTip(price.summary);
            name->setWordWrap(true);
            grid->addWidget(name, row, 0);
            auto* free_cell = new QLabel(price.free_price, this);
            free_cell->setObjectName(QStringLiteral("pbCell"));
            free_cell->setProperty("yours", !paid);
            free_cell->setWordWrap(true);
            grid->addWidget(free_cell, row, 1);
            auto* pro_cell = new QLabel(price.pro_price, this);
            pro_cell->setObjectName(QStringLiteral("pbCell"));
            if (price.cheaper_on_pro())
                pro_cell->setProperty("role", "pro");
            pro_cell->setProperty("yours", paid);
            pro_cell->setWordWrap(true);
            grid->addWidget(pro_cell, row, 2);
            ++row;
        }
        layout_->addLayout(grid);
    }

    if (!paid && show_upgrade_button_) {
        layout_->addSpacing(8);
        auto* actions = new QHBoxLayout;
        actions->setContentsMargins(0, 0, 0, 0);
        auto* upgrade =
            enterprise::make_cta_button(tr("UPGRADE TO PRO"), QString(), enterprise::CtaStyle::Primary, this);
        upgrade->setToolTip(tr("Open Plans && Pricing"));
        connect(upgrade, &QPushButton::clicked, this, [this]() { billing::open_plans(this); });
        actions->addWidget(upgrade);
        actions->addStretch();
        layout_->addLayout(actions);
    }
}

} // namespace fincept::ui
