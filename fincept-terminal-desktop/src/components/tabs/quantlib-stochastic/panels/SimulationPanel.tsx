import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { simulationApi } from '../quantlibStochasticApi';

export default function SimulationPanel() {
  // Euler-Maruyama 1D
  const em = useEndpoint();
  const [emX0, setEmX0] = useState('1');
  const [emT, setEmT] = useState('1');
  const [emSteps, setEmSteps] = useState('252');
  const [emPaths, setEmPaths] = useState('5');
  const [emMu, setEmMu] = useState('0.05');
  const [emSigma, setEmSigma] = useState('0.2');

  // Euler-Maruyama ND
  const emN = useEndpoint();
  const [emnX0, setEmnX0] = useState('1, 1');
  const [emnT, setEmnT] = useState('1');
  const [emnSteps, setEmnSteps] = useState('252');
  const [emnMu, setEmnMu] = useState('0.05, 0.03');
  const [emnSigma, setEmnSigma] = useState('0.2, 0.15');

  // Milstein 1D
  const mil = useEndpoint();
  const [milX0, setMilX0] = useState('1');
  const [milT, setMilT] = useState('1');
  const [milSteps, setMilSteps] = useState('252');
  const [milPaths, setMilPaths] = useState('5');
  const [milMu, setMilMu] = useState('0.05');
  const [milSigma, setMilSigma] = useState('0.2');

  // Milstein ND
  const milN = useEndpoint();
  const [milnX0, setMilnX0] = useState('1, 1');
  const [milnT, setMilnT] = useState('1');
  const [milnSteps, setMilnSteps] = useState('252');
  const [milnMu, setMilnMu] = useState('0.05, 0.03');
  const [milnSigma, setMilnSigma] = useState('0.2, 0.15');

  // Multilevel MC
  const mlmc = useEndpoint();
  const [mlX0, setMlX0] = useState('1');
  const [mlT, setMlT] = useState('1');
  const [mlMu, setMlMu] = useState('0.05');
  const [mlSigma, setMlSigma] = useState('0.2');
  const [mlLevels, setMlLevels] = useState('5');
  const [mlSamples, setMlSamples] = useState('1000');

  return (
    <>
      <EndpointCard title="Euler-Maruyama (1D)" description="First-order SDE discretization">
        <Row>
          <Field label="x0"><Input value={emX0} onChange={setEmX0} type="number" /></Field>
          <Field label="T"><Input value={emT} onChange={setEmT} type="number" /></Field>
          <Field label="Steps"><Input value={emSteps} onChange={setEmSteps} type="number" /></Field>
          <Field label="Paths"><Input value={emPaths} onChange={setEmPaths} type="number" /></Field>
          <Field label="Mu"><Input value={emMu} onChange={setEmMu} type="number" /></Field>
          <Field label="Sigma"><Input value={emSigma} onChange={setEmSigma} type="number" /></Field>
          <RunButton loading={em.loading} onClick={() => em.run(() => simulationApi.eulerMaruyama(Number(emX0), Number(emT), Number(emSteps), Number(emPaths), Number(emMu), Number(emSigma)))} />
        </Row>
        <ResultPanel data={em.result} error={em.error} />
      </EndpointCard>

      <EndpointCard title="Euler-Maruyama (ND)" description="Multi-dimensional EM scheme">
        <Row>
          <Field label="x0 (comma-sep)" width="160px"><Input value={emnX0} onChange={setEmnX0} /></Field>
          <Field label="T"><Input value={emnT} onChange={setEmnT} type="number" /></Field>
          <Field label="Steps"><Input value={emnSteps} onChange={setEmnSteps} type="number" /></Field>
          <Field label="Mu (comma-sep)" width="160px"><Input value={emnMu} onChange={setEmnMu} /></Field>
          <Field label="Sigma (comma-sep)" width="160px"><Input value={emnSigma} onChange={setEmnSigma} /></Field>
          <RunButton loading={emN.loading} onClick={() => emN.run(() => simulationApi.eulerMaruyamaNd(emnX0.split(',').map(Number), Number(emnT), Number(emnSteps), emnMu.split(',').map(Number), emnSigma.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={emN.result} error={emN.error} />
      </EndpointCard>

      <EndpointCard title="Milstein (1D)" description="Second-order SDE discretization">
        <Row>
          <Field label="x0"><Input value={milX0} onChange={setMilX0} type="number" /></Field>
          <Field label="T"><Input value={milT} onChange={setMilT} type="number" /></Field>
          <Field label="Steps"><Input value={milSteps} onChange={setMilSteps} type="number" /></Field>
          <Field label="Paths"><Input value={milPaths} onChange={setMilPaths} type="number" /></Field>
          <Field label="Mu"><Input value={milMu} onChange={setMilMu} type="number" /></Field>
          <Field label="Sigma"><Input value={milSigma} onChange={setMilSigma} type="number" /></Field>
          <RunButton loading={mil.loading} onClick={() => mil.run(() => simulationApi.milstein(Number(milX0), Number(milT), Number(milSteps), Number(milPaths), Number(milMu), Number(milSigma)))} />
        </Row>
        <ResultPanel data={mil.result} error={mil.error} />
      </EndpointCard>

      <EndpointCard title="Milstein (ND)" description="Multi-dimensional Milstein scheme">
        <Row>
          <Field label="x0 (comma-sep)" width="160px"><Input value={milnX0} onChange={setMilnX0} /></Field>
          <Field label="T"><Input value={milnT} onChange={setMilnT} type="number" /></Field>
          <Field label="Steps"><Input value={milnSteps} onChange={setMilnSteps} type="number" /></Field>
          <Field label="Mu (comma-sep)" width="160px"><Input value={milnMu} onChange={setMilnMu} /></Field>
          <Field label="Sigma (comma-sep)" width="160px"><Input value={milnSigma} onChange={setMilnSigma} /></Field>
          <RunButton loading={milN.loading} onClick={() => milN.run(() => simulationApi.milsteinNd(milnX0.split(',').map(Number), Number(milnT), Number(milnSteps), milnMu.split(',').map(Number), milnSigma.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={milN.result} error={milN.error} />
      </EndpointCard>

      <EndpointCard title="Multilevel Monte Carlo" description="MLMC variance reduction">
        <Row>
          <Field label="x0"><Input value={mlX0} onChange={setMlX0} type="number" /></Field>
          <Field label="T"><Input value={mlT} onChange={setMlT} type="number" /></Field>
          <Field label="Mu"><Input value={mlMu} onChange={setMlMu} type="number" /></Field>
          <Field label="Sigma"><Input value={mlSigma} onChange={setMlSigma} type="number" /></Field>
          <Field label="Levels"><Input value={mlLevels} onChange={setMlLevels} type="number" /></Field>
          <Field label="Samples/Level"><Input value={mlSamples} onChange={setMlSamples} type="number" /></Field>
          <RunButton loading={mlmc.loading} onClick={() => mlmc.run(() => simulationApi.multilevelMc(Number(mlX0), Number(mlT), Number(mlMu), Number(mlSigma), Number(mlLevels), Number(mlSamples)))} />
        </Row>
        <ResultPanel data={mlmc.result} error={mlmc.error} />
      </EndpointCard>
    </>
  );
}
