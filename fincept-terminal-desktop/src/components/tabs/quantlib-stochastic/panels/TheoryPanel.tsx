import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { theoryApi } from '../quantlibStochasticApi';

export default function TheoryPanel() {
  // Quadratic Variation
  const qv = useEndpoint();
  const [qvPath, setQvPath] = useState('0, 0.1, 0.15, 0.3, 0.25, 0.4');
  const [qvTimes, setQvTimes] = useState('0, 0.2, 0.4, 0.6, 0.8, 1.0');

  // Covariation
  const cov = useEndpoint();
  const [covX, setCovX] = useState('0, 0.1, 0.15, 0.3, 0.25, 0.4');
  const [covY, setCovY] = useState('0, -0.05, 0.1, 0.2, 0.15, 0.3');
  const [covTimes, setCovTimes] = useState('0, 0.2, 0.4, 0.6, 0.8, 1.0');

  // Martingale Test
  const mart = useEndpoint();
  const [martPaths, setMartPaths] = useState('[[1,1.1,0.9,1.05],[1,0.95,1.1,1.02]]');
  const [martTimes, setMartTimes] = useState('0, 0.33, 0.67, 1.0');
  const [martConf, setMartConf] = useState('0.95');

  // Risk-Neutral Drift
  const rnd = useEndpoint();
  const [rndMu, setRndMu] = useState('0.08');
  const [rndR, setRndR] = useState('0.05');
  const [rndSigma, setRndSigma] = useState('0.2');

  // Measure Change
  const mc = useEndpoint();
  const [mcPaths, setMcPaths] = useState('[[1,1.05,1.1],[1,0.98,1.02]]');
  const [mcTimes, setMcTimes] = useState('0, 0.5, 1.0');
  const [mcMu, setMcMu] = useState('0.05');
  const [mcR, setMcR] = useState('0.02');
  const [mcSigma, setMcSigma] = useState('0.2');

  // Ito Lemma
  const ito = useEndpoint();
  const [itoPath, setItoPath] = useState('1, 1.05, 1.1, 1.08, 1.15');
  const [itoTimes, setItoTimes] = useState('0, 0.25, 0.5, 0.75, 1.0');

  // Ito Product Rule
  const itoP = useEndpoint();
  const [itpX, setItpX] = useState('1, 1.05, 1.1, 1.08, 1.15');
  const [itpY, setItpY] = useState('2, 2.1, 1.95, 2.05, 2.2');
  const [itpTimes, setItpTimes] = useState('0, 0.25, 0.5, 0.75, 1.0');

  return (
    <>
      <EndpointCard title="Quadratic Variation" description="[X,X]_t of a path">
        <Row>
          <Field label="Path (comma-sep)" width="300px"><Input value={qvPath} onChange={setQvPath} /></Field>
          <Field label="Times (comma-sep)" width="300px"><Input value={qvTimes} onChange={setQvTimes} /></Field>
          <RunButton loading={qv.loading} onClick={() => qv.run(() => theoryApi.quadraticVariation(qvPath.split(',').map(Number), qvTimes.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={qv.result} error={qv.error} />
      </EndpointCard>

      <EndpointCard title="Covariation" description="[X,Y]_t of two paths">
        <Row>
          <Field label="Path X" width="250px"><Input value={covX} onChange={setCovX} /></Field>
          <Field label="Path Y" width="250px"><Input value={covY} onChange={setCovY} /></Field>
          <Field label="Times" width="250px"><Input value={covTimes} onChange={setCovTimes} /></Field>
          <RunButton loading={cov.loading} onClick={() => cov.run(() => theoryApi.covariation(covX.split(',').map(Number), covY.split(',').map(Number), covTimes.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={cov.result} error={cov.error} />
      </EndpointCard>

      <EndpointCard title="Martingale Test" description="Statistical test for martingale property">
        <Row>
          <Field label="Paths (JSON 2D)" width="350px"><Input value={martPaths} onChange={setMartPaths} /></Field>
          <Field label="Times" width="200px"><Input value={martTimes} onChange={setMartTimes} /></Field>
          <Field label="Confidence"><Input value={martConf} onChange={setMartConf} type="number" /></Field>
          <RunButton loading={mart.loading} onClick={() => mart.run(() => theoryApi.martingaleTest(JSON.parse(martPaths), martTimes.split(',').map(Number), Number(martConf)))} />
        </Row>
        <ResultPanel data={mart.result} error={mart.error} />
      </EndpointCard>

      <EndpointCard title="Risk-Neutral Drift" description="Girsanov: market price of risk">
        <Row>
          <Field label="Mu"><Input value={rndMu} onChange={setRndMu} type="number" /></Field>
          <Field label="r"><Input value={rndR} onChange={setRndR} type="number" /></Field>
          <Field label="Sigma"><Input value={rndSigma} onChange={setRndSigma} type="number" /></Field>
          <RunButton loading={rnd.loading} onClick={() => rnd.run(() => theoryApi.riskNeutralDrift(Number(rndMu), Number(rndR), Number(rndSigma)))} />
        </Row>
        <ResultPanel data={rnd.result} error={rnd.error} />
      </EndpointCard>

      <EndpointCard title="Measure Change" description="Girsanov: Radon-Nikodym derivative paths">
        <Row>
          <Field label="Paths (JSON 2D)" width="300px"><Input value={mcPaths} onChange={setMcPaths} /></Field>
          <Field label="Times" width="200px"><Input value={mcTimes} onChange={setMcTimes} /></Field>
          <Field label="Mu"><Input value={mcMu} onChange={setMcMu} type="number" /></Field>
          <Field label="r"><Input value={mcR} onChange={setMcR} type="number" /></Field>
          <Field label="Sigma"><Input value={mcSigma} onChange={setMcSigma} type="number" /></Field>
          <RunButton loading={mc.loading} onClick={() => mc.run(() => theoryApi.measureChange(JSON.parse(mcPaths), mcTimes.split(',').map(Number), Number(mcMu), Number(mcR), Number(mcSigma)))} />
        </Row>
        <ResultPanel data={mc.result} error={mc.error} />
      </EndpointCard>

      <EndpointCard title="Ito's Lemma" description="Apply Ito's lemma to a path">
        <Row>
          <Field label="Path" width="300px"><Input value={itoPath} onChange={setItoPath} /></Field>
          <Field label="Times" width="300px"><Input value={itoTimes} onChange={setItoTimes} /></Field>
          <RunButton loading={ito.loading} onClick={() => ito.run(() => theoryApi.itoLemma(itoPath.split(',').map(Number), itoTimes.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ito.result} error={ito.error} />
      </EndpointCard>

      <EndpointCard title="Ito Product Rule" description="d(XY) = X dY + Y dX + d[X,Y]">
        <Row>
          <Field label="Path X" width="250px"><Input value={itpX} onChange={setItpX} /></Field>
          <Field label="Path Y" width="250px"><Input value={itpY} onChange={setItpY} /></Field>
          <Field label="Times" width="250px"><Input value={itpTimes} onChange={setItpTimes} /></Field>
          <RunButton loading={itoP.loading} onClick={() => itoP.run(() => theoryApi.itoProductRule(itpX.split(',').map(Number), itpY.split(',').map(Number), itpTimes.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={itoP.result} error={itoP.error} />
      </EndpointCard>
    </>
  );
}
