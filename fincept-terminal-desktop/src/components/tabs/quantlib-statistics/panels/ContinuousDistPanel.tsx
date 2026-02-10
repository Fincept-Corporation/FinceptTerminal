import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { normalApi, lognormalApi, studentTApi, chiSquaredApi, gammaApi, betaApi, exponentialApi, fDistApi } from '../quantlibStatisticsApi';

export default function ContinuousDistPanel() {
  // --- Normal ---
  const nProp = useEndpoint();
  const [nMu, setNMu] = useState('0');
  const [nSig, setNSig] = useState('1');
  const nPdf = useEndpoint();
  const [npX, setNpX] = useState('0');
  const nCdf = useEndpoint();
  const [ncX, setNcX] = useState('0');
  const nPpf = useEndpoint();
  const [nppP, setNppP] = useState('0.5');

  // --- Lognormal ---
  const lnProp = useEndpoint();
  const [lnMu, setLnMu] = useState('0');
  const [lnSig, setLnSig] = useState('1');
  const lnPdf = useEndpoint();
  const [lnpX, setLnpX] = useState('1');
  const lnCdf = useEndpoint();
  const [lncX, setLncX] = useState('1');
  const lnPpf = useEndpoint();
  const [lnppP, setLnppP] = useState('0.5');

  // --- Student-t ---
  const tProp = useEndpoint();
  const [tDf, setTDf] = useState('5');
  const [tMu, setTMu] = useState('0');
  const [tSig, setTSig] = useState('1');
  const tPdf = useEndpoint();
  const [tpX, setTpX] = useState('0');
  const tCdf = useEndpoint();
  const [tcX, setTcX] = useState('0');

  // --- Chi-Squared ---
  const chi2Prop = useEndpoint();
  const [chi2Df, setChi2Df] = useState('5');
  const chi2Pdf = useEndpoint();
  const [chi2pX, setChi2pX] = useState('3');
  const chi2Cdf = useEndpoint();
  const [chi2cX, setChi2cX] = useState('3');

  // --- Gamma ---
  const gProp = useEndpoint();
  const [gAlpha, setGAlpha] = useState('2');
  const [gBeta, setGBeta] = useState('1');
  const gPdf = useEndpoint();
  const [gpX, setGpX] = useState('1');
  const gCdf = useEndpoint();
  const [gcX, setGcX] = useState('1');

  // --- Beta ---
  const bProp = useEndpoint();
  const [bAlpha, setBAlpha] = useState('2');
  const [bBeta, setBBeta] = useState('5');
  const bPdf = useEndpoint();
  const [bpX, setBpX] = useState('0.5');
  const bCdf = useEndpoint();
  const [bcX, setBcX] = useState('0.5');

  // --- Exponential ---
  const eProp = useEndpoint();
  const [eRate, setERate] = useState('1');
  const ePdf = useEndpoint();
  const [epX, setEpX] = useState('1');
  const eCdf = useEndpoint();
  const [ecX, setEcX] = useState('1');
  const ePpf = useEndpoint();
  const [eppP, setEppP] = useState('0.5');

  // --- F ---
  const fProp = useEndpoint();
  const [fDf1, setFDf1] = useState('5');
  const [fDf2, setFDf2] = useState('10');
  const fPdf = useEndpoint();
  const [fpX, setFpX] = useState('1');

  // Helper for action select
  const [normalAction, setNormalAction] = useState('properties');
  const [lnAction, setLnAction] = useState('properties');
  const [tAction, setTAction] = useState('properties');
  const [chi2Action, setChi2Action] = useState('properties');
  const [gammaAction, setGammaAction] = useState('properties');
  const [betaAction, setBetaAction] = useState('properties');
  const [expAction, setExpAction] = useState('properties');
  const [fAction, setFAction] = useState('properties');

  return (
    <>
      {/* Normal */}
      <EndpointCard title="Normal Distribution" description="N(mu, sigma) — properties, PDF, CDF, PPF">
        <Row>
          <Field label="Mu"><Input value={nMu} onChange={setNMu} type="number" /></Field>
          <Field label="Sigma"><Input value={nSig} onChange={setNSig} type="number" /></Field>
          <Field label="Action"><Select value={normalAction} onChange={setNormalAction} options={['properties', 'pdf', 'cdf', 'ppf']} /></Field>
          {(normalAction === 'pdf' || normalAction === 'cdf') && <Field label="x"><Input value={npX} onChange={setNpX} type="number" /></Field>}
          {normalAction === 'ppf' && <Field label="p"><Input value={nppP} onChange={setNppP} type="number" /></Field>}
          <RunButton loading={normalAction === 'properties' ? nProp.loading : normalAction === 'pdf' ? nPdf.loading : normalAction === 'cdf' ? nCdf.loading : nPpf.loading}
            onClick={() => {
              if (normalAction === 'properties') nProp.run(() => normalApi.properties(Number(nMu), Number(nSig)));
              else if (normalAction === 'pdf') nPdf.run(() => normalApi.pdf(Number(nMu), Number(nSig), Number(npX)));
              else if (normalAction === 'cdf') nCdf.run(() => normalApi.cdf(Number(nMu), Number(nSig), Number(ncX)));
              else nPpf.run(() => normalApi.ppf(Number(nMu), Number(nSig), Number(nppP)));
            }} />
        </Row>
        <ResultPanel data={normalAction === 'properties' ? nProp.result : normalAction === 'pdf' ? nPdf.result : normalAction === 'cdf' ? nCdf.result : nPpf.result}
          error={normalAction === 'properties' ? nProp.error : normalAction === 'pdf' ? nPdf.error : normalAction === 'cdf' ? nCdf.error : nPpf.error} />
      </EndpointCard>

      {/* Lognormal */}
      <EndpointCard title="Lognormal Distribution" description="LogN(mu, sigma) — properties, PDF, CDF, PPF">
        <Row>
          <Field label="Mu"><Input value={lnMu} onChange={setLnMu} type="number" /></Field>
          <Field label="Sigma"><Input value={lnSig} onChange={setLnSig} type="number" /></Field>
          <Field label="Action"><Select value={lnAction} onChange={setLnAction} options={['properties', 'pdf', 'cdf', 'ppf']} /></Field>
          {(lnAction === 'pdf' || lnAction === 'cdf') && <Field label="x"><Input value={lnpX} onChange={setLnpX} type="number" /></Field>}
          {lnAction === 'ppf' && <Field label="p"><Input value={lnppP} onChange={setLnppP} type="number" /></Field>}
          <RunButton loading={lnAction === 'properties' ? lnProp.loading : lnAction === 'pdf' ? lnPdf.loading : lnAction === 'cdf' ? lnCdf.loading : lnPpf.loading}
            onClick={() => {
              if (lnAction === 'properties') lnProp.run(() => lognormalApi.properties(Number(lnMu), Number(lnSig)));
              else if (lnAction === 'pdf') lnPdf.run(() => lognormalApi.pdf(Number(lnMu), Number(lnSig), Number(lnpX)));
              else if (lnAction === 'cdf') lnCdf.run(() => lognormalApi.cdf(Number(lnMu), Number(lnSig), Number(lncX)));
              else lnPpf.run(() => lognormalApi.ppf(Number(lnMu), Number(lnSig), Number(lnppP)));
            }} />
        </Row>
        <ResultPanel data={lnAction === 'properties' ? lnProp.result : lnAction === 'pdf' ? lnPdf.result : lnAction === 'cdf' ? lnCdf.result : lnPpf.result}
          error={lnAction === 'properties' ? lnProp.error : lnAction === 'pdf' ? lnPdf.error : lnAction === 'cdf' ? lnCdf.error : lnPpf.error} />
      </EndpointCard>

      {/* Student-t */}
      <EndpointCard title="Student-t Distribution" description="t(df, mu, sigma) — properties, PDF, CDF">
        <Row>
          <Field label="df"><Input value={tDf} onChange={setTDf} type="number" /></Field>
          <Field label="Mu"><Input value={tMu} onChange={setTMu} type="number" /></Field>
          <Field label="Sigma"><Input value={tSig} onChange={setTSig} type="number" /></Field>
          <Field label="Action"><Select value={tAction} onChange={setTAction} options={['properties', 'pdf', 'cdf']} /></Field>
          {tAction === 'pdf' && <Field label="x"><Input value={tpX} onChange={setTpX} type="number" /></Field>}
          {tAction === 'cdf' && <Field label="x"><Input value={tcX} onChange={setTcX} type="number" /></Field>}
          <RunButton loading={tAction === 'properties' ? tProp.loading : tAction === 'pdf' ? tPdf.loading : tCdf.loading}
            onClick={() => {
              if (tAction === 'properties') tProp.run(() => studentTApi.properties(Number(tDf), Number(tMu), Number(tSig)));
              else if (tAction === 'pdf') tPdf.run(() => studentTApi.pdf(Number(tDf), Number(tMu), Number(tSig), Number(tpX)));
              else tCdf.run(() => studentTApi.cdf(Number(tDf), Number(tMu), Number(tSig), Number(tcX)));
            }} />
        </Row>
        <ResultPanel data={tAction === 'properties' ? tProp.result : tAction === 'pdf' ? tPdf.result : tCdf.result}
          error={tAction === 'properties' ? tProp.error : tAction === 'pdf' ? tPdf.error : tCdf.error} />
      </EndpointCard>

      {/* Chi-Squared */}
      <EndpointCard title="Chi-Squared Distribution" description="chi2(df) — properties, PDF, CDF">
        <Row>
          <Field label="df"><Input value={chi2Df} onChange={setChi2Df} type="number" /></Field>
          <Field label="Action"><Select value={chi2Action} onChange={setChi2Action} options={['properties', 'pdf', 'cdf']} /></Field>
          {(chi2Action === 'pdf' || chi2Action === 'cdf') && <Field label="x"><Input value={chi2Action === 'pdf' ? chi2pX : chi2cX} onChange={chi2Action === 'pdf' ? setChi2pX : setChi2cX} type="number" /></Field>}
          <RunButton loading={chi2Action === 'properties' ? chi2Prop.loading : chi2Action === 'pdf' ? chi2Pdf.loading : chi2Cdf.loading}
            onClick={() => {
              if (chi2Action === 'properties') chi2Prop.run(() => chiSquaredApi.properties(Number(chi2Df)));
              else if (chi2Action === 'pdf') chi2Pdf.run(() => chiSquaredApi.pdf(Number(chi2Df), Number(chi2pX)));
              else chi2Cdf.run(() => chiSquaredApi.cdf(Number(chi2Df), Number(chi2cX)));
            }} />
        </Row>
        <ResultPanel data={chi2Action === 'properties' ? chi2Prop.result : chi2Action === 'pdf' ? chi2Pdf.result : chi2Cdf.result}
          error={chi2Action === 'properties' ? chi2Prop.error : chi2Action === 'pdf' ? chi2Pdf.error : chi2Cdf.error} />
      </EndpointCard>

      {/* Gamma */}
      <EndpointCard title="Gamma Distribution" description="Gamma(alpha, beta) — properties, PDF, CDF">
        <Row>
          <Field label="Alpha"><Input value={gAlpha} onChange={setGAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={gBeta} onChange={setGBeta} type="number" /></Field>
          <Field label="Action"><Select value={gammaAction} onChange={setGammaAction} options={['properties', 'pdf', 'cdf']} /></Field>
          {(gammaAction === 'pdf' || gammaAction === 'cdf') && <Field label="x"><Input value={gammaAction === 'pdf' ? gpX : gcX} onChange={gammaAction === 'pdf' ? setGpX : setGcX} type="number" /></Field>}
          <RunButton loading={gammaAction === 'properties' ? gProp.loading : gammaAction === 'pdf' ? gPdf.loading : gCdf.loading}
            onClick={() => {
              if (gammaAction === 'properties') gProp.run(() => gammaApi.properties(Number(gAlpha), Number(gBeta)));
              else if (gammaAction === 'pdf') gPdf.run(() => gammaApi.pdf(Number(gAlpha), Number(gBeta), Number(gpX)));
              else gCdf.run(() => gammaApi.cdf(Number(gAlpha), Number(gBeta), Number(gcX)));
            }} />
        </Row>
        <ResultPanel data={gammaAction === 'properties' ? gProp.result : gammaAction === 'pdf' ? gPdf.result : gCdf.result}
          error={gammaAction === 'properties' ? gProp.error : gammaAction === 'pdf' ? gPdf.error : gCdf.error} />
      </EndpointCard>

      {/* Beta */}
      <EndpointCard title="Beta Distribution" description="Beta(alpha, beta) — properties, PDF, CDF">
        <Row>
          <Field label="Alpha"><Input value={bAlpha} onChange={setBAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={bBeta} onChange={setBBeta} type="number" /></Field>
          <Field label="Action"><Select value={betaAction} onChange={setBetaAction} options={['properties', 'pdf', 'cdf']} /></Field>
          {(betaAction === 'pdf' || betaAction === 'cdf') && <Field label="x"><Input value={betaAction === 'pdf' ? bpX : bcX} onChange={betaAction === 'pdf' ? setBpX : setBcX} type="number" /></Field>}
          <RunButton loading={betaAction === 'properties' ? bProp.loading : betaAction === 'pdf' ? bPdf.loading : bCdf.loading}
            onClick={() => {
              if (betaAction === 'properties') bProp.run(() => betaApi.properties(Number(bAlpha), Number(bBeta)));
              else if (betaAction === 'pdf') bPdf.run(() => betaApi.pdf(Number(bAlpha), Number(bBeta), Number(bpX)));
              else bCdf.run(() => betaApi.cdf(Number(bAlpha), Number(bBeta), Number(bcX)));
            }} />
        </Row>
        <ResultPanel data={betaAction === 'properties' ? bProp.result : betaAction === 'pdf' ? bPdf.result : bCdf.result}
          error={betaAction === 'properties' ? bProp.error : betaAction === 'pdf' ? bPdf.error : bCdf.error} />
      </EndpointCard>

      {/* Exponential */}
      <EndpointCard title="Exponential Distribution" description="Exp(rate) — properties, PDF, CDF, PPF">
        <Row>
          <Field label="Rate"><Input value={eRate} onChange={setERate} type="number" /></Field>
          <Field label="Action"><Select value={expAction} onChange={setExpAction} options={['properties', 'pdf', 'cdf', 'ppf']} /></Field>
          {(expAction === 'pdf' || expAction === 'cdf') && <Field label="x"><Input value={expAction === 'pdf' ? epX : ecX} onChange={expAction === 'pdf' ? setEpX : setEcX} type="number" /></Field>}
          {expAction === 'ppf' && <Field label="p"><Input value={eppP} onChange={setEppP} type="number" /></Field>}
          <RunButton loading={expAction === 'properties' ? eProp.loading : expAction === 'pdf' ? ePdf.loading : expAction === 'cdf' ? eCdf.loading : ePpf.loading}
            onClick={() => {
              if (expAction === 'properties') eProp.run(() => exponentialApi.properties(Number(eRate)));
              else if (expAction === 'pdf') ePdf.run(() => exponentialApi.pdf(Number(eRate), Number(epX)));
              else if (expAction === 'cdf') eCdf.run(() => exponentialApi.cdf(Number(eRate), Number(ecX)));
              else ePpf.run(() => exponentialApi.ppf(Number(eRate), Number(eppP)));
            }} />
        </Row>
        <ResultPanel data={expAction === 'properties' ? eProp.result : expAction === 'pdf' ? ePdf.result : expAction === 'cdf' ? eCdf.result : ePpf.result}
          error={expAction === 'properties' ? eProp.error : expAction === 'pdf' ? ePdf.error : expAction === 'cdf' ? eCdf.error : ePpf.error} />
      </EndpointCard>

      {/* F Distribution */}
      <EndpointCard title="F Distribution" description="F(df1, df2) — properties, PDF">
        <Row>
          <Field label="df1"><Input value={fDf1} onChange={setFDf1} type="number" /></Field>
          <Field label="df2"><Input value={fDf2} onChange={setFDf2} type="number" /></Field>
          <Field label="Action"><Select value={fAction} onChange={setFAction} options={['properties', 'pdf']} /></Field>
          {fAction === 'pdf' && <Field label="x"><Input value={fpX} onChange={setFpX} type="number" /></Field>}
          <RunButton loading={fAction === 'properties' ? fProp.loading : fPdf.loading}
            onClick={() => {
              if (fAction === 'properties') fProp.run(() => fDistApi.properties(Number(fDf1), Number(fDf2)));
              else fPdf.run(() => fDistApi.pdf(Number(fDf1), Number(fDf2), Number(fpX)));
            }} />
        </Row>
        <ResultPanel data={fAction === 'properties' ? fProp.result : fPdf.result}
          error={fAction === 'properties' ? fProp.error : fPdf.error} />
      </EndpointCard>
    </>
  );
}
