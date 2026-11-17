#include "screens/news/NewsSidePanel.h"

#include "ui/formatting/DateFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFontMetrics>
#include <QHeaderView>
#include <QLocale>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

/// Width of a text of `chars` characters in the user's font at `offset`.
int text_width(int chars, int offset, int weight = QFont::Normal) {
    return QFontMetrics(ui::fonts::font(offset, weight)).horizontalAdvance(QString(chars, QLatin1Char('M')));
}

/// A push button that shows as much of its text as fits and elides the rest
/// (the full text is its tooltip), re-eliding when the drawer is resized —
/// drawer rows used to be cut to a fixed 16–50 characters.
class NewsElidedButton final : public QPushButton {
  public:
    NewsElidedButton(const QString& full, QWidget* parent) : QPushButton(parent), full_(full) {
        setMinimumWidth(40);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        relabel();
    }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        QPushButton::resizeEvent(e);
        relabel();
    }

  private:
    void relabel() {
        const int w = std::max(20, width() - 14);
        QString t = fontMetrics().elidedText(full_, Qt::ElideRight, w);
        setText(t.replace(QLatin1Char('&'), QStringLiteral("&&"))); // '&' is a mnemonic marker
    }
    QString full_;
};

/// The QLabel counterpart of NewsElidedButton.
class NewsElidedLabel final : public QLabel {
  public:
    NewsElidedLabel(const QString& full, QWidget* parent) : QLabel(parent), full_(full) {
        setTextFormat(Qt::PlainText);
        setMinimumWidth(40);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setToolTip(full);
        relabel();
    }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        QLabel::resizeEvent(e);
        relabel();
    }

  private:
    void relabel() { setText(fontMetrics().elidedText(full_, Qt::ElideRight, std::max(20, width() - 4))); }
    QString full_;
};

void nsp_clear_layout(QVBoxLayout* layout) {
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

} // namespace

NewsSidePanel::NewsSidePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("newsDrawerPanel");
    // Sized by the screen's splitter; this is only the floor (and the first-open
    // width hint below).
    setMinimumWidth(220);
    hide(); // start hidden — toggled by INTEL button

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Drawer header with close button
    auto* header = new QWidget(this);
    header->setObjectName("newsDrawerHeader");
    header->setMinimumHeight(30);
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(10, 0, 6, 0);
    header_layout->setSpacing(0);

    drawer_title_ = new QLabel(tr("INTELLIGENCE"), header);
    drawer_title_->setObjectName("newsDrawerTitle");
    header_layout->addWidget(drawer_title_);
    header_layout->addStretch();

    auto* close_btn = new QPushButton("x", header);
    close_btn->setObjectName("newsDrawerCloseBtn");
    close_btn->setFixedSize(22, 22);
    close_btn->setCursor(Qt::PointingHandCursor);
    // The screen owns open/close (it also syncs the INTEL button).
    connect(close_btn, &QPushButton::clicked, this, &NewsSidePanel::close_requested);
    header_layout->addWidget(close_btn);
    outer->addWidget(header);

    // Scrollable content area
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("newsDrawerScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("newsDrawerContent");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    build_top_stories_section(layout);

    // Trending tickers + the user's holdings in the news — hidden until there is
    // something to show (both are recomputed with every filter pass).
    auto make_mention_section = [&](const QString& title, QVBoxLayout*& out_layout, QLabel*& title_out) {
        auto* section = new QWidget(this);
        section->hide();
        auto* inner = new QVBoxLayout(section);
        inner->setContentsMargins(0, 0, 0, 0);
        inner->setSpacing(2);
        title_out = new QLabel(title, section);
        title_out->setObjectName("newsDrawerSectionTitle");
        inner->addWidget(title_out);
        auto* container = new QWidget(section);
        out_layout = new QVBoxLayout(container);
        out_layout->setContentsMargins(0, 0, 0, 0);
        out_layout->setSpacing(1);
        inner->addWidget(container);
        layout->addWidget(section);
        return section;
    };
    holdings_section_ = make_mention_section(tr("YOUR HOLDINGS IN THE NEWS"), holdings_layout_, holdings_title_);
    trending_section_ = make_mention_section(tr("TRENDING TICKERS"), trending_layout_, trending_title_);

    build_categories_section(layout);
    build_monitors_section(layout);
    build_deviations_section(layout);

    // Hidden intelligence sections
    auto build_hidden_section = [&](const QString& section_title, QVBoxLayout*& out_layout,
                                    QLabel** title_out) -> QWidget* {
        auto* section = new QWidget(this);
        section->hide();
        auto* inner = new QVBoxLayout(section);
        inner->setContentsMargins(0, 0, 0, 0);
        inner->setSpacing(2);
        auto* lbl = new QLabel(section_title, section);
        lbl->setObjectName("newsDrawerSectionTitle");
        inner->addWidget(lbl);
        auto* container = new QWidget(section);
        out_layout = new QVBoxLayout(container);
        out_layout->setContentsMargins(0, 0, 0, 0);
        out_layout->setSpacing(1);
        inner->addWidget(container);
        layout->addWidget(section);
        if (title_out)
            *title_out = lbl;
        return section;
    };

    entities_section_ = build_hidden_section(tr("ENTITIES"), entities_layout_, &entities_title_);
    locations_section_ = build_hidden_section(tr("LOCATIONS"), locations_layout_, &locations_title_);
    signals_section_ = build_hidden_section(tr("SIGNALS"), signals_layout_, &signals_title_);
    predictions_section_ = build_hidden_section(tr("PREDICTIONS ON THESE TOPICS"), predictions_layout_, &predictions_title_);
    // Say where the odds come from: Manifold Markets, a play-money site — not
    // tradable real-money prices — and that they are matched to the headlines.
    predictions_source_ = new QLabel(services::NewsCorrelationService::predictions_source_label(), predictions_section_);
    predictions_source_->setObjectName("newsPredSource");
    predictions_source_->setWordWrap(true);
    if (auto* inner = qobject_cast<QVBoxLayout*>(predictions_section_->layout()))
        inner->insertWidget(1, predictions_source_);
    saved_section_ = build_hidden_section(tr("BOOKMARKS"), saved_layout_, &saved_title_);
    saved_more_btn_ = new QPushButton(saved_section_);
    saved_more_btn_->setObjectName("newsCategoryBtn");
    saved_more_btn_->setCursor(Qt::PointingHandCursor);
    saved_more_btn_->hide();
    connect(saved_more_btn_, &QPushButton::clicked, this, [this]() {
        saved_show_all_ = !saved_show_all_;
        render_saved();
    });
    if (auto* inner = qobject_cast<QVBoxLayout*>(saved_section_->layout()))
        inner->addWidget(saved_more_btn_);

    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
    resize(std::max(300, text_width(40, -3)), height());
}

void NewsSidePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NewsSidePanel::retranslateUi() {
    if (drawer_title_)
        drawer_title_->setText(tr("INTELLIGENCE"));
    if (top_stories_title_)
        top_stories_title_->setText(tr("TOP STORIES"));
    if (holdings_title_)
        holdings_title_->setText(tr("YOUR HOLDINGS IN THE NEWS"));
    if (trending_title_)
        trending_title_->setText(tr("TRENDING TICKERS"));
    if (categories_title_)
        categories_title_->setText(tr("CATEGORIES"));
    if (monitors_title_)
        monitors_title_->setText(tr("KEYWORD MONITORS"));
    if (deviations_title_)
        deviations_title_->setText(tr("DEVIATIONS"));
    if (entities_title_)
        entities_title_->setText(tr("ENTITIES"));
    if (locations_title_)
        locations_title_->setText(tr("LOCATIONS"));
    if (signals_title_)
        signals_title_->setText(tr("SIGNALS"));
    if (predictions_title_)
        predictions_title_->setText(tr("PREDICTIONS ON THESE TOPICS"));
    if (predictions_source_)
        predictions_source_->setText(services::NewsCorrelationService::predictions_source_label());
    if (saved_title_)
        saved_title_->setText(tr("BOOKMARKS"));
    if (monitor_input_)
        monitor_input_->setPlaceholderText(tr("label: kw1, kw2"));
    render_saved();
    // Per-item rows (categories, monitors, entities, etc.) are rebuilt from
    // live data on the next update — not re-applied here.
}

void NewsSidePanel::set_drawer_open(bool open) {
    drawer_open_ = open;
    setVisible(open);
}

void NewsSidePanel::build_top_stories_section(QVBoxLayout* parent) {
    top_stories_title_ = new QLabel(tr("TOP STORIES"), this);
    top_stories_title_->setObjectName("newsDrawerSectionTitle");
    parent->addWidget(top_stories_title_);

    auto* container = new QWidget(this);
    top_stories_layout_ = new QVBoxLayout(container);
    top_stories_layout_->setContentsMargins(0, 0, 0, 0);
    top_stories_layout_->setSpacing(2);
    parent->addWidget(container);
}

void NewsSidePanel::build_categories_section(QVBoxLayout* parent) {
    categories_title_ = new QLabel(tr("CATEGORIES"), this);
    categories_title_->setObjectName("newsDrawerSectionTitle");
    parent->addWidget(categories_title_);

    auto* container = new QWidget(this);
    categories_layout_ = new QVBoxLayout(container);
    categories_layout_->setContentsMargins(0, 0, 0, 0);
    categories_layout_->setSpacing(1);
    parent->addWidget(container);
}

void NewsSidePanel::build_monitors_section(QVBoxLayout* parent) {
    monitors_title_ = new QLabel(tr("KEYWORD MONITORS"), this);
    monitors_title_->setObjectName("newsDrawerSectionTitle");
    parent->addWidget(monitors_title_);

    auto* container = new QWidget(this);
    monitors_layout_ = new QVBoxLayout(container);
    monitors_layout_->setContentsMargins(0, 0, 0, 0);
    monitors_layout_->setSpacing(2);
    parent->addWidget(container);

    // Add monitor input
    auto* add_row = new QWidget(this);
    auto* add_layout = new QHBoxLayout(add_row);
    add_layout->setContentsMargins(0, 4, 0, 0);
    add_layout->setSpacing(4);

    monitor_input_ = new QLineEdit(add_row);
    monitor_input_->setObjectName("newsMonitorInput");
    monitor_input_->setPlaceholderText(tr("label: kw1, kw2"));
    monitor_input_->setMinimumHeight(22);

    auto* add_btn = new QPushButton("+", add_row);
    add_btn->setObjectName("newsMonitorAddBtn");
    add_btn->setFixedSize(22, 22);

    add_layout->addWidget(monitor_input_, 1);
    add_layout->addWidget(add_btn);

    connect(add_btn, &QPushButton::clicked, this, [this]() {
        QString text = monitor_input_->text().trimmed();
        if (text.isEmpty())
            return;
        int colon = text.indexOf(':');
        QString label = colon > 0 ? text.left(colon).trimmed() : text;
        QStringList keywords;
        if (colon > 0) {
            QString kw_str = text.mid(colon + 1).trimmed();
            keywords = kw_str.split(',', Qt::SkipEmptyParts);
            for (auto& kw : keywords)
                kw = kw.trimmed();
            keywords.removeAll(QString()); // "a, ,b" leaves a blank entry after trimming
        } else {
            keywords = {label};
        }
        if (label.isEmpty() || keywords.isEmpty())
            return; // nothing usable typed (e.g. "label: , ,")
        emit monitor_added(label, keywords);
        monitor_input_->clear();
    });

    connect(monitor_input_, &QLineEdit::returnPressed, add_btn, &QPushButton::click);

    parent->addWidget(add_row);
}

void NewsSidePanel::build_deviations_section(QVBoxLayout* parent) {
    deviations_section_ = new QWidget(this);
    deviations_section_->hide();

    auto* inner = new QVBoxLayout(deviations_section_);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(2);

    deviations_title_ = new QLabel(tr("DEVIATIONS"), deviations_section_);
    deviations_title_->setObjectName("newsDrawerSectionTitle");
    inner->addWidget(deviations_title_);

    baseline_note_ = new QLabel(deviations_section_);
    baseline_note_->setObjectName("newsPredSource");
    baseline_note_->setWordWrap(true);
    baseline_note_->hide();
    inner->addWidget(baseline_note_);

    auto* container = new QWidget(deviations_section_);
    deviations_layout_ = new QVBoxLayout(container);
    deviations_layout_->setContentsMargins(0, 0, 0, 0);
    deviations_layout_->setSpacing(1);
    inner->addWidget(container);

    parent->addWidget(deviations_section_);
}

// ── Update methods ──────────────────────────────────────────────────────────

void NewsSidePanel::update_top_stories(const QVector<services::NewsArticle>& top) {
    nsp_clear_layout(top_stories_layout_);
    for (int i = 0; i < top.size(); ++i) {
        const auto& article = top[i];
        auto* btn = new NewsElidedButton(QString("%1. %2").arg(i + 1).arg(article.headline), this);
        btn->setObjectName("newsTopStoryBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(article.headline + QLatin1Char('\n') + article.source + QStringLiteral(" · ") +
                        (article.undated ? tr("undated")
                                         : ui::formatting::relative(static_cast<qint64>(article.sort_ts))));
        // Priority stripe (left border) — a rule per priority in the screen's sheet.
        btn->setProperty("prio", services::priority_string(article.priority).toLower());
        connect(btn, &QPushButton::clicked, this, [this, article]() { emit article_clicked(article); });
        top_stories_layout_->addWidget(btn);
    }
}

void NewsSidePanel::fill_mentions(QVBoxLayout* layout, QWidget* section, const QVector<NewsTickerMention>& rows) {
    if (!layout || !section)
        return;
    nsp_clear_layout(layout);
    section->setVisible(!rows.isEmpty());
    if (rows.isEmpty())
        return;
    // A compact table: symbol | mentions (bar) | change vs the previous window | tone.
    auto* table = new ui::DataTable(this);
    table->set_headers({tr("SYMBOL"), tr("MENT."), tr("VS PREV"), tr("TONE")});
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setItemDelegateForColumn(1, new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table));
    table->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < rows.size(); ++r) {
        const auto& m = rows[r];
        auto* sym = new QTableWidgetItem(m.earnings.isEmpty() ? m.symbol : m.symbol + QStringLiteral("  ") + m.earnings);
        sym->setData(Qt::UserRole, m.symbol);
        sym->setForeground(QColor(ui::colors::WARNING()));
        QString tip = tr("%1: %n article(s) in the current feed", "", m.count).arg(m.symbol) +
                      tr("\n%1 bullish · %2 bearish").arg(m.bullish).arg(m.bearish);
        if (m.scored > 0)
            tip += tr(" · mean tone %1 over %n scored", "", m.scored).arg(QString::number(m.net_sentiment, 'f', 2));
        if (m.latest_ts > 0)
            tip += tr("\nLatest: %1").arg(ui::formatting::relative(static_cast<qint64>(m.latest_ts)));
        if (!m.earnings.isEmpty())
            tip += tr("\nEarnings: %1").arg(m.earnings);
        tip += tr("\nClick to filter the feed by %1.").arg(m.symbol);
        sym->setToolTip(tip);
        auto* count = new QTableWidgetItem();
        count->setData(Qt::EditRole, m.count);
        count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QString delta = QStringLiteral("—");
        if (m.previous == 0)
            delta = tr("new");
        else if (m.previous > 0)
            delta = (m.count >= m.previous ? QStringLiteral("+") : QString()) + QString::number(m.count - m.previous);
        auto* d = new QTableWidgetItem(delta);
        d->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        d->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        auto* tone = new QTableWidgetItem(m.scored > 0 ? QString::number(m.net_sentiment, 'f', 2) : tr("n/s"));
        tone->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        // Same ±0.15 band the sentiment engine labels articles with.
        tone->setForeground(m.scored == 0              ? QColor(ui::colors::TEXT_TERTIARY())
                            : m.net_sentiment >= 0.15  ? QColor(ui::colors::POSITIVE())
                            : m.net_sentiment <= -0.15 ? QColor(ui::colors::NEGATIVE())
                                                       : QColor(ui::colors::TEXT_SECONDARY()));
        table->setItem(r, 0, sym);
        table->setItem(r, 1, count);
        table->setItem(r, 2, d);
        table->setItem(r, 3, tone);
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    int h = table->horizontalHeader()->sizeHint().height() + 2 * table->frameWidth();
    for (int r = 0; r < table->rowCount(); ++r)
        h += table->rowHeight(r);
    table->setFixedHeight(h);
    connect(table, &QTableWidget::cellClicked, this, [this, table](int row, int) {
        if (auto* it = table->item(row, 0))
            emit ticker_clicked(it->data(Qt::UserRole).toString());
    });
    layout->addWidget(table);
}

void NewsSidePanel::update_trending(const QVector<NewsTickerMention>& trending) {
    fill_mentions(trending_layout_, trending_section_, trending);
}

void NewsSidePanel::update_holdings_mentions(const QVector<NewsTickerMention>& mentions) {
    fill_mentions(holdings_layout_, holdings_section_, mentions);
}

void NewsSidePanel::update_categories(const QMap<QString, int>& counts) {
    nsp_clear_layout(categories_layout_);
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);
        auto* btn = new QPushButton(it.key(), row);
        btn->setObjectName("newsCategoryBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr("Filter the feed to %1").arg(it.key()));
        const QString cat = it.key();
        connect(btn, &QPushButton::clicked, this, [this, cat]() { emit category_clicked(cat); });
        hl->addWidget(btn, 1);
        auto* n = new QLabel(QLocale().toString(it.value()), row);
        n->setObjectName("newsCategoryBtn");
        hl->addWidget(n);
        categories_layout_->addWidget(row);
    }
}

void NewsSidePanel::update_monitors(const QVector<services::NewsMonitor>& monitors,
                                    const QMap<QString, QVector<services::NewsArticle>>& matches) {
    nsp_clear_layout(monitors_layout_);

    for (const auto& monitor : monitors) {
        auto* row = new QWidget(this);
        row->setObjectName("newsMonitorRow");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        // Color dot
        auto* dot = new QLabel(this);
        dot->setFixedSize(8, 8);
        // The monitor's user-chosen colour, shown as a swatch.
        dot->setStyleSheet(QString("background: %1; border-radius: 0;").arg(monitor.color));
        hl->addWidget(dot);

        // Label + match count (labels can come from MCP/LLM tools: plain text, elided)
        int match_count = matches.contains(monitor.id) ? matches[monitor.id].size() : 0;
        // One .arg() call: a chained .arg(match_count) would also fill a "%2" inside the label.
        auto* label = new NewsElidedLabel(QString("%1 (%2)").arg(monitor.label, QString::number(match_count)), this);
        label->setObjectName("newsMonitorLabel");
        label->setToolTip(monitor.keywords.join(QStringLiteral(", ")));
        hl->addWidget(label, 1);

        // Toggle button
        auto* toggle = new QPushButton(monitor.enabled ? tr("ON") : tr("OFF"), this);
        toggle->setObjectName(monitor.enabled ? "newsMonitorToggleOn" : "newsMonitorToggleOff");
        toggle->setFixedSize(std::max(28, text_width(3, -5, QFont::Bold) + 8),
                             std::max(18, QFontMetrics(ui::fonts::font(-5)).height() + 4));
        toggle->setCursor(Qt::PointingHandCursor);
        QString mid = monitor.id;
        connect(toggle, &QPushButton::clicked, this, [this, mid]() { emit monitor_toggled(mid); });
        hl->addWidget(toggle);

        // Delete button
        auto* del_btn = new QPushButton("x", this);
        del_btn->setObjectName("newsMonitorDeleteBtn");
        del_btn->setFixedSize(18, 18);
        del_btn->setCursor(Qt::PointingHandCursor);
        connect(del_btn, &QPushButton::clicked, this, [this, mid]() { emit monitor_deleted(mid); });
        hl->addWidget(del_btn);

        monitors_layout_->addWidget(row);
    }
}

void NewsSidePanel::update_baseline_status(int observed_hours, int needed_hours) {
    if (!baseline_note_)
        return;
    if (observed_hours >= needed_hours) {
        baseline_note_->hide();
        deviations_section_->setVisible(!last_deviations_.isEmpty());
        return;
    }
    baseline_note_->setText(tr("Volume baseline: %1 of %2 hours of history collected — spikes are flagged once "
                               "the baseline is complete (history persists across restarts).")
                                .arg(observed_hours)
                                .arg(needed_hours));
    baseline_note_->show();
    deviations_section_->show();
}

void NewsSidePanel::update_deviations(const QVector<QPair<QString, double>>& deviations) {
    last_deviations_ = deviations;
    nsp_clear_layout(deviations_layout_);

    if (deviations.isEmpty()) {
        deviations_section_->setVisible(baseline_note_ && baseline_note_->isVisibleTo(deviations_section_));
        return;
    }

    deviations_section_->show();

    for (const auto& [category, z_score] : deviations) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        auto* cat_label = new QLabel(category, this);
        cat_label->setObjectName("newsDeviationCategory");
        hl->addWidget(cat_label);

        hl->addStretch();

        // A z-score, not a multiple: "+3.2σ" = 3.2 standard deviations above the
        // category's hourly baseline (it used to read "3.2x").
        auto* score_label = new QLabel(QString("+%1σ").arg(z_score, 0, 'f', 1), this);
        score_label->setObjectName("newsDeviationScore");
        score_label->setToolTip(tr("Last-hour volume is %1 standard deviations above this category's hourly "
                                   "baseline")
                                    .arg(z_score, 0, 'f', 1));
        hl->addWidget(score_label);

        deviations_layout_->addWidget(row);
    }
}

// ── Intelligence section update methods ────────────────────────────────────

void NewsSidePanel::update_entities(const services::NerResult& ner) {
    nsp_clear_layout(entities_layout_);
    bool has_data = false;
    auto add_row = [&](const QString& kind, const QString& name, int count) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        auto* k = new QLabel(kind, row);
        k->setObjectName("newsPredSource");
        hl->addWidget(k);
        auto* n = new NewsElidedLabel(name, row);
        n->setObjectName("newsMonitorLabel");
        hl->addWidget(n, 1);
        auto* c = new QLabel(QString::number(count), row);
        c->setObjectName("newsMonitorLabel");
        hl->addWidget(c);
        entities_layout_->addWidget(row);
        has_data = true;
    };
    for (const auto& c : ner.top_countries)
        add_row(tr("CTY"), c.name, c.count);
    for (const auto& o : ner.top_organizations)
        add_row(tr("ORG"), o.name, o.count);
    for (const auto& p : ner.top_people)
        add_row(tr("PER"), p.name, p.count);

    entities_section_->setVisible(has_data);
}

void NewsSidePanel::update_locations(const QVector<services::ArticleGeo>& geo) {
    nsp_clear_layout(locations_layout_);

    if (geo.isEmpty()) {
        locations_section_->hide();
        return;
    }
    locations_section_->show();

    QMap<QString, int> loc_counts;
    for (const auto& g : geo) {
        for (const auto& l : g.locations)
            loc_counts[l.name]++;
    }

    auto sorted = loc_counts.keys();
    std::sort(sorted.begin(), sorted.end(),
              [&](const QString& a, const QString& b) { return loc_counts[a] > loc_counts[b]; });

    for (int i = 0; i < std::min(8, static_cast<int>(sorted.size())); ++i) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        auto* n = new NewsElidedLabel(sorted[i], row);
        n->setObjectName("newsMonitorLabel");
        hl->addWidget(n, 1);
        auto* c = new QLabel(QString::number(loc_counts[sorted[i]]), row);
        c->setObjectName("newsMonitorLabel");
        hl->addWidget(c);
        locations_layout_->addWidget(row);
    }
}

void NewsSidePanel::update_signals(const QVector<services::CorrelationSignal>& sigs) {
    nsp_clear_layout(signals_layout_);

    if (sigs.isEmpty()) {
        signals_section_->hide();
        return;
    }
    signals_section_->show();

    for (int i = 0; i < std::min(8, static_cast<int>(sigs.size())); ++i) {
        const auto& sig = sigs[i];
        // "critical" is red, everything else amber (rules in the screen's sheet).
        auto* lbl = new NewsElidedLabel(sig.detail, this);
        lbl->setObjectName(sig.severity == QLatin1String("critical") ? "newsSignalCritical" : "newsSignalWarn");
        lbl->setToolTip(QString("[%1] %2").arg(sig.severity.toUpper(), sig.detail));
        signals_layout_->addWidget(lbl);
    }
}

void NewsSidePanel::update_predictions(const QVector<services::PredictionMarket>& predictions) {
    nsp_clear_layout(predictions_layout_);

    // Only markets matched to what the feed is discussing are shown; with no
    // match the section stays hidden rather than listing unrelated markets.
    if (predictions.isEmpty()) {
        predictions_section_->hide();
        return;
    }
    predictions_section_->show();

    for (int i = 0; i < std::min(6, static_cast<int>(predictions.size())); ++i) {
        const auto& pm = predictions[i];
        const int pct = static_cast<int>(std::lround(pm.yes_price * 100.0));
        const QString text = pm.matched_term.isEmpty()
                                 ? tr("YES %1%  %2").arg(pct, 2).arg(pm.question)
                                 : tr("[%1] YES %2%  %3").arg(pm.matched_term).arg(pct, 2).arg(pm.question);
        auto* lbl = new NewsElidedLabel(text, this);
        lbl->setObjectName(pct >= 70 ? "newsPredHigh" : (pct <= 30 ? "newsPredLow" : "newsPredMid"));
        // One .arg() call: the question is network text, and a chained call would re-scan it.
        lbl->setToolTip(tr("%1\nYES %2% · volume %3 (play-money mana)%4")
                            .arg(pm.question, QString::number(pm.yes_price * 100.0, 'f', 1),
                                 QLocale().toString(pm.volume, 'f', 0),
                                 pm.matched_term.isEmpty()
                                     ? QString()
                                     : tr("\nMatched to the feed topic \"%1\"").arg(pm.matched_term)));
        predictions_layout_->addWidget(lbl);
    }
}

void NewsSidePanel::update_saved(const QVector<services::NewsArticle>& saved) {
    saved_all_ = saved;
    render_saved();
}

void NewsSidePanel::render_saved() {
    if (!saved_layout_)
        return;
    nsp_clear_layout(saved_layout_);
    if (saved_all_.isEmpty()) {
        saved_section_->hide();
        return;
    }
    saved_section_->show();
    const int n = static_cast<int>(saved_all_.size());
    const int shown = saved_show_all_ ? n : std::min(kSavedPreview, n);
    for (int i = 0; i < shown; ++i) {
        const auto& a = saved_all_[i];
        auto* btn = new NewsElidedButton(a.headline, this);
        btn->setObjectName("newsTopStoryBtn");
        btn->setToolTip(a.headline + "\n" + a.source + QStringLiteral(" · ") +
                        ui::formatting::date_time(static_cast<qint64>(a.sort_ts)));
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, a]() { emit article_clicked(a); });
        saved_layout_->addWidget(btn);
    }
    saved_title_->setText(tr("BOOKMARKS (%1)").arg(n));
    saved_more_btn_->setVisible(n > kSavedPreview);
    saved_more_btn_->setText(saved_show_all_ ? tr("Show fewer") : tr("Show all %1").arg(n));
}

} // namespace fincept::screens
