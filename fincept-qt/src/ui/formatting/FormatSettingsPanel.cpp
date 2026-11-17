// FormatSettingsPanel.cpp — see FormatSettingsPanel.h.

#include "ui/formatting/FormatSettingsPanel.h"

#include "ui/formatting/CurrencyOverrideModel.h"
#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimeZone>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::ui::formatting {

namespace fmtpanel_detail {

QString fmtpanel_tr(const char* text, const char* comment = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::ui::formatting::FormatSettingsPanel", text, comment, n);
}

// ── Preview ─────────────────────────────────────────────────────────────────

struct FmtPanelCell {
    QString caption;
    QString value;
    QString sub;
    bool negative = false;
};

/// Approximate USD per unit for the preview's sample conversion only. The
/// panel needs no data and no FX fetch; the cell says "(sample)".
double fmtpanel_sample_usd_per_unit(const QString& code) {
    static const QHash<QString, double> k = {
        {"USD", 1.0},          {"EUR", 1.10},         {"GBP", 1.32},         {"JPY", 1.0 / 159.0},
        {"CHF", 1.0 / 0.84},   {"CAD", 1.0 / 1.38},   {"AUD", 0.67},         {"NZD", 0.55},
        {"HKD", 1.0 / 7.85},   {"SGD", 1.0 / 1.30},   {"CNY", 1.0 / 6.66},   {"CNH", 1.0 / 6.66},
        {"INR", 1.0 / 94.0},   {"KRW", 1.0 / 1390.0}, {"SEK", 1.0 / 9.5},    {"NOK", 1.0 / 10.1},
        {"DKK", 1.0 / 6.8},    {"BRL", 1.0 / 5.4},    {"MXN", 1.0 / 18.5},   {"ZAR", 1.0 / 17.0},
        {"TRY", 1.0 / 50.0},   {"PLN", 1.0 / 3.65},   {"ILS", 1.0 / 3.25},   {"AED", 1.0 / 3.6725},
        {"SAR", 1.0 / 3.75},   {"TWD", 1.0 / 32.0},   {"THB", 1.0 / 33.0},   {"IDR", 1.0 / 16500.0},
        {"MYR", 1.0 / 4.2},    {"PHP", 1.0 / 58.0},   {"CZK", 1.0 / 21.0},   {"HUF", 1.0 / 330.0},
        {"KWD", 1.0 / 0.307},  {"BHD", 1.0 / 0.376},  {"QAR", 1.0 / 3.64},   {"PKR", 1.0 / 280.0},
        {"VND", 1.0 / 25750.0}, {"BTC", 65000.0},     {"ETH", 3000.0},       {"USDT", 1.0},
        {"USDC", 1.0},
    };
    return k.value(code, 0.0);
}

QVector<FmtPanelCell> fmtpanel_preview_cells() {
    QVector<FmtPanelCell> cells;
    const QString display = display_currency();
    const QString dot = QStringLiteral(" \u00B7 ");

    cells.append({fmtpanel_tr("Price"), price_with_symbol(351.71, InstrumentFmt{QStringLiteral("USD")}),
                  QStringLiteral("AAPL") + dot + QStringLiteral("USD")});

    // A total converted into the display currency, with its rate and time.
    const QString source = display == QLatin1String("EUR") ? QStringLiteral("USD") : QStringLiteral("EUR");
    const double from_usd = fmtpanel_sample_usd_per_unit(source);
    const double to_usd = fmtpanel_sample_usd_per_unit(display);
    FmtPanelCell total;
    total.caption = fmtpanel_tr("Total, converted");
    if (to_usd > 0) {
        ConvertedMoney c;
        c.source_amount = 10000.0;
        c.source_currency = source;
        c.currency = display;
        c.rate = from_usd / to_usd;
        c.amount = c.source_amount * c.rate;
        c.as_of = QDateTime(QDate(2026, 11, 16), QTime(14, 32), QTimeZone(QTimeZone::UTC));
        c.converted = true;
        total.value = converted_money(c);
        total.sub = money(10000.0, source) + dot + fx_note(c) + QLatin1Char(' ') + fmtpanel_tr("(sample)");
    } else {
        total.value = money(10000.0, display);
        total.sub = fmtpanel_tr("total in %1 (sample)").arg(display);
    }
    cells.append(total);

    cells.append({fmtpanel_tr("Loss"), signed_money(-1234.56, display), fmtpanel_tr("negative amounts"), true});
    cells.append({fmtpanel_tr("London quote"), price_with_symbol(117.70, InstrumentFmt{QStringLiteral("GBp")}),
                  QStringLiteral("VOD.L") + dot + QStringLiteral("GBp")});
    cells.append({fmtpanel_tr("Large amount"), money(12345678.0, QStringLiteral("INR"), Compact::FromSettings),
                  number(12345678.0, 0) + QStringLiteral(" INR")});
    cells.append({fmtpanel_tr("Crypto price"), price_with_symbol(0.00001234, InstrumentFmt{QStringLiteral("USD")}),
                  QStringLiteral("SHIB-USD") + dot + money_precise(0.00001234 * 2500000.0, QStringLiteral("USDT"))});
    return cells;
}

/// The live preview row: one custom-painted widget, no child labels, no
/// stylesheet, no background of its own; colours read at paint time, so a
/// theme change is just update().
class FmtPanelPreviewStrip : public QWidget {
  public:
    explicit FmtPanelPreviewStrip(QWidget* parent) : QWidget(parent) {
        QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sp.setHeightForWidth(true);
        setSizePolicy(sp);
        setAccessibleName(fmtpanel_tr("Preview"));
        refresh();
    }

    void refresh() {
        cells_ = fmtpanel_preview_cells();
        QStringList texts;
        for (const auto& c : std::as_const(cells_))
            texts << c.caption + QStringLiteral(": ") + c.value + QStringLiteral(" (") + c.sub + QLatin1Char(')');
        const QString all = texts.join(QStringLiteral("; "));
        setAccessibleDescription(all);
        setToolTip(texts.join(QLatin1Char('\n')));
        updateGeometry();
        update();
    }

    /// Fonts changed (appearance settled): sizes depend on them.
    void refit() {
        updateGeometry();
        update();
    }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override {
        const int cols = columns_for(w);
        const int rows = (int(cells_.size()) + cols - 1) / cols;
        return rows * cell_height() + (rows - 1) * kGap;
    }
    QSize sizeHint() const override {
        const int w = 3 * (min_cell_width() + kGap);
        return {w, heightForWidth(w)};
    }
    QSize minimumSizeHint() const override { return {min_cell_width(), heightForWidth(min_cell_width())}; }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        const int cols = columns_for(width());
        const int ch = cell_height();
        const int cw = (width() - (cols - 1) * kGap) / std::max(1, cols);
        const QFont cap_font = fonts::font(-3);
        const QFont val_font = fonts::font(2, QFont::Bold);
        const QFont sub_font = fonts::font(-3);
        const QFontMetrics cap_fm(cap_font), val_fm(val_font), sub_fm(sub_font);
        for (int i = 0; i < cells_.size(); ++i) {
            const int r = i / cols;
            const int c = i % cols;
            const QRect cell(c * (cw + kGap), r * (ch + kGap), cw, ch);
            p.setPen(QColor(colors::BORDER_DIM()));
            p.setBrush(Qt::NoBrush);
            p.drawRect(cell.adjusted(0, 0, -1, -1));
            const QRect inner = cell.adjusted(kPad, kPad, -kPad, -kPad);
            int y = inner.top();
            p.setFont(cap_font);
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            p.drawText(QRect(inner.left(), y, inner.width(), cap_fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       cap_fm.elidedText(cells_[i].caption, Qt::ElideRight, inner.width()));
            y += cap_fm.height() + 2;
            p.setFont(val_font);
            p.setPen(QColor(cells_[i].negative ? colors::NEGATIVE() : colors::TEXT_PRIMARY()));
            p.drawText(QRect(inner.left(), y, inner.width(), val_fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       val_fm.elidedText(cells_[i].value, Qt::ElideRight, inner.width()));
            y += val_fm.height() + 2;
            p.setFont(sub_font);
            p.setPen(QColor(colors::TEXT_TERTIARY()));
            p.drawText(QRect(inner.left(), y, inner.width(), sub_fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       sub_fm.elidedText(cells_[i].sub, Qt::ElideRight, inner.width()));
        }
    }

  private:
    static constexpr int kGap = 6;
    static constexpr int kPad = 8;

    int cell_height() const {
        return 2 * kPad + QFontMetrics(fonts::font(-3)).height() * 2 + QFontMetrics(fonts::font(2, QFont::Bold)).height() +
               4;
    }
    int min_cell_width() const {
        const QFontMetrics val_fm(fonts::font(2, QFont::Bold));
        const QFontMetrics cap_fm(fonts::font(-3));
        int w = 0;
        for (const auto& c : cells_)
            w = std::max({w, val_fm.horizontalAdvance(c.value), cap_fm.horizontalAdvance(c.caption)});
        return std::max(w, cap_fm.averageCharWidth() * 16) + 2 * kPad;
    }
    int columns_for(int w) const {
        const int n = std::max(1, int(cells_.size()));
        return std::clamp((w + kGap) / (min_cell_width() + kGap), 1, n);
    }

    QVector<FmtPanelCell> cells_;
};

// ── Choice row ──────────────────────────────────────────────────────────────

/// Exclusive checkable buttons side by side, stacked when the width is too
/// small for them (font 24 at 1280 px).
class FmtPanelChoiceRow : public QWidget {
  public:
    explicit FmtPanelChoiceRow(QWidget* parent = nullptr)
        : QWidget(parent), group_(new QButtonGroup(this)), box_(new QBoxLayout(QBoxLayout::LeftToRight, this)) {
        box_->setContentsMargins(0, 0, 0, 0);
        box_->setSpacing(6);
        box_->addStretch(1);
        group_->setExclusive(true);
    }

    QPushButton* add(int id, const QString& tooltip) {
        auto* b = new QPushButton(this);
        b->setCheckable(true);
        b->setAutoDefault(false);
        b->setObjectName(QStringLiteral("fmtChoice"));
        b->setToolTip(tooltip);
        group_->addButton(b, id);
        box_->insertWidget(box_->count() - 1, b);
        return b;
    }
    QButtonGroup* group() const { return group_; }
    void set_checked(int id) {
        const QSignalBlocker block(group_);
        if (auto* b = group_->button(id); b && !b->isChecked())
            b->setChecked(true);
    }
    void set_text(int id, const QString& text) {
        if (auto* b = group_->button(id); b && b->text() != text) {
            b->setText(text);
            b->setAccessibleName(text);
        }
    }
    void set_accessible_name(const QString& name) {
        setAccessibleName(name);
        for (auto* b : group_->buttons())
            b->setAccessibleDescription(name);
    }

    QSize minimumSizeHint() const override {
        // Stacked, the row needs only its widest button.
        int w = 0;
        int h = 0;
        for (const auto* b : group_->buttons()) {
            w = std::max(w, b->minimumSizeHint().width());
            h += b->sizeHint().height() + box_->spacing();
        }
        return {w, box_->direction() == QBoxLayout::TopToBottom ? h : QWidget::minimumSizeHint().height()};
    }

    void refit() {
        int need = 0;
        for (const auto* b : group_->buttons())
            need += b->sizeHint().width() + box_->spacing();
        const auto dir = width() >= need ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
        if (box_->direction() != dir) {
            box_->setDirection(dir);
            updateGeometry();
        }
    }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (!ThemeManager::instance().notifying()) // during a restyle: refit on appearance_settled
            refit();
    }

  private:
    QButtonGroup* group_;
    QBoxLayout* box_;
};

// ── Per-currency table editor ───────────────────────────────────────────────

class FmtPanelDecimalsDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override {
        auto* s = new QSpinBox(parent);
        s->setRange(-1, 10);
        s->setSpecialValueText(fmtpanel_tr("default"));
        return s;
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        static_cast<QSpinBox*>(editor)->setValue(index.data(Qt::EditRole).toInt());
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        auto* s = static_cast<QSpinBox*>(editor);
        s->interpretText();
        model->setData(index, s->value(), Qt::EditRole);
    }
};

/// The curated number locales of the picker (BCP-47).
const QStringList& fmtpanel_locale_names() {
    static const QStringList k = {
        "en-US", "en-GB", "en-IN", "en-CA", "en-AU", "en-SG", "de-DE", "de-CH", "fr-FR", "fr-CH", "fr-CA",
        "es-ES", "es-MX", "it-IT", "pt-BR", "pt-PT", "nl-NL", "sv-SE", "nb-NO", "da-DK", "fi-FI", "pl-PL",
        "tr-TR", "ru-RU", "ja-JP", "ko-KR", "zh-CN", "zh-HK", "zh-TW", "hi-IN", "id-ID", "vi-VN", "th-TH",
    };
    return k;
}

QString fmtpanel_locale_label(const QLocale& l) {
    return QStringLiteral("%1 (%2) \u2014 %3")
        .arg(QLocale::languageToString(l.language()), QLocale::territoryToString(l.territory()),
             l.toString(1234567.89, 'f', 2));
}

} // namespace fmtpanel_detail

using fmtpanel_detail::FmtPanelChoiceRow;
using fmtpanel_detail::FmtPanelPreviewStrip;

// ── Construction ────────────────────────────────────────────────────────────

FormatSettingsPanel::FormatSettingsPanel(QWidget* parent, Mode mode) : QWidget(parent), mode_(mode) {
    setObjectName(QStringLiteral("formatSettingsPanel"));
    // The one stylesheet of the panel, set once: transparent label backgrounds
    // (the host may paint its own), section and hint colours from the tokens.
    // ThemeManager's restyle pass translates it on a theme / font change.
    setStyleSheet(QStringLiteral("QLabel { background: transparent; }"
                                 "QLabel#fmtSectionTitle { color: %1; font-weight: bold; letter-spacing: 0.5px; }"
                                 "QLabel#fmtRowLabel { color: %2; }"
                                 "QLabel#fmtHint { color: %3; font-size: %4px; }"
                                 "QToolButton#fmtSectionToggle { color: %1; font-weight: bold; border: none;"
                                 " background: transparent; letter-spacing: 0.5px; }"
                                 "QPushButton#fmtChoice:checked { color: %1; border: 1px solid %1; }")
                      .arg(colors::AMBER(), colors::TEXT_SECONDARY(), colors::TEXT_TERTIARY(),
                           fonts::px(-2)));

    root_ = new QVBoxLayout(this);
    root_->setContentsMargins(0, 0, 0, 0);
    root_->setSpacing(10);

    auto* preview_title = new QLabel(tr("PREVIEW"), this);
    preview_title->setObjectName(QStringLiteral("fmtSectionTitle"));
    root_->addWidget(preview_title);
    auto* preview_hint = new QLabel(tr("Sample values \u2014 not live data"), this);
    preview_hint->setObjectName(QStringLiteral("fmtHint"));
    root_->addWidget(preview_hint);
    preview_ = new FmtPanelPreviewStrip(this);
    root_->addWidget(preview_);
    root_->addStretch(1);

    // Settings changes (from this panel, another instance or anywhere else):
    // re-read into the controls on the next turn, or when next shown.
    on_format_changed(this, [this]() { sync_from_settings(); });
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        if (preview_)
            preview_->update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { refit(); });
    refit();
}

QString FormatSettingsPanel::section_title() {
    return tr("Currency & Number Format");
}

QStringList FormatSettingsPanel::search_keywords() {
    return {tr("currency"),      tr("number format"), tr("symbol"),        tr("locale"),     tr("decimal"),
            tr("lakh"),          tr("crore"),         tr("grouping"),      tr("negative"),   tr("parentheses"),
            tr("pence"),         tr("minor unit"),    tr("exchange rate"), tr("FX"),         tr("display currency"),
            tr("thousands separator"), tr("ISO code"), tr("stale")};
}

QWidget* FormatSettingsPanel::build_section(QVBoxLayout* root, const QString& title, QGridLayout** grid) {
    auto* title_label = new QLabel(title, this);
    title_label->setObjectName(QStringLiteral("fmtSectionTitle"));
    root->addSpacing(6);
    root->addWidget(title_label);
    auto* body = new QWidget(this);
    auto* g = new QGridLayout(body);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(16);
    g->setVerticalSpacing(6);
    g->setColumnStretch(1, 1);
    root->addWidget(body);
    grids_.append(g);
    *grid = g;
    return body;
}

QLabel* FormatSettingsPanel::add_row(QGridLayout* grid, const QString& label, QWidget* control, const QString& hint) {
    const int row = grid->rowCount();
    auto* l = new QLabel(label, this);
    l->setObjectName(QStringLiteral("fmtRowLabel"));
    l->setWordWrap(true);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(l, row, 0, Qt::AlignTop);
    grid->addWidget(control, row, 1);
    control->setAccessibleName(label);
    label_texts_ << label;
    row_labels_ << l;
    if (!hint.isEmpty()) {
        auto* h = new QLabel(hint, this);
        h->setObjectName(QStringLiteral("fmtHint"));
        h->setWordWrap(true);
        grid->addWidget(h, row + 1, 1);
        return h;
    }
    return nullptr;
}

namespace {
QComboBox* fmtpanel_combo(QWidget* parent) {
    auto* c = new QComboBox(parent);
    c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    c->setMinimumContentsLength(12);
    return c;
}
} // namespace

void FormatSettingsPanel::build_currency_section(QVBoxLayout* root) {
    QGridLayout* g = nullptr;
    build_section(root, tr("CURRENCY"), &g);
    auto& fs = FormatSettings::instance();

    display_ccy_ = fmtpanel_combo(this);
    display_ccy_->setEditable(true);
    display_ccy_->setInsertPolicy(QComboBox::NoInsert);
    display_ccy_->setMinimumContentsLength(18);
    display_hint_ = add_row(g, tr("Display currency"), display_ccy_,
                            tr("Totals and calculators use this currency. Prices stay in the instrument's own "
                               "currency, and a portfolio keeps its own base currency."));
    connect(display_ccy_, &QComboBox::activated, this, [this](int idx) {
        if (idx >= 0)
            FormatSettings::instance().set_display_currency(display_ccy_->itemData(idx).toString());
    });

    if (mode_ == Mode::Compact) {
        locale_ = fmtpanel_combo(this);
        add_row(g, tr("Number locale"), locale_);
        connect(locale_, &QComboBox::activated, this, [this](int idx) {
            const QString name = locale_->itemData(idx).toString();
            if (name.isEmpty())
                FormatSettings::instance().follow_app_locale();
            else
                FormatSettings::instance().set_locale(QLocale(name));
        });
        return;
    }

    style_row_ = new FmtPanelChoiceRow(this);
    style_row_->add(int(SymbolStyle::Symbol),
                    tr("Local symbols; a symbol that two shown currencies share becomes unambiguous (US$, C$)"));
    style_row_->add(int(SymbolStyle::Code), tr("ISO codes everywhere: USD 1,234.50"));
    style_row_->add(int(SymbolStyle::Disambiguated), tr("Always the unambiguous form: US$, C$, JP\u00A5, CN\u00A5"));
    add_row(g, tr("Symbol style"), style_row_);
    style_row_->set_accessible_name(tr("Symbol style"));
    connect(style_row_->group(), &QButtonGroup::idClicked, this,
            [](int id) { FormatSettings::instance().set_symbol_style(SymbolStyle(id)); });

    position_ = fmtpanel_combo(this);
    position_->addItem(tr("Locale default"), int(SymbolPosition::Locale));
    position_->addItem(tr("Before the amount"), int(SymbolPosition::Before));
    position_->addItem(tr("After the amount"), int(SymbolPosition::After));
    add_row(g, tr("Symbol position"), position_);
    connect(position_, &QComboBox::activated, this, [this](int idx) {
        FormatSettings::instance().set_symbol_position(SymbolPosition(position_->itemData(idx).toInt()));
    });

    spacing_ = fmtpanel_combo(this);
    spacing_->addItem(tr("Automatic"), int(SymbolSpacing::Auto));
    spacing_->addItem(tr("No space"), int(SymbolSpacing::None));
    spacing_->addItem(tr("Space"), int(SymbolSpacing::Space));
    add_row(g, tr("Symbol spacing"), spacing_);
    connect(spacing_, &QComboBox::activated, this, [this](int idx) {
        FormatSettings::instance().set_symbol_spacing(SymbolSpacing(spacing_->itemData(idx).toInt()));
    });

    negative_row_ = new FmtPanelChoiceRow(this);
    negative_row_->add(int(NegativeStyle::Minus), tr("A leading minus sign"));
    negative_row_->add(int(NegativeStyle::Parentheses),
                       tr("Accounting style. Prices, percentages and chart axes keep the minus sign."));
    add_row(g, tr("Negative amounts"), negative_row_);
    negative_row_->set_accessible_name(tr("Negative amounts"));
    connect(negative_row_->group(), &QButtonGroup::idClicked, this,
            [](int id) { FormatSettings::instance().set_negative_style(NegativeStyle(id)); });

    minor_row_ = new FmtPanelChoiceRow(this);
    minor_row_->add(int(MinorUnits::AsQuoted), tr("As the exchange quotes it (London in pence, Johannesburg in cents)"));
    minor_row_->add(int(MinorUnits::Major), tr("Converted to the main unit. Bare price columns stay as quoted."));
    add_row(g, tr("Prices quoted in pence or cents"), minor_row_);
    minor_row_->set_accessible_name(tr("Prices quoted in pence or cents"));
    connect(minor_row_->group(), &QButtonGroup::idClicked, this,
            [](int id) { FormatSettings::instance().set_minor_units(MinorUnits(id)); });

    code_when_mixed_ = new QCheckBox(tr("Show ISO codes"), this);
    add_row(g, tr("Mixed columns"), code_when_mixed_,
            tr("When a table column holds several currencies, print the code (USD, EUR) on every cell."));
    connect(code_when_mixed_, &QCheckBox::toggled, this,
            [](bool on) { FormatSettings::instance().set_code_when_mixed(on); });
    Q_UNUSED(fs);
}

void FormatSettingsPanel::build_numbers_section(QVBoxLayout* root) {
    QGridLayout* g = nullptr;
    build_section(root, tr("NUMBERS"), &g);

    locale_ = fmtpanel_combo(this);
    locale_->setMinimumContentsLength(18);
    add_row(g, tr("Number locale"), locale_);
    connect(locale_, &QComboBox::activated, this, [this](int idx) {
        const QString name = locale_->itemData(idx).toString();
        if (name.isEmpty())
            FormatSettings::instance().follow_app_locale();
        else
            FormatSettings::instance().set_locale(QLocale(name));
    });

    auto* grouping_box = new QWidget(this);
    auto* gh = new QHBoxLayout(grouping_box);
    gh->setContentsMargins(0, 0, 0, 0);
    gh->setSpacing(12);
    grouping_ = fmtpanel_combo(grouping_box);
    grouping_->addItem(tr("Always (from 1,000)"), int(Grouping::Always));
    grouping_->addItem(tr("Locale rule"), int(Grouping::Locale));
    grouping_->addItem(tr("Indian (12,34,567)"), int(Grouping::Indian));
    grouping_->addItem(tr("None"), int(Grouping::None));
    grouping_->setAccessibleName(tr("Digit grouping"));
    grouping_example_ = new QLabel(grouping_box);
    grouping_example_->setObjectName(QStringLiteral("fmtHint"));
    gh->addWidget(grouping_, 1);
    gh->addWidget(grouping_example_);
    add_row(g, tr("Digit grouping"), grouping_box);
    connect(grouping_, &QComboBox::activated, this,
            [this](int idx) { FormatSettings::instance().set_grouping(Grouping(grouping_->itemData(idx).toInt())); });

    auto* decimals_box = new QWidget(this);
    auto* dh = new QHBoxLayout(decimals_box);
    dh->setContentsMargins(0, 0, 0, 0);
    dh->setSpacing(12);
    decimals_policy_ = fmtpanel_combo(decimals_box);
    decimals_policy_->addItem(tr("Currency default"), int(DecimalsPolicy::CurrencyDefault));
    decimals_policy_->addItem(tr("Adaptive for small amounts"), int(DecimalsPolicy::Adaptive));
    decimals_policy_->addItem(tr("Fixed"), int(DecimalsPolicy::Fixed));
    decimals_policy_->setAccessibleName(tr("Decimals for amounts"));
    fixed_decimals_ = new QSpinBox(decimals_box);
    fixed_decimals_->setRange(0, 8);
    fixed_decimals_->setAccessibleName(tr("Fixed number of decimals"));
    dh->addWidget(decimals_policy_, 1);
    dh->addWidget(fixed_decimals_);
    add_row(g, tr("Decimals for amounts"), decimals_box,
            tr("Prices always keep the precision their market quotes."));
    decimals_debounce_ = new QTimer(this);
    decimals_debounce_->setSingleShot(true);
    decimals_debounce_->setInterval(150);
    connect(decimals_debounce_, &QTimer::timeout, this, [this]() { commit_decimals(); });
    connect(decimals_policy_, &QComboBox::activated, this, [this](int) { commit_decimals(); });
    connect(fixed_decimals_, &QSpinBox::valueChanged, this, [this](int) { decimals_debounce_->start(); });
    connect(fixed_decimals_, &QSpinBox::editingFinished, this, [this]() {
        if (decimals_debounce_->isActive())
            commit_decimals();
    });

    compact_ = fmtpanel_combo(this);
    compact_->addItem(tr("Automatic (by currency)"), int(Compact::Auto));
    compact_->addItem(tr("K, M, B, T"), int(Compact::KMB));
    compact_->addItem(tr("Lakh, Crore"), int(Compact::LakhCrore));
    add_row(g, tr("Large numbers"), compact_);
    connect(compact_, &QComboBox::activated, this, [this](int idx) {
        FormatSettings::instance().set_compact_scheme(Compact(compact_->itemData(idx).toInt()));
    });
}

void FormatSettingsPanel::build_fx_section(QVBoxLayout* root) {
    QGridLayout* g = nullptr;
    build_section(root, tr("EXCHANGE RATES"), &g);

    // In a stretchable box: a capped widget alone would cap the whole grid column.
    auto* stale_box = new QWidget(this);
    auto* sh = new QHBoxLayout(stale_box);
    sh->setContentsMargins(0, 0, 0, 0);
    stale_minutes_ = new QSpinBox(stale_box);
    stale_minutes_->setRange(1, 1440);
    stale_minutes_->setSuffix(tr(" min"));
    sh->addWidget(stale_minutes_);
    sh->addStretch(1);
    stale_minutes_->setAccessibleName(tr("Mark a rate as stale after"));
    add_row(g, tr("Mark a rate as stale after"), stale_box,
            tr("Source: Yahoo Finance; crosses are computed through USD. A figure converted at a stale rate is "
               "marked \u2248 and keeps its last rate; after 7 days without a rate it is left out."));
    stale_debounce_ = new QTimer(this);
    stale_debounce_->setSingleShot(true);
    stale_debounce_->setInterval(400);
    connect(stale_debounce_, &QTimer::timeout, this, [this]() { commit_stale_minutes(); });
    connect(stale_minutes_, &QSpinBox::valueChanged, this, [this](int) { stale_debounce_->start(); });
    connect(stale_minutes_, &QSpinBox::editingFinished, this, [this]() {
        if (stale_debounce_->isActive())
            commit_stale_minutes();
    });

    fx_pnl_split_ = new QCheckBox(tr("Split price and FX P&&L"), this);
    add_row(g, tr("Foreign-currency holdings"), fx_pnl_split_,
            tr("When a holding's currency differs from its portfolio's base currency, split its gain or loss "
               "into the part from the price and the part from the exchange rate."));
    connect(fx_pnl_split_, &QCheckBox::toggled, this, [](bool on) { FormatSettings::instance().set_fx_pnl_split(on); });
}

void FormatSettingsPanel::build_overrides_section(QVBoxLayout* root) {
    root->addSpacing(6);
    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    overrides_toggle_ = new QToolButton(this);
    overrides_toggle_->setObjectName(QStringLiteral("fmtSectionToggle"));
    overrides_toggle_->setText(tr("PER-CURRENCY SYMBOLS AND DECIMALS"));
    overrides_toggle_->setCheckable(true);
    overrides_toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    overrides_toggle_->setArrowType(Qt::RightArrow);
    overrides_toggle_->setAccessibleName(tr("Per-currency symbols and decimals"));
    overrides_summary_ = new QLabel(this);
    overrides_summary_->setObjectName(QStringLiteral("fmtHint"));
    header->addWidget(overrides_toggle_);
    header->addWidget(overrides_summary_);
    header->addStretch(1);
    root->addLayout(header);
    overrides_body_ = new QWidget(this);
    overrides_body_->setVisible(false);
    root->addWidget(overrides_body_);
    connect(overrides_toggle_, &QToolButton::toggled, this, [this](bool open) {
        overrides_toggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        if (open)
            ensure_override_table();
        overrides_body_->setVisible(open);
    });
}

void FormatSettingsPanel::ensure_override_table() {
    if (overrides_view_)
        return;
    auto* v = new QVBoxLayout(overrides_body_);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto* tools = new QHBoxLayout;
    overrides_filter_ = new QLineEdit(overrides_body_);
    overrides_filter_->setPlaceholderText(tr("Filter by code or name"));
    overrides_filter_->setClearButtonEnabled(true);
    overrides_filter_->setAccessibleName(tr("Filter currencies"));
    auto* add = new QPushButton(tr("Add custom code\u2026"), overrides_body_);
    add->setAutoDefault(false);
    auto* reset_all = new QPushButton(tr("Reset all"), overrides_body_);
    reset_all->setAutoDefault(false);
    reset_all->setToolTip(tr("Remove every per-currency symbol, decimals and custom code"));
    tools->addWidget(overrides_filter_, 1);
    tools->addWidget(add);
    tools->addWidget(reset_all);
    v->addLayout(tools);

    overrides_model_ = new CurrencyOverrideModel(this);
    overrides_proxy_ = new QSortFilterProxyModel(this);
    overrides_proxy_->setSourceModel(overrides_model_);
    overrides_proxy_->setSortRole(CurrencyOverrideModel::SortRole);
    overrides_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    overrides_proxy_->setFilterKeyColumn(-1);
    overrides_view_ = new QTableView(overrides_body_);
    overrides_view_->setModel(overrides_proxy_);
    overrides_view_->setItemDelegateForColumn(CurrencyOverrideModel::ColDecimals,
                                              new fmtpanel_detail::FmtPanelDecimalsDelegate(overrides_view_));
    overrides_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    overrides_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    overrides_view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                     QAbstractItemView::SelectedClicked);
    overrides_view_->verticalHeader()->hide();
    overrides_view_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    overrides_view_->horizontalHeader()->setSectionResizeMode(CurrencyOverrideModel::ColName, QHeaderView::Stretch);
    overrides_view_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    overrides_view_->setSortingEnabled(true);
    overrides_view_->setWordWrap(false);
    overrides_view_->setAccessibleName(tr("Per-currency symbols and decimals"));
    v->addWidget(overrides_view_);
    connect(overrides_filter_, &QLineEdit::textChanged, overrides_proxy_, &QSortFilterProxyModel::setFilterFixedString);
    connect(add, &QPushButton::clicked, this, &FormatSettingsPanel::add_custom_code);
    connect(reset_all, &QPushButton::clicked, this, []() {
        auto v2 = FormatSettings::instance().values();
        v2.overrides.clear();
        FormatSettings::instance().set_values(v2); // one change for the whole reset
    });
    connect(overrides_view_, &QTableView::clicked, this, [this](const QModelIndex& index) {
        if (index.column() != CurrencyOverrideModel::ColReset)
            return;
        const QModelIndex src = overrides_proxy_->mapToSource(index);
        if (src.data(CurrencyOverrideModel::OverriddenRole).toBool())
            overrides_model_->reset_row(src.row());
    });
    refit();
}

void FormatSettingsPanel::add_custom_code() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add custom currency code"));
    auto* form = new QFormLayout(&dlg);
    auto* code = new QLineEdit(&dlg);
    code->setMaxLength(8);
    code->setPlaceholderText(tr("3 to 8 letters or digits, e.g. XNO"));
    auto* name = new QLineEdit(&dlg);
    name->setMaxLength(64);
    auto* symbol = new QLineEdit(&dlg);
    symbol->setMaxLength(8);
    auto* decimals = new QSpinBox(&dlg);
    decimals->setRange(0, 10);
    decimals->setValue(2);
    auto* error = new QLabel(&dlg);
    error->setWordWrap(true);
    error->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(colors::NEGATIVE()));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(tr("Code"), code);
    form->addRow(tr("Name"), name);
    form->addRow(tr("Symbol"), symbol);
    form->addRow(tr("Decimals"), decimals);
    form->addRow(error);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&]() {
        const QString c = code->text().trimmed();
        if (is_known_currency(c)) {
            error->setText(tr("%1 is already in the table: change its row instead.").arg(c.toUpper()));
            return;
        }
        CurrencyOverride o;
        o.symbol = symbol->text();
        o.decimals = decimals->value();
        o.name = name->text();
        if (!FormatSettings::instance().set_currency_override(c, o)) {
            error->setText(tr("Use 3 to 8 letters or digits starting with a letter, and a symbol of up to 8 "
                              "characters without digits or %."));
            return;
        }
        dlg.accept();
    });
    dlg.exec();
}

// ── First show ──────────────────────────────────────────────────────────────

void FormatSettingsPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!built_)
        ensure_built(); // before the first paint, so the controls never flash in empty
}

void FormatSettingsPanel::ensure_built() {
    if (built_)
        return;
    built_ = true;
    body_ = new QWidget(this);
    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(10);
    build_currency_section(bl);
    if (mode_ == Mode::Full) {
        build_numbers_section(bl);
        build_fx_section(bl);
        build_overrides_section(bl);
        auto* reset_row = new QHBoxLayout;
        reset_ = new QPushButton(tr("Reset to defaults"), body_);
        reset_->setAutoDefault(false);
        reset_->setAccessibleName(tr("Reset currency and number format settings to defaults"));
        connect(reset_, &QPushButton::clicked, this, [this]() {
            if (QMessageBox::question(this, tr("Reset to defaults"),
                                      tr("Reset every currency and number format setting, including your "
                                         "per-currency symbols, to the defaults?")) == QMessageBox::Yes)
                reset_settings();
        });
        reset_row->addWidget(reset_);
        reset_row->addStretch(1);
        bl->addLayout(reset_row);
    }
    fill_display_currency_combo();
    fill_locale_combo();
    if (auto* completer = display_ccy_->completer()) {
        completer->setCompletionMode(QCompleter::PopupCompletion);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
    }
    root_->insertWidget(root_->count() - 1, body_); // above the closing stretch
    refit();
    sync_from_settings();
    body_->show();
}

void FormatSettingsPanel::fill_display_currency_combo() {
    const QSignalBlocker block(display_ccy_);
    display_ccy_->clear();
    display_ccy_->addItem(QString(), QString()); // Automatic; text set by sync_from_settings()
    QStringList fiat;
    QStringList crypto;
    for (const QString& code : known_currency_codes()) {
        CurrencySpec spec;
        if (!find_currency(code, &spec) || spec.minor_divisor != 1 || spec.metal)
            continue;
        (spec.crypto ? crypto : fiat) << code;
    }
    std::sort(fiat.begin(), fiat.end());
    auto add = [this](const QString& code) {
        CurrencySpec spec;
        effective_currency(code, &spec);
        display_ccy_->addItem(QStringLiteral("%1 \u2014 %2 (%3)").arg(code, spec.name, spec.symbol), code);
    };
    for (const QString& c : std::as_const(fiat))
        add(c);
    display_ccy_->insertSeparator(display_ccy_->count());
    for (const QString& c : std::as_const(crypto))
        add(c);
    custom_codes_in_combo_.clear();
    const auto ov = FormatSettings::instance().currency_overrides();
    for (auto it = ov.constBegin(); it != ov.constEnd(); ++it)
        if (!is_known_currency(it.key()))
            custom_codes_in_combo_ << it.key();
    std::sort(custom_codes_in_combo_.begin(), custom_codes_in_combo_.end());
    if (!custom_codes_in_combo_.isEmpty()) {
        display_ccy_->insertSeparator(display_ccy_->count());
        for (const QString& c : std::as_const(custom_codes_in_combo_))
            add(c);
    }
}

void FormatSettingsPanel::fill_locale_combo() {
    const QSignalBlocker block(locale_);
    locale_->clear();
    locale_->addItem(QString(), QString()); // "Same as language"; text set by sync_from_settings()
    for (const QString& name : fmtpanel_detail::fmtpanel_locale_names())
        locale_->addItem(fmtpanel_detail::fmtpanel_locale_label(QLocale(name)), QLocale(name).bcp47Name());
}

// ── Sync ────────────────────────────────────────────────────────────────────

void FormatSettingsPanel::sync_from_settings() {
    if (preview_)
        preview_->refresh();
    if (!built_)
        return;
    auto& fs = FormatSettings::instance();
    const FormatSettings::Values v = fs.values();

    // Display currency (custom codes may have come or gone).
    {
        QStringList customs;
        for (auto it = v.overrides.constBegin(); it != v.overrides.constEnd(); ++it)
            if (!is_known_currency(it.key()))
                customs << it.key();
        std::sort(customs.begin(), customs.end());
        if (customs != custom_codes_in_combo_)
            fill_display_currency_combo();
        const QSignalBlocker block(display_ccy_);
        const QString auto_code = fs.auto_display_currency();
        display_ccy_->setItemText(0, tr("Automatic \u2014 %1 (%2)").arg(auto_code, currency_name(auto_code)));
        int idx = v.display_currency.isEmpty() ? 0 : display_ccy_->findData(v.display_currency);
        if (idx < 0)
            idx = 0;
        display_ccy_->setCurrentIndex(idx);
        if (display_ccy_->lineEdit() && !display_ccy_->lineEdit()->hasFocus())
            display_ccy_->lineEdit()->setText(display_ccy_->itemText(idx));
    }

    // Number locale.
    {
        const QSignalBlocker block(locale_);
        locale_->setItemText(0, tr("Same as language \u2014 %1").arg(fmtpanel_detail::fmtpanel_locale_label(QLocale())));
        int idx = 0;
        if (!v.locale_follows_app) {
            const QString name = v.pinned_locale.bcp47Name();
            idx = locale_->findData(name);
            if (idx < 0) {
                locale_->addItem(fmtpanel_detail::fmtpanel_locale_label(v.pinned_locale), name);
                idx = locale_->count() - 1;
            }
        }
        locale_->setCurrentIndex(idx);
    }

    if (mode_ == Mode::Compact)
        return;

    // Symbol style buttons show the display currency's three forms.
    CurrencySpec disp;
    if (!find_currency(fs.display_currency(), &disp)) {
        disp.code = disp.symbol = disp.plain = disp.strict = fs.display_currency();
    }
    style_row_->set_text(int(SymbolStyle::Symbol), tr("%1  Symbol").arg(disp.plain));
    style_row_->set_text(int(SymbolStyle::Code), tr("%1  Code").arg(disp.code));
    style_row_->set_text(int(SymbolStyle::Disambiguated), tr("%1  Disambiguated").arg(disp.strict));
    style_row_->set_checked(int(v.symbol_style));

    auto set_combo = [](QComboBox* c, int data) {
        const QSignalBlocker block(c);
        const int idx = c->findData(data);
        if (idx >= 0 && c->currentIndex() != idx)
            c->setCurrentIndex(idx);
    };
    set_combo(position_, int(v.symbol_position));
    set_combo(spacing_, int(v.symbol_spacing));

    negative_row_->set_text(int(NegativeStyle::Minus), number(-1234.5));
    negative_row_->set_text(int(NegativeStyle::Parentheses), QLatin1Char('(') + number(1234.5) + QLatin1Char(')'));
    negative_row_->set_checked(int(v.negative_style));

    minor_row_->set_text(int(MinorUnits::AsQuoted), tr("As quoted  %1").arg(number(117.7) + QStringLiteral("p")));
    minor_row_->set_text(int(MinorUnits::Major), tr("Main unit  %1").arg(QStringLiteral("\u00A3") + number(1.177, 4)));
    minor_row_->set_checked(int(v.minor_units));

    {
        const QSignalBlocker block(code_when_mixed_);
        code_when_mixed_->setChecked(v.code_when_mixed);
    }

    set_combo(grouping_, int(v.grouping));
    grouping_example_->setText(number(1234567.89));
    set_combo(decimals_policy_, int(v.decimals_policy));
    {
        const QSignalBlocker block(fixed_decimals_);
        if (!fixed_decimals_->hasFocus() && !decimals_debounce_->isActive())
            fixed_decimals_->setValue(v.fixed_decimals);
        fixed_decimals_->setEnabled(v.decimals_policy == DecimalsPolicy::Fixed);
    }
    set_combo(compact_, int(v.compact));

    {
        const QSignalBlocker block(stale_minutes_);
        if (!stale_minutes_->hasFocus() && !stale_debounce_->isActive())
            stale_minutes_->setValue(v.fx_stale_minutes);
    }
    {
        const QSignalBlocker block(fx_pnl_split_);
        fx_pnl_split_->setChecked(v.fx_pnl_split);
    }

    const int n = int(v.overrides.size());
    overrides_summary_->setText(n == 0 ? tr("none") : tr("%n override(s)", nullptr, n));
}

void FormatSettingsPanel::commit_decimals() {
    decimals_debounce_->stop();
    const auto policy = DecimalsPolicy(decimals_policy_->currentData().toInt());
    FormatSettings::instance().set_money_decimals(policy, fixed_decimals_->value());
}

void FormatSettingsPanel::commit_stale_minutes() {
    stale_debounce_->stop();
    FormatSettings::instance().set_fx_stale_minutes(stale_minutes_->value());
}

void FormatSettingsPanel::reset_settings() {
    auto& fs = FormatSettings::instance();
    const FormatSettings::Values cur = fs.values();
    FormatSettings::Values v; // every default ...
    v.tz_mode = cur.tz_mode;  // ... except the time zone and clock, which this panel does not show
    v.fixed_tz = cur.fixed_tz;
    v.hour_cycle = cur.hour_cycle;
    fs.set_values(v);
}

// ── Fit ─────────────────────────────────────────────────────────────────────

void FormatSettingsPanel::refit() {
    const QFontMetrics fm(fonts::font());
    int w = 0;
    for (const QString& t : std::as_const(label_texts_))
        w = std::max(w, fm.horizontalAdvance(t));
    // Labels wrap beyond ~26 characters, so a large font does not squeeze the controls.
    w = std::min(w + 12, fm.averageCharWidth() * 26);
    for (QGridLayout* g : std::as_const(grids_))
        g->setColumnMinimumWidth(0, w);
    if (preview_)
        preview_->refit();
    for (FmtPanelChoiceRow* r : {style_row_, negative_row_, minor_row_})
        if (r)
            r->refit();
    if (overrides_view_) {
        const QFontMetrics vfm(overrides_view_->font());
        const int row_h = vfm.height() + 8;
        overrides_view_->verticalHeader()->setDefaultSectionSize(row_h);
        auto* hh = overrides_view_->horizontalHeader();
        auto fit = [&](int col, std::initializer_list<QString> samples) {
            int w = 0;
            for (const QString& s : samples)
                w = std::max(w, vfm.horizontalAdvance(s));
            hh->resizeSection(col, w + 24);
        };
        fit(CurrencyOverrideModel::ColCode, {QStringLiteral("FDUSD"), tr("Code")});
        fit(CurrencyOverrideModel::ColSymbol, {QStringLiteral("MOP$$"), tr("Symbol")});
        fit(CurrencyOverrideModel::ColDecimals, {tr("default (%1)").arg(2), tr("Decimals")});
        fit(CurrencyOverrideModel::ColExample, {QStringLiteral("USDT 1,234.50"), tr("Example")});
        fit(CurrencyOverrideModel::ColReset, {tr("Remove"), tr("Reset")});
        overrides_view_->setMinimumHeight(row_h * 10 + hh->sizeHint().height() + 4);
    }
}

} // namespace fincept::ui::formatting
