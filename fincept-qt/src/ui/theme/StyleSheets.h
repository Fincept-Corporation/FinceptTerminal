#pragma once
#include <QString>

namespace fincept::ui::styles {

/// Pre-built stylesheets for common widget patterns.
QString card_frame();
QString card_title();
QString card_close_button();
QString section_header();
QString data_label();
QString data_value();
QString accent_button();
QString muted_button();

/// News screen global styles (applied once at NewsScreen level, cascades to children).
QString news_screen_styles();

/// Crypto trading screen global styles (applied once at CryptoTradingScreen level).
QString crypto_trading_styles();

/// Equity trading screen global styles (applied once at EquityTradingScreen level).
QString equity_trading_styles();

} // namespace fincept::ui::styles
