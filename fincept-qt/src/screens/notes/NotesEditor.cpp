// src/screens/notes/NotesEditor.cpp
#include "screens/notes/NotesEditor.h"

#include "screens/notes/NotesLogic.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractItemView>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QStringListModel>
#include <QStyle>
#include <QSyntaxHighlighter>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::notes {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;

namespace {

QColor token(const fincept::ui::ColorToken& t) {
    return QColor(t.get());
}

QString tr_editor(const char* text) {
    return QCoreApplication::translate("NotesEditor", text);
}

/// Horizontal margin of the title, fields and body text.
constexpr int kSideMargin = 24;

} // namespace

QStringList category_codes() {
    return default_category_codes();
}

QString category_label(const QString& code) {
    const QString c = category_key(code);
    if (c == QLatin1String("TRADE_IDEA"))
        return tr_editor("Trade ideas");
    if (c == QLatin1String("RESEARCH"))
        return tr_editor("Research");
    if (c == QLatin1String("MARKET_ANALYSIS"))
        return tr_editor("Market analysis");
    if (c == QLatin1String("EARNINGS"))
        return tr_editor("Earnings");
    if (c == QLatin1String("ECONOMIC"))
        return tr_editor("Economic");
    if (c == QLatin1String("PORTFOLIO"))
        return tr_editor("Portfolio");
    if (c == QLatin1String("GENERAL"))
        return tr_editor("General");
    QString s = c.toLower();
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

// ── FlowLayout: chips that wrap onto the next line ───────────────────────────

class FlowLayout : public QLayout {
  public:
    explicit FlowLayout(QWidget* parent, int h_spacing, int v_spacing)
        : QLayout(parent), h_(h_spacing), v_(v_spacing) {
        setContentsMargins(0, 0, 0, 0);
    }
    ~FlowLayout() override {
        while (QLayoutItem* item = takeAt(0))
            delete item;
    }
    void addItem(QLayoutItem* item) override { items_.append(item); }
    int count() const override { return static_cast<int>(items_.size()); }
    QLayoutItem* itemAt(int i) const override { return items_.value(i); }
    QLayoutItem* takeAt(int i) override { return (i >= 0 && i < items_.size()) ? items_.takeAt(i) : nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return do_layout(QRect(0, 0, w, 0), true); }
    void setGeometry(const QRect& r) override {
        QLayout::setGeometry(r);
        do_layout(r, false);
    }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize s;
        for (QLayoutItem* it : items_)
            s = s.expandedTo(it->minimumSize());
        return s;
    }
    /// Insert `w` before position `index` (the add-input stays last).
    void insert_widget(int index, QWidget* w) {
        addChildWidget(w);
        items_.insert(std::clamp<qsizetype>(index, 0, items_.size()), new QWidgetItem(w));
        invalidate();
    }

  private:
    int do_layout(const QRect& rect, bool test) const {
        int x = rect.x();
        int y = rect.y();
        int line_h = 0;
        for (QLayoutItem* it : items_) {
            if (it->isEmpty())
                continue;
            const QSize sh = it->sizeHint();
            if (x > rect.x() && x + sh.width() > rect.right() + 1) {
                x = rect.x();
                y += line_h + v_;
                line_h = 0;
            }
            if (!test)
                it->setGeometry(QRect(QPoint(x, y), sh));
            x += sh.width() + h_;
            line_h = std::max(line_h, sh.height());
        }
        return y + line_h - rect.y();
    }

    QList<QLayoutItem*> items_;
    int h_;
    int v_;
};

// ── NotesBodyEdit: the Markdown source, kept to a readable line length ───────

class NotesBodyEdit : public QPlainTextEdit {
  public:
    explicit NotesBodyEdit(QWidget* parent) : QPlainTextEdit(parent) {}

    void update_measure() {
        const QFontMetrics fm(font());
        const int measure = fm.averageCharWidth() * 110;
        const int sb = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
        const int side = std::max(kSideMargin - 4, (width() - sb - measure) / 2);
        if (side != side_) {
            side_ = side;
            setViewportMargins(side, 0, side, 0);
        }
    }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        update_measure();
    }

  private:
    int side_ = -1;
};

// ── NotesHighlighter: light Markdown colouring of the source ─────────────────

class NotesHighlighter : public QSyntaxHighlighter {
  public:
    explicit NotesHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) { refresh_formats(); }

    void refresh_formats() {
        heading_ = QTextCharFormat();
        heading_.setForeground(token(colors::AMBER));
        heading_.setFontWeight(QFont::Bold);
        strong_ = QTextCharFormat();
        strong_.setFontWeight(QFont::Bold);
        strong_.setForeground(token(colors::TEXT_PRIMARY));
        em_ = QTextCharFormat();
        em_.setFontItalic(true);
        code_ = QTextCharFormat();
        code_.setForeground(token(colors::CYAN));
        code_.setBackground(token(colors::BG_RAISED));
        fence_ = QTextCharFormat();
        fence_.setForeground(token(colors::CYAN));
        marker_ = QTextCharFormat();
        marker_.setForeground(token(colors::AMBER_DIM));
        quote_ = QTextCharFormat();
        quote_.setForeground(token(colors::TEXT_SECONDARY));
        quote_.setFontItalic(true);
        ticker_ = QTextCharFormat();
        ticker_.setForeground(token(colors::AMBER));
        ticker_.setFontWeight(QFont::DemiBold);
        link_ = QTextCharFormat();
        link_.setForeground(token(colors::CYAN));
        dim_ = QTextCharFormat();
        dim_.setForeground(token(colors::TEXT_TERTIARY));
    }

  protected:
    void highlightBlock(const QString& text) override {
        static const QRegularExpression heading(QStringLiteral("^\\s{0,3}#{1,6}\\s"));
        static const QRegularExpression rule(QStringLiteral("^\\s*([-*_]\\s*){3,}$"));
        static const QRegularExpression quote(QStringLiteral("^\\s*>"));
        static const QRegularExpression list(QStringLiteral("^\\s*([-*+]|\\d+[.)])\\s+(\\[[ xX]\\]\\s)?"));
        static const QRegularExpression strong(QStringLiteral("\\*\\*[^*\\n]+\\*\\*|__[^_\\n]+__"));
        static const QRegularExpression em(QStringLiteral("(?<![*\\w])\\*(?![\\s*])[^*\\n]+(?<!\\s)\\*(?![*\\w])"));
        static const QRegularExpression link(QStringLiteral("\\[[^\\]\\n]*\\]\\([^)\\n]*\\)"));
        static const QRegularExpression cashtag(QStringLiteral("(?<![\\w$])\\$[A-Za-z][A-Za-z0-9.\\-^=]{0,14}\\b"));
        static const QRegularExpression code(QStringLiteral("`[^`\\n]*`"));

        const QString trimmed = text.trimmed();
        const bool fence_line = trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"));
        if (previousBlockState() == 1) {
            setFormat(0, static_cast<int>(text.size()), fence_);
            setCurrentBlockState(fence_line ? 0 : 1);
            return;
        }
        if (fence_line) {
            setFormat(0, static_cast<int>(text.size()), fence_);
            setCurrentBlockState(1);
            return;
        }
        setCurrentBlockState(0);
        if (heading.match(text).hasMatch()) {
            setFormat(0, static_cast<int>(text.size()), heading_);
            return;
        }
        if (rule.match(text).hasMatch()) {
            setFormat(0, static_cast<int>(text.size()), dim_);
            return;
        }
        if (quote.match(text).hasMatch())
            setFormat(0, static_cast<int>(text.size()), quote_);
        if (const auto m = list.match(text); m.hasMatch())
            setFormat(0, static_cast<int>(m.capturedLength()), marker_);
        if (trimmed.startsWith(QLatin1Char('|')))
            for (qsizetype i = 0; i < text.size(); ++i)
                if (text.at(i) == QLatin1Char('|'))
                    setFormat(static_cast<int>(i), 1, marker_);
        const auto apply = [this, &text](const QRegularExpression& re, const QTextCharFormat& f) {
            auto it = re.globalMatch(text);
            while (it.hasNext()) {
                const auto m = it.next();
                setFormat(static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()), f);
            }
        };
        apply(link, link_);
        apply(em, em_);
        apply(strong, strong_);
        apply(cashtag, ticker_);
        apply(code, code_);
    }

  private:
    QTextCharFormat heading_, strong_, em_, code_, fence_, marker_, quote_, ticker_, link_, dim_;
};

// ── NotesEditor ──────────────────────────────────────────────────────────────

NotesEditor::NotesEditor(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("notesEditor"));
    preview_timer_ = new QTimer(this);
    preview_timer_->setSingleShot(true);
    connect(preview_timer_, &QTimer::timeout, this, &NotesEditor::render_preview);
    stats_timer_ = new QTimer(this);
    stats_timer_->setSingleShot(true);
    stats_timer_->setInterval(350);
    connect(stats_timer_, &QTimer::timeout, this, &NotesEditor::update_stats);
    build_ui();
    retranslate();
}

NotesEditor::~NotesEditor() = default;

void NotesEditor::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Bar ──────────────────────────────────────────────────────────────────
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("notesEditorBar"));
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(kSideMargin - 8, 6, 12, 6);
    bl->setSpacing(6);

    pin_btn_ = new QToolButton(bar);
    pin_btn_->setObjectName(QStringLiteral("notesPinBtn"));
    pin_btn_->setCheckable(true);
    pin_btn_->setCursor(Qt::PointingHandCursor);
    connect(pin_btn_, &QToolButton::toggled, this, [this](bool on) {
        update_pin_button();
        if (!loading_)
            emit pin_toggled(on);
    });
    bl->addWidget(pin_btn_);
    bl->addSpacing(8);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    for (int i = 0; i < 3; ++i) {
        auto* b = new QToolButton(bar);
        b->setObjectName(QStringLiteral("notesModeBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b, i);
        bl->addWidget(b);
        mode_btns_[i] = b;
    }
    bl->setSpacing(6);
    mode_btns_[0]->setChecked(true);
    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        set_mode(static_cast<Mode>(id));
        emit mode_changed(id);
    });

    bl->addStretch(1);
    save_label_ = new QLabel(bar);
    save_label_->setObjectName(QStringLiteral("notesSaveState"));
    save_label_->setTextFormat(Qt::RichText);
    bl->addWidget(save_label_);
    bl->addSpacing(6);

    more_btn_ = new QToolButton(bar);
    more_btn_->setObjectName(QStringLiteral("notesMoreBtn"));
    more_btn_->setPopupMode(QToolButton::InstantPopup);
    more_btn_->setCursor(Qt::PointingHandCursor);
    more_menu_ = new QMenu(more_btn_);
    const auto add = [this](const char* action) {
        QAction* a = more_menu_->addAction(QString());
        const QString id = QString::fromLatin1(action);
        connect(a, &QAction::triggered, this, [this, id]() { emit action_requested(id); });
        return a;
    };
    act_duplicate_ = add("duplicate");
    act_export_fm_ = add("export_fm");
    act_export_dir_ = add("export_dir");
    act_template_ = add("save_template");
    more_menu_->addSeparator();
    act_revert_ = add("revert");
    act_archive_ = add("archive");
    more_menu_->addSeparator();
    act_delete_ = add("delete");
    more_btn_->setMenu(more_menu_);
    bl->addWidget(more_btn_);
    root->addWidget(bar);

    // ── Title, metadata, links ───────────────────────────────────────────────
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("notesEditorHead"));
    auto* hl = new QVBoxLayout(head);
    hl->setContentsMargins(kSideMargin, 14, kSideMargin, 10);
    hl->setSpacing(10);

    title_ = new QLineEdit(head);
    title_->setObjectName(QStringLiteral("notesTitle"));
    title_->setFrame(false);
    connect(title_, &QLineEdit::textEdited, this, [this]() { mark_edited(); });
    connect(title_, &QLineEdit::returnPressed, this, [this]() { focus_body(); });
    hl->addWidget(title_);

    auto* meta = new QHBoxLayout;
    meta->setSpacing(8);
    const auto caption = [head](QLayout* lay) {
        auto* l = new QLabel(head);
        l->setObjectName(QStringLiteral("notesCaption"));
        lay->addWidget(l);
        return l;
    };
    const auto combo = [this, head](QLayout* lay) {
        auto* c = new QComboBox(head);
        c->setObjectName(QStringLiteral("notesMetaCombo"));
        c->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        connect(c, &QComboBox::activated, this, [this]() { mark_edited(); });
        lay->addWidget(c);
        return c;
    };
    cap_category_ = caption(meta);
    category_ = combo(meta);
    meta->addSpacing(10);
    cap_priority_ = caption(meta);
    priority_ = combo(meta);
    meta->addSpacing(10);
    cap_sentiment_ = caption(meta);
    sentiment_ = combo(meta);
    meta->addStretch(1);
    hl->addLayout(meta);

    auto* links = new QGridLayout;
    links->setHorizontalSpacing(12);
    links->setVerticalSpacing(6);
    links->setColumnStretch(1, 1);
    const auto chip_row = [head, links](int row, QLabel*& cap, QWidget*& box, FlowLayout*& flow) {
        cap = new QLabel(head);
        cap->setObjectName(QStringLiteral("notesCaption"));
        links->addWidget(cap, row, 0, Qt::AlignLeft | Qt::AlignTop);
        box = new QWidget(head);
        box->setObjectName(QStringLiteral("notesChipBox"));
        QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sp.setHeightForWidth(true);
        box->setSizePolicy(sp);
        flow = new FlowLayout(box, 6, 6);
        links->addWidget(box, row, 1);
    };
    chip_row(0, cap_tickers_, ticker_box_, ticker_flow_);
    chip_row(1, cap_tags_, tag_box_, tag_flow_);
    hl->addLayout(links);

    // Tickers: the terminal's symbol picker (resolved instruments only, compact).
    ticker_picker_ = new fincept::ui::SymbolSearchBox(ticker_box_);
    ticker_picker_->setObjectName(QStringLiteral("notesTickerPicker"));
    ticker_picker_->set_compact(true);
    ticker_picker_->set_multi(true);
    ticker_picker_->set_clear_on_pick(true);
    ticker_picker_->set_recent_key(QStringLiteral("notes"));
    ticker_input_ = ticker_picker_->line_edit();
    if (ticker_input_)
        ticker_input_->installEventFilter(this);
    ticker_flow_->addWidget(ticker_picker_);
    connect(ticker_picker_, &fincept::ui::SymbolSearchBox::instrument_picked, this,
            [this](const fincept::core::instrument::Instrument& inst) { add_ticker(inst.symbol); });
    connect(ticker_picker_, &fincept::ui::SymbolSearchBox::instruments_picked, this,
            [this](const QVector<fincept::core::instrument::Instrument>& list) {
                for (const auto& inst : list)
                    add_ticker(inst.symbol);
            });
    connect(ticker_picker_, &fincept::ui::SymbolSearchBox::unresolved, this,
            [this](const QStringList&, const QString& message) {
                if (!message.isEmpty())
                    emit notice(message);
            });

    tag_input_ = new QLineEdit(tag_box_);
    tag_input_->setObjectName(QStringLiteral("notesChipInput"));
    tag_input_->installEventFilter(this);
    tag_flow_->addWidget(tag_input_);
    tag_words_ = new QStringListModel(this);
    tag_completer_ = new QCompleter(tag_words_, this);
    tag_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    tag_input_->setCompleter(tag_completer_);
    connect(tag_completer_, qOverload<const QString&>(&QCompleter::activated), this,
            [this]() { QTimer::singleShot(0, this, [this]() { add_from_input(tag_input_, false); }); });
    // While a suggestion list is open the keys go to it, not to the field. See
    // eventFilter(): Enter with no suggestion picked must still add what was typed.
    tag_completer_->popup()->installEventFilter(this);
    root->addWidget(head);

    auto* rule = new QFrame(this);
    rule->setObjectName(QStringLiteral("notesRule"));
    rule->setFixedHeight(1);
    root->addWidget(rule);

    // ── Body (and preview) ───────────────────────────────────────────────────
    body_split_ = new QSplitter(Qt::Horizontal, this);
    body_split_->setObjectName(QStringLiteral("notesBodySplit"));
    body_split_->setChildrenCollapsible(false);
    body_split_->setHandleWidth(1);
    body_ = new NotesBodyEdit(body_split_);
    body_->setObjectName(QStringLiteral("notesBody"));
    body_->setFrameShape(QFrame::NoFrame);
    body_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    body_->document()->setDocumentMargin(10);
    highlighter_ = new NotesHighlighter(body_->document());
    connect(body_, &QPlainTextEdit::textChanged, this, [this]() {
        if (loading_)
            return;
        mark_edited();
        stats_timer_->start();
        schedule_preview();
    });
    body_split_->addWidget(body_);
    root->addWidget(body_split_, 1);

    // ── Footer ───────────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("notesEditorFooter"));
    auto* fl = new QHBoxLayout(footer);
    fl->setContentsMargins(kSideMargin, 5, 14, 5);
    fl->setSpacing(12);
    stats_label_ = new QLabel(footer);
    stats_label_->setObjectName(QStringLiteral("notesFooterText"));
    fl->addWidget(stats_label_);
    fl->addStretch(1);
    dates_label_ = new QLabel(footer);
    dates_label_->setObjectName(QStringLiteral("notesFooterText"));
    fl->addWidget(dates_label_);
    root->addWidget(footer);

    setTabOrder(title_, category_);
    setTabOrder(category_, priority_);
    setTabOrder(priority_, sentiment_);
    setTabOrder(sentiment_, ticker_input_);
    setTabOrder(ticker_input_, tag_input_);
    setTabOrder(tag_input_, body_);

    fill_combos();
    refresh_theme();
}

void NotesEditor::fill_combos() {
    const QString cat = category_->currentData().toString();
    const QString pri = priority_->currentData().toString();
    const QString sen = sentiment_->currentData().toString();
    category_->clear();
    for (const QString& code : category_codes())
        category_->addItem(category_label(code), code);
    priority_->clear();
    priority_->addItem(tr("Critical"), QStringLiteral("CRITICAL"));
    priority_->addItem(tr("High"), QStringLiteral("HIGH"));
    priority_->addItem(tr("Medium"), QStringLiteral("MEDIUM"));
    priority_->addItem(tr("Low"), QStringLiteral("LOW"));
    sentiment_->clear();
    sentiment_->addItem(tr("Bullish"), QStringLiteral("BULLISH"));
    sentiment_->addItem(tr("Bearish"), QStringLiteral("BEARISH"));
    sentiment_->addItem(tr("Neutral"), QStringLiteral("NEUTRAL"));
    select_code(category_, cat, QStringLiteral("GENERAL"));
    select_code(priority_, pri, QStringLiteral("MEDIUM"));
    select_code(sentiment_, sen, QStringLiteral("NEUTRAL"));
}

void NotesEditor::select_code(QComboBox* combo, const QString& code, const QString& fallback) {
    const QString c = code.trimmed().toUpper().isEmpty() ? fallback : code.trimmed().toUpper();
    int i = combo->findData(c);
    if (i < 0) {
        // A value written elsewhere (MCP tools, cloud sync): keep it, do not rewrite it.
        combo->addItem(category_label(c), c);
        i = combo->count() - 1;
    }
    combo->setCurrentIndex(i);
}

void NotesEditor::retranslate() {
    mode_btns_[0]->setText(tr("WRITE"));
    mode_btns_[1]->setText(tr("SPLIT"));
    mode_btns_[2]->setText(tr("READ"));
    mode_btns_[0]->setToolTip(tr("Edit the Markdown source (Ctrl+E switches mode)"));
    mode_btns_[1]->setToolTip(tr("Source and rendered note side by side"));
    mode_btns_[2]->setToolTip(tr("Rendered note only"));
    more_btn_->setText(tr("MORE"));
    act_duplicate_->setText(tr("Duplicate note\tCtrl+D"));
    act_export_fm_->setText(tr("Export to File Manager (Markdown)"));
    act_export_dir_->setText(tr("Export to folder…"));
    act_template_->setText(tr("Save as template…"));
    act_revert_->setText(tr("Revert to the version you opened"));
    act_archive_->setText(archived_ ? tr("Unarchive") : tr("Archive"));
    act_delete_->setText(tr("Delete note\tDel"));
    title_->setPlaceholderText(tr("Untitled note"));
    title_->setAccessibleName(tr("Note title"));
    cap_category_->setText(tr("CATEGORY"));
    cap_priority_->setText(tr("PRIORITY"));
    cap_sentiment_->setText(tr("SENTIMENT"));
    cap_tickers_->setText(tr("TICKERS"));
    cap_tags_->setText(tr("TAGS"));
    category_->setAccessibleName(tr("Note category"));
    priority_->setAccessibleName(tr("Note priority"));
    sentiment_->setAccessibleName(tr("Note sentiment"));
    ticker_picker_->set_placeholder(tr("+ ticker"));
    ticker_picker_->setToolTip(tr("Add a ticker: type a symbol, name or ISIN and pick it. Typing $AAPL in the note "
                                  "links it too. A symbol dragged from another panel onto the card adds it."));
    ticker_picker_->setAccessibleName(tr("Add ticker"));
    tag_input_->setPlaceholderText(tr("+ tag"));
    tag_input_->setToolTip(tr("Add a tag (Enter or comma)"));
    tag_input_->setAccessibleName(tr("Add tag"));
    body_->setPlaceholderText(tr("Write in Markdown — # heading, **bold**, - list, - [ ] task, | table |.\n"
                                 "Type $AAPL to link a ticker."));
    body_->setAccessibleName(tr("Note text (Markdown)"));
    fill_combos();
    update_pin_button();
    update_save_label();
    update_stats();
    set_dates(created_at_, updated_at_);
    rebuild_ticker_chips();
    rebuild_tag_chips();
}

void NotesEditor::refresh_theme() {
    highlighter_->refresh_formats();
    highlighter_->rehighlight();
    const QFontMetrics fm(fonts::font(-2));
    const int chip_h = fm.height() + 8;
    tag_input_->setFixedHeight(chip_h);
    ticker_picker_->setMinimumWidth(fm.averageCharWidth() * 16 + 24);
    tag_input_->setMinimumWidth(fm.horizontalAdvance(QStringLiteral("+ tag")) + 24);
    body_->setTabStopDistance(QFontMetricsF(fonts::font(1)).horizontalAdvance(QLatin1Char(' ')) * 4);
    body_->update_measure();
    if (preview_) {
        preview_->document()->setDocumentMargin(kSideMargin - 6);
        preview_stale_ = true; // the rendered HTML carries the old theme's colours
        if (mode_ != Mode::Write)
            render_preview();
    }
    update_pin_button();
    update_save_label();
    rebuild_ticker_chips();
    rebuild_tag_chips();
    apply_highlight();
}

// ── Loading / collecting ─────────────────────────────────────────────────────

void NotesEditor::load(const fincept::FinancialNote& note) {
    loading_ = true;
    stats_timer_->stop();
    preview_timer_->stop();
    title_->setText(note.title);
    title_->setCursorPosition(0);
    select_code(category_, category_key(note.category), QStringLiteral("GENERAL"));
    select_code(priority_, note.priority, QStringLiteral("MEDIUM"));
    select_code(sentiment_, note.sentiment, QStringLiteral("NEUTRAL"));
    body_->setPlainText(note.content); // also clears undo / redo
    body_->moveCursor(QTextCursor::Start);
    tags_ = split_tags(note.tags);
    body_tickers_ = extract_cashtags(note.content);
    explicit_tickers_.clear();
    for (const QString& t : split_tickers(note.tickers))
        if (!body_tickers_.contains(t))
            explicit_tickers_ << t;
    pin_btn_->setChecked(note.is_favorite);
    set_archived(note.is_archived);
    ticker_input_->clear();
    tag_input_->clear();
    created_at_ = note.created_at;
    updated_at_ = note.updated_at;
    set_dates(note.created_at, note.updated_at);
    rebuild_ticker_chips();
    rebuild_tag_chips();
    update_stats();
    apply_highlight();
    preview_stale_ = true;
    if (preview_ && mode_ != Mode::Write) { // the page may not be on screen yet: render anyway
        render_preview();
        preview_->verticalScrollBar()->setValue(0);
    }
    loading_ = false;
    emit tickers_changed(linked_tickers());
}

fincept::FinancialNote NotesEditor::collect(const fincept::FinancialNote& base) const {
    fincept::FinancialNote n = base;
    n.content = body_->toPlainText();
    n.title = title_->text().trimmed();
    if (n.title.isEmpty())
        n.title = title_from_body(n.content);
    n.category = category_->currentData().toString();
    n.priority = priority_->currentData().toString();
    n.sentiment = sentiment_->currentData().toString();
    n.tags = join_list(tags_);
    n.tickers = join_list(merge_unique(explicit_tickers_, extract_cashtags(n.content)));
    n.word_count = word_count(n.content);
    n.is_favorite = pin_btn_->isChecked();
    return n;
}

bool NotesEditor::is_blank() const {
    return title_->text().trimmed().isEmpty() && body_->document()->isEmpty() && tags_.isEmpty() &&
           explicit_tickers_.isEmpty();
}

QStringList NotesEditor::linked_tickers() const {
    return merge_unique(explicit_tickers_, body_tickers_);
}

void NotesEditor::mark_edited() {
    if (loading_)
        return;
    ++revision_;
    emit edited();
}

// ── Mode / preview ───────────────────────────────────────────────────────────

void NotesEditor::ensure_preview() {
    if (preview_)
        return;
    preview_ = new QTextBrowser(body_split_);
    preview_->setObjectName(QStringLiteral("notesPreview"));
    preview_->setFrameShape(QFrame::NoFrame);
    preview_->setOpenLinks(false);
    preview_->setOpenExternalLinks(false);
    preview_->document()->setDocumentMargin(kSideMargin - 6);
    preview_->setAccessibleName(tr("Rendered note"));
    connect(preview_, &QTextBrowser::anchorClicked, this, &NotesEditor::link_clicked);
    body_split_->addWidget(preview_);
    preview_stale_ = true;
}

void NotesEditor::set_mode(Mode m) {
    mode_ = m;
    if (mode_btns_[static_cast<int>(m)] && !mode_btns_[static_cast<int>(m)]->isChecked())
        mode_btns_[static_cast<int>(m)]->setChecked(true);
    if (m != Mode::Write)
        ensure_preview();
    body_->setVisible(m != Mode::Read);
    if (preview_)
        preview_->setVisible(m != Mode::Write);
    if (m == Mode::Split) {
        const int w = std::max(2, body_split_->width());
        body_split_->setSizes({w / 2, w - w / 2});
    }
    if (m != Mode::Write && preview_stale_)
        render_preview();
}

void NotesEditor::schedule_preview() {
    preview_stale_ = true;
    if (!preview_ || mode_ == Mode::Write)
        return; // rendered when SPLIT / READ is next chosen
    // Longer notes wait longer for a pause in typing: the layout of the rendered
    // document is the expensive part and has to run on this thread.
    const qsizetype n = body_->document()->characterCount();
    preview_timer_->start(static_cast<int>(std::clamp<qsizetype>(250 + n / 200, 250, 1200)));
}

void NotesEditor::render_preview() {
    if (!preview_)
        return;
    preview_timer_->stop();
    const QString md = body_->toPlainText();
    QScrollBar* sb = preview_->verticalScrollBar();
    const int max_before = sb->maximum();
    const double frac = max_before > 0 ? double(sb->value()) / max_before : 0.0;
    if (md.trimmed().isEmpty())
        preview_->setHtml(QStringLiteral("<p style=\"color:%1\">%2</p>")
                              .arg(colors::TEXT_TERTIARY(), tr("Nothing to preview yet.").toHtmlEscaped()));
    else
        preview_->setHtml(fincept::ui::MarkdownRenderer::render(markdown_with_symbol_links(md)));
    if (sb->maximum() > 0)
        sb->setValue(static_cast<int>(frac * sb->maximum()));
    preview_stale_ = false;
}

// ── Bar state ────────────────────────────────────────────────────────────────

void NotesEditor::set_pinned(bool pinned) {
    const bool was = loading_;
    loading_ = true;
    pin_btn_->setChecked(pinned);
    loading_ = was;
    update_pin_button();
}

void NotesEditor::update_pin_button() {
    const bool on = pin_btn_->isChecked();
    pin_btn_->setText(on ? tr("★ PINNED") : tr("☆ PIN"));
    pin_btn_->setToolTip(on ? tr("Unpin this note") : tr("Pin this note to the top (shown as a favourite on the dashboard)"));
}

void NotesEditor::set_archived(bool archived) {
    archived_ = archived;
    if (act_archive_)
        act_archive_->setText(archived ? tr("Unarchive") : tr("Archive"));
}

void NotesEditor::set_tags(const QStringList& tags) {
    tags_ = tags;
    rebuild_tag_chips();
}

void NotesEditor::set_save_state(SaveState s, const QString& detail) {
    save_state_ = s;
    save_detail_ = detail;
    update_save_label();
}

void NotesEditor::update_save_label() {
    QString dot_color;
    QString text;
    switch (save_state_) {
    case SaveState::Idle:
        dot_color = colors::TEXT_TERTIARY();
        text = tr("New note — saved as you type");
        break;
    case SaveState::Unsaved:
        dot_color = colors::WARNING();
        text = tr("Unsaved changes");
        break;
    case SaveState::Saving:
        dot_color = colors::TEXT_TERTIARY();
        text = tr("Saving…");
        break;
    case SaveState::Saved:
        dot_color = colors::SUCCESS();
        text = save_detail_.isEmpty() ? tr("Saved") : tr("Saved %1").arg(save_detail_);
        break;
    case SaveState::Failed:
        dot_color = colors::DANGER();
        text = save_detail_.isEmpty() ? tr("Not saved — retrying") : save_detail_;
        break;
    }
    save_label_->setText(QStringLiteral("<span style=\"color:%1\">●</span>&nbsp;<span style=\"color:%2\">%3</span>")
                             .arg(dot_color, colors::TEXT_SECONDARY(), text.toHtmlEscaped()));
    save_label_->setAccessibleName(text);
}

void NotesEditor::set_dates(const QString& created_at, const QString& updated_at) {
    created_at_ = created_at;
    updated_at_ = updated_at;
    const auto fmt = [](const QString& s) {
        const QDateTime dt = parse_db_time(s);
        return dt.isValid() ? dt.toLocalTime().toString(QStringLiteral("MMM d, yyyy HH:mm")) : QString();
    };
    const QString c = fmt(created_at);
    const QString u = fmt(updated_at);
    QStringList parts;
    if (!c.isEmpty())
        parts << tr("Created %1").arg(c);
    if (!u.isEmpty() && u != c)
        parts << tr("Edited %1").arg(u);
    dates_label_->setText(parts.join(QStringLiteral("  ·  ")));
}

void NotesEditor::update_stats() {
    const QString text = body_->toPlainText();
    const int words = word_count(text);
    stats_label_->setText(tr("%n word(s)", nullptr, words) + QStringLiteral("  ·  ") +
                          tr("%n character(s)", nullptr, static_cast<int>(text.size())));
    const QStringList cashtags = extract_cashtags(text);
    if (cashtags != body_tickers_) {
        body_tickers_ = cashtags;
        rebuild_ticker_chips();
        if (!loading_)
            emit tickers_changed(linked_tickers());
    }
}

// ── Search highlight ─────────────────────────────────────────────────────────

void NotesEditor::set_highlight(const QStringList& terms) {
    if (terms == highlight_terms_)
        return;
    highlight_terms_ = terms;
    apply_highlight();
}

void NotesEditor::apply_highlight() {
    QList<QTextEdit::ExtraSelection> sels;
    if (!highlight_terms_.isEmpty()) {
        QColor wash = token(colors::AMBER);
        wash.setAlphaF(0.35f);
        QTextDocument* doc = body_->document();
        for (const QString& term : highlight_terms_) {
            QTextCursor c(doc);
            while (sels.size() < 400) {
                c = doc->find(term, c);
                if (c.isNull())
                    break;
                QTextEdit::ExtraSelection s;
                s.cursor = c;
                s.format.setBackground(wash);
                sels.append(s);
            }
        }
    }
    body_->setExtraSelections(sels);
}

// ── Chips ────────────────────────────────────────────────────────────────────

QFrame* NotesEditor::make_chip(QWidget* parent, const QString& text_html, const QString& tooltip, bool removable,
                               const QString& key, bool ticker) {
    auto* chip = new QFrame(parent);
    chip->setObjectName(ticker ? QStringLiteral("notesTickerChip") : QStringLiteral("notesTagChip"));
    chip->setToolTip(tooltip);
    auto* l = new QHBoxLayout(chip);
    const QFontMetrics fm(fonts::font(-2));
    const int h = fm.height() + 8;
    chip->setFixedHeight(h);
    l->setContentsMargins(8, 0, removable ? 2 : 8, 0);
    l->setSpacing(2);
    auto* label = new QLabel(chip);
    label->setObjectName(QStringLiteral("notesChipText"));
    label->setTextFormat(Qt::RichText);
    label->setText(text_html);
    l->addWidget(label);
    if (ticker) {
        label->setCursor(Qt::PointingHandCursor);
        label->setProperty("symbol", key);
        label->installEventFilter(this);
        ticker_labels_.insert(key, label);
    }
    if (removable) {
        auto* x = new QToolButton(chip);
        x->setObjectName(QStringLiteral("notesChipX"));
        x->setText(QStringLiteral("×"));
        x->setCursor(Qt::PointingHandCursor);
        x->setToolTip(ticker ? tr("Unlink %1").arg(key) : tr("Remove tag %1").arg(key));
        x->setFixedSize(h - 4, h - 4);
        connect(x, &QToolButton::clicked, this, [this, key, ticker]() {
            if (ticker) {
                explicit_tickers_.removeAll(key);
                rebuild_ticker_chips();
                emit tickers_changed(linked_tickers());
            } else {
                tags_.removeAll(key);
                rebuild_tag_chips();
            }
            mark_edited();
        });
        l->addWidget(x);
    }
    return chip;
}

void NotesEditor::rebuild_ticker_chips() {
    for (QFrame* f : ticker_box_->findChildren<QFrame*>(QStringLiteral("notesTickerChip"), Qt::FindDirectChildrenOnly)) {
        ticker_flow_->removeWidget(f);
        f->hide();
        f->setObjectName(QString()); // not found again before deleteLater runs
        f->deleteLater();
    }
    ticker_labels_.clear();
    const QString accent = colors::AMBER;
    int pos = 0;
    for (const QString& t : linked_tickers()) {
        const bool from_body = !explicit_tickers_.contains(t);
        QString html = QStringLiteral("<span style=\"color:%1;font-weight:600\">$%2</span>").arg(accent, t.toHtmlEscaped());
        html += quote_html_.value(t);
        const QString tip = (from_body ? tr("Linked by $%1 in the note text.").arg(t) + QLatin1Char('\n') : QString()) +
                            tr("Click: open %1 in another screen or send it to a link group").arg(t);
        QFrame* chip = make_chip(ticker_box_, html, tip, !from_body, t, true);
        ticker_flow_->insert_widget(pos++, chip);
        chip->show();
    }
    ticker_box_->updateGeometry();
}

void NotesEditor::rebuild_tag_chips() {
    for (QFrame* f : tag_box_->findChildren<QFrame*>(QStringLiteral("notesTagChip"), Qt::FindDirectChildrenOnly)) {
        tag_flow_->removeWidget(f);
        f->hide();
        f->setObjectName(QString()); // not found again before deleteLater runs
        f->deleteLater();
    }
    const QString dim = colors::TEXT_TERTIARY;
    const QString fg = colors::TEXT_PRIMARY;
    int pos = 0;
    for (const QString& t : tags_) {
        const QString html =
            QStringLiteral("<span style=\"color:%1\">#</span><span style=\"color:%2\">%3</span>").arg(dim, fg, t.toHtmlEscaped());
        QFrame* chip = make_chip(tag_box_, html, tr("Tag %1").arg(t), true, t, false);
        tag_flow_->insert_widget(pos++, chip);
        chip->show();
    }
    tag_box_->updateGeometry();
}

void NotesEditor::add_from_input(QLineEdit* input, bool tickers) {
    const QString text = input->text();
    bool changed = false;
    bool bad = false;
    if (tickers) {
        for (const QString& raw : text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts)) {
            const QString t = normalize_ticker(raw);
            if (t.isEmpty()) {
                bad = true;
                continue;
            }
            if (!linked_tickers().contains(t)) {
                explicit_tickers_ << t;
                changed = true;
            }
        }
    } else {
        for (const QString& t : split_tags(text)) {
            bool dup = false;
            for (const QString& mine : tags_)
                dup = dup || mine.compare(t, Qt::CaseInsensitive) == 0;
            if (!dup) {
                tags_ << t;
                changed = true;
            }
        }
    }
    input->setProperty("invalid", bad);
    input->style()->unpolish(input);
    input->style()->polish(input);
    if (!bad)
        input->clear();
    if (!changed)
        return;
    if (tickers) {
        rebuild_ticker_chips();
        emit tickers_changed(linked_tickers());
    } else {
        rebuild_tag_chips();
    }
    mark_edited();
}

void NotesEditor::set_quote(const QString& symbol, const QString& price, double change_pct, bool has_change) {
    QString html;
    if (!price.isEmpty()) {
        html = QStringLiteral("&nbsp;<span style=\"color:%1\">%2</span>").arg(colors::TEXT_SECONDARY(), price.toHtmlEscaped());
        if (has_change)
            html += QStringLiteral("&nbsp;<span style=\"color:%1\">%2%3%</span>")
                        .arg(fincept::ui::change_color(change_pct), change_pct >= 0 ? QStringLiteral("+") : QString())
                        .arg(change_pct, 0, 'f', 2);
    }
    if (quote_html_.value(symbol) == html)
        return;
    quote_html_.insert(symbol, html);
    if (QLabel* l = ticker_labels_.value(symbol))
        l->setText(QStringLiteral("<span style=\"color:%1;font-weight:600\">$%2</span>")
                       .arg(colors::AMBER(), symbol.toHtmlEscaped()) + html);
}

void NotesEditor::clear_quotes() {
    if (quote_html_.isEmpty())
        return;
    quote_html_.clear();
    rebuild_ticker_chips();
}

void NotesEditor::set_completions(const QStringList& tickers, const QStringList& tags) {
    Q_UNUSED(tickers); // the symbol picker suggests from the instrument index and its recent picks
    tag_words_->setStringList(tags);
}

void NotesEditor::add_ticker(const QString& symbol) {
    const QString t = normalize_ticker(symbol);
    if (t.isEmpty() || linked_tickers().contains(t))
        return;
    explicit_tickers_ << t;
    rebuild_ticker_chips();
    emit tickers_changed(linked_tickers());
    mark_edited();
}

void NotesEditor::replace_ticker(const QString& from, const QString& to) {
    const QString f = normalize_ticker(from);
    const QString t = normalize_ticker(to);
    if (f.isEmpty() || t.isEmpty() || f == t)
        return;
    const int at = static_cast<int>(explicit_tickers_.indexOf(f));
    if (at < 0) {
        add_ticker(t); // only in the text: link the new symbol, the text stays the user's
        return;
    }
    explicit_tickers_.removeAll(t);
    explicit_tickers_[std::min(at, static_cast<int>(explicit_tickers_.size()) - 1)] = t;
    rebuild_ticker_chips();
    emit tickers_changed(linked_tickers());
    mark_edited();
}

// ── Focus / events ───────────────────────────────────────────────────────────

void NotesEditor::focus_title() {
    title_->setFocus(Qt::ShortcutFocusReason);
    title_->selectAll();
}

void NotesEditor::focus_body() {
    if (mode_ == Mode::Read)
        set_mode(Mode::Write);
    body_->setFocus(Qt::ShortcutFocusReason);
}

bool NotesEditor::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* label = qobject_cast<QLabel*>(obj);
        if (label && label->property("symbol").isValid() &&
            static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            emit symbol_clicked(label->property("symbol").toString());
            return true;
        }
    }
    if (event->type() == QEvent::KeyPress && tag_completer_ && obj == tag_completer_->popup()) {
        // Suggestions are showing. Enter with none of them picked only closed the
        // list and threw the key away, so a typed tag was not added until Enter
        // was pressed a second time; a comma was typed into the text.
        auto* view = static_cast<QAbstractItemView*>(obj);
        auto* ke = static_cast<QKeyEvent*>(event);
        const bool enter = ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter;
        const bool comma = ke->key() == Qt::Key_Comma && !tag_input_->text().trimmed().isEmpty();
        if ((enter && !view->currentIndex().isValid()) || comma) {
            view->hide();
            add_from_input(tag_input_, false);
            return true;
        }
        return QWidget::eventFilter(obj, event);
    }
    if (event->type() == QEvent::KeyPress && ticker_input_ && obj == ticker_input_) {
        // The symbol picker handles Enter / Esc / arrows itself; Backspace in an
        // empty box unlinks the last ticker chip, as in the tag box.
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Backspace && ticker_input_->text().isEmpty() && !explicit_tickers_.isEmpty()) {
            explicit_tickers_.removeLast();
            rebuild_ticker_chips();
            emit tickers_changed(linked_tickers());
            mark_edited();
            return true;
        }
        return QWidget::eventFilter(obj, event);
    }
    if (event->type() == QEvent::KeyPress && obj == tag_input_) {
        auto* input = static_cast<QLineEdit*>(obj);
        auto* ke = static_cast<QKeyEvent*>(event);
        const bool tickers = false;
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter ||
            (ke->key() == Qt::Key_Comma && !input->text().trimmed().isEmpty())) {
            if (tag_completer_->popup() && tag_completer_->popup()->isVisible())
                return false; // the completer takes Enter
            add_from_input(input, tickers);
            return true;
        }
        if (ke->key() == Qt::Key_Backspace && input->text().isEmpty() && !tags_.isEmpty()) {
            tags_.removeLast();
            rebuild_tag_chips();
            mark_edited();
            return true;
        }
        if (ke->key() == Qt::Key_Escape && !input->text().isEmpty()) {
            input->clear();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace fincept::screens::notes
