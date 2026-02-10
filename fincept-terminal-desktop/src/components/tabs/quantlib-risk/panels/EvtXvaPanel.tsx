import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { evtApi, tailRiskApi, xvaApi, correlationApi } from '../quantlibRiskApi';

const sampleReturns = '0.01, -0.02, 0.03, -0.01, 0.005, -0.03, 0.02, 0.01, -0.005, 0.015, -0.025, 0.008, -0.012, 0.018, -0.007, 0.022, -0.015, 0.009, -0.004, 0.013';

export default function EvtXvaPanel() {
  // GPD
  const gpd = useEndpoint();
  const [gpdData, setGpdData] = useState(sampleReturns);
  const [gpdQ, setGpdQ] = useState('0.95');

  // GEV
  const gev = useEndpoint();
  const [gevData, setGevData] = useState(sampleReturns);

  // Hill Estimator
  const hill = useEndpoint();
  const [hillData, setHillData] = useState(sampleReturns);
  const [hillQ, setHillQ] = useState('0.95');

  // Tail Risk Comprehensive
  const tr = useEndpoint();
  const [trRet, setTrRet] = useState(sampleReturns);
  const [trConf, setTrConf] = useState('0.99');

  // CVA
  const cva = useEndpoint();
  const [cvaPath, setCvaPath] = useState('[[100,105,98,110],[100,95,102,108],[100,103,97,105]]');
  const [cvaSpread, setCvaSpread] = useState('0.02');
  const [cvaRec, setCvaRec] = useState('0.4');

  // PFE
  const pfe = useEndpoint();
  const [pfePath, setPfePath] = useState('[[100,105,98,110],[100,95,102,108],[100,103,97,105]]');
  const [pfeSpread, setPfeSpread] = useState('0.02');

  // Correlation Stress
  const cs = useEndpoint();
  const [csMatrix, setCsMatrix] = useState('[[1,0.5,0.3],[0.5,1,0.4],[0.3,0.4,1]]');
  const [csType, setCsType] = useState('crisis');

  return (
    <>
      <EndpointCard title="EVT: GPD" description="Generalized Pareto Distribution tail fit">
        <Row>
          <Field label="Data (comma-sep)" width="450px"><Input value={gpdData} onChange={setGpdData} /></Field>
          <Field label="Quantile"><Input value={gpdQ} onChange={setGpdQ} type="number" /></Field>
          <RunButton loading={gpd.loading} onClick={() => gpd.run(() => evtApi.gpd(gpdData.split(',').map(Number), undefined, Number(gpdQ)))} />
        </Row>
        <ResultPanel data={gpd.result} error={gpd.error} />
      </EndpointCard>

      <EndpointCard title="EVT: GEV" description="Generalized Extreme Value distribution fit">
        <Row>
          <Field label="Data (comma-sep)" width="450px"><Input value={gevData} onChange={setGevData} /></Field>
          <RunButton loading={gev.loading} onClick={() => gev.run(() => evtApi.gev(gevData.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={gev.result} error={gev.error} />
      </EndpointCard>

      <EndpointCard title="Hill Estimator" description="Hill tail index estimator">
        <Row>
          <Field label="Data (comma-sep)" width="450px"><Input value={hillData} onChange={setHillData} /></Field>
          <Field label="Quantile"><Input value={hillQ} onChange={setHillQ} type="number" /></Field>
          <RunButton loading={hill.loading} onClick={() => hill.run(() => evtApi.hill(hillData.split(',').map(Number), undefined, Number(hillQ)))} />
        </Row>
        <ResultPanel data={hill.result} error={hill.error} />
      </EndpointCard>

      <EndpointCard title="Tail Risk Comprehensive" description="Full tail risk analysis">
        <Row>
          <Field label="Returns (comma-sep)" width="450px"><Input value={trRet} onChange={setTrRet} /></Field>
          <Field label="Confidence"><Input value={trConf} onChange={setTrConf} type="number" /></Field>
          <RunButton loading={tr.loading} onClick={() => tr.run(() => tailRiskApi.comprehensive(trRet.split(',').map(Number), Number(trConf)))} />
        </Row>
        <ResultPanel data={tr.result} error={tr.error} />
      </EndpointCard>

      <EndpointCard title="XVA: CVA" description="Credit Valuation Adjustment">
        <Row>
          <Field label="Exposure Paths (JSON 2D)" width="350px"><Input value={cvaPath} onChange={setCvaPath} /></Field>
          <Field label="Cpty Spread"><Input value={cvaSpread} onChange={setCvaSpread} type="number" /></Field>
          <Field label="Recovery"><Input value={cvaRec} onChange={setCvaRec} type="number" /></Field>
          <RunButton loading={cva.loading} onClick={() => cva.run(() => xvaApi.cva(JSON.parse(cvaPath), Number(cvaSpread), Number(cvaRec)))} />
        </Row>
        <ResultPanel data={cva.result} error={cva.error} />
      </EndpointCard>

      <EndpointCard title="XVA: PFE" description="Potential Future Exposure">
        <Row>
          <Field label="Exposure Paths (JSON 2D)" width="350px"><Input value={pfePath} onChange={setPfePath} /></Field>
          <Field label="Cpty Spread"><Input value={pfeSpread} onChange={setPfeSpread} type="number" /></Field>
          <RunButton loading={pfe.loading} onClick={() => pfe.run(() => xvaApi.pfe(JSON.parse(pfePath), Number(pfeSpread)))} />
        </Row>
        <ResultPanel data={pfe.result} error={pfe.error} />
      </EndpointCard>

      <EndpointCard title="Correlation Stress" description="Stress test correlation matrix">
        <Row>
          <Field label="Correlation Matrix (JSON 2D)" width="350px"><Input value={csMatrix} onChange={setCsMatrix} /></Field>
          <Field label="Type"><Select value={csType} onChange={setCsType} options={['crisis', 'moderate', 'mild']} /></Field>
          <RunButton loading={cs.loading} onClick={() => cs.run(() => correlationApi.stress(JSON.parse(csMatrix), csType))} />
        </Row>
        <ResultPanel data={cs.result} error={cs.error} />
      </EndpointCard>
    </>
  );
}
