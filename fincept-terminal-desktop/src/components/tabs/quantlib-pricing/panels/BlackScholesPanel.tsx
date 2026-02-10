import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { bsApi, kirkApi } from '../quantlibPricingApi';

export default function BlackScholesPanel() {
  // BS Price
  const pr = useEndpoint();
  const [prS, setPrS] = useState('100');
  const [prK, setPrK] = useState('100');
  const [prR, setPrR] = useState('0.05');
  const [prV, setPrV] = useState('0.2');
  const [prT, setPrT] = useState('1');
  const [prType, setPrType] = useState('call');
  const [prDiv, setPrDiv] = useState('0');

  // BS Greeks
  const gr = useEndpoint();
  const [grS, setGrS] = useState('100');
  const [grK, setGrK] = useState('100');
  const [grR, setGrR] = useState('0.05');
  const [grV, setGrV] = useState('0.2');
  const [grT, setGrT] = useState('1');
  const [grType, setGrType] = useState('call');

  // BS Implied Vol
  const iv = useEndpoint();
  const [ivPrice, setIvPrice] = useState('10.45');
  const [ivS, setIvS] = useState('100');
  const [ivK, setIvK] = useState('100');
  const [ivR, setIvR] = useState('0.05');
  const [ivT, setIvT] = useState('1');
  const [ivType, setIvType] = useState('call');

  // BS Greeks Full
  const gf = useEndpoint();
  const [gfS, setGfS] = useState('100');
  const [gfK, setGfK] = useState('100');
  const [gfR, setGfR] = useState('0.05');
  const [gfV, setGfV] = useState('0.2');
  const [gfT, setGfT] = useState('1');
  const [gfType, setGfType] = useState('call');

  // Digital Call
  const dc = useEndpoint();
  const [dcS, setDcS] = useState('100');
  const [dcK, setDcK] = useState('100');
  const [dcR, setDcR] = useState('0.05');
  const [dcV, setDcV] = useState('0.2');
  const [dcT, setDcT] = useState('1');

  // Digital Put
  const dp = useEndpoint();
  const [dpS, setDpS] = useState('100');
  const [dpK, setDpK] = useState('100');
  const [dpR, setDpR] = useState('0.05');
  const [dpV, setDpV] = useState('0.2');
  const [dpT, setDpT] = useState('1');

  // Asset-or-Nothing Call
  const ac = useEndpoint();
  const [acS, setAcS] = useState('100');
  const [acK, setAcK] = useState('100');
  const [acR, setAcR] = useState('0.05');
  const [acV, setAcV] = useState('0.2');
  const [acT, setAcT] = useState('1');

  // Asset-or-Nothing Put
  const ap = useEndpoint();
  const [apS, setApS] = useState('100');
  const [apK, setApK] = useState('100');
  const [apR, setApR] = useState('0.05');
  const [apV, setApV] = useState('0.2');
  const [apT, setApT] = useState('1');

  // Kirk Spread
  const kirk = useEndpoint();
  const [kF1, setKF1] = useState('100');
  const [kF2, setKF2] = useState('95');
  const [kK, setKK] = useState('5');
  const [kR, setKR] = useState('0.05');
  const [kS1, setKS1] = useState('0.2');
  const [kS2, setKS2] = useState('0.25');
  const [kRho, setKRho] = useState('0.5');
  const [kT, setKT] = useState('1');
  const [kType, setKType] = useState('call');

  return (
    <>
      <EndpointCard title="Black-Scholes Price" description="European option price via BS formula">
        <Row>
          <Field label="Spot"><Input value={prS} onChange={setPrS} type="number" /></Field>
          <Field label="Strike"><Input value={prK} onChange={setPrK} type="number" /></Field>
          <Field label="Rate"><Input value={prR} onChange={setPrR} type="number" /></Field>
          <Field label="Vol"><Input value={prV} onChange={setPrV} type="number" /></Field>
          <Field label="T (yrs)"><Input value={prT} onChange={setPrT} type="number" /></Field>
          <Field label="Type"><Select value={prType} onChange={setPrType} options={['call', 'put']} /></Field>
          <Field label="Div Yield"><Input value={prDiv} onChange={setPrDiv} type="number" /></Field>
          <RunButton loading={pr.loading} onClick={() => pr.run(() => bsApi.price(Number(prS), Number(prK), Number(prR), Number(prV), Number(prT), prType, Number(prDiv)))} />
        </Row>
        <ResultPanel data={pr.result} error={pr.error} />
      </EndpointCard>

      <EndpointCard title="Black-Scholes Greeks" description="Delta, Gamma, Theta, Vega, Rho">
        <Row>
          <Field label="Spot"><Input value={grS} onChange={setGrS} type="number" /></Field>
          <Field label="Strike"><Input value={grK} onChange={setGrK} type="number" /></Field>
          <Field label="Rate"><Input value={grR} onChange={setGrR} type="number" /></Field>
          <Field label="Vol"><Input value={grV} onChange={setGrV} type="number" /></Field>
          <Field label="T"><Input value={grT} onChange={setGrT} type="number" /></Field>
          <Field label="Type"><Select value={grType} onChange={setGrType} options={['call', 'put']} /></Field>
          <RunButton loading={gr.loading} onClick={() => gr.run(() => bsApi.greeks(Number(grS), Number(grK), Number(grR), Number(grV), Number(grT), grType))} />
        </Row>
        <ResultPanel data={gr.result} error={gr.error} />
      </EndpointCard>

      <EndpointCard title="BS Implied Volatility" description="Solve for IV from market price">
        <Row>
          <Field label="Mkt Price"><Input value={ivPrice} onChange={setIvPrice} type="number" /></Field>
          <Field label="Spot"><Input value={ivS} onChange={setIvS} type="number" /></Field>
          <Field label="Strike"><Input value={ivK} onChange={setIvK} type="number" /></Field>
          <Field label="Rate"><Input value={ivR} onChange={setIvR} type="number" /></Field>
          <Field label="T"><Input value={ivT} onChange={setIvT} type="number" /></Field>
          <Field label="Type"><Select value={ivType} onChange={setIvType} options={['call', 'put']} /></Field>
          <RunButton loading={iv.loading} onClick={() => iv.run(() => bsApi.impliedVol(Number(ivPrice), Number(ivS), Number(ivK), Number(ivR), Number(ivT), ivType))} />
        </Row>
        <ResultPanel data={iv.result} error={iv.error} />
      </EndpointCard>

      <EndpointCard title="BS Greeks Full" description="All Greeks including higher-order">
        <Row>
          <Field label="Spot"><Input value={gfS} onChange={setGfS} type="number" /></Field>
          <Field label="Strike"><Input value={gfK} onChange={setGfK} type="number" /></Field>
          <Field label="Rate"><Input value={gfR} onChange={setGfR} type="number" /></Field>
          <Field label="Vol"><Input value={gfV} onChange={setGfV} type="number" /></Field>
          <Field label="T"><Input value={gfT} onChange={setGfT} type="number" /></Field>
          <Field label="Type"><Select value={gfType} onChange={setGfType} options={['call', 'put']} /></Field>
          <RunButton loading={gf.loading} onClick={() => gf.run(() => bsApi.greeksFull(Number(gfS), Number(gfK), Number(gfR), Number(gfV), Number(gfT), gfType))} />
        </Row>
        <ResultPanel data={gf.result} error={gf.error} />
      </EndpointCard>

      <EndpointCard title="Digital Call" description="Cash-or-nothing call option">
        <Row>
          <Field label="Spot"><Input value={dcS} onChange={setDcS} type="number" /></Field>
          <Field label="Strike"><Input value={dcK} onChange={setDcK} type="number" /></Field>
          <Field label="Rate"><Input value={dcR} onChange={setDcR} type="number" /></Field>
          <Field label="Vol"><Input value={dcV} onChange={setDcV} type="number" /></Field>
          <Field label="T"><Input value={dcT} onChange={setDcT} type="number" /></Field>
          <RunButton loading={dc.loading} onClick={() => dc.run(() => bsApi.digitalCall(Number(dcS), Number(dcK), Number(dcR), Number(dcV), Number(dcT)))} />
        </Row>
        <ResultPanel data={dc.result} error={dc.error} />
      </EndpointCard>

      <EndpointCard title="Digital Put" description="Cash-or-nothing put option">
        <Row>
          <Field label="Spot"><Input value={dpS} onChange={setDpS} type="number" /></Field>
          <Field label="Strike"><Input value={dpK} onChange={setDpK} type="number" /></Field>
          <Field label="Rate"><Input value={dpR} onChange={setDpR} type="number" /></Field>
          <Field label="Vol"><Input value={dpV} onChange={setDpV} type="number" /></Field>
          <Field label="T"><Input value={dpT} onChange={setDpT} type="number" /></Field>
          <RunButton loading={dp.loading} onClick={() => dp.run(() => bsApi.digitalPut(Number(dpS), Number(dpK), Number(dpR), Number(dpV), Number(dpT)))} />
        </Row>
        <ResultPanel data={dp.result} error={dp.error} />
      </EndpointCard>

      <EndpointCard title="Asset-or-Nothing Call" description="Pays asset value if in-the-money">
        <Row>
          <Field label="Spot"><Input value={acS} onChange={setAcS} type="number" /></Field>
          <Field label="Strike"><Input value={acK} onChange={setAcK} type="number" /></Field>
          <Field label="Rate"><Input value={acR} onChange={setAcR} type="number" /></Field>
          <Field label="Vol"><Input value={acV} onChange={setAcV} type="number" /></Field>
          <Field label="T"><Input value={acT} onChange={setAcT} type="number" /></Field>
          <RunButton loading={ac.loading} onClick={() => ac.run(() => bsApi.assetOrNothingCall(Number(acS), Number(acK), Number(acR), Number(acV), Number(acT)))} />
        </Row>
        <ResultPanel data={ac.result} error={ac.error} />
      </EndpointCard>

      <EndpointCard title="Asset-or-Nothing Put" description="Pays asset value if OTM at expiry">
        <Row>
          <Field label="Spot"><Input value={apS} onChange={setApS} type="number" /></Field>
          <Field label="Strike"><Input value={apK} onChange={setApK} type="number" /></Field>
          <Field label="Rate"><Input value={apR} onChange={setApR} type="number" /></Field>
          <Field label="Vol"><Input value={apV} onChange={setApV} type="number" /></Field>
          <Field label="T"><Input value={apT} onChange={setApT} type="number" /></Field>
          <RunButton loading={ap.loading} onClick={() => ap.run(() => bsApi.assetOrNothingPut(Number(apS), Number(apK), Number(apR), Number(apV), Number(apT)))} />
        </Row>
        <ResultPanel data={ap.result} error={ap.error} />
      </EndpointCard>

      <EndpointCard title="Kirk Spread Option" description="Spread option pricing via Kirk approximation">
        <Row>
          <Field label="F1"><Input value={kF1} onChange={setKF1} type="number" /></Field>
          <Field label="F2"><Input value={kF2} onChange={setKF2} type="number" /></Field>
          <Field label="Strike"><Input value={kK} onChange={setKK} type="number" /></Field>
          <Field label="Rate"><Input value={kR} onChange={setKR} type="number" /></Field>
          <Field label="Sigma1"><Input value={kS1} onChange={setKS1} type="number" /></Field>
          <Field label="Sigma2"><Input value={kS2} onChange={setKS2} type="number" /></Field>
          <Field label="Rho"><Input value={kRho} onChange={setKRho} type="number" /></Field>
          <Field label="T"><Input value={kT} onChange={setKT} type="number" /></Field>
          <Field label="Type"><Select value={kType} onChange={setKType} options={['call', 'put']} /></Field>
          <RunButton loading={kirk.loading} onClick={() => kirk.run(() => kirkApi.spreadPrice(Number(kF1), Number(kF2), Number(kK), Number(kR), Number(kS1), Number(kS2), Number(kRho), Number(kT), kType))} />
        </Row>
        <ResultPanel data={kirk.result} error={kirk.error} />
      </EndpointCard>
    </>
  );
}
