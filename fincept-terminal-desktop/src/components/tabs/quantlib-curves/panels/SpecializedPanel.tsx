import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { inflationApi, multicurveApi, crossCurrencyApi } from '../quantlibCurvesApi';

export default function SpecializedPanel() {
  // Inflation Build
  const ib = useEndpoint();
  const [ibRef, setIbRef] = useState('2025-01-15');
  const [ibCpi, setIbCpi] = useState('300');
  const [ibT, setIbT] = useState('1, 2, 3, 5, 10');
  const [ibR, setIbR] = useState('0.03, 0.028, 0.026, 0.025, 0.024');

  // Inflation Bootstrap
  const ibs = useEndpoint();
  const [ibsRef, setIbsRef] = useState('2025-01-15');
  const [ibsCpi, setIbsCpi] = useState('300');
  const [ibsST, setIbsST] = useState('1, 2, 3, 5, 10');
  const [ibsSR, setIbsSR] = useState('0.03, 0.028, 0.026, 0.025, 0.024');
  const [ibsNT, setIbsNT] = useState('1, 2, 3, 5, 10');
  const [ibsNR, setIbsNR] = useState('0.04, 0.042, 0.045, 0.048, 0.054');

  // Inflation Seasonality
  const iseas = useEndpoint();
  const [isF, setIsF] = useState('1.01, 0.99, 1.005, 1.008, 1.01, 1.003, 0.995, 0.998, 1.002, 1.004, 1.006, 1.001');
  const [isC, setIsC] = useState('300, 303, 300, 301.5, 303.4, 306.4, 307.3, 305.4, 304.8, 305.4, 306.6, 308, 309');
  const [isM, setIsM] = useState('1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13');

  // Multicurve Setup
  const mc = useEndpoint();
  const [mcRef, setMcRef] = useState('2025-01-15');
  const [mcOT, setMcOT] = useState('0.25, 0.5, 1, 2, 5, 10');
  const [mcOR, setMcOR] = useState('0.035, 0.037, 0.04, 0.042, 0.047, 0.05');
  const [mcIT, setMcIT] = useState('0.25, 0.5, 1, 2, 5, 10');
  const [mcIR, setMcIR] = useState('0.038, 0.04, 0.043, 0.046, 0.051, 0.054');
  const [mcQ, setMcQ] = useState('0.5, 1, 2, 5, 10');

  // Multicurve Basis Spread
  const bs = useEndpoint();
  const [bsRef, setBsRef] = useState('2025-01-15');
  const [bsOT, setBsOT] = useState('0.25, 0.5, 1, 2, 5, 10');
  const [bsOR, setBsOR] = useState('0.035, 0.037, 0.04, 0.042, 0.047, 0.05');
  const [bsIT, setBsIT] = useState('0.25, 0.5, 1, 2, 5, 10');
  const [bsIR, setBsIR] = useState('0.038, 0.04, 0.043, 0.046, 0.051, 0.054');
  const [bsQ, setBsQ] = useState('3');

  // Cross Currency Basis
  const ccb = useEndpoint();
  const [ccRef, setCcRef] = useState('2025-01-15');
  const [ccDom, setCcDom] = useState('USD');
  const [ccFor, setCcFor] = useState('EUR');
  const [ccT, setCcT] = useState('1, 2, 3, 5, 10');
  const [ccS, setCcS] = useState('-10, -12, -15, -18, -20');
  const [ccQ, setCcQ] = useState('1, 2, 5, 10');

  // Real Rate Curve
  const rr = useEndpoint();
  const [rrRef, setRrRef] = useState('2025-01-15');
  const [rrNT, setRrNT] = useState('1, 2, 3, 5, 10');
  const [rrNR, setRrNR] = useState('0.04, 0.042, 0.045, 0.048, 0.054');
  const [rrCpi, setRrCpi] = useState('300');
  const [rrIT, setRrIT] = useState('1, 2, 3, 5, 10');
  const [rrIR, setRrIR] = useState('0.03, 0.028, 0.026, 0.025, 0.024');
  const [rrQ, setRrQ] = useState('1, 2, 5, 10');

  return (
    <>
      <EndpointCard title="Inflation Curve Build" description="Build inflation curve from CPI and rates">
        <Row>
          <Field label="Ref Date"><Input value={ibRef} onChange={setIbRef} /></Field>
          <Field label="Base CPI"><Input value={ibCpi} onChange={setIbCpi} type="number" /></Field>
          <Field label="Tenors" width="200px"><Input value={ibT} onChange={setIbT} /></Field>
          <Field label="Inflation Rates" width="250px"><Input value={ibR} onChange={setIbR} /></Field>
          <RunButton loading={ib.loading} onClick={() => ib.run(() => inflationApi.build(ibRef, Number(ibCpi), ibT.split(',').map(Number), ibR.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ib.result} error={ib.error} />
      </EndpointCard>

      <EndpointCard title="Inflation Bootstrap" description="Bootstrap inflation curve from swaps and nominal rates">
        <Row>
          <Field label="Ref Date"><Input value={ibsRef} onChange={setIbsRef} /></Field>
          <Field label="Base CPI"><Input value={ibsCpi} onChange={setIbsCpi} type="number" /></Field>
          <Field label="Swap Tenors" width="180px"><Input value={ibsST} onChange={setIbsST} /></Field>
          <Field label="Swap Rates" width="200px"><Input value={ibsSR} onChange={setIbsSR} /></Field>
          <Field label="Nom Tenors" width="180px"><Input value={ibsNT} onChange={setIbsNT} /></Field>
          <Field label="Nom Rates" width="200px"><Input value={ibsNR} onChange={setIbsNR} /></Field>
          <RunButton loading={ibs.loading} onClick={() => ibs.run(() => inflationApi.bootstrap(ibsRef, Number(ibsCpi), ibsST.split(',').map(Number), ibsSR.split(',').map(Number), ibsNT.split(',').map(Number), ibsNR.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ibs.result} error={ibs.error} />
      </EndpointCard>

      <EndpointCard title="Inflation Seasonality" description="Apply seasonal adjustment to CPI data">
        <Row>
          <Field label="Monthly Factors" width="350px"><Input value={isF} onChange={setIsF} /></Field>
          <Field label="CPI Values" width="300px"><Input value={isC} onChange={setIsC} /></Field>
          <Field label="Months" width="250px"><Input value={isM} onChange={setIsM} /></Field>
          <RunButton loading={iseas.loading} onClick={() => iseas.run(() => inflationApi.seasonality(isF.split(',').map(Number), isC.split(',').map(Number), isM.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={iseas.result} error={iseas.error} />
      </EndpointCard>

      <EndpointCard title="Multicurve Setup" description="OIS + IBOR dual-curve framework">
        <Row>
          <Field label="Ref Date"><Input value={mcRef} onChange={setMcRef} /></Field>
          <Field label="OIS Tenors" width="180px"><Input value={mcOT} onChange={setMcOT} /></Field>
          <Field label="OIS Rates" width="200px"><Input value={mcOR} onChange={setMcOR} /></Field>
          <Field label="IBOR Tenors" width="180px"><Input value={mcIT} onChange={setMcIT} /></Field>
          <Field label="IBOR Rates" width="200px"><Input value={mcIR} onChange={setMcIR} /></Field>
          <Field label="Query Tenors" width="150px"><Input value={mcQ} onChange={setMcQ} /></Field>
          <RunButton loading={mc.loading} onClick={() => mc.run(() => multicurveApi.setup(mcRef, mcOT.split(',').map(Number), mcOR.split(',').map(Number), mcIT.split(',').map(Number), mcIR.split(',').map(Number), mcQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={mc.result} error={mc.error} />
      </EndpointCard>

      <EndpointCard title="Multicurve Basis Spread" description="OIS/IBOR basis spread at a tenor">
        <Row>
          <Field label="Ref Date"><Input value={bsRef} onChange={setBsRef} /></Field>
          <Field label="OIS Tenors" width="180px"><Input value={bsOT} onChange={setBsOT} /></Field>
          <Field label="OIS Rates" width="200px"><Input value={bsOR} onChange={setBsOR} /></Field>
          <Field label="IBOR Tenors" width="180px"><Input value={bsIT} onChange={setBsIT} /></Field>
          <Field label="IBOR Rates" width="200px"><Input value={bsIR} onChange={setBsIR} /></Field>
          <Field label="Query Tenor"><Input value={bsQ} onChange={setBsQ} type="number" /></Field>
          <RunButton loading={bs.loading} onClick={() => bs.run(() => multicurveApi.basisSpread(bsRef, bsOT.split(',').map(Number), bsOR.split(',').map(Number), bsIT.split(',').map(Number), bsIR.split(',').map(Number), Number(bsQ)))} />
        </Row>
        <ResultPanel data={bs.result} error={bs.error} />
      </EndpointCard>

      <EndpointCard title="Cross-Currency Basis" description="Cross-currency basis curve construction">
        <Row>
          <Field label="Ref Date"><Input value={ccRef} onChange={setCcRef} /></Field>
          <Field label="Domestic"><Input value={ccDom} onChange={setCcDom} /></Field>
          <Field label="Foreign"><Input value={ccFor} onChange={setCcFor} /></Field>
          <Field label="Tenors" width="150px"><Input value={ccT} onChange={setCcT} /></Field>
          <Field label="Spreads (bps)" width="180px"><Input value={ccS} onChange={setCcS} /></Field>
          <Field label="Query Tenors" width="150px"><Input value={ccQ} onChange={setCcQ} /></Field>
          <RunButton loading={ccb.loading} onClick={() => ccb.run(() => crossCurrencyApi.basis(ccRef, ccDom, ccFor, ccT.split(',').map(Number), ccS.split(',').map(Number), ccQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ccb.result} error={ccb.error} />
      </EndpointCard>

      <EndpointCard title="Real Rate Curve" description="Real rate = nominal - inflation">
        <Row>
          <Field label="Ref Date"><Input value={rrRef} onChange={setRrRef} /></Field>
          <Field label="Nom Tenors" width="150px"><Input value={rrNT} onChange={setRrNT} /></Field>
          <Field label="Nom Rates" width="200px"><Input value={rrNR} onChange={setRrNR} /></Field>
          <Field label="Base CPI"><Input value={rrCpi} onChange={setRrCpi} type="number" /></Field>
          <Field label="Infl Tenors" width="150px"><Input value={rrIT} onChange={setRrIT} /></Field>
          <Field label="Infl Rates" width="200px"><Input value={rrIR} onChange={setRrIR} /></Field>
          <Field label="Query Tenors" width="150px"><Input value={rrQ} onChange={setRrQ} /></Field>
          <RunButton loading={rr.loading} onClick={() => rr.run(() => crossCurrencyApi.realRate(rrRef, rrNT.split(',').map(Number), rrNR.split(',').map(Number), Number(rrCpi), rrIT.split(',').map(Number), rrIR.split(',').map(Number), rrQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={rr.result} error={rr.error} />
      </EndpointCard>
    </>
  );
}
