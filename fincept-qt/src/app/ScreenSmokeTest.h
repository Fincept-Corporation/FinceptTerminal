#pragma once

namespace fincept {

class DockScreenRouter;

// Screen-construction smoke test. Walks every screen registered with `router`,
// navigating to each so its factory runs AND showEvent fires — which is where
// the fragile native runtimes actually get instantiated (Qt WebEngine views,
// Qt Charts, QGeoView map tiles, multimedia). This catches the class of failure
// that static DLL-dependency walking (dumpbin /dependents) can NEVER see:
// missing runtime DATA files and helper executables — e.g. QtWebEngineProcess
// .exe / icudtl.dat / *.pak — that abort the process only when a screen opens.
// It is meant to run on a clean machine (Qt/VS stripped from PATH) so the app
// can use only its bundled runtime, reproducing an end user's environment.
//
// Each screen id is written to stderr (flushed) BEFORE construction, so if a
// screen hard-aborts the process the culprit's id is the last line in the log.
//
// Returns 0 if every screen constructed, 1 if any failed to construct (or a
// non-zero process exit on a hard crash, which is itself the CI signal), and 2
// on harness error (null router).
int run_screen_smoke_test(DockScreenRouter* router);

} // namespace fincept
