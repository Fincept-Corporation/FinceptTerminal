import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { exactApi } from '../quantlibStochasticApi';

export default function ExactPanel() {
  // Exact GBM
  const eg = useEndpoint();
  const [egS0, setEgS0] = useState('100');
  const [egMu, setEgMu] = useState('0.05');
  const [egSigma, setEgSigma] = useState('0.2');
  const [egT, setEgT] = useState('1');
  const [egSteps, setEgSteps] = useState('252');
  const [egPaths, setEgPaths] = useState('5');

  // Exact CIR
  const ec = useEndpoint();
  const [ecR0, setEcR0] = useState('0.03');
  const [ecKappa, setEcKappa] = useState('0.5');
  const [ecTheta, setEcTheta] = useState('0.05');
  const [ecSigma, setEcSigma] = useState('0.01');
  const [ecT, setEcT] = useState('1');
  const [ecSteps, setEcSteps] = useState('252');
  const [ecPaths, setEcPaths] = useState('3');

  // Exact OU
  const eo = useEndpoint();
  const [eoX0, setEoX0] = useState('0.05');
  const [eoKappa, setEoKappa] = useState('5');
  const [eoTheta, setEoTheta] = useState('0.04');
  const [eoSigma, setEoSigma] = useState('0.01');
  const [eoT, setEoT] = useState('1');
  const [eoSteps, setEoSteps] = useState('252');
  const [eoPaths, setEoPaths] = useState('3');

  // Exact Heston
  const eh = useEndpoint();
  const [ehS0, setEhS0] = useState('100');
  const [ehV0, setEhV0] = useState('0.04');
  const [ehKappa, setEhKappa] = useState('2');
  const [ehTheta, setEhTheta] = useState('0.04');
  const [ehXi, setEhXi] = useState('0.3');
  const [ehRho, setEhRho] = useState('-0.7');
  const [ehR, setEhR] = useState('0.05');
  const [ehT, setEhT] = useState('1');
  const [ehSteps, setEhSteps] = useState('252');
  const [ehPaths, setEhPaths] = useState('3');

  return (
    <>
      <EndpointCard title="Exact GBM" description="Exact (non-discretized) GBM sampling">
        <Row>
          <Field label="S0"><Input value={egS0} onChange={setEgS0} type="number" /></Field>
          <Field label="Mu"><Input value={egMu} onChange={setEgMu} type="number" /></Field>
          <Field label="Sigma"><Input value={egSigma} onChange={setEgSigma} type="number" /></Field>
          <Field label="T"><Input value={egT} onChange={setEgT} type="number" /></Field>
          <Field label="Steps"><Input value={egSteps} onChange={setEgSteps} type="number" /></Field>
          <Field label="Paths"><Input value={egPaths} onChange={setEgPaths} type="number" /></Field>
          <RunButton loading={eg.loading} onClick={() => eg.run(() => exactApi.gbm(Number(egS0), Number(egMu), Number(egSigma), Number(egT), Number(egSteps), Number(egPaths)))} />
        </Row>
        <ResultPanel data={eg.result} error={eg.error} />
      </EndpointCard>

      <EndpointCard title="Exact CIR" description="Exact CIR via non-central chi-squared">
        <Row>
          <Field label="r0"><Input value={ecR0} onChange={setEcR0} type="number" /></Field>
          <Field label="Kappa"><Input value={ecKappa} onChange={setEcKappa} type="number" /></Field>
          <Field label="Theta"><Input value={ecTheta} onChange={setEcTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={ecSigma} onChange={setEcSigma} type="number" /></Field>
          <Field label="T"><Input value={ecT} onChange={setEcT} type="number" /></Field>
          <Field label="Steps"><Input value={ecSteps} onChange={setEcSteps} type="number" /></Field>
          <Field label="Paths"><Input value={ecPaths} onChange={setEcPaths} type="number" /></Field>
          <RunButton loading={ec.loading} onClick={() => ec.run(() => exactApi.cir(Number(ecR0), Number(ecKappa), Number(ecTheta), Number(ecSigma), Number(ecT), Number(ecSteps), Number(ecPaths)))} />
        </Row>
        <ResultPanel data={ec.result} error={ec.error} />
      </EndpointCard>

      <EndpointCard title="Exact OU" description="Exact Ornstein-Uhlenbeck sampling">
        <Row>
          <Field label="X0"><Input value={eoX0} onChange={setEoX0} type="number" /></Field>
          <Field label="Kappa"><Input value={eoKappa} onChange={setEoKappa} type="number" /></Field>
          <Field label="Theta"><Input value={eoTheta} onChange={setEoTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={eoSigma} onChange={setEoSigma} type="number" /></Field>
          <Field label="T"><Input value={eoT} onChange={setEoT} type="number" /></Field>
          <Field label="Steps"><Input value={eoSteps} onChange={setEoSteps} type="number" /></Field>
          <Field label="Paths"><Input value={eoPaths} onChange={setEoPaths} type="number" /></Field>
          <RunButton loading={eo.loading} onClick={() => eo.run(() => exactApi.ou(Number(eoX0), Number(eoKappa), Number(eoTheta), Number(eoSigma), Number(eoT), Number(eoSteps), Number(eoPaths)))} />
        </Row>
        <ResultPanel data={eo.result} error={eo.error} />
      </EndpointCard>

      <EndpointCard title="Exact Heston" description="Broadie-Kaya exact Heston simulation">
        <Row>
          <Field label="S0"><Input value={ehS0} onChange={setEhS0} type="number" /></Field>
          <Field label="v0"><Input value={ehV0} onChange={setEhV0} type="number" /></Field>
          <Field label="Kappa"><Input value={ehKappa} onChange={setEhKappa} type="number" /></Field>
          <Field label="Theta"><Input value={ehTheta} onChange={setEhTheta} type="number" /></Field>
          <Field label="Xi"><Input value={ehXi} onChange={setEhXi} type="number" /></Field>
          <Field label="Rho"><Input value={ehRho} onChange={setEhRho} type="number" /></Field>
          <Field label="r"><Input value={ehR} onChange={setEhR} type="number" /></Field>
          <Field label="T"><Input value={ehT} onChange={setEhT} type="number" /></Field>
          <Field label="Steps"><Input value={ehSteps} onChange={setEhSteps} type="number" /></Field>
          <Field label="Paths"><Input value={ehPaths} onChange={setEhPaths} type="number" /></Field>
          <RunButton loading={eh.loading} onClick={() => eh.run(() => exactApi.heston(Number(ehS0), Number(ehV0), Number(ehKappa), Number(ehTheta), Number(ehXi), Number(ehRho), Number(ehR), Number(ehT), Number(ehSteps), Number(ehPaths)))} />
        </Row>
        <ResultPanel data={eh.result} error={eh.error} />
      </EndpointCard>
    </>
  );
}
