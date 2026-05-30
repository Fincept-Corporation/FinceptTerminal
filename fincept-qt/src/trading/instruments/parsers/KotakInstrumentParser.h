#pragma once
// Kotak Neo instrument source. Authenticated: reads the master file-paths API
// (Authorization + base_url unpacked from the account's :::-packed access_token),
// then downloads the per-segment CSVs. Strike/tick are paise (÷100); F&O expiry
// is a unix epoch with a +315513000 offset for NSE_FO/CDE_FO only.
namespace fincept::trading {
void register_kotak_source();
}
