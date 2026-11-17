#pragma once
// EmptyState — the designed state a pane shows when there is nothing to draw
// yet: a glyph, a title, a one-line explanation, an optional preview of the
// inputs that would be used, an optional note (credit cost, required plan,
// data source) and up to two actions.
//
//   auto* es = new ui::EmptyState(pane);
//   es->set_glyph(QStringLiteral("∑"));
//   es->set_title(tr("Black-Scholes price"));
//   es->set_message(tr("Prices a European option and its Greeks from the inputs on the left."));
//   es->set_details({{tr("Spot"), "100"}, {tr("Strike"), "105"}});
//   es->set_note(tr("1 credit per run on the Free plan"), ui::EmptyState::Tone::Warning);
//   es->set_primary_action(tr("RUN (1 CREDIT)"));
//   connect(es, &ui::EmptyState::primary_clicked, this, &MyPane::run);
//
// Use it for a "preview" (a calculation that is not run automatically because
// it costs credits or needs a plan), a genuine "nothing here yet" (no notes, no
// runs) and a "not available" (sign-in needed, service unreachable) — always
// with the action that fills the pane. Never leave a result area blank.
//
// Styling: one selector-based stylesheet set on the widget before its children
// exist, built from theme tokens, so the theme manager's re-style pass keeps
// it in step with theme and font changes; the glyph is custom-painted from the
// live theme. No theme_changed handler is needed (and none may restyle).
// Action texts are plain text: a '&' is shown as '&', not as a mnemonic.
//
// The standard states use the same words everywhere — build them from a Spec:
//   es->apply(ui::EmptyState::preview_spec(tr("Monte Carlo VaR"),               // class C: costs / heavy
//                                         tr("Simulates 10,000 paths of your holdings."),
//                                         tr("RUN · 1 CREDIT"), tr("1 credit per run"),
//                                         ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::Chart));
//   es->apply(ui::EmptyState::no_data_spec(tr("No positions yet"), tr("Buy a symbol to start."), tr("BUY")));
//   es->apply(ui::EmptyState::error_spec(reason));                            // "Could not load" + RETRY
//   es->apply(ui::EmptyState::needs_setup_spec(ui::check_requirements(needs))); // key / model / Python / broker
// or create one directly: ui::EmptyState::create(spec, parent).
//
// Short panes (a 3-row dashboard tile at base font 24): the full layout is
// measured from font metrics at the current width and, when it does not fit,
// the state compacts in steps (empty_state_fit::choose_level in
// AutoLoadPolicy.h): no glyph and tight margins → no detail / suggestion rows →
// title + primary action on one line → the title alone. Hidden text moves into
// the title's tooltip. Re-evaluated on resize and on
// ThemeManager::appearance_settled. set_auto_fit(false) keeps the old
// behaviour (full minimum height, height-for-width).
//
// Over live content (a dashboard tile, a chart that lost its feed):
// set_opaque(true) paints the panel token behind the state, so no cover widget
// with its own paintEvent is needed.

#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWidget>

class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace fincept::ui {

class EmptyStateGlyph;
class EmptyStateSuggestion;
class EmptyStateDetailRow;

class EmptyState : public QWidget {
    Q_OBJECT
  public:
    /// Colour of the glyph ring and the note. Warning/Accent for cost and plan
    /// notes, Info for neutral facts (source, schedule), Danger for failures.
    enum class Tone { Neutral, Info, Accent, Warning, Danger, Success };

    /// The token an opaque EmptyState paints behind itself (set_opaque):
    /// Surface = panels, cards and dashboard tiles; Base = the window
    /// background; Raised = headers and strips.
    enum class Backdrop { Surface, Base, Raised };

    /// A clickable suggestion row under the actions ("Try AAPL", "Start from a
    /// template"): a label, optional secondary text under it, optional tooltip.
    /// Rows are keyboard reachable (Tab; Enter / Space click) and carry the label
    /// as accessible name and the secondary text as accessible description.
    struct Suggestion {
        QString label;
        QString secondary;
        QString tooltip;
        /// Since round 6 (7): the row's "fincept_agent_risk" kind ("spend", …); empty = none.
        QString agent_risk{};
        bool operator==(const Suggestion&) const = default;
    };
    /// Most suggestion rows shown.
    static constexpr int kMaxSuggestions = 6;

    explicit EmptyState(QWidget* parent = nullptr);

    /// Every field of the state at once (a reused EmptyState never keeps stale text).
    struct Spec {
        QString glyph;
        Tone glyph_tone = Tone::Accent;
        QString title;
        QString message;
        QList<QPair<QString, QString>> details;
        QString details_heading;
        QString note;
        Tone note_tone = Tone::Neutral;
        QString primary;   ///< empty = no primary button
        QString secondary; ///< empty = no secondary button
        bool illustration = false;
        LoadingSkeleton::Shape illustration_shape = LoadingSkeleton::Shape::Result;
        /// Technical text behind a DETAILS button (set_technical_details); empty = none.
        QString technical;
        // Since 2026-11-17 round 3 — new fields go last, so positional initialisers stay valid.
        QString primary_tooltip;   ///< tooltip of the primary button; empty = none
        QString secondary_tooltip; ///< tooltip of the secondary button; empty = none
        /// Clickable rows under the actions (≤ kMaxSuggestions); emit suggestion_clicked(index).
        QList<Suggestion> suggestions;
        QString suggestions_heading; ///< empty = no heading above the rows
        // Since 2026-11-17 round 5.
        /// The `details` rows become keyboard-reachable buttons (set_details_clickable):
        /// detail_clicked(index, key) — e.g. "open this failed symbol" on a failure page.
        bool details_clickable = false;
        // Since round 6 (7): the buttons' "fincept_agent_risk" kinds (set_primary_agent_risk); empty = none.
        QString primary_agent_risk;
        QString secondary_agent_risk;
        // Since round 7: the "fincept_agent_risk" kinds of the clickable detail rows
        // (details_clickable; set_details_agent_risk). `details_agent_risk` = every row;
        // `details_agent_risks[i]` = row i (index-aligned with `details`; a missing or empty
        // entry falls back to `details_agent_risk`). Both empty = no marker.
        QString details_agent_risk;
        QStringList details_agent_risks;

        /// Field-wise equality (since round 6): apply() of an equal spec is a no-op.
        bool operator==(const Spec&) const = default;
    };
    /// Sets every field. Not part of the Spec (kept across apply()): set_opaque,
    /// set_auto_fit, set_compact, set_alignment.
    /// Since round 6: a no-op (no relayout, repaint, fit pass or flicker; an open
    /// DETAILS box stays open) when `spec` equals the last one applied and no
    /// single setter (set_title, set_primary_enabled, …) has changed the state
    /// since — after one, apply() of the old spec applies again.
    void apply(const Spec& spec);
    static EmptyState* create(const Spec& spec, QWidget* parent);

    /// Detail rows without a key column: each line becomes a bullet row
    /// ({"", line}) — set_details() draws a row with an empty key as a bullet
    /// spanning the block. Mix freely with key / value rows.
    static QList<QPair<QString, QString>> bullet_rows(const QStringList& lines);

    /// Class C (costs credits, rate-limited, heavy, sends a prompt): what it is,
    /// what it returns, ONE primary action, the cost / plan note, and a faint
    /// still illustration of the result's shape. `details` (≤ 8 rows) and its
    /// `details_heading` ("WHAT WILL BE LOADED"; default "SAMPLE INPUTS") fill the
    /// key / value block, as set_details() does.
    static Spec preview_spec(const QString& title, const QString& what_it_returns, const QString& run_text,
                             const QString& cost_note = QString(), Tone cost_tone = Tone::Warning,
                             LoadingSkeleton::Shape illustration = LoadingSkeleton::Shape::Result,
                             const QList<QPair<QString, QString>>& details = {},
                             const QString& details_heading = QString());
    /// Genuinely nothing to show, with the action that creates the first item.
    static Spec no_data_spec(const QString& title, const QString& message, const QString& action_text = QString());
    /// A plain-sentence failure; default title "Could not load" and action "RETRY".
    /// `technical` (a traceback, a raw server message) goes behind DETAILS.
    static Spec error_spec(const QString& message, const QString& title = QString(),
                           const QString& action_text = QString(), const QString& technical = QString());
    /// Something is missing (key, model, Python, broker, credits): names it and
    /// offers the button that opens where to fix it (status.action_text).
    static Spec needs_setup_spec(const RequirementStatus& status);

    /// A faint, still skeleton of the result's shape behind the text.
    void set_preview_shape(LoadingSkeleton::Shape shape);
    void clear_preview_shape();

    /// One or two characters drawn large inside a ring ("∑", "◇", "⌁", "!").
    void set_glyph(const QString& glyph, Tone tone = Tone::Accent);
    void set_title(const QString& title);
    /// One or two sentences; word-wrapped, centred.
    void set_message(const QString& text);
    /// Optional key / value preview (sample inputs, what will be fetched).
    /// Up to 8 rows are shown (one per line); an empty list hides the block.
    /// A row with an empty key is a bullet row ("• value", word-wrapped, no key
    /// column) — see bullet_rows().
    void set_details(const QList<QPair<QString, QString>>& rows, const QString& heading = QString());
    /// Detail rows as actions (default off = plain text rows): each row is a
    /// button — hover fill, a "›" cue, Tab-reachable before the action buttons,
    /// Enter / Space click, 2 px amber focus frame, accessible name = key (the
    /// bullet text for a bullet row), description = value — emitting
    /// detail_clicked(index, key). Kept across set_details(); apply() sets it
    /// from Spec::details_clickable.
    void set_details_clickable(bool clickable);
    bool details_clickable() const { return details_clickable_; }
    /// Agent-approval markers of the clickable detail rows (since round 7): `kind` on
    /// every row button, `per_row[i]` (index-aligned with the set_details() rows; an
    /// empty or missing entry = `kind`) on row i — e.g. "order" on a row that re-sends a
    /// rejected order. Set with mark_agent_risk(), like the primary / secondary /
    /// suggestion buttons (empty REMOVES the property). Kept across set_details() and
    /// set_details_clickable(); plain (non-clickable) rows are labels and carry none.
    /// apply() sets it from Spec::details_agent_risk / Spec::details_agent_risks.
    void set_details_agent_risk(const QString& kind, const QStringList& per_row = QStringList());
    QString details_agent_risk() const { return details_risk_; }
    QStringList details_agent_risks() const { return details_risks_; }
    /// The kind on detail row `index` now (its own, else the block's); empty = none.
    QString detail_agent_risk(int index) const { return detail_row_agent_risk(details_risk_, details_risks_, index); }
    /// The per-row rule, header-inline so it is unit-testable without the widget's
    /// sources: `per_row[index]` when present and non-empty, else `all`.
    static QString detail_row_agent_risk(const QString& all, const QStringList& per_row, int index) {
        if (index >= 0 && index < per_row.size() && !per_row.at(index).isEmpty())
            return per_row.at(index);
        return all;
    }
    /// Detail rows shown (≤ 8).
    int detail_count() const { return detail_rows_; }
    /// Clickable rows under the actions (≤ kMaxSuggestions; an empty list hides
    /// them); `heading` is a small caption above them ("OR TRY"), empty = none.
    void set_suggestions(const QList<Suggestion>& rows, const QString& heading = QString());
    int suggestion_count() const;
    /// Optional short note in a framed chip under the message (credit cost,
    /// required plan, "as of" time). Empty hides it.
    void set_note(const QString& text, Tone tone = Tone::Neutral);
    /// Optional technical text (a raw traceback, a long server message) behind a
    /// DETAILS button under the message, so the page itself keeps the plain
    /// sentence. Collapsed by default (collapsed again whenever the text changes);
    /// selectable and copyable; shown through log_scrub() (keys, tokens, e-mail,
    /// home paths and URL queries masked). Empty hides the button.
    void set_technical_details(const QString& text);
    QString technical_details() const { return technical_; }
    /// Primary (filled) and secondary (outlined) actions. Empty text hides the
    /// button. Emit primary_clicked() / secondary_clicked().
    void set_primary_action(const QString& text);
    void set_secondary_action(const QString& text);
    void set_primary_enabled(bool enabled);
    /// Tooltips of the two buttons (empty = none); apply() sets them from the Spec.
    void set_primary_tooltip(const QString& tooltip);
    void set_secondary_tooltip(const QString& tooltip);
    /// Agent-approval marker (since round 6 (7)): the dynamic property
    /// kAgentRiskProperty ("fincept_agent_risk") on the button — the kind of
    /// commitment ("spend", "delete", …) an outside agent operating the UI needs
    /// the user's approval for. Empty REMOVES the property (an empty value would
    /// hide a marker on an ancestor). apply() sets both from the Spec and each
    /// suggestion row (a QAbstractButton of its own) from Suggestion::agent_risk.
    void set_primary_agent_risk(const QString& kind);
    void set_secondary_agent_risk(const QString& kind);
    /// The primary's kind whenever the applied Spec / set_primary_agent_risk names
    /// none. Not part of the Spec (kept across apply(), like set_opaque): for a page
    /// a host fills while its owner knows the button re-runs a billed load
    /// (AutoLoadPane's preview RUN). Empty = none.
    void set_primary_agent_risk_default(const QString& kind);
    /// The kind on the primary button now (explicit, else the default); empty = none.
    QString primary_agent_risk() const;
    QString secondary_agent_risk() const { return secondary_risk_; }
    /// The property name the MCP UI risk walk reads (mcp/core/UiRisk.cpp).
    static constexpr const char* kAgentRiskProperty = "fincept_agent_risk";
    /// Sets `kind` on `target` (empty removes the property); touches it only on a
    /// change. Header-inline so it is unit-testable without the widget's sources.
    static void mark_agent_risk(QObject* target, const QString& kind) {
        if (!target)
            return;
        const QVariant now = target->property(kAgentRiskProperty);
        if (kind.isEmpty()) {
            // Removed, never "": an empty value would stop the walk to an ancestor's marker.
            if (now.isValid())
                target->setProperty(kAgentRiskProperty, QVariant());
            return;
        }
        if (!now.isValid() || now.toString() != kind)
            target->setProperty(kAgentRiskProperty, kind);
    }
    /// Compact: smaller glyph and margins for narrow panes and cards.
    void set_compact(bool compact);
    /// Vertical placement in the pane (default centred).
    void set_alignment(Qt::Alignment vertical);
    /// Paint the `backdrop` token behind the state (read from the live theme at
    /// paint time), so it can sit over live content as a cover. Default off
    /// (transparent: the pane behind shows through).
    void set_opaque(bool opaque, Backdrop backdrop = Backdrop::Surface);
    bool is_opaque() const { return opaque_; }
    /// Auto-compaction in short panes (default on). Off: the full layout is the
    /// minimum height (and height-for-width), as before 2026-11-17 round 3.
    void set_auto_fit(bool enabled);
    bool auto_fit() const { return auto_fit_; }
    /// The compaction step in effect (Full unless the pane is too short).
    empty_state_fit::Level fit_level() const { return fit_level_; }

    /// Everything at once — the common case.
    void set_content(const QString& glyph, const QString& title, const QString& message,
                     const QString& primary_action = QString(), const QString& note = QString(),
                     Tone note_tone = Tone::Neutral);

    QString title() const;
    QPushButton* primary_button() const { return primary_; }
    QPushButton* secondary_button() const { return secondary_; }

    /// With auto-fit: the full layout as the preferred size, the title line as
    /// the minimum height, and no height-for-width (the state fits itself).
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;

  signals:
    void primary_clicked();
    void secondary_clicked();
    /// A suggestion row was clicked (mouse, Enter or Space); `index` into the rows given.
    void suggestion_clicked(int index);
    /// A detail row was clicked while set_details_clickable(true): `index` into the
    /// rows given to set_details(), `key` its key ("" for a bullet row).
    void detail_clicked(int index, const QString& key);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    bool event(QEvent* e) override;

  private:
    static QString tone_key(Tone tone);
    /// The DETAILS button and its text box, built on the first technical text.
    void ensure_technical();
    void set_technical_open(bool open);
    /// Measure the blocks at the current width and pick the compaction level
    /// (deferred to appearance_settled while the theme manager notifies).
    void refit();
    /// Show / hide every block for the content and fit_level_.
    void apply_fit();
    empty_state_fit::Parts measure(int column_width) const;
    int column_width() const;
    int full_margins() const;
    void update_inline_title();

    LoadingSkeleton* illustration_ = nullptr; ///< created on first set_preview_shape()
    QPushButton* technical_btn_ = nullptr;    ///< DETAILS / HIDE DETAILS
    QPlainTextEdit* technical_box_ = nullptr;
    QString technical_;                       ///< as given (scrubbed when shown)
    QVBoxLayout* outer_ = nullptr;
    QWidget* column_ = nullptr;
    EmptyStateGlyph* glyph_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* message_ = nullptr;
    QWidget* details_box_ = nullptr;
    QLabel* details_heading_ = nullptr;
    QGridLayout* details_grid_ = nullptr;
    QLabel* note_ = nullptr;
    QPushButton* primary_ = nullptr;
    QPushButton* secondary_ = nullptr;
    QString note_tone_;
    bool compact_ = false;
    // Since 2026-11-17 round 3.
    QLabel* inline_title_ = nullptr;          ///< title on the action row (Inline / TitleOnly)
    QWidget* suggestions_box_ = nullptr;
    QLabel* suggestions_heading_ = nullptr;
    QList<EmptyStateSuggestion*> suggestion_rows_;
    int suggestions_shown_ = 0;
    int detail_rows_ = 0;
    bool opaque_ = false;
    Backdrop backdrop_ = Backdrop::Surface;
    bool auto_fit_ = true;
    bool fitting_ = false;                    ///< inside refit(): no nested refit
    bool batching_ = false;                   ///< inside apply(): one refit at the end
    bool technical_open_ = false;
    empty_state_fit::Level fit_level_ = empty_state_fit::Level::Full;
    int full_height_ = 0;                     ///< needed height of the full layout (last measure)
    // Since 2026-11-17 round 5.
    /// Clickable detail rows: technical button → detail rows → primary → secondary → suggestions.
    /// Deferred (tab_chain_pending_) while the EmptyState has no parent and is not shown — it is
    /// applied on the parent change / first show, inside the window it will live in.
    void order_tab_chain();
    /// The chaining itself: only non-null, focusable widgets of this window (never across windows).
    void chain_tab_order();
    bool tab_chain_pending_ = false;
    QList<QPair<QString, QString>> details_;  ///< rows as given (rebuilt when clickability changes)
    QString details_heading_text_;            ///< heading as given
    QList<EmptyStateDetailRow*> detail_buttons_;
    bool details_clickable_ = false;
    // Since round 6: the spec on screen (apply() of an equal one does nothing).
    empty_state_apply::LastApplied<Spec> applied_;
    void setter_used() {
        if (!batching_)
            applied_.invalidate(); // changed outside apply(): the next apply() always applies
    }
    // Since round 6 (7): agent-approval markers.
    QString primary_risk_;          ///< explicit (Spec / set_primary_agent_risk)
    QString primary_risk_default_;  ///< set_primary_agent_risk_default (kept across apply())
    QString secondary_risk_;
    // Since round 7: agent-approval markers of the clickable detail rows.
    QString details_risk_;          ///< every row (set_details_agent_risk / Spec::details_agent_risk)
    QStringList details_risks_;     ///< per row, index-aligned with details_
    void mark_detail_rows();        ///< detail_buttons_[i] ← detail_agent_risk(i)
};

} // namespace fincept::ui
