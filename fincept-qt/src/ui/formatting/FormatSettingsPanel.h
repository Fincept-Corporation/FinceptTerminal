#pragma once
// FormatSettingsPanel — "Currency & Number Format": every FormatSettings value
// the user can change (display currency, symbol style / position / spacing,
// negative style, number locale and grouping, decimals, compact scheme, minor
// units, codes in mixed columns, FX stale threshold, FX P&L split, per-currency
// symbols and decimals) with a live preview row.
//
// Self-contained and reusable: a plain QWidget with no scroll area, page title
// or outer frame (the host supplies those); depends on nothing in
// screens/settings. Settings:     new FormatSettingsPanel(parent)
//                  first-time setup: new FormatSettingsPanel(parent, FormatSettingsPanel::Mode::Compact)
//
// Behaviour:
//   * apply on change — exactly one FormatSettings setter (so one changed())
//     per user action; persisted by FormatSettingsStore; never touches
//     SettingsRepository itself. Several instances stay in sync.
//   * cheap constructor: only the preview; the controls are built when the
//     panel is first shown, the per-currency table (model/view, no per-row
//     widgets) when it is first expanded.
//   * appearance: colours / fonts from ui::colors / ui::fonts only; one
//     stylesheet set at construction (QLabel backgrounds transparent);
//     ThemeManager::theme_changed -> preview update() only;
//     appearance_settled -> re-fit (label column, preview height).

#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;
class QTableView;
class QTimer;
class QToolButton;
class QVBoxLayout;

namespace fincept::ui::formatting {

class CurrencyOverrideModel;

namespace fmtpanel_detail {
class FmtPanelPreviewStrip;
class FmtPanelChoiceRow;
} // namespace fmtpanel_detail

class FormatSettingsPanel : public QWidget {
    Q_OBJECT
  public:
    /// Full: everything. Compact: display currency, number locale and the
    /// preview (first-time setup's region step).
    enum class Mode { Full, Compact };

    explicit FormatSettingsPanel(QWidget* parent = nullptr, Mode mode = Mode::Full);

    /// tr("Currency & Number Format") — the section's title for a host's navigation.
    static QString section_title();
    /// Words a settings search should match for this section.
    static QStringList search_keywords();

  protected:
    void showEvent(QShowEvent* event) override;

  private slots:
    /// Back to the defaults (the time zone and clock are kept: not on this panel).
    void reset_settings();
    /// Opens the "Add custom code" dialog.
    void add_custom_code();

  private:
    QWidget* build_section(QVBoxLayout* root, const QString& title, QGridLayout** grid);
    QLabel* add_row(QGridLayout* grid, const QString& label, QWidget* control, const QString& hint = {});
    void build_currency_section(QVBoxLayout* root);
    void build_numbers_section(QVBoxLayout* root);
    void build_fx_section(QVBoxLayout* root);
    void build_overrides_section(QVBoxLayout* root);
    void ensure_built();
    void ensure_override_table();
    void fill_display_currency_combo();
    void fill_locale_combo();
    void sync_from_settings();
    void refit();
    void commit_decimals();
    void commit_stale_minutes();

    Mode mode_;
    bool built_ = false;
    QVBoxLayout* root_ = nullptr;
    QWidget* body_ = nullptr; ///< every section below the preview; built on first show
    QStringList label_texts_;      ///< row labels, for the label column width
    QList<QLabel*> row_labels_;
    QList<QGridLayout*> grids_;
    QStringList custom_codes_in_combo_;

    fmtpanel_detail::FmtPanelPreviewStrip* preview_ = nullptr;
    QComboBox* display_ccy_ = nullptr;
    QLabel* display_hint_ = nullptr;
    fmtpanel_detail::FmtPanelChoiceRow* style_row_ = nullptr;
    QComboBox* position_ = nullptr;
    QComboBox* spacing_ = nullptr;
    fmtpanel_detail::FmtPanelChoiceRow* negative_row_ = nullptr;
    fmtpanel_detail::FmtPanelChoiceRow* minor_row_ = nullptr;
    QCheckBox* code_when_mixed_ = nullptr;
    QComboBox* locale_ = nullptr;
    QComboBox* grouping_ = nullptr;
    QLabel* grouping_example_ = nullptr;
    QComboBox* decimals_policy_ = nullptr;
    QSpinBox* fixed_decimals_ = nullptr;
    QTimer* decimals_debounce_ = nullptr;
    QComboBox* compact_ = nullptr;
    QSpinBox* stale_minutes_ = nullptr;
    QTimer* stale_debounce_ = nullptr;
    QCheckBox* fx_pnl_split_ = nullptr;
    QToolButton* overrides_toggle_ = nullptr;
    QLabel* overrides_summary_ = nullptr;
    QWidget* overrides_body_ = nullptr;
    QLineEdit* overrides_filter_ = nullptr;
    QTableView* overrides_view_ = nullptr;
    CurrencyOverrideModel* overrides_model_ = nullptr;
    QSortFilterProxyModel* overrides_proxy_ = nullptr;
    QPushButton* reset_ = nullptr;
};

} // namespace fincept::ui::formatting
