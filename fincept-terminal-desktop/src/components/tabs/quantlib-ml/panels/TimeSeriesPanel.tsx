import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { timeseriesApi, hmmApi } from '../quantlibMlApi';

export default function TimeSeriesPanel() {
  // Feature Importance
  const fi = useEndpoint();
  const [fiX, setFiX] = useState('[[1,2,3],[4,5,6],[7,8,9],[10,11,12]]');
  const [fiY, setFiY] = useState('[1.5, 3.5, 5.5, 7.5]');
  const [fiTop, setFiTop] = useState('3');

  // HMM Fit
  const hmm = useEndpoint();
  const [hmmRet, setHmmRet] = useState('0.01, -0.02, 0.03, -0.01, 0.005, -0.03, 0.02, 0.01, -0.005, 0.015');
  const [hmmStates, setHmmStates] = useState('2');
  const [hmmIter, setHmmIter] = useState('100');

  // Changepoint
  const cp = useEndpoint();
  const [cpData, setCpData] = useState('1, 1.1, 0.9, 1.05, 5, 5.2, 4.8, 5.1, 2, 2.1, 1.9, 2.05');
  const [cpMethod, setCpMethod] = useState('cusum');
  const [cpThresh, setCpThresh] = useState('1');
  const [cpHazard, setCpHazard] = useState('0.01');

  // GARCH Hybrid
  const gh = useEndpoint();
  const [ghRet, setGhRet] = useState('0.01, -0.02, 0.03, -0.01, 0.005, -0.03, 0.02, 0.01, -0.005, 0.015');
  const [ghP, setGhP] = useState('1');
  const [ghQ, setGhQ] = useState('1');
  const [ghHorizon, setGhHorizon] = useState('5');

  return (
    <>
      <EndpointCard title="Feature Importance" description="Time series feature importance ranking">
        <Row>
          <Field label="X (JSON 2D)" width="300px"><Input value={fiX} onChange={setFiX} /></Field>
          <Field label="y (JSON)" width="250px"><Input value={fiY} onChange={setFiY} /></Field>
          <Field label="N Top"><Input value={fiTop} onChange={setFiTop} type="number" /></Field>
          <RunButton loading={fi.loading} onClick={() => fi.run(() => timeseriesApi.featureImportance(JSON.parse(fiX), JSON.parse(fiY), Number(fiTop)))} />
        </Row>
        <ResultPanel data={fi.result} error={fi.error} />
      </EndpointCard>

      <EndpointCard title="HMM Fit" description="Hidden Markov Model regime detection">
        <Row>
          <Field label="Returns (comma-sep)" width="350px"><Input value={hmmRet} onChange={setHmmRet} /></Field>
          <Field label="N States"><Input value={hmmStates} onChange={setHmmStates} type="number" /></Field>
          <Field label="Max Iter"><Input value={hmmIter} onChange={setHmmIter} type="number" /></Field>
          <RunButton loading={hmm.loading} onClick={() => hmm.run(() => hmmApi.fit(hmmRet.split(',').map(Number), Number(hmmStates), Number(hmmIter)))} />
        </Row>
        <ResultPanel data={hmm.result} error={hmm.error} />
      </EndpointCard>

      <EndpointCard title="Changepoint Detection" description="CUSUM / Bayesian Online / Z-Score changepoints">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={cpData} onChange={setCpData} /></Field>
          <Field label="Method"><Select value={cpMethod} onChange={setCpMethod} options={['cusum', 'bayesian_online', 'zscore']} /></Field>
          <Field label="Threshold"><Input value={cpThresh} onChange={setCpThresh} type="number" /></Field>
          <Field label="Hazard Rate"><Input value={cpHazard} onChange={setCpHazard} type="number" /></Field>
          <RunButton loading={cp.loading} onClick={() => cp.run(() => hmmApi.changepoint(cpData.split(',').map(Number), cpMethod, Number(cpThresh), Number(cpHazard)))} />
        </Row>
        <ResultPanel data={cp.result} error={cp.error} />
      </EndpointCard>

      <EndpointCard title="GARCH Hybrid" description="GARCH + ML hybrid volatility forecast">
        <Row>
          <Field label="Returns (comma-sep)" width="350px"><Input value={ghRet} onChange={setGhRet} /></Field>
          <Field label="p"><Input value={ghP} onChange={setGhP} type="number" /></Field>
          <Field label="q"><Input value={ghQ} onChange={setGhQ} type="number" /></Field>
          <Field label="Forecast Horizon"><Input value={ghHorizon} onChange={setGhHorizon} type="number" /></Field>
          <RunButton loading={gh.loading} onClick={() => gh.run(() => hmmApi.garchHybrid(ghRet.split(',').map(Number), undefined, Number(ghP), Number(ghQ), Number(ghHorizon)))} />
        </Row>
        <ResultPanel data={gh.result} error={gh.error} />
      </EndpointCard>
    </>
  );
}
