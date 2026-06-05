#pragma once
namespace fincept::feeds {
/// Runs FeedScraper over canned RSS/Atom/JSON/CSV blobs and prints PASS/FAIL
/// counts to stdout. Returns process exit code (0 = all pass). No GUI/network.
int run_feed_selftest();
} // namespace fincept::feeds
