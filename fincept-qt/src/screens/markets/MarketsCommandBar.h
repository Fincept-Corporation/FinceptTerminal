#pragma once
// MarketsCommandBar — the Markets screen's one-row command bar (spec 2.3 a):
// [symbol entry] PANELS | HEATMAP | MOVERS ........ LAYOUT v  DETAIL  (refresh) [interval v] (...)
// Watchlist's top-bar look (BG_SURFACE, 1 px BORDER_DIM bottom rule). Holds no data;
// everything it does is a signal to MarketsScreen.

#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QHBoxLayout;
class QPushButton;
class QToolButton;
class QEvent;

namespace fincept::screens {

class MarketsCommandBar : public QWidget {
    Q_OBJECT
  public:
    enum class View { Panels = 0, Heatmap = 1, Movers = 2 };
    Q_ENUM(View)

    explicit MarketsCommandBar(QWidget* parent = nullptr);

    void set_view(View view);
    void set_detail_checked(bool on);
    /// `ms`: 0 = LIVE, else the interval; `auto_on` false shows OFF (the interval is remembered).
    void set_interval_ms(int ms, bool auto_on);
    void set_refreshing(bool on);
    void set_pulse_checked(bool on);
    /// The symbol search widget (increment 5), placed first in the bar.
    void set_symbol_entry(QWidget* entry);
    /// Columns currently shown by the panels (the "Columns for all panels" checkmarks).
    void set_columns_hint(const QStringList& ids);
    void refresh_theme();
    void retranslate();
    /// Narrow windows: the bar never demands its full-content width (the view switch
    /// folds into the overflow menu in resizeEvent instead).
    QSize minimumSizeHint() const override;

  signals:
    void view_selected(fincept::screens::MarketsCommandBar::View view);
    void detail_toggled(bool on);
    void refresh_requested();
    /// -1 = OFF (auto refresh off), 0 = LIVE, else ms.
    void interval_changed(int ms);
    void add_panel_requested();
    void reset_requested();
    void equalise_requested();
    void pulse_toggled(bool on);
    void export_all_requested();
    void columns_for_all_changed(const QStringList& ids);
    void reset_widths_requested();

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void show_layout_menu();
    void show_overflow_menu();
    void apply_collapse();

    QHBoxLayout* row_ = nullptr;
    QWidget* entry_ = nullptr;
    QButtonGroup* views_ = nullptr;
    QPushButton* panels_btn_ = nullptr;
    QPushButton* heatmap_btn_ = nullptr;
    QPushButton* movers_btn_ = nullptr;
    QToolButton* layout_btn_ = nullptr;
    QPushButton* detail_btn_ = nullptr;
    QToolButton* refresh_btn_ = nullptr;
    QComboBox* interval_ = nullptr;
    QToolButton* overflow_btn_ = nullptr;
    QStringList columns_hint_;
    bool pulse_checked_ = true;
    bool collapsed_ = false;
    bool updating_ = false;
};

} // namespace fincept::screens
