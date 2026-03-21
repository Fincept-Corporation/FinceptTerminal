#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>

namespace fincept::screens {

/// Right-side market pulse panel — Fear/Greed, breadth, top movers, global snapshot, market hours.
class MarketPulsePanel : public QWidget {
    Q_OBJECT
public:
    explicit MarketPulsePanel(QWidget* parent = nullptr);

private:
    QWidget* build_header();
    QWidget* build_fear_greed_section();
    QWidget* build_breadth_section();
    QWidget* build_gainers_section();
    QWidget* build_losers_section();
    QWidget* build_global_snapshot_section();
    QWidget* build_market_hours_section();

    QWidget* build_section_header(const QString& title, const QString& icon_char, const QString& color);
    QWidget* build_mover_row(const QString& symbol, double change, const QString& volume);
    QWidget* build_stat_row(const QString& label, const QString& value, const QString& change, const QString& color);
    QWidget* build_breadth_bar(const QString& label, int advancing, int declining);

    static QString market_status(const QString& region);
};

} // namespace fincept::screens
