import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { shortRateApi } from '../quantlibModelsApi';

export default function ShortRatePanel() {
  // Bond Price
  const bp = useEndpoint();
  const [bpModel, setBpModel] = useState('vasicek');
  const [bpKappa, setBpKappa] = useState('0.1');
  const [bpTheta, setBpTheta] = useState('0.05');
  const [bpSigma, setBpSigma] = useState('0.01');
  const [bpR0, setBpR0] = useState('0.03');
  const [bpMat, setBpMat] = useState('1, 2, 3, 5, 10');

  // Yield Curve
  const yc = useEndpoint();
  const [ycModel, setYcModel] = useState('vasicek');
  const [ycKappa, setYcKappa] = useState('0.1');
  const [ycTheta, setYcTheta] = useState('0.05');
  const [ycSigma, setYcSigma] = useState('0.01');
  const [ycR0, setYcR0] = useState('0.03');
  const [ycMat, setYcMat] = useState('0.5, 1, 2, 3, 5, 7, 10, 20, 30');

  // Monte Carlo
  const mc = useEndpoint();
  const [mcModel, setMcModel] = useState('vasicek');
  const [mcKappa, setMcKappa] = useState('0.1');
  const [mcTheta, setMcTheta] = useState('0.05');
  const [mcSigma, setMcSigma] = useState('0.01');
  const [mcR0, setMcR0] = useState('0.03');
  const [mcT, setMcT] = useState('1');
  const [mcSteps, setMcSteps] = useState('252');
  const [mcPaths, setMcPaths] = useState('100');

  // Bond Option
  const bo = useEndpoint();
  const [boModel, setBoModel] = useState('vasicek');
  const [boKappa, setBoKappa] = useState('0.1');
  const [boTheta, setBoTheta] = useState('0.05');
  const [boSigma, setBoSigma] = useState('0.01');
  const [boR0, setBoR0] = useState('0.03');
  const [boT, setBoT] = useState('1');

  const models = ['vasicek', 'cir', 'ho_lee', 'hull_white'];

  return (
    <>
      <EndpointCard title="Short Rate Bond Price" description="Zero-coupon bond prices from short rate model">
        <Row>
          <Field label="Model"><Select value={bpModel} onChange={setBpModel} options={models} /></Field>
          <Field label="Kappa"><Input value={bpKappa} onChange={setBpKappa} type="number" /></Field>
          <Field label="Theta"><Input value={bpTheta} onChange={setBpTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={bpSigma} onChange={setBpSigma} type="number" /></Field>
          <Field label="r0"><Input value={bpR0} onChange={setBpR0} type="number" /></Field>
          <Field label="Maturities" width="200px"><Input value={bpMat} onChange={setBpMat} /></Field>
          <RunButton loading={bp.loading} onClick={() => bp.run(() =>
            shortRateApi.bondPrice(bpModel, Number(bpKappa), Number(bpTheta), Number(bpSigma), Number(bpR0), bpMat.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={bp.result} error={bp.error} />
      </EndpointCard>

      <EndpointCard title="Short Rate Yield Curve" description="Yield curve from short rate model">
        <Row>
          <Field label="Model"><Select value={ycModel} onChange={setYcModel} options={models} /></Field>
          <Field label="Kappa"><Input value={ycKappa} onChange={setYcKappa} type="number" /></Field>
          <Field label="Theta"><Input value={ycTheta} onChange={setYcTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={ycSigma} onChange={setYcSigma} type="number" /></Field>
          <Field label="r0"><Input value={ycR0} onChange={setYcR0} type="number" /></Field>
          <Field label="Maturities" width="250px"><Input value={ycMat} onChange={setYcMat} /></Field>
          <RunButton loading={yc.loading} onClick={() => yc.run(() =>
            shortRateApi.yieldCurve(ycModel, Number(ycKappa), Number(ycTheta), Number(ycSigma), Number(ycR0), ycMat.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={yc.result} error={yc.error} />
      </EndpointCard>

      <EndpointCard title="Short Rate Monte Carlo" description="Simulate short rate paths">
        <Row>
          <Field label="Model"><Select value={mcModel} onChange={setMcModel} options={models} /></Field>
          <Field label="Kappa"><Input value={mcKappa} onChange={setMcKappa} type="number" /></Field>
          <Field label="Theta"><Input value={mcTheta} onChange={setMcTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={mcSigma} onChange={setMcSigma} type="number" /></Field>
          <Field label="r0"><Input value={mcR0} onChange={setMcR0} type="number" /></Field>
          <Field label="T"><Input value={mcT} onChange={setMcT} type="number" /></Field>
          <Field label="Steps"><Input value={mcSteps} onChange={setMcSteps} type="number" /></Field>
          <Field label="Paths"><Input value={mcPaths} onChange={setMcPaths} type="number" /></Field>
          <RunButton loading={mc.loading} onClick={() => mc.run(() =>
            shortRateApi.monteCarlo(mcModel, Number(mcKappa), Number(mcTheta), Number(mcSigma), Number(mcR0), Number(mcT), Number(mcSteps), Number(mcPaths))
          )} />
        </Row>
        <ResultPanel data={mc.result} error={mc.error} />
      </EndpointCard>

      <EndpointCard title="Short Rate Bond Option" description="Bond option under short rate model">
        <Row>
          <Field label="Model"><Select value={boModel} onChange={setBoModel} options={models} /></Field>
          <Field label="Kappa"><Input value={boKappa} onChange={setBoKappa} type="number" /></Field>
          <Field label="Theta"><Input value={boTheta} onChange={setBoTheta} type="number" /></Field>
          <Field label="Sigma"><Input value={boSigma} onChange={setBoSigma} type="number" /></Field>
          <Field label="r0"><Input value={boR0} onChange={setBoR0} type="number" /></Field>
          <Field label="T"><Input value={boT} onChange={setBoT} type="number" /></Field>
          <RunButton loading={bo.loading} onClick={() => bo.run(() =>
            shortRateApi.bondOption(boModel, Number(boKappa), Number(boTheta), Number(boSigma), Number(boR0), Number(boT))
          )} />
        </Row>
        <ResultPanel data={bo.result} error={bo.error} />
      </EndpointCard>
    </>
  );
}
