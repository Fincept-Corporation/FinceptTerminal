#pragma once
// FormatSettingsStore — persists FormatSettings through SettingsRepository,
// the same way LanguageManager / the appearance settings do (keys
// "appearance.*", category "appearance", so cloud settings sync carries them
// too). Kept apart from FormatSettings so the formatters stay Qt Core only.

namespace fincept::ui::formatting {

/// Reads the persisted preferences into FormatSettings::instance(), then keeps
/// the repository in sync: every later change writes the keys whose value
/// changed. Keys that failed to read are never written back unless the user
/// changes them, so a transient DB error cannot overwrite a saved choice. Also
/// re-emits FormatSettings::changed() when the UI language (and with it
/// QLocale()) changes.
///
/// Display currency:
///   * first run of this version: the old Settings > General choice
///     ("general.currency") becomes the explicit display currency when it was
///     not the old default (USD); otherwise Automatic. Done once (marker key
///     "appearance.currency_settings_migrated"); "general.currency" is left as
///     it was and no longer written.
///   * Automatic resolves to the user's chosen region's currency, else the
///     system locale's, else the inferred region's, else USD — and follows
///     MarketProfileStore::profile_changed.
///
/// Call once at startup, after LanguageManager::initialize(); later calls do nothing.
void load_format_settings();

} // namespace fincept::ui::formatting
