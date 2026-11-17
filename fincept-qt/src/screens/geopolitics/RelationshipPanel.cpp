// src/screens/geopolitics/RelationshipPanel.cpp
#include "screens/geopolitics/RelationshipPanel.h"

#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QAction>
#include <QCursor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QLocale>
#include <QMouseEvent>
#include <QScrollArea>

#include <algorithm>
#include <utility>

namespace fincept::screens {

using namespace fincept::services::geo;

/// The humanitarian organisations shown as entry points to their HDX datasets.
/// `id` is the HDX organisation slug; the pills are their fields of work.
static QVector<RelationshipNode> organization_nodes() {
    QVector<RelationshipNode> nodes;
    nodes.append({"unhcr", "UNHCR", "organization", "", {"displacement", "refugees"}});
    nodes.append({"wfp", "WFP", "organization", "", {"food security", "humanitarian"}});
    nodes.append({"world-health-organization", "WHO", "organization", "", {"health", "humanitarian"}});
    nodes.append({"fao", "FAO", "organization", "", {"food security", "agriculture"}});
    return nodes;
}

/// Most active countries shown as conflict cards.
static constexpr int kRelMaxCountries = 9;
/// Event categories shown as crisis cards.
static constexpr int kRelMaxCategories = 9;

// Severity is a state, not a price direction: red → orange → yellow → green.
static QColor severity_color(const QString& sev) {
    if (sev == "critical")
        return QColor(ui::colors::DANGER());
    if (sev == "high")
        return QColor(ui::colors::AMBER());
    if (sev == "medium")
        return QColor(ui::colors::WARNING());
    if (sev == "low")
        return QColor(ui::colors::SUCCESS());
    return QColor(ui::colors::INFO()); // organization
}

RelationshipPanel::RelationshipPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void RelationshipPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setMinimumHeight(48);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    hhl->setSpacing(12);

    title_lbl_ = new QLabel(tr("GEOPOLITICAL RELATIONSHIP NETWORK"), header);
    title_lbl_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                                  .arg(ui::colors::INFO())
                                  .arg(ui::fonts::TINY.get())
                                  .arg(ui::fonts::DATA_FAMILY()));
    hhl->addWidget(title_lbl_);

    // Shown until the live country / category data has arrived.
    sample_badge_ = new QLabel(tr("LOADING"), header);
    sample_badge_->setToolTip(tr("Waiting for the live event statistics."));
    sample_badge_->setStyleSheet(QString("color:%4; background:%1; font-size:%2px; font-weight:700;"
                                         "font-family:%3; padding:2px 6px; border-radius:2px; letter-spacing:1px;")
                                     .arg(ui::colors::WARNING())
                                     .arg(ui::fonts::TINY.get())
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::colors::TEXT_ON_ACCENT()));
    hhl->addWidget(sample_badge_);

    hhl->addStretch();

    // The live statistics are two paid reference lists: fetched only on request.
    load_stats_btn_ = new QPushButton(header);
    load_stats_btn_->setProperty("fincept_agent_risk", "spend"); // agents: loads relationship statistics (6 credits)
    load_stats_btn_->setCursor(Qt::PointingHandCursor);
    load_stats_btn_->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2;"
                                           "font-family:%3; font-size:%4px; font-weight:700; padding:3px 10px; }"
                                           "QPushButton:hover { background:%5; }"
                                           "QPushButton:disabled { color:%6; border-color:%7; }")
                                       .arg(ui::colors::INFO(), ui::colors::alpha(ui::colors::INFO, 0.5))
                                       .arg(ui::fonts::DATA_FAMILY())
                                       .arg(ui::fonts::TINY.get())
                                       .arg(ui::colors::alpha(ui::colors::INFO, 0.10), ui::colors::TEXT_TERTIARY(),
                                            ui::colors::BORDER_DIM()));
    load_stats_btn_->setToolTip(tr("Fetch the live country and category statistics of the news-event feed "
                                   "(two lists, 3 Fincept credits each)."));
    connect(load_stats_btn_, &QPushButton::clicked, this, &RelationshipPanel::stats_requested);
    hhl->addWidget(load_stats_btn_);

    stats_lbl_ = new QLabel(header);
    stats_lbl_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:2px 8px;"
                                      "background:%5; border:1px solid %4;")
                                  .arg(ui::colors::TEXT_SECONDARY())
                                  .arg(ui::fonts::TINY.get())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::BORDER_DIM())
                                  .arg(ui::colors::overlay(0.04)));
    hhl->addWidget(stats_lbl_);
    root->addWidget(header);

    // Network view
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border:none; background:%1; }"
                                  "QScrollBar:vertical { background:%1; width:6px; }"
                                  "QScrollBar::handle:vertical { background:%2; border-radius:3px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));

    auto* content = new QWidget(scroll);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(16, 16, 16, 16);
    cvl->setSpacing(16);

    // How the cards are derived (live event statistics, relative severity bands).
    {
        sample_note_ = new QLabel(content);
        sample_note_->setWordWrap(true);
        sample_note_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:8px 10px;"
                                            "background:%4; border:1px solid %5;")
                                        .arg(ui::colors::TEXT_SECONDARY())
                                        .arg(ui::fonts::SMALL.get())
                                        .arg(ui::fonts::DATA_FAMILY)
                                        .arg(ui::colors::overlay(0.03))
                                        .arg(ui::colors::BORDER_DIM()));
        cvl->addWidget(sample_note_);
    }

    // Card sections — rebuilt whenever live countries / categories arrive.
    sections_layout_ = new QVBoxLayout;
    sections_layout_->setContentsMargins(0, 0, 0, 0);
    sections_layout_->setSpacing(16);
    cvl->addLayout(sections_layout_);
    rebuild_sections();

    cvl->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

void RelationshipPanel::set_countries(const QVector<UniqueCountry>& countries) {
    countries_ = countries;
    stats_error_.clear();
    rebuild_sections();
}

void RelationshipPanel::set_categories(const QVector<UniqueCategory>& categories) {
    categories_ = categories;
    stats_error_.clear();
    rebuild_sections();
}

void RelationshipPanel::set_page_statistics(const QVector<UniqueCountry>& countries,
                                            const QVector<UniqueCategory>& categories, int events) {
    page_countries_ = countries;
    page_categories_ = categories;
    page_events_ = events;
    // Live statistics win; the page counts only fill in while they are missing.
    if (countries_.isEmpty() && categories_.isEmpty())
        rebuild_sections();
}

void RelationshipPanel::set_stats_loading(bool on) {
    if (stats_loading_ == on)
        return;
    stats_loading_ = on;
    if (on)
        stats_error_.clear();
    rebuild_sections();
}

void RelationshipPanel::set_stats_error(const QString& error) {
    stats_loading_ = false;
    stats_error_ = error;
    rebuild_sections();
}

void RelationshipPanel::rebuild_sections() {
    if (!sections_layout_)
        return;
    while (sections_layout_->count() > 0) {
        auto* item = sections_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    sec_conflicts_ = sec_crisis_ = sec_orgs_ = nullptr;
    QWidget* host = sections_layout_->parentWidget();

    // Live inputs, most active first — or, while those are not loaded, the
    // counts of the events page on the Monitor. The unknown-country
    // placeholder ("null") is not a place.
    showing_live_ = !countries_.isEmpty() || !categories_.isEmpty();
    const QVector<UniqueCountry>& src_countries = showing_live_ ? countries_ : page_countries_;
    const QVector<UniqueCategory>& src_categories = showing_live_ ? categories_ : page_categories_;
    QVector<UniqueCountry> countries;
    for (const auto& c : src_countries)
        if (!c.country.isEmpty() && c.country.compare(QLatin1String("null"), Qt::CaseInsensitive) != 0 &&
            c.event_count > 0)
            countries.append(c);
    std::sort(countries.begin(), countries.end(),
              [](const UniqueCountry& a, const UniqueCountry& b) { return a.event_count > b.event_count; });
    countries.resize(std::min<qsizetype>(countries.size(), kRelMaxCountries));
    QVector<UniqueCategory> cats;
    for (const auto& c : src_categories)
        if (!c.category.isEmpty() && c.event_count > 0)
            cats.append(c);
    std::sort(cats.begin(), cats.end(),
              [](const UniqueCategory& a, const UniqueCategory& b) { return a.event_count > b.event_count; });
    cats.resize(std::min<qsizetype>(cats.size(), kRelMaxCategories));

    // Severity is relative to the busiest entry of the same list: >= 50 % of its
    // event count critical, >= 20 % high, otherwise medium.
    auto band = [](int count, int top) -> QString {
        if (top <= 0)
            return QStringLiteral("medium");
        const double r = static_cast<double>(count) / top;
        return r >= 0.5 ? QStringLiteral("critical") : (r >= 0.2 ? QStringLiteral("high") : QStringLiteral("medium"));
    };
    auto events_text = [](int n) { return tr("%1 events").arg(QLocale().toString(n)); };

    auto add_section = [&](const QString& title, const QString& color, QLabel** out,
                           const QVector<QPair<RelationshipNode, QString>>& cards, const QString& empty_text) {
        auto* sec_lbl = new QLabel(title, host);
        if (out)
            *out = sec_lbl;
        sec_lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                       "letter-spacing:2px; padding-bottom:4px; border-bottom:1px solid %4;")
                                   .arg(color)
                                   .arg(ui::fonts::SMALL.get())
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::colors::alpha(QColor(color), 0.3)));
        sections_layout_->addWidget(sec_lbl);
        if (cards.isEmpty()) {
            auto* empty = new QLabel(empty_text, host);
            empty->setWordWrap(true);
            empty->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:6px 0;")
                                     .arg(ui::colors::TEXT_TERTIARY())
                                     .arg(ui::fonts::SMALL.get())
                                     .arg(ui::fonts::DATA_FAMILY));
            sections_layout_->addWidget(empty);
            return;
        }
        auto* grid_w = new QWidget(host);
        auto* gl = new QGridLayout(grid_w);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setSpacing(10);
        for (int c = 0; c < 3; ++c)
            gl->setColumnStretch(c, 1);
        int col = 0, row = 0;
        for (const auto& [node, detail] : cards) {
            gl->addWidget(build_node_card(node, grid_w, detail), row, col);
            if (++col >= 3) {
                col = 0;
                row++;
            }
        }
        while (col > 0 && col < 3) {
            auto* spacer = new QWidget(grid_w);
            spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            gl->addWidget(spacer, row, col++);
        }
        sections_layout_->addWidget(grid_w);
    };

    QVector<QPair<RelationshipNode, QString>> conflict_cards;
    const int top_country = countries.isEmpty() ? 0 : countries.first().event_count;
    for (const auto& c : std::as_const(countries))
        conflict_cards.append({RelationshipNode{c.country, c.country, QStringLiteral("conflict"),
                                                band(c.event_count, top_country), {}},
                               events_text(c.event_count)});
    QVector<QPair<RelationshipNode, QString>> crisis_cards;
    const int top_cat = cats.isEmpty() ? 0 : cats.first().event_count;
    for (const auto& c : std::as_const(cats))
        crisis_cards.append({RelationshipNode{c.category, pretty_category(c.category), QStringLiteral("crisis"),
                                              band(c.event_count, top_cat), {}},
                             events_text(c.event_count)});
    QVector<QPair<RelationshipNode, QString>> org_cards;
    for (const auto& o : organization_nodes())
        org_cards.append({o, tr("HDX datasets")});

    if (conflict_cards.isEmpty() && crisis_cards.isEmpty()) {
        // Nothing to draw yet: a skeleton while the lists load, else what LOAD
        // fetches and what it costs (or why it failed) — never "waiting…".
        if (stats_loading_) {
            auto* sk = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Cards, host);
            sk->set_message(tr("LOADING LIVE EVENT STATISTICS…"));
            sk->set_opaque(false);
            sk->setMinimumHeight(260);
            sections_layout_->addWidget(sk);
        } else {
            auto* es = new ui::EmptyState(host);
            es->set_alignment(Qt::AlignTop);
            if (!stats_error_.isEmpty()) {
                es->set_glyph(QStringLiteral("!"), ui::EmptyState::Tone::Danger);
                es->set_title(tr("Live statistics could not be loaded"));
                es->set_message(stats_error_.left(160));
                es->set_note(tr("6 credits per load (countries + categories)"), ui::EmptyState::Tone::Warning);
                es->set_primary_action(tr("RETRY (6 CREDITS)"));
            } else {
                es->set_glyph(QStringLiteral("◎"), ui::EmptyState::Tone::Info);
                es->set_title(tr("Most active countries and event categories"));
                es->set_message(tr("Cards for the busiest countries and event categories of the news-event feed, "
                                   "ranked by event count with a relative severity band. Load events on the "
                                   "MONITOR tab to see the counts of that page here for free."));
                es->set_details({{tr("Countries list"), tr("3 credits")},
                                 {tr("Categories list"), tr("3 credits")},
                                 {tr("Kept for"), tr("6 hours — reopening is free")}},
                                tr("WHAT LOAD FETCHES"));
                es->set_note(tr("Not loaded automatically — 6 credits per load"), ui::EmptyState::Tone::Warning);
                es->set_primary_action(tr("LOAD LIVE STATISTICS (6 CREDITS)"));
            }
            connect(es, &ui::EmptyState::primary_clicked, this, &RelationshipPanel::stats_requested);
            sections_layout_->addWidget(es);
        }
    } else {
        // Page counts are labelled as such: they describe one page of events,
        // not the whole feed.
        const QString scope = showing_live_ ? QString() : tr("  ·  LATEST %1 EVENTS").arg(page_events_);
        const QString none = tr("No entries in this list.");
        add_section(tr("MOST ACTIVE COUNTRIES") + scope, ui::colors::DANGER(), &sec_conflicts_, conflict_cards, none);
        add_section(tr("EVENT CATEGORIES") + scope, ui::colors::WARNING(), &sec_crisis_, crisis_cards, none);
    }
    add_section(tr("ORGANIZATIONS"), ui::colors::INFO(), &sec_orgs_, org_cards, QString());

    node_count_ = conflict_cards.size() + crisis_cards.size() + org_cards.size();
    conflict_count_ = conflict_cards.size();
    org_count_ = org_cards.size();
    retranslateUi(); // header badge / button / stats + note text for the new state
}

QWidget* RelationshipPanel::build_node_card(const RelationshipNode& node, QWidget* parent, const QString& detail) {
    auto color = severity_color(node.severity.isEmpty() ? "organization" : node.severity);
    const QString col_hex = color.name();

    auto* card = new QWidget(parent);
    card->setObjectName("nodeCard");
    card->setMinimumHeight(110);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // Every card opens its drill-down menu on click: a country its events and
    // HDX datasets, a category its HDX topic, an organisation its HDX datasets.
    card->setProperty("node_id", node.id);
    card->setProperty("node_type", node.type);
    card->setProperty("node_label", node.label);
    card->setCursor(Qt::PointingHandCursor);
    card->setToolTip(node.type == QLatin1String("organization") ? tr("Click to browse %1 datasets on HDX").arg(node.label)
                                                                : tr("Click for related events and HDX datasets"));
    card->installEventFilter(this);
    card->setStyleSheet(QString("#nodeCard { background:%1; border:1px solid %2;"
                                "border-left:3px solid %3; }")
                            .arg(ui::colors::BG_RAISED())
                            .arg(ui::colors::alpha(color, 0.25))
                            .arg(col_hex));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 10, 12, 10);
    vl->setSpacing(4);

    // Name row
    auto* name_row = new QWidget(card);
    auto* name_hl = new QHBoxLayout(name_row);
    name_hl->setContentsMargins(0, 0, 0, 0);
    name_hl->setSpacing(6);

    auto* name = new QLabel(node.label.toUpper(), name_row);
    name->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                            .arg(col_hex)
                            .arg(ui::fonts::SMALL.get())
                            .arg(ui::fonts::DATA_FAMILY));
    name_hl->addWidget(name, 1);

    const QString badge_text = node.severity.isEmpty() ? node.type.toUpper() : node.severity.toUpper();
    auto* badge = new QLabel(badge_text, name_row);
    badge->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                 "padding:2px 6px; background:%4; border:1px solid %5;"
                                 "border-radius:8px; letter-spacing:1px;")
                             .arg(col_hex)
                             .arg(ui::fonts::TINY.get())
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::colors::alpha(color, 0.12))
                             .arg(ui::colors::alpha(color, 0.3)));
    name_hl->addWidget(badge);
    vl->addWidget(name_row);

    // Live figure (event count) when there is one, else the node type.
    auto* type_lbl = new QLabel(detail.isEmpty() ? node.type.toUpper() : detail, card);
    type_lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::TINY.get())
                                .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(type_lbl);

    // Connection pills (max 3)
    if (!node.connections.isEmpty()) {
        auto* pills_row = new QWidget(card);
        auto* pills_hl = new QHBoxLayout(pills_row);
        pills_hl->setContentsMargins(0, 2, 0, 0);
        pills_hl->setSpacing(4);

        const int max_pills = 3;
        int shown = qMin(node.connections.size(), max_pills);
        for (int i = 0; i < shown; ++i) {
            auto* pill = new QLabel(node.connections[i], pills_row);
            pill->setStyleSheet(QString("color:%1; font-size:%2px; padding:1px 6px;"
                                        "background:%5; border:1px solid %3;"
                                        "border-radius:8px; font-family:%4;")
                                    .arg(ui::colors::TEXT_SECONDARY())
                                    .arg(ui::fonts::TINY.get())
                                    .arg(ui::colors::BORDER_DIM())
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::colors::overlay(0.05)));
            pills_hl->addWidget(pill);
        }

        if (node.connections.size() > max_pills) {
            auto* more = new QLabel(QString("+%1").arg(node.connections.size() - max_pills), pills_row);
            more->setStyleSheet(QString("color:%1; font-size:%2px; padding:1px 6px;"
                                        "background:%5; border:1px solid %3;"
                                        "border-radius:8px; font-family:%4;")
                                    .arg(ui::colors::TEXT_SECONDARY())
                                    .arg(ui::fonts::TINY.get())
                                    .arg(ui::colors::BORDER_DIM())
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::colors::overlay(0.05)));
            pills_hl->addWidget(more);
        }

        pills_hl->addStretch();
        vl->addWidget(pills_row);
    }

    return card;
}

bool RelationshipPanel::eventFilter(QObject* obj, QEvent* event) {
    // Labels inside a card ignore the press, so it propagates up to the card.
    if (event->type() == QEvent::MouseButtonPress) {
        auto* card = qobject_cast<QWidget*>(obj);
        auto* me = static_cast<QMouseEvent*>(event);
        if (card && me->button() == Qt::LeftButton && !card->property("node_type").toString().isEmpty()) {
            show_node_menu(card);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void RelationshipPanel::show_node_menu(QWidget* card) {
    const QString type = card->property("node_type").toString();
    const QString id = card->property("node_id").toString();
    const QString label = card->property("node_label").toString();

    QMenu menu(this);
    if (type == QLatin1String("conflict")) {
        connect(menu.addAction(tr("Show %1 events in Monitor").arg(label)), &QAction::triggered, this,
                [this, label]() { emit events_requested(label); });
        connect(menu.addAction(tr("Browse %1 datasets on HDX").arg(label)), &QAction::triggered, this,
                [this, label]() { emit hdx_country_requested(label); });
    } else if (type == QLatin1String("crisis")) {
        // Node ids are snake_case slugs ("food_security"); HDX topics are plain words.
        QString topic = id;
        topic.replace(QLatin1Char('_'), QLatin1Char(' '));
        connect(menu.addAction(tr("Browse HDX datasets: %1").arg(label)), &QAction::triggered, this,
                [this, topic]() { emit hdx_topic_requested(topic); });
    } else if (type == QLatin1String("organization")) {
        connect(menu.addAction(tr("Browse %1 datasets on HDX").arg(label)), &QAction::triggered, this,
                [this, id]() { emit hdx_organization_requested(id); });
    }
    if (!menu.isEmpty())
        menu.exec(QCursor::pos());
}

void RelationshipPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void RelationshipPanel::retranslateUi() {
    if (title_lbl_)
        title_lbl_->setText(tr("GEOPOLITICAL RELATIONSHIP NETWORK"));
    const bool page_counts = !showing_live_ && (!page_countries_.isEmpty() || !page_categories_.isEmpty());
    if (sample_badge_) {
        // What the cards show: live statistics (no badge), the Monitor's page
        // counts, a load in progress, or nothing loaded yet.
        sample_badge_->setVisible(!showing_live_);
        if (stats_loading_) {
            sample_badge_->setText(tr("LOADING"));
            sample_badge_->setToolTip(tr("Fetching the live event statistics."));
        } else if (page_counts) {
            sample_badge_->setText(tr("LATEST %1 EVENTS").arg(page_events_));
            sample_badge_->setToolTip(tr("Counts of the events page loaded on the Monitor tab (no credits). "
                                         "LOAD LIVE STATISTICS ranks the whole feed."));
        } else {
            sample_badge_->setText(tr("NOT LOADED"));
            sample_badge_->setToolTip(tr("The live statistics are loaded on request (6 credits)."));
        }
    }
    if (load_stats_btn_) {
        load_stats_btn_->setVisible(!showing_live_);
        load_stats_btn_->setEnabled(!stats_loading_);
        load_stats_btn_->setText(stats_loading_ ? tr("LOADING…") : tr("LOAD LIVE STATISTICS · 6 CR"));
    }
    if (sample_note_)
        sample_note_->setText(
            page_counts
                ? tr("Counts of the %1 events on the Monitor tab, with severity relative to the busiest entry "
                     "(critical ≥ 50 %, high ≥ 20 % of its events). LOAD LIVE STATISTICS ranks the whole feed. "
                     "Click a card for its events in the Monitor and its HDX datasets.")
                      .arg(page_events_)
                : tr("Live event statistics: the most active countries and event categories of the news-event feed, "
                     "with severity relative to the busiest entry (critical ≥ 50 %, high ≥ 20 % of its events). "
                     "Click a card for its events in the Monitor and its HDX datasets."));
    if (stats_lbl_)
        stats_lbl_->setText(tr("COUNTRIES: %1  |  CATEGORIES: %2  |  ORGANIZATIONS: %3")
                                .arg(conflict_count_)
                                .arg(node_count_ - conflict_count_ - org_count_)
                                .arg(org_count_));
    const QString scope = page_counts ? tr("  ·  LATEST %1 EVENTS").arg(page_events_) : QString();
    if (sec_conflicts_)
        sec_conflicts_->setText(tr("MOST ACTIVE COUNTRIES") + scope);
    if (sec_crisis_)
        sec_crisis_->setText(tr("EVENT CATEGORIES") + scope);
    if (sec_orgs_)
        sec_orgs_->setText(tr("ORGANIZATIONS"));
    // Node-card entity names/types/connections are fixed data values (not translated).
}

} // namespace fincept::screens
