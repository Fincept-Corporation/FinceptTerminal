import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { cdsApi, futuresApi } from '../quantlibInstrumentsApi';

const sampleTenors = '0.5, 1, 2, 3, 5, 7, 10';
const sampleRates = '0.04, 0.042, 0.044, 0.045, 0.048, 0.049, 0.05';

export default function CreditFuturesPanel() {
  // CDS Value
  const cv = useEndpoint();
  const [cvRef, setCvRef] = useState('2024-01-15');
  const [cvEff, setCvEff] = useState('2024-03-20');
  const [cvMat, setCvMat] = useState('2029-03-20');
  const [cvNot, setCvNot] = useState('10000000');
  const [cvSpread, setCvSpread] = useState('100');
  const [cvRec, setCvRec] = useState('0.4');

  // Hazard Rate
  const hr = useEndpoint();
  const [hrSpread, setHrSpread] = useState('100');
  const [hrRec, setHrRec] = useState('0.4');

  // Survival Probability
  const sp = useEndpoint();
  const [spHz, setSpHz] = useState('0.02');
  const [spT, setSpT] = useState('5');
  const [spPts, setSpPts] = useState('1, 2, 3, 4, 5');

  // STIR Future
  const stir = useEndpoint();
  const [stirPrice, setStirPrice] = useState('96.5');
  const [stirContract, setStirContract] = useState('2024-06-15');
  const [stirRef, setStirRef] = useState('2024-01-15');
  const [stirNot, setStirNot] = useState('1000000');

  // Bond Future CTD
  const ctd = useEndpoint();
  const [ctdFut, setCtdFut] = useState('120');
  const [ctdDel, setCtdDel] = useState('2024-06-15');
  const [ctdRef, setCtdRef] = useState('2024-01-15');
  const [ctdDlv, setCtdDlv] = useState('[{"coupon":0.05,"maturity":"2030-06-15","conversion_factor":0.85},{"coupon":0.04,"maturity":"2031-06-15","conversion_factor":0.80}]');

  return (
    <>
      <EndpointCard title="CDS Value" description="Credit Default Swap valuation">
        <Row>
          <Field label="Ref Date"><Input value={cvRef} onChange={setCvRef} /></Field>
          <Field label="Effective"><Input value={cvEff} onChange={setCvEff} /></Field>
          <Field label="Maturity"><Input value={cvMat} onChange={setCvMat} /></Field>
          <Field label="Notional"><Input value={cvNot} onChange={setCvNot} type="number" /></Field>
          <Field label="Spread (bps)"><Input value={cvSpread} onChange={setCvSpread} type="number" /></Field>
          <Field label="Recovery"><Input value={cvRec} onChange={setCvRec} type="number" /></Field>
          <RunButton loading={cv.loading} onClick={() => cv.run(() => cdsApi.value(cvRef, cvEff, cvMat, Number(cvNot), Number(cvSpread), sampleTenors.split(',').map(Number), sampleRates.split(',').map(Number), Number(cvRec)))} />
        </Row>
        <ResultPanel data={cv.result} error={cv.error} />
      </EndpointCard>

      <EndpointCard title="CDS Hazard Rate" description="Implied hazard rate from spread">
        <Row>
          <Field label="Spread (bps)"><Input value={hrSpread} onChange={setHrSpread} type="number" /></Field>
          <Field label="Recovery"><Input value={hrRec} onChange={setHrRec} type="number" /></Field>
          <RunButton loading={hr.loading} onClick={() => hr.run(() => cdsApi.hazardRate(Number(hrSpread), Number(hrRec)))} />
        </Row>
        <ResultPanel data={hr.result} error={hr.error} />
      </EndpointCard>

      <EndpointCard title="Survival Probability" description="Compute survival probabilities from hazard rate">
        <Row>
          <Field label="Hazard Rate"><Input value={spHz} onChange={setSpHz} type="number" /></Field>
          <Field label="Time Horizon"><Input value={spT} onChange={setSpT} type="number" /></Field>
          <Field label="Time Points" width="250px"><Input value={spPts} onChange={setSpPts} /></Field>
          <RunButton loading={sp.loading} onClick={() => sp.run(() => cdsApi.survivalProbability(Number(spHz), Number(spT), spPts ? spPts.split(',').map(Number) : undefined))} />
        </Row>
        <ResultPanel data={sp.result} error={sp.error} />
      </EndpointCard>

      <EndpointCard title="STIR Future" description="Short-term interest rate future">
        <Row>
          <Field label="Price"><Input value={stirPrice} onChange={setStirPrice} type="number" /></Field>
          <Field label="Contract Date"><Input value={stirContract} onChange={setStirContract} /></Field>
          <Field label="Ref Date"><Input value={stirRef} onChange={setStirRef} /></Field>
          <Field label="Notional"><Input value={stirNot} onChange={setStirNot} type="number" /></Field>
          <RunButton loading={stir.loading} onClick={() => stir.run(() => futuresApi.stir(Number(stirPrice), stirContract, stirRef, Number(stirNot)))} />
        </Row>
        <ResultPanel data={stir.result} error={stir.error} />
      </EndpointCard>

      <EndpointCard title="Bond Future CTD" description="Cheapest-to-deliver analysis">
        <Row>
          <Field label="Futures Price"><Input value={ctdFut} onChange={setCtdFut} type="number" /></Field>
          <Field label="Delivery Date"><Input value={ctdDel} onChange={setCtdDel} /></Field>
          <Field label="Ref Date"><Input value={ctdRef} onChange={setCtdRef} /></Field>
          <Field label="Deliverables (JSON)" width="400px"><Input value={ctdDlv} onChange={setCtdDlv} /></Field>
          <RunButton loading={ctd.loading} onClick={() => ctd.run(() => futuresApi.bondCtd(Number(ctdFut), ctdDel, ctdRef, JSON.parse(ctdDlv)))} />
        </Row>
        <ResultPanel data={ctd.result} error={ctd.error} />
      </EndpointCard>
    </>
  );
}
