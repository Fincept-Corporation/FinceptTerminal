// InstrumentResolverIndex.cpp — the InstrumentIndex half of the resolver's sources
// (see InstrumentResolver.h, index_resolver_sources()). Kept apart from the network /
// Python sources so the resolver's unit test can run against the real index without
// linking HttpClient or PythonRunner.

#include "services/instruments/InstrumentIndex.h"
#include "services/instruments/InstrumentResolver.h"

namespace fincept::services {

ResolverSources index_resolver_sources() {
    ResolverSources s;
    s.local_search = [](const ResolverSources::LocalQuery& q, QObject* context,
                        ResolverSources::LocalSearchDone done) -> quint64 {
        InstrumentIndex::SearchRequest req;
        req.parsed = q.parsed; // text stays empty: the index uses the parsed input as given
        req.venue_hint = q.venue_hint;
        req.ctx = q.ctx;
        req.limit = q.limit;
        req.channel = q.channel;
        return InstrumentIndex::instance().search(
            req, context, [done = std::move(done)](const InstrumentIndex::SearchResult& r) { done(r.hits, r.error); });
    };
    s.local_cancel = [](quint64 id) { InstrumentIndex::instance().cancel(id); };
    s.local_lookup = [](const QString& key, ResolverSources::LookupMode mode, QObject* context,
                        ResolverSources::LookupDone done) {
        InstrumentIndex::LookupRequest req;
        req.kind = mode == ResolverSources::LookupMode::Isin ? InstrumentIndex::LookupKind::Isin
                                                             : InstrumentIndex::LookupKind::Current;
        req.key = key;
        InstrumentIndex::instance().lookup(
            req, context,
            [done = std::move(done)](const InstrumentIndex::LookupResult& r) { done(r.found, r.error); });
    };
    s.learn = [](const core::instrument::Instrument& inst) { InstrumentIndex::instance().learn(inst); };
    return s;
}

} // namespace fincept::services
