#include "screens/crypto_center/WalletActionConfirmDialog.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

constexpr int kArmTickMs = 100;       // 10 Hz countdown tick — smooth-enough
constexpr int kMinDialogWidth = 480;
constexpr int kRowMinHeight = 28;

QString fontStack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_remaining(int ms) {
    // 1500 → "1.5s", 800 → "0.8s", <=0 → ""
    if (ms <= 0) return QString();
    const double secs = ms / 1000.0;
    return QStringLiteral("%1s").arg(secs, 0, 'f', 1);
}

} // namespace

WalletActionConfirmDialog::WalletActionConfirmDialog(WalletActionSummary summary,
                                                     QWidget* parent)
    : QDialog(parent), summary_(std::move(summary)) {
    setObjectName(QStringLiteral("walletActionConfirmDialog"));
    setModal(true);
    setWindowTitle(summary_.title.isEmpty() ? tr("Confirm") : summary_.title);
    setMinimumWidth(kMinDialogWidth);

    arm_timer_ = new QTimer(this);
    arm_timer_->setInterval(kArmTickMs);
    connect(arm_timer_, &QTimer::timeout, this,
            &WalletActionConfirmDialog::on_arm_tick);

    build_ui();
    apply_theme();
}

WalletActionConfirmDialog::~WalletActionConfirmDialog() = default;

bool WalletActionConfirmDialog::prompt() {
    return exec() == QDialog::Accepted;
}

void WalletActionConfirmDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar — flat block colour, mirrors cryptoCenterPanelHead.
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("walletActionDialogHeader"));
    header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(0);
    title_label_ = new QLabel(summary_.title, header);
    title_label_->setObjectName(QStringLiteral("walletActionDialogTitle"));
    hl->addWidget(title_label_);
    hl->addStretch();
    auto* head_status = new QLabel(QStringLiteral("AWAITING CONFIRMATION"), header);
    head_status->setObjectName(QStringLiteral("walletActionDialogHeadStatus"));
    hl->addWidget(head_status);
    root->addWidget(header);

    // Body wrapper
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("walletActionDialogBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(18, 14, 18, 14);
    bl->setSpacing(10);

    if (!summary_.lede.isEmpty()) {
        lede_label_ = new QLabel(summary_.lede, body);
        lede_label_->setObjectName(QStringLiteral("walletActionDialogLede"));
        lede_label_->setWordWrap(true);
        bl->addWidget(lede_label_);
    }

    // Rows
    if (!summary_.rows.isEmpty()) {
        auto* rows_block = new QFrame(body);
        rows_block->setObjectName(QStringLiteral("walletActionDialogRowsBlock"));
        rows_layout_ = new QVBoxLayout(rows_block);
        rows_layout_->setContentsMargins(0, 0, 0, 0);
        rows_layout_->setSpacing(0);
        for (int i = 0; i < summary_.rows.size(); ++i) {
            const auto& r = summary_.rows.at(i);
            const bool last = (i + 1 == summary_.rows.size());

            auto* row_widget = new QWidget(rows_block);
            row_widget->setObjectName(QStringLiteral("walletActionDialogRow"));
            row_widget->setProperty("isLast", last);
            row_widget->setMinimumHeight(kRowMinHeight);
            auto* rl = new QHBoxLayout(row_widget);
            rl->setContentsMargins(12, 6, 12, 6);
            rl->setSpacing(8);
            auto* label = new QLabel(r.label, row_widget);
            label->setObjectName(QStringLiteral("walletActionDialogRowLabel"));
            auto* value = new QLabel(r.value, row_widget);
            value->setObjectName(r.monospace
                                     ? QStringLiteral("walletActionDialogRowValueMono")
                                     : QStringLiteral("walletActionDialogRowValue"));
            value->setWordWrap(!r.monospace);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            rl->addWidget(label);
            rl->addStretch(1);
            rl->addWidget(value);
            rows_layout_->addWidget(row_widget);
        }
        bl->addWidget(rows_block);
    }

    // Warnings
    if (!summary_.warnings.isEmpty()) {
        auto* warn_block = new QFrame(body);
        warn_block->setObjectName(QStringLiteral("walletActionDialogWarnBlock"));
        warnings_layout_ = new QVBoxLayout(warn_block);
        warnings_layout_->setContentsMargins(10, 8, 10, 8);
        warnings_layout_->setSpacing(4);
        for (const auto& w : summary_.warnings) {
            auto* line = new QLabel(QStringLiteral("· ") + w, warn_block);
            line->setObjectName(QStringLiteral("walletActionDialogWarnText"));
            line->setWordWrap(true);
            warnings_layout_->addWidget(line);
        }
        bl->addWidget(warn_block);
    }

    bl->addStretch(1);
    root->addWidget(body, 1);

    // Footer with buttons
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("walletActionDialogFooter"));
    footer->setFixedHeight(50);
    auto* fl = new QHBoxLayout(footer);
    fl->setContentsMargins(14, 8, 14, 8);
    fl->setSpacing(8);

    cancel_button_ = new QPushButton(tr("CANCEL"), footer);
    cancel_button_->setObjectName(QStringLiteral("walletActionDialogCancel"));
    cancel_button_->setFixedHeight(30);
    cancel_button_->setCursor(Qt::PointingHandCursor);

    primary_button_ = new QPushButton(summary_.primary_button_text, footer);
    primary_button_->setObjectName(
        summary_.primary_is_safe ? QStringLiteral("walletActionDialogPrimary")
                                 : QStringLiteral("walletActionDialogPrimaryDanger"));
    primary_button_->setFixedHeight(30);
    primary_button_->setCursor(Qt::PointingHandCursor);
    primary_button_->setEnabled(false); // armed by timer

    fl->addStretch(1);
    fl->addWidget(cancel_button_);
    fl->addWidget(primary_button_);
    root->addWidget(footer);

    connect(cancel_button_, &QPushButton::clicked, this, [this]() {
        emit cancelled();
        reject();
    });
    connect(primary_button_, &QPushButton::clicked, this, [this]() {
        emit confirmed();
        accept();
    });
}

void WalletActionConfirmDialog::apply_theme() {
    using namespace ui::colors;

    const QString font = fontStack();

    const QString ss = QStringLiteral(
        // Root
        "QDialog#walletActionConfirmDialog { background:%1; }"

        // Header
        "QWidget#walletActionDialogHeader { background:%2; border-bottom:1px solid %3; }"
        "QLabel#walletActionDialogTitle { color:%4; font-family:%5; font-size:12px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#walletActionDialogHeadStatus { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"

        // Body
        "QWidget#walletActionDialogBody { background:%1; }"
        "QLabel#walletActionDialogLede { color:%7; font-family:%5; font-size:12px;"
        "  background:transparent; }"

        // Row block
        "QFrame#walletActionDialogRowsBlock { background:%8; border:1px solid %3; }"
        "QWidget#walletActionDialogRow { background:transparent;"
        "  border-bottom:1px solid %3; }"
        "QWidget#walletActionDialogRow[isLast=\"true\"] { border-bottom:none; }"
        "QLabel#walletActionDialogRowLabel { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#walletActionDialogRowValue { color:%9; font-family:%5; font-size:12px;"
        "  background:transparent; }"
        "QLabel#walletActionDialogRowValueMono { color:%9; font-family:%5; font-size:12px;"
        "  background:transparent; }"

        // Warnings
        "QFrame#walletActionDialogWarnBlock { background:rgba(220,38,38,0.10);"
        "  border:1px solid %10; }"
        "QLabel#walletActionDialogWarnText { color:%10; font-family:%5; font-size:11px;"
        "  background:transparent; }"

        // Footer
        "QWidget#walletActionDialogFooter { background:%2; border-top:1px solid %3; }"

        // Cancel button
        "QPushButton#walletActionDialogCancel { background:%8; color:%7; border:1px solid %3;"
        "  font-family:%5; font-size:11px; font-weight:700; letter-spacing:1px; padding:0 14px; }"
        "QPushButton#walletActionDialogCancel:hover { background:%11; color:%9;"
        "  border-color:%12; }"

        // Primary (safe — amber)
        "QPushButton#walletActionDialogPrimary { background:rgba(217,119,6,0.10); color:%4;"
        "  border:1px solid %13; font-family:%5; font-size:11px; font-weight:700;"
        "  letter-spacing:1.2px; padding:0 16px; }"
        "QPushButton#walletActionDialogPrimary:hover { background:%4; color:%1; }"
        "QPushButton#walletActionDialogPrimary:disabled { background:%8; color:%6;"
        "  border-color:%3; }"

        // Primary (danger — red)
        "QPushButton#walletActionDialogPrimaryDanger { background:rgba(220,38,38,0.10); color:%10;"
        "  border:1px solid %14; font-family:%5; font-size:11px; font-weight:700;"
        "  letter-spacing:1.2px; padding:0 16px; }"
        "QPushButton#walletActionDialogPrimaryDanger:hover { background:%10; color:%9; }"
        "QPushButton#walletActionDialogPrimaryDanger:disabled { background:%8; color:%6;"
        "  border-color:%3; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2
             BORDER_DIM(),      // %3
             AMBER(),           // %4
             font,              // %5
             TEXT_TERTIARY(),   // %6
             TEXT_SECONDARY(),  // %7
             BG_RAISED(),       // %8
             TEXT_PRIMARY())    // %9
        .arg(NEGATIVE(),                       // %10
             BG_HOVER(),                       // %11
             BORDER_BRIGHT(),                  // %12
             QStringLiteral("#78350f"),        // %13 darker amber border
             QStringLiteral("#7f1d1d"));       // %14 darker red border

    setStyleSheet(ss);
}

void WalletActionConfirmDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    arm_remaining_ms_ = summary_.arm_delay_ms;
    if (arm_remaining_ms_ <= 0) {
        primary_button_->setEnabled(true);
        primary_button_->setText(summary_.primary_button_text);
        return;
    }
    primary_button_->setEnabled(false);
    primary_button_->setText(QStringLiteral("%1 in %2")
                                 .arg(summary_.primary_button_text,
                                      format_remaining(arm_remaining_ms_)));
    arm_timer_->start();
}

void WalletActionConfirmDialog::on_arm_tick() {
    arm_remaining_ms_ -= kArmTickMs;
    if (arm_remaining_ms_ <= 0) {
        arm_timer_->stop();
        primary_button_->setEnabled(true);
        primary_button_->setText(summary_.primary_button_text);
        return;
    }
    primary_button_->setText(QStringLiteral("%1 in %2")
                                 .arg(summary_.primary_button_text,
                                      format_remaining(arm_remaining_ms_)));
}

} // namespace fincept::screens
