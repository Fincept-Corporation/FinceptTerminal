import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { optimizeApi, riskParityApi, blackLittermanApi } from '../quantlibPortfolioApi';

export default function OptimizePanel() {
  const sampleRet = '[0.08, 0.12, 0.06]';
  const sampleCov = '[[0.04,0.006,0.002],[0.006,0.09,0.009],[0.002,0.009,0.01]]';
  const sampleCaps = '[500, 300, 200]';

  // Min Variance
  const mv = useEndpoint();
  const [mvRet, setMvRet] = useState(sampleRet);
  const [mvCov, setMvCov] = useState(sampleCov);
  const [mvRf, setMvRf] = useState('0');

  // Max Sharpe
  const ms = useEndpoint();
  const [msRet, setMsRet] = useState(sampleRet);
  const [msCov, setMsCov] = useState(sampleCov);
  const [msRf, setMsRf] = useState('0.02');

  // Target Return
  const tr = useEndpoint();
  const [trRet, setTrRet] = useState(sampleRet);
  const [trCov, setTrCov] = useState(sampleCov);
  const [trTarget, setTrTarget] = useState('0.10');

  // Efficient Frontier
  const ef = useEndpoint();
  const [efRet, setEfRet] = useState(sampleRet);
  const [efCov, setEfCov] = useState(sampleCov);
  const [efPts, setEfPts] = useState('20');

  // Black-Litterman Equilibrium
  const blEq = useEndpoint();
  const [blCov, setBlCov] = useState(sampleCov);
  const [blCaps, setBlCaps] = useState(sampleCaps);
  const [blRA, setBlRA] = useState('2.5');

  // Black-Litterman Posterior
  const blPost = useEndpoint();
  const [blpCov, setBlpCov] = useState(sampleCov);
  const [blpCaps, setBlpCaps] = useState(sampleCaps);
  const [blpRA, setBlpRA] = useState('2.5');

  // Risk Parity
  const rp = useEndpoint();
  const [rpCov, setRpCov] = useState(sampleCov);

  return (
    <>
      <EndpointCard title="Min Variance" description="Minimum variance portfolio optimization">
        <Row>
          <Field label="Expected Returns (JSON)" width="250px"><Input value={mvRet} onChange={setMvRet} /></Field>
          <Field label="Covariance (JSON 2D)" width="350px"><Input value={mvCov} onChange={setMvCov} /></Field>
          <Field label="Rf Rate"><Input value={mvRf} onChange={setMvRf} type="number" /></Field>
          <RunButton loading={mv.loading} onClick={() => mv.run(() => optimizeApi.minVariance(JSON.parse(mvRet), JSON.parse(mvCov), Number(mvRf)))} />
        </Row>
        <ResultPanel data={mv.result} error={mv.error} />
      </EndpointCard>

      <EndpointCard title="Max Sharpe" description="Maximum Sharpe ratio portfolio">
        <Row>
          <Field label="Expected Returns (JSON)" width="250px"><Input value={msRet} onChange={setMsRet} /></Field>
          <Field label="Covariance (JSON 2D)" width="350px"><Input value={msCov} onChange={setMsCov} /></Field>
          <Field label="Rf Rate"><Input value={msRf} onChange={setMsRf} type="number" /></Field>
          <RunButton loading={ms.loading} onClick={() => ms.run(() => optimizeApi.maxSharpe(JSON.parse(msRet), JSON.parse(msCov), Number(msRf)))} />
        </Row>
        <ResultPanel data={ms.result} error={ms.error} />
      </EndpointCard>

      <EndpointCard title="Target Return" description="Optimize for target return">
        <Row>
          <Field label="Expected Returns (JSON)" width="250px"><Input value={trRet} onChange={setTrRet} /></Field>
          <Field label="Covariance (JSON 2D)" width="350px"><Input value={trCov} onChange={setTrCov} /></Field>
          <Field label="Target Return"><Input value={trTarget} onChange={setTrTarget} type="number" /></Field>
          <RunButton loading={tr.loading} onClick={() => tr.run(() => optimizeApi.targetReturn(JSON.parse(trRet), JSON.parse(trCov), Number(trTarget)))} />
        </Row>
        <ResultPanel data={tr.result} error={tr.error} />
      </EndpointCard>

      <EndpointCard title="Efficient Frontier" description="Compute efficient frontier points">
        <Row>
          <Field label="Expected Returns (JSON)" width="250px"><Input value={efRet} onChange={setEfRet} /></Field>
          <Field label="Covariance (JSON 2D)" width="350px"><Input value={efCov} onChange={setEfCov} /></Field>
          <Field label="N Points"><Input value={efPts} onChange={setEfPts} type="number" /></Field>
          <RunButton loading={ef.loading} onClick={() => ef.run(() => optimizeApi.efficientFrontier(JSON.parse(efRet), JSON.parse(efCov), Number(efPts)))} />
        </Row>
        <ResultPanel data={ef.result} error={ef.error} />
      </EndpointCard>

      <EndpointCard title="Black-Litterman Equilibrium" description="Implied equilibrium returns">
        <Row>
          <Field label="Covariance (JSON 2D)" width="350px"><Input value={blCov} onChange={setBlCov} /></Field>
          <Field label="Market Caps (JSON)" width="200px"><Input value={blCaps} onChange={setBlCaps} /></Field>
          <Field label="Risk Aversion"><Input value={blRA} onChange={setBlRA} type="number" /></Field>
          <RunButton loading={blEq.loading} onClick={() => blEq.run(() => blackLittermanApi.equilibrium(JSON.parse(blCov), JSON.parse(blCaps), Number(blRA)))} />
        </Row>
        <ResultPanel data={blEq.result} error={blEq.error} />
      </EndpointCard>

      <EndpointCard title="Black-Litterman Posterior" description="Posterior returns with views">
        <Row>
          <Field label="Covariance (JSON 2D)" width="350px"><Input value={blpCov} onChange={setBlpCov} /></Field>
          <Field label="Market Caps (JSON)" width="200px"><Input value={blpCaps} onChange={setBlpCaps} /></Field>
          <Field label="Risk Aversion"><Input value={blpRA} onChange={setBlpRA} type="number" /></Field>
          <RunButton loading={blPost.loading} onClick={() => blPost.run(() => blackLittermanApi.posterior(JSON.parse(blpCov), JSON.parse(blpCaps), Number(blpRA)))} />
        </Row>
        <ResultPanel data={blPost.result} error={blPost.error} />
      </EndpointCard>

      <EndpointCard title="Risk Parity" description="Equal risk contribution optimization">
        <Row>
          <Field label="Covariance (JSON 2D)" width="450px"><Input value={rpCov} onChange={setRpCov} /></Field>
          <RunButton loading={rp.loading} onClick={() => rp.run(() => riskParityApi.optimize(JSON.parse(rpCov)))} />
        </Row>
        <ResultPanel data={rp.result} error={rp.error} />
      </EndpointCard>
    </>
  );
}
