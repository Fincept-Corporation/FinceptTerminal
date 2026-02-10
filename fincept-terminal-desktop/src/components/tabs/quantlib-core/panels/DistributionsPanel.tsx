import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../shared';
import { distributionsApi } from '../quantlibCoreApi';

export default function DistributionsPanel() {
  // Normal
  const nCdf = useEndpoint(); const [ncX, setNcX] = useState('0');
  const nPdf = useEndpoint(); const [npX, setNpX] = useState('0');
  const nPpf = useEndpoint(); const [nppP, setNppP] = useState('0.975');
  // Student-t
  const tCdf = useEndpoint(); const [tcX, setTcX] = useState('1.96'); const [tcDf, setTcDf] = useState('10');
  const tPdf = useEndpoint(); const [tpX, setTpX] = useState('0'); const [tpDf, setTpDf] = useState('10');
  const tPpf = useEndpoint(); const [tppP, setTppP] = useState('0.975'); const [tppDf, setTppDf] = useState('10');
  // Chi2
  const c2Cdf = useEndpoint(); const [c2X, setC2X] = useState('9.21'); const [c2Df, setC2Df] = useState('2');
  const c2Pdf = useEndpoint(); const [c2pX, setC2pX] = useState('3'); const [c2pDf, setC2pDf] = useState('2');
  // Gamma
  const gCdf = useEndpoint(); const [gcX, setGcX] = useState('2'); const [gcA, setGcA] = useState('2'); const [gcB, setGcB] = useState('1');
  const gPdf = useEndpoint(); const [gpX, setGpX] = useState('2'); const [gpA, setGpA] = useState('2'); const [gpB, setGpB] = useState('1');
  // Exponential
  const eCdf = useEndpoint(); const [ecX, setEcX] = useState('1'); const [ecR, setEcR] = useState('1');
  const ePdf = useEndpoint(); const [epX, setEpX] = useState('1'); const [epR, setEpR] = useState('1');
  const ePpf = useEndpoint(); const [eppP, setEppP] = useState('0.5'); const [eppR, setEppR] = useState('1');
  // Bivariate Normal
  const bvn = useEndpoint(); const [bvX, setBvX] = useState('0'); const [bvY, setBvY] = useState('0'); const [bvRho, setBvRho] = useState('0.5');

  return (
    <>
      <EndpointCard title="Normal CDF" description="\u03A6(x)">
        <Row>
          <Field label="x"><Input value={ncX} onChange={setNcX} type="number" /></Field>
          <RunButton loading={nCdf.loading} onClick={() => nCdf.run(() => distributionsApi.normalCdf(Number(ncX)))} />
        </Row>
        <ResultPanel data={nCdf.result} error={nCdf.error} />
      </EndpointCard>

      <EndpointCard title="Normal PDF" description="\u03C6(x)">
        <Row>
          <Field label="x"><Input value={npX} onChange={setNpX} type="number" /></Field>
          <RunButton loading={nPdf.loading} onClick={() => nPdf.run(() => distributionsApi.normalPdf(Number(npX)))} />
        </Row>
        <ResultPanel data={nPdf.result} error={nPdf.error} />
      </EndpointCard>

      <EndpointCard title="Normal PPF (Inverse CDF)" description="\u03A6\u207B\u00B9(p)">
        <Row>
          <Field label="p"><Input value={nppP} onChange={setNppP} type="number" /></Field>
          <RunButton loading={nPpf.loading} onClick={() => nPpf.run(() => distributionsApi.normalPpf(Number(nppP)))} />
        </Row>
        <ResultPanel data={nPpf.result} error={nPpf.error} />
      </EndpointCard>

      <EndpointCard title="Student-t CDF" description="T distribution CDF">
        <Row>
          <Field label="x"><Input value={tcX} onChange={setTcX} type="number" /></Field>
          <Field label="df"><Input value={tcDf} onChange={setTcDf} type="number" /></Field>
          <RunButton loading={tCdf.loading} onClick={() => tCdf.run(() => distributionsApi.tCdf(Number(tcX), Number(tcDf)))} />
        </Row>
        <ResultPanel data={tCdf.result} error={tCdf.error} />
      </EndpointCard>

      <EndpointCard title="Student-t PDF" description="T distribution PDF">
        <Row>
          <Field label="x"><Input value={tpX} onChange={setTpX} type="number" /></Field>
          <Field label="df"><Input value={tpDf} onChange={setTpDf} type="number" /></Field>
          <RunButton loading={tPdf.loading} onClick={() => tPdf.run(() => distributionsApi.tPdf(Number(tpX), Number(tpDf)))} />
        </Row>
        <ResultPanel data={tPdf.result} error={tPdf.error} />
      </EndpointCard>

      <EndpointCard title="Student-t PPF" description="T distribution inverse CDF">
        <Row>
          <Field label="p"><Input value={tppP} onChange={setTppP} type="number" /></Field>
          <Field label="df"><Input value={tppDf} onChange={setTppDf} type="number" /></Field>
          <RunButton loading={tPpf.loading} onClick={() => tPpf.run(() => distributionsApi.tPpf(Number(tppP), Number(tppDf)))} />
        </Row>
        <ResultPanel data={tPpf.result} error={tPpf.error} />
      </EndpointCard>

      <EndpointCard title="Chi-Squared CDF" description="\u03C7\u00B2 CDF">
        <Row>
          <Field label="x"><Input value={c2X} onChange={setC2X} type="number" /></Field>
          <Field label="df"><Input value={c2Df} onChange={setC2Df} type="number" /></Field>
          <RunButton loading={c2Cdf.loading} onClick={() => c2Cdf.run(() => distributionsApi.chi2Cdf(Number(c2X), Number(c2Df)))} />
        </Row>
        <ResultPanel data={c2Cdf.result} error={c2Cdf.error} />
      </EndpointCard>

      <EndpointCard title="Chi-Squared PDF" description="\u03C7\u00B2 PDF">
        <Row>
          <Field label="x"><Input value={c2pX} onChange={setC2pX} type="number" /></Field>
          <Field label="df"><Input value={c2pDf} onChange={setC2pDf} type="number" /></Field>
          <RunButton loading={c2Pdf.loading} onClick={() => c2Pdf.run(() => distributionsApi.chi2Pdf(Number(c2pX), Number(c2pDf)))} />
        </Row>
        <ResultPanel data={c2Pdf.result} error={c2Pdf.error} />
      </EndpointCard>

      <EndpointCard title="Gamma CDF" description="Gamma distribution CDF">
        <Row>
          <Field label="x"><Input value={gcX} onChange={setGcX} type="number" /></Field>
          <Field label="\u03B1"><Input value={gcA} onChange={setGcA} type="number" /></Field>
          <Field label="\u03B2"><Input value={gcB} onChange={setGcB} type="number" /></Field>
          <RunButton loading={gCdf.loading} onClick={() => gCdf.run(() => distributionsApi.gammaCdf(Number(gcX), Number(gcA), Number(gcB)))} />
        </Row>
        <ResultPanel data={gCdf.result} error={gCdf.error} />
      </EndpointCard>

      <EndpointCard title="Gamma PDF" description="Gamma distribution PDF">
        <Row>
          <Field label="x"><Input value={gpX} onChange={setGpX} type="number" /></Field>
          <Field label="\u03B1"><Input value={gpA} onChange={setGpA} type="number" /></Field>
          <Field label="\u03B2"><Input value={gpB} onChange={setGpB} type="number" /></Field>
          <RunButton loading={gPdf.loading} onClick={() => gPdf.run(() => distributionsApi.gammaPdf(Number(gpX), Number(gpA), Number(gpB)))} />
        </Row>
        <ResultPanel data={gPdf.result} error={gPdf.error} />
      </EndpointCard>

      <EndpointCard title="Exponential CDF" description="Exp distribution CDF">
        <Row>
          <Field label="x"><Input value={ecX} onChange={setEcX} type="number" /></Field>
          <Field label="Rate (\u03BB)"><Input value={ecR} onChange={setEcR} type="number" /></Field>
          <RunButton loading={eCdf.loading} onClick={() => eCdf.run(() => distributionsApi.expCdf(Number(ecX), Number(ecR)))} />
        </Row>
        <ResultPanel data={eCdf.result} error={eCdf.error} />
      </EndpointCard>

      <EndpointCard title="Exponential PDF" description="Exp distribution PDF">
        <Row>
          <Field label="x"><Input value={epX} onChange={setEpX} type="number" /></Field>
          <Field label="Rate (\u03BB)"><Input value={epR} onChange={setEpR} type="number" /></Field>
          <RunButton loading={ePdf.loading} onClick={() => ePdf.run(() => distributionsApi.expPdf(Number(epX), Number(epR)))} />
        </Row>
        <ResultPanel data={ePdf.result} error={ePdf.error} />
      </EndpointCard>

      <EndpointCard title="Exponential PPF" description="Exp distribution inverse CDF">
        <Row>
          <Field label="p"><Input value={eppP} onChange={setEppP} type="number" /></Field>
          <Field label="Rate (\u03BB)"><Input value={eppR} onChange={setEppR} type="number" /></Field>
          <RunButton loading={ePpf.loading} onClick={() => ePpf.run(() => distributionsApi.expPpf(Number(eppP), Number(eppR)))} />
        </Row>
        <ResultPanel data={ePpf.result} error={ePpf.error} />
      </EndpointCard>

      <EndpointCard title="Bivariate Normal CDF" description="Joint normal CDF">
        <Row>
          <Field label="x"><Input value={bvX} onChange={setBvX} type="number" /></Field>
          <Field label="y"><Input value={bvY} onChange={setBvY} type="number" /></Field>
          <Field label="\u03C1"><Input value={bvRho} onChange={setBvRho} type="number" /></Field>
          <RunButton loading={bvn.loading} onClick={() => bvn.run(() => distributionsApi.bivariateNormalCdf(Number(bvX), Number(bvY), Number(bvRho)))} />
        </Row>
        <ResultPanel data={bvn.result} error={bvn.error} />
      </EndpointCard>
    </>
  );
}
