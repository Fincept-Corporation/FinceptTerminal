import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { varApi, stressApi } from '../quantlibRiskApi';

const sampleReturns = '0.01, -0.02, 0.03, -0.01, 0.005, -0.03, 0.02, 0.01, -0.005, 0.015, -0.025, 0.008, -0.012, 0.018, -0.007, 0.022, -0.015, 0.009, -0.004, 0.013';

export default function VaRPanel() {
  // Parametric VaR
  const pv = useEndpoint();
  const [pvVal, setPvVal] = useState('1000000');
  const [pvVol, setPvVol] = useState('0.02');
  const [pvConf, setPvConf] = useState('0.99');
  const [pvH, setPvH] = useState('1');

  // Historical VaR
  const hv = useEndpoint();
  const [hvRet, setHvRet] = useState(sampleReturns);
  const [hvConf, setHvConf] = useState('0.99');
  const [hvVal, setHvVal] = useState('1000000');

  // Incremental VaR
  const iv = useEndpoint();
  const [ivPR, setIvPR] = useState(sampleReturns);
  const [ivNR, setIvNR] = useState('0.005, -0.01, 0.02, -0.005, 0.01, -0.02, 0.015, 0.005, -0.008, 0.012, -0.018, 0.003, -0.007, 0.011, -0.004, 0.016, -0.009, 0.006, -0.002, 0.01');
  const [ivW, setIvW] = useState('0.1');

  // Marginal VaR
  const mv = useEndpoint();
  const [mvPR, setMvPR] = useState(sampleReturns);
  const [mvNR, setMvNR] = useState('0.005, -0.01, 0.02, -0.005, 0.01, -0.02, 0.015, 0.005, -0.008, 0.012, -0.018, 0.003, -0.007, 0.011, -0.004, 0.016, -0.009, 0.006, -0.002, 0.01');
  const [mvW, setMvW] = useState('0.1');

  // ES Optimization
  const es = useEndpoint();
  const [esRet, setEsRet] = useState('[[0.01,-0.02,0.03],[0.005,-0.01,0.02],[-0.005,0.015,-0.008]]');
  const [esConf, setEsConf] = useState('0.95');

  // Stress Scenario
  const ss = useEndpoint();
  const [ssName, setSsName] = useState('Market Crash');
  const [ssShocks, setSsShocks] = useState('{"equity": -0.3, "rates": 0.02, "credit": 0.05}');

  // VaR Backtest
  const bt = useEndpoint();
  const [btActual, setBtActual] = useState(sampleReturns);
  const [btVar, setBtVar] = useState('0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025, 0.025');

  return (
    <>
      <EndpointCard title="Parametric VaR" description="Normal distribution VaR estimate">
        <Row>
          <Field label="Portfolio Value"><Input value={pvVal} onChange={setPvVal} type="number" /></Field>
          <Field label="Volatility"><Input value={pvVol} onChange={setPvVol} type="number" /></Field>
          <Field label="Confidence"><Input value={pvConf} onChange={setPvConf} type="number" /></Field>
          <Field label="Horizon (days)"><Input value={pvH} onChange={setPvH} type="number" /></Field>
          <RunButton loading={pv.loading} onClick={() => pv.run(() => varApi.parametric(Number(pvVal), Number(pvVol), Number(pvConf), Number(pvH)))} />
        </Row>
        <ResultPanel data={pv.result} error={pv.error} />
      </EndpointCard>

      <EndpointCard title="Historical VaR" description="VaR from historical return distribution">
        <Row>
          <Field label="Returns (comma-sep)" width="450px"><Input value={hvRet} onChange={setHvRet} /></Field>
          <Field label="Confidence"><Input value={hvConf} onChange={setHvConf} type="number" /></Field>
          <Field label="Port Value"><Input value={hvVal} onChange={setHvVal} type="number" /></Field>
          <RunButton loading={hv.loading} onClick={() => hv.run(() => varApi.historical(hvRet.split(',').map(Number), Number(hvConf), Number(hvVal)))} />
        </Row>
        <ResultPanel data={hv.result} error={hv.error} />
      </EndpointCard>

      <EndpointCard title="Incremental VaR" description="VaR impact of adding a new position">
        <Row>
          <Field label="Portfolio Returns" width="350px"><Input value={ivPR} onChange={setIvPR} /></Field>
          <Field label="New Position Returns" width="350px"><Input value={ivNR} onChange={setIvNR} /></Field>
          <Field label="Weight"><Input value={ivW} onChange={setIvW} type="number" /></Field>
          <RunButton loading={iv.loading} onClick={() => iv.run(() => varApi.incremental(ivPR.split(',').map(Number), ivNR.split(',').map(Number), Number(ivW)))} />
        </Row>
        <ResultPanel data={iv.result} error={iv.error} />
      </EndpointCard>

      <EndpointCard title="Marginal VaR" description="Marginal VaR contribution of a position">
        <Row>
          <Field label="Portfolio Returns" width="350px"><Input value={mvPR} onChange={setMvPR} /></Field>
          <Field label="New Position Returns" width="350px"><Input value={mvNR} onChange={setMvNR} /></Field>
          <Field label="Weight"><Input value={mvW} onChange={setMvW} type="number" /></Field>
          <RunButton loading={mv.loading} onClick={() => mv.run(() => varApi.marginal(mvPR.split(',').map(Number), mvNR.split(',').map(Number), Number(mvW)))} />
        </Row>
        <ResultPanel data={mv.result} error={mv.error} />
      </EndpointCard>

      <EndpointCard title="ES Optimization" description="Expected Shortfall portfolio optimization">
        <Row>
          <Field label="Returns (JSON 2D)" width="400px"><Input value={esRet} onChange={setEsRet} /></Field>
          <Field label="Confidence"><Input value={esConf} onChange={setEsConf} type="number" /></Field>
          <RunButton loading={es.loading} onClick={() => es.run(() => varApi.esOptimization(JSON.parse(esRet), Number(esConf)))} />
        </Row>
        <ResultPanel data={es.result} error={es.error} />
      </EndpointCard>

      <EndpointCard title="Stress Scenario" description="Define and evaluate a stress scenario">
        <Row>
          <Field label="Name"><Input value={ssName} onChange={setSsName} /></Field>
          <Field label="Factor Shocks (JSON)" width="350px"><Input value={ssShocks} onChange={setSsShocks} /></Field>
          <RunButton loading={ss.loading} onClick={() => ss.run(() => stressApi.scenario(ssName, JSON.parse(ssShocks)))} />
        </Row>
        <ResultPanel data={ss.result} error={ss.error} />
      </EndpointCard>

      <EndpointCard title="VaR Backtest" description="Backtest VaR estimates against actual returns">
        <Row>
          <Field label="Actual Returns" width="350px"><Input value={btActual} onChange={setBtActual} /></Field>
          <Field label="VaR Estimates" width="350px"><Input value={btVar} onChange={setBtVar} /></Field>
          <RunButton loading={bt.loading} onClick={() => bt.run(() => stressApi.backtest(btActual.split(',').map(Number), btVar.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={bt.result} error={bt.error} />
      </EndpointCard>
    </>
  );
}
