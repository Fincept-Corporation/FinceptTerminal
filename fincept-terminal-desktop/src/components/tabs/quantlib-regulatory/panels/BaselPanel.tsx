import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { baselApi } from '../quantlibRegulatoryApi';

export default function BaselPanel() {
  // Capital Ratios
  const cr = useEndpoint();
  const [crCet1, setCrCet1] = useState('50000');
  const [crTier1, setCrTier1] = useState('60000');
  const [crTotal, setCrTotal] = useState('80000');
  const [crRwa, setCrRwa] = useState('500000');
  const [crExposure, setCrExposure] = useState('1000000');

  // Credit RWA
  const crwa = useEndpoint();
  const [crwaMethod, setCrwaMethod] = useState('standardized');
  const [crwaExposures, setCrwaExposures] = useState('[{"amount": 1000000, "risk_weight": 0.5}, {"amount": 500000, "risk_weight": 1.0}]');

  // Operational RWA
  const orwa = useEndpoint();
  const [orwaIncome, setOrwaIncome] = useState('100000, 120000, 110000');

  return (
    <>
      <EndpointCard title="Capital Ratios" description="Basel III CET1, Tier 1, Total capital & leverage ratios">
        <Row>
          <Field label="CET1 Capital"><Input value={crCet1} onChange={setCrCet1} type="number" /></Field>
          <Field label="Tier 1 Capital"><Input value={crTier1} onChange={setCrTier1} type="number" /></Field>
          <Field label="Total Capital"><Input value={crTotal} onChange={setCrTotal} type="number" /></Field>
          <Field label="RWA"><Input value={crRwa} onChange={setCrRwa} type="number" /></Field>
          <Field label="Total Exposure"><Input value={crExposure} onChange={setCrExposure} type="number" /></Field>
          <RunButton loading={cr.loading} onClick={() => cr.run(() =>
            baselApi.capitalRatios(Number(crCet1), Number(crTier1), Number(crTotal), Number(crRwa), Number(crExposure))
          )} />
        </Row>
        <ResultPanel data={cr.result} error={cr.error} />
      </EndpointCard>

      <EndpointCard title="Credit RWA" description="Standardized / IRB credit risk-weighted assets">
        <Row>
          <Field label="Method"><Select value={crwaMethod} onChange={setCrwaMethod} options={['standardized', 'irb']} /></Field>
          <Field label="Exposures (JSON)" width="400px"><Input value={crwaExposures} onChange={setCrwaExposures} /></Field>
          <RunButton loading={crwa.loading} onClick={() => crwa.run(() =>
            baselApi.creditRwa(JSON.parse(crwaExposures), crwaMethod)
          )} />
        </Row>
        <ResultPanel data={crwa.result} error={crwa.error} />
      </EndpointCard>

      <EndpointCard title="Operational RWA" description="Basic Indicator Approach operational risk">
        <Row>
          <Field label="Gross Income 3Y (comma-sep)" width="300px"><Input value={orwaIncome} onChange={setOrwaIncome} /></Field>
          <RunButton loading={orwa.loading} onClick={() => orwa.run(() =>
            baselApi.operationalRwa(orwaIncome.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={orwa.result} error={orwa.error} />
      </EndpointCard>
    </>
  );
}
