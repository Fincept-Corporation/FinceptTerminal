#pragma once
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::ui {

struct ScreenCommand {
    QString id;
    QString name;
    QString description;
    QStringList aliases;
    QString shortcut;
    QStringList keywords;
};

/// Asset type for slash-command search (maps to /market/search `type` param).
struct AssetType {
    QString slash;       // e.g. "/stock"
    QString api_type;    // e.g. "stock"  — sent as ?type= to API
    QString label;       // e.g. "Stock"
    QString description; // e.g. "Search stocks by symbol or name"
};

/// Bloomberg-style command bar with live search and dropdown navigation.
/// Type a command alias, screen name, or keyword — press Enter or click to navigate.
/// Supports /type queries (e.g. "/stock AAPL") to search assets via API.
class CommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit CommandBar(QWidget* parent = nullptr);

  signals:
    void navigate_to(const QString& screen_id);

    // Emitted when the user types a compound command:
    //   "markets add settings"   → action="add",     primary="markets", secondary="settings"
    //   "markets replace news"   → action="replace",  primary="markets", secondary="news"
    //   "markets remove"         → action="remove",   primary="markets", secondary=""
    void dock_command(const QString& action,
                      const QString& primary,
                      const QString& secondary);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private slots:
    void on_text_changed(const QString& text);
    void on_return_pressed();
    void on_item_clicked(QListWidgetItem* item);
    void on_item_entered(QListWidgetItem* item);

  private:
    QLineEdit* input_ = nullptr;
    QFrame* dropdown_ = nullptr;
    QListWidget* list_ = nullptr;

    // ── Screen command search ──────────────────────────────────────────────────
    QVector<ScreenCommand> commands_;
    void build_commands();
    QVector<ScreenCommand> search(const QString& query) const;

    // ── Slash-command asset search ─────────────────────────────────────────────
    enum class Mode {
        Screen,       // normal screen search
        SlashPicker,  // user typed "/" — picking asset type
        AssetSearch,  // user picked /type and is typing a symbol
        DockCommand,  // user typed a valid screen + space — picking add/replace/remove
        DockSecondary // user picked add/replace — picking the second screen
    };
    Mode mode_ = Mode::Screen;

    QVector<AssetType> asset_types_;
    QString active_asset_type_;   // e.g. "stock" — set after user picks /stock
    QTimer* search_debounce_ = nullptr;
    QString pending_query_;

    void build_asset_types();
    void show_slash_suggestions(const QString& partial);
    void activate_asset_mode(const QString& api_type);
    void schedule_asset_search(const QString& query);
    void fire_asset_search(const QString& query);
    void on_asset_results(const QJsonArray& results);
    void select_asset(const QString& symbol, const QString& type);

    // ── Compound dock command parser ──────────────────────────────────────────
    bool try_parse_dock_command(const QString& text);
    QString resolve_screen_id(const QString& token) const;
    void show_dock_verb_suggestions(const QString& primary_id);
    void show_dock_secondary_suggestions(const QString& verb, const QString& partial);

    QString dock_primary_id_;  // screen id locked in after user types "<screen> "
    QString dock_verb_;        // "add" or "replace" — locked in after user picks verb

    // ── Theme ─────────────────────────────────────────────────────────────────
    void refresh_theme();

    // ── Dropdown helpers ───────────────────────────────────────────────────────
    void show_dropdown();
    void hide_dropdown();
    void update_position();
    void execute_index(int index);
};

} // namespace fincept::ui
