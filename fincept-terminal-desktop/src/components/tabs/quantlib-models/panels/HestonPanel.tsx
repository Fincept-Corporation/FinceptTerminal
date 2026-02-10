import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { hestonApi } from '../quantlibModelsApi';

export default function HestonPanel() {
  // Price
  const pr = useEndpoint();
  const [prS0, setPrS0] = useState('100');
  const [prV0, setPrV0] = useState('0.04');
  const [prR, setPrR] = useState('0.05');
  const [prKappa, setPrKappa] = useState('2');
  const [prTheta, setPrTheta] = useState('0.04');
  const [prSigmaV, setPrSigmaV] = useState('0.3');
  const [prRho, setPrRho] = useState('-0.7');
  const [prStrike, setPrStrike] = useState('100');
  const [prT, setPrT] = useState('1');
  const [prType, setPrType] = useState('call');

  // Implied Vol
  const iv = useEndpoint();
  const [ivS0, setIvS0] = useState('100');
  const [ivV0, setIvV0] = useState('0.04');
  const [ivR, setIvR] = useState('0.05');
  const [ivKappa, setIvKappa] = useState('2');
  const [ivTheta, setIvTheta] = useState('0.04');
  const [ivSigmaV, setIvSigmaV] = useState('0.3');
  const [ivRho, setIvRho] = useState('-0.7');
  const [ivStrike, setIvStrike] = useState('100');
  const [ivT, setIvT] = useState('1');
  const [ivType, setIvType] = useState('call');

  // Monte Carlo
  const mc = useEndpoint();
  const [mcS0, setMcS0] = useState('100');
  const [mcV0, setMcV0] = useState('0.04');
  const [mcR, setMcR] = useState('0.05');
  const [mcKappa, setMcKappa] = useState('2');
  const [mcTheta, setMcTheta] = useState('0.04');
  const [mcSigmaV, setMcSigmaV] = useState('0.3');
  const [mcRho, setMcRho] = useState('-0.7');
  const [mcStrike, setMcStrike] = useState('100');
  const [mcT, setMcT] = useState('1');
  const [mcType, setMcType] = useState('call');
  const [mcPaths, setMcPaths] = useState('10000');
  const [mcSteps, setMcSteps] = useState('252');

  return (
    <>
      <EndpointCard title="Heston Price" description="Semi-analytical Heston option price">
        <Row>
          <Field label="S0"><Input value={prS0} onChange={setPrS0} type="number" /></Field>
          <Field label="v0"><Input value={prV0} onChange={setPrV0} type="number" /></Field>
          <Field label="r"><Input value={prR} onChange={setPrR} type="number" /></Field>
          <Field label="Kappa"><Input value={prKappa} onChange={setPrKappa} type="number" /></Field>
          <Field label="Theta"><Input value={prTheta} onChange={setPrTheta} type="number" /></Field>
          <Field label="Sigma_v"><Input value={prSigmaV} onChange={setPrSigmaV} type="number" /></Field>
          <Field label="Rho"><Input value={prRho} onChange={setPrRho} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Strike"><Input value={prStrike} onChange={setPrStrike} type="number" /></Field>
          <Field label="T"><Input value={prT} onChange={setPrT} type="number" /></Field>
          <Field label="Type"><Select value={prType} onChange={setPrType} options={['call', 'put']} /></Field>
          <RunButton loading={pr.loading} onClick={() => pr.run(() =>
            hestonApi.price(Number(prS0), Number(prV0), Number(prR), Number(prKappa), Number(prTheta), Number(prSigmaV), Number(prRho), Number(prStrike), Number(prT), prType)
          )} />
        </Row>
        <ResultPanel data={pr.result} error={pr.error} />
      </EndpointCard>

      <EndpointCard title="Heston Implied Vol" description="Implied volatility from Heston model">
        <Row>
          <Field label="S0"><Input value={ivS0} onChange={setIvS0} type="number" /></Field>
          <Field label="v0"><Input value={ivV0} onChange={setIvV0} type="number" /></Field>
          <Field label="r"><Input value={ivR} onChange={setIvR} type="number" /></Field>
          <Field label="Kappa"><Input value={ivKappa} onChange={setIvKappa} type="number" /></Field>
          <Field label="Theta"><Input value={ivTheta} onChange={setIvTheta} type="number" /></Field>
          <Field label="Sigma_v"><Input value={ivSigmaV} onChange={setIvSigmaV} type="number" /></Field>
          <Field label="Rho"><Input value={ivRho} onChange={setIvRho} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Strike"><Input value={ivStrike} onChange={setIvStrike} type="number" /></Field>
          <Field label="T"><Input value={ivT} onChange={setIvT} type="number" /></Field>
          <Field label="Type"><Select value={ivType} onChange={setIvType} options={['call', 'put']} /></Field>
          <RunButton loading={iv.loading} onClick={() => iv.run(() =>
            hestonApi.impliedVol(Number(ivS0), Number(ivV0), Number(ivR), Number(ivKappa), Number(ivTheta), Number(ivSigmaV), Number(ivRho), Number(ivStrike), Number(ivT), ivType)
          )} />
        </Row>
        <ResultPanel data={iv.result} error={iv.error} />
      </EndpointCard>

      <EndpointCard title="Heston Monte Carlo" description="MC simulation under Heston dynamics">
        <Row>
          <Field label="S0"><Input value={mcS0} onChange={setMcS0} type="number" /></Field>
          <Field label="v0"><Input value={mcV0} onChange={setMcV0} type="number" /></Field>
          <Field label="r"><Input value={mcR} onChange={setMcR} type="number" /></Field>
          <Field label="Kappa"><Input value={mcKappa} onChange={setMcKappa} type="number" /></Field>
          <Field label="Theta"><Input value={mcTheta} onChange={setMcTheta} type="number" /></Field>
          <Field label="Sigma_v"><Input value={mcSigmaV} onChange={setMcSigmaV} type="number" /></Field>
          <Field label="Rho"><Input value={mcRho} onChange={setMcRho} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Strike"><Input value={mcStrike} onChange={setMcStrike} type="number" /></Field>
          <Field label="T"><Input value={mcT} onChange={setMcT} type="number" /></Field>
          <Field label="Type"><Select value={mcType} onChange={setMcType} options={['call', 'put']} /></Field>
          <Field label="Paths"><Input value={mcPaths} onChange={setMcPaths} type="number" /></Field>
          <Field label="Steps"><Input value={mcSteps} onChange={setMcSteps} type="number" /></Field>
          <RunButton loading={mc.loading} onClick={() => mc.run(() =>
            hestonApi.monteCarlo(Number(mcS0), Number(mcV0), Number(mcR), Number(mcKappa), Number(mcTheta), Number(mcSigmaV), Number(mcRho), Number(mcStrike), Number(mcT), mcType, Number(mcPaths), Number(mcSteps))
          )} />
        </Row>
        <ResultPanel data={mc.result} error={mc.error} />
      </EndpointCard>
    </>
  );
}
