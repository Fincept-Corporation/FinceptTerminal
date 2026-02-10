import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { mertonApi, kouApi, varianceGammaApi } from '../quantlibModelsApi';

export default function JumpDiffusionPanel() {
  // Merton Price
  const mp = useEndpoint();
  const [mpS, setMpS] = useState('100');
  const [mpK, setMpK] = useState('100');
  const [mpT, setMpT] = useState('1');
  const [mpR, setMpR] = useState('0.05');
  const [mpSigma, setMpSigma] = useState('0.2');
  const [mpLambda, setMpLambda] = useState('1');
  const [mpMuJ, setMpMuJ] = useState('-0.1');
  const [mpSigmaJ, setMpSigmaJ] = useState('0.15');

  // Merton FFT
  const mf = useEndpoint();
  const [mfS, setMfS] = useState('100');
  const [mfK, setMfK] = useState('100');
  const [mfT, setMfT] = useState('1');
  const [mfR, setMfR] = useState('0.05');
  const [mfSigma, setMfSigma] = useState('0.2');
  const [mfLambda, setMfLambda] = useState('1');
  const [mfMuJ, setMfMuJ] = useState('-0.1');
  const [mfSigmaJ, setMfSigmaJ] = useState('0.15');

  // Kou Price
  const kp = useEndpoint();
  const [kpS, setKpS] = useState('100');
  const [kpK, setKpK] = useState('100');
  const [kpT, setKpT] = useState('1');
  const [kpR, setKpR] = useState('0.05');
  const [kpSigma, setKpSigma] = useState('0.2');
  const [kpLambda, setKpLambda] = useState('1');
  const [kpP, setKpP] = useState('0.4');
  const [kpEta1, setKpEta1] = useState('10');
  const [kpEta2, setKpEta2] = useState('5');

  // Variance Gamma Price
  const vg = useEndpoint();
  const [vgS, setVgS] = useState('100');
  const [vgK, setVgK] = useState('100');
  const [vgT, setVgT] = useState('1');
  const [vgR, setVgR] = useState('0.05');
  const [vgSigma, setVgSigma] = useState('0.2');
  const [vgTheta, setVgTheta] = useState('-0.1');
  const [vgNu, setVgNu] = useState('0.2');

  return (
    <>
      <EndpointCard title="Merton Jump-Diffusion Price" description="Analytic Merton model option pricing">
        <Row>
          <Field label="S"><Input value={mpS} onChange={setMpS} type="number" /></Field>
          <Field label="K"><Input value={mpK} onChange={setMpK} type="number" /></Field>
          <Field label="T"><Input value={mpT} onChange={setMpT} type="number" /></Field>
          <Field label="r"><Input value={mpR} onChange={setMpR} type="number" /></Field>
          <Field label="Sigma"><Input value={mpSigma} onChange={setMpSigma} type="number" /></Field>
          <Field label="Lambda"><Input value={mpLambda} onChange={setMpLambda} type="number" /></Field>
          <Field label="Mu Jump"><Input value={mpMuJ} onChange={setMpMuJ} type="number" /></Field>
          <Field label="Sigma Jump"><Input value={mpSigmaJ} onChange={setMpSigmaJ} type="number" /></Field>
          <RunButton loading={mp.loading} onClick={() => mp.run(() =>
            mertonApi.price(Number(mpS), Number(mpK), Number(mpT), Number(mpR), Number(mpSigma), Number(mpLambda), Number(mpMuJ), Number(mpSigmaJ))
          )} />
        </Row>
        <ResultPanel data={mp.result} error={mp.error} />
      </EndpointCard>

      <EndpointCard title="Merton FFT" description="FFT-based Merton pricing">
        <Row>
          <Field label="S"><Input value={mfS} onChange={setMfS} type="number" /></Field>
          <Field label="K"><Input value={mfK} onChange={setMfK} type="number" /></Field>
          <Field label="T"><Input value={mfT} onChange={setMfT} type="number" /></Field>
          <Field label="r"><Input value={mfR} onChange={setMfR} type="number" /></Field>
          <Field label="Sigma"><Input value={mfSigma} onChange={setMfSigma} type="number" /></Field>
          <Field label="Lambda"><Input value={mfLambda} onChange={setMfLambda} type="number" /></Field>
          <Field label="Mu Jump"><Input value={mfMuJ} onChange={setMfMuJ} type="number" /></Field>
          <Field label="Sigma Jump"><Input value={mfSigmaJ} onChange={setMfSigmaJ} type="number" /></Field>
          <RunButton loading={mf.loading} onClick={() => mf.run(() =>
            mertonApi.fft(Number(mfS), Number(mfK), Number(mfT), Number(mfR), Number(mfSigma), Number(mfLambda), Number(mfMuJ), Number(mfSigmaJ))
          )} />
        </Row>
        <ResultPanel data={mf.result} error={mf.error} />
      </EndpointCard>

      <EndpointCard title="Kou Double-Exponential" description="Kou jump-diffusion model pricing">
        <Row>
          <Field label="S"><Input value={kpS} onChange={setKpS} type="number" /></Field>
          <Field label="K"><Input value={kpK} onChange={setKpK} type="number" /></Field>
          <Field label="T"><Input value={kpT} onChange={setKpT} type="number" /></Field>
          <Field label="r"><Input value={kpR} onChange={setKpR} type="number" /></Field>
          <Field label="Sigma"><Input value={kpSigma} onChange={setKpSigma} type="number" /></Field>
          <Field label="Lambda"><Input value={kpLambda} onChange={setKpLambda} type="number" /></Field>
          <Field label="p (up prob)"><Input value={kpP} onChange={setKpP} type="number" /></Field>
          <Field label="Eta1 (up)"><Input value={kpEta1} onChange={setKpEta1} type="number" /></Field>
          <Field label="Eta2 (down)"><Input value={kpEta2} onChange={setKpEta2} type="number" /></Field>
          <RunButton loading={kp.loading} onClick={() => kp.run(() =>
            kouApi.price(Number(kpS), Number(kpK), Number(kpT), Number(kpR), Number(kpSigma), Number(kpLambda), Number(kpP), Number(kpEta1), Number(kpEta2))
          )} />
        </Row>
        <ResultPanel data={kp.result} error={kp.error} />
      </EndpointCard>

      <EndpointCard title="Variance Gamma" description="VG process option pricing">
        <Row>
          <Field label="S"><Input value={vgS} onChange={setVgS} type="number" /></Field>
          <Field label="K"><Input value={vgK} onChange={setVgK} type="number" /></Field>
          <Field label="T"><Input value={vgT} onChange={setVgT} type="number" /></Field>
          <Field label="r"><Input value={vgR} onChange={setVgR} type="number" /></Field>
          <Field label="Sigma"><Input value={vgSigma} onChange={setVgSigma} type="number" /></Field>
          <Field label="Theta VG"><Input value={vgTheta} onChange={setVgTheta} type="number" /></Field>
          <Field label="Nu"><Input value={vgNu} onChange={setVgNu} type="number" /></Field>
          <RunButton loading={vg.loading} onClick={() => vg.run(() =>
            varianceGammaApi.price(Number(vgS), Number(vgK), Number(vgT), Number(vgR), Number(vgSigma), Number(vgTheta), Number(vgNu))
          )} />
        </Row>
        <ResultPanel data={vg.result} error={vg.error} />
      </EndpointCard>
    </>
  );
}
