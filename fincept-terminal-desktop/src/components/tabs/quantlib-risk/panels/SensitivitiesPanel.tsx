import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { sensitivityApi, portfolioRiskApi } from '../quantlibRiskApi';

const sampleCF = '5, 5, 5, 5, 105';
const sampleTimes = '1, 2, 3, 4, 5';
const sampleRates = '0.04, 0.042, 0.045, 0.048, 0.05';

export default function SensitivitiesPanel() {
  // Full Greeks
  const gr = useEndpoint();
  const [grS, setGrS] = useState('100');
  const [grK, setGrK] = useState('100');
  const [grT, setGrT] = useState('1');
  const [grR, setGrR] = useState('0.05');
  const [grSig, setGrSig] = useState('0.2');

  // Cross Gamma
  const cg = useEndpoint();
  const [cgS, setCgS] = useState('100');
  const [cgK, setCgK] = useState('100');
  const [cgT, setCgT] = useState('1');
  const [cgR, setCgR] = useState('0.05');
  const [cgSig, setCgSig] = useState('0.2');
  const [cgBump, setCgBump] = useState('0.01');

  // Key Rate Duration
  const krd = useEndpoint();
  const [krdCF, setKrdCF] = useState(sampleCF);
  const [krdT, setKrdT] = useState(sampleTimes);
  const [krdR, setKrdR] = useState(sampleRates);

  // Parallel Shift Sensitivity
  const ps = useEndpoint();
  const [psCF, setPsCF] = useState(sampleCF);
  const [psT, setPsT] = useState(sampleTimes);
  const [psR, setPsR] = useState(sampleRates);

  // Twist Sensitivity
  const tw = useEndpoint();
  const [twCF, setTwCF] = useState(sampleCF);
  const [twT, setTwT] = useState(sampleTimes);
  const [twR, setTwR] = useState(sampleRates);

  // Bucket Delta
  const bd = useEndpoint();
  const [bdPos, setBdPos] = useState('[{"type":"bond","cashflows":[5,5,105],"times":[1,2,3],"curve_rates":[0.04,0.042,0.045]}]');

  // Optimal Hedge
  const oh = useEndpoint();
  const [ohPD, setOhPD] = useState('0.5');
  const [ohHD, setOhHD] = useState('-0.45');
  const [ohPG, setOhPG] = useState('0.02');
  const [ohHG, setOhHG] = useState('-0.03');

  // Exposure Profile
  const ep = useEndpoint();
  const [epPaths, setEpPaths] = useState('[[100,105,98,110,115],[100,95,102,108,103],[100,103,97,105,112]]');
  const [epConf, setEpConf] = useState('0.95');

  return (
    <>
      <EndpointCard title="Full Greeks" description="All option sensitivities (delta, gamma, vega, theta, rho)">
        <Row>
          <Field label="S"><Input value={grS} onChange={setGrS} type="number" /></Field>
          <Field label="K"><Input value={grK} onChange={setGrK} type="number" /></Field>
          <Field label="T"><Input value={grT} onChange={setGrT} type="number" /></Field>
          <Field label="r"><Input value={grR} onChange={setGrR} type="number" /></Field>
          <Field label="Sigma"><Input value={grSig} onChange={setGrSig} type="number" /></Field>
          <RunButton loading={gr.loading} onClick={() => gr.run(() => sensitivityApi.greeks(Number(grS), Number(grK), Number(grT), Number(grR), Number(grSig)))} />
        </Row>
        <ResultPanel data={gr.result} error={gr.error} />
      </EndpointCard>

      <EndpointCard title="Cross Gamma" description="Cross-gamma between spot and vol">
        <Row>
          <Field label="S"><Input value={cgS} onChange={setCgS} type="number" /></Field>
          <Field label="K"><Input value={cgK} onChange={setCgK} type="number" /></Field>
          <Field label="T"><Input value={cgT} onChange={setCgT} type="number" /></Field>
          <Field label="r"><Input value={cgR} onChange={setCgR} type="number" /></Field>
          <Field label="Sigma"><Input value={cgSig} onChange={setCgSig} type="number" /></Field>
          <Field label="Bump"><Input value={cgBump} onChange={setCgBump} type="number" /></Field>
          <RunButton loading={cg.loading} onClick={() => cg.run(() => sensitivityApi.crossGamma(Number(cgS), Number(cgK), Number(cgT), Number(cgR), Number(cgSig), Number(cgBump)))} />
        </Row>
        <ResultPanel data={cg.result} error={cg.error} />
      </EndpointCard>

      <EndpointCard title="Key Rate Duration" description="Duration sensitivity to each key rate">
        <Row>
          <Field label="Cashflows" width="200px"><Input value={krdCF} onChange={setKrdCF} /></Field>
          <Field label="Times" width="150px"><Input value={krdT} onChange={setKrdT} /></Field>
          <Field label="Curve Rates" width="200px"><Input value={krdR} onChange={setKrdR} /></Field>
          <RunButton loading={krd.loading} onClick={() => krd.run(() => sensitivityApi.keyRateDuration(krdCF.split(',').map(Number), krdT.split(',').map(Number), krdR.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={krd.result} error={krd.error} />
      </EndpointCard>

      <EndpointCard title="Parallel Shift Sensitivity" description="PV change from parallel curve shift">
        <Row>
          <Field label="Cashflows" width="200px"><Input value={psCF} onChange={setPsCF} /></Field>
          <Field label="Times" width="150px"><Input value={psT} onChange={setPsT} /></Field>
          <Field label="Curve Rates" width="200px"><Input value={psR} onChange={setPsR} /></Field>
          <RunButton loading={ps.loading} onClick={() => ps.run(() => sensitivityApi.parallelShift(psCF.split(',').map(Number), psT.split(',').map(Number), psR.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ps.result} error={ps.error} />
      </EndpointCard>

      <EndpointCard title="Twist Sensitivity" description="PV change from curve twist">
        <Row>
          <Field label="Cashflows" width="200px"><Input value={twCF} onChange={setTwCF} /></Field>
          <Field label="Times" width="150px"><Input value={twT} onChange={setTwT} /></Field>
          <Field label="Curve Rates" width="200px"><Input value={twR} onChange={setTwR} /></Field>
          <RunButton loading={tw.loading} onClick={() => tw.run(() => sensitivityApi.twist(twCF.split(',').map(Number), twT.split(',').map(Number), twR.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={tw.result} error={tw.error} />
      </EndpointCard>

      <EndpointCard title="Bucket Delta" description="Delta per rate bucket for positions">
        <Row>
          <Field label="Positions (JSON)" width="500px"><Input value={bdPos} onChange={setBdPos} /></Field>
          <RunButton loading={bd.loading} onClick={() => bd.run(() => sensitivityApi.bucketDelta(JSON.parse(bdPos)))} />
        </Row>
        <ResultPanel data={bd.result} error={bd.error} />
      </EndpointCard>

      <EndpointCard title="Optimal Hedge" description="Find optimal hedge ratio for delta/gamma">
        <Row>
          <Field label="Port Delta"><Input value={ohPD} onChange={setOhPD} type="number" /></Field>
          <Field label="Hedge Delta"><Input value={ohHD} onChange={setOhHD} type="number" /></Field>
          <Field label="Port Gamma"><Input value={ohPG} onChange={setOhPG} type="number" /></Field>
          <Field label="Hedge Gamma"><Input value={ohHG} onChange={setOhHG} type="number" /></Field>
          <RunButton loading={oh.loading} onClick={() => oh.run(() => portfolioRiskApi.optimalHedge(Number(ohPD), Number(ohHD), Number(ohPG), Number(ohHG)))} />
        </Row>
        <ResultPanel data={oh.result} error={oh.error} />
      </EndpointCard>

      <EndpointCard title="Exposure Profile" description="Expected / potential future exposure profile">
        <Row>
          <Field label="Paths (JSON 2D)" width="400px"><Input value={epPaths} onChange={setEpPaths} /></Field>
          <Field label="Confidence"><Input value={epConf} onChange={setEpConf} type="number" /></Field>
          <RunButton loading={ep.loading} onClick={() => ep.run(() => portfolioRiskApi.exposureProfile(JSON.parse(epPaths), Number(epConf)))} />
        </Row>
        <ResultPanel data={ep.result} error={ep.error} />
      </EndpointCard>
    </>
  );
}
