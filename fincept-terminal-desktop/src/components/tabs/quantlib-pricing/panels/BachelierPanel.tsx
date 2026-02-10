import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { bachelierApi } from '../quantlibPricingApi';

export default function BachelierPanel() {
  // Price
  const pr = useEndpoint();
  const [prF, setPrF] = useState('100');
  const [prK, setPrK] = useState('100');
  const [prR, setPrR] = useState('0.05');
  const [prNV, setPrNV] = useState('20');
  const [prT, setPrT] = useState('1');
  const [prType, setPrType] = useState('call');

  // Greeks
  const gr = useEndpoint();
  const [grF, setGrF] = useState('100');
  const [grK, setGrK] = useState('100');
  const [grR, setGrR] = useState('0.05');
  const [grNV, setGrNV] = useState('20');
  const [grT, setGrT] = useState('1');
  const [grType, setGrType] = useState('call');

  // Greeks Full
  const gf = useEndpoint();
  const [gfF, setGfF] = useState('100');
  const [gfK, setGfK] = useState('100');
  const [gfR, setGfR] = useState('0.05');
  const [gfNV, setGfNV] = useState('20');
  const [gfT, setGfT] = useState('1');
  const [gfType, setGfType] = useState('call');

  // Implied Vol
  const iv = useEndpoint();
  const [ivPrice, setIvPrice] = useState('7.97');
  const [ivF, setIvF] = useState('100');
  const [ivK, setIvK] = useState('100');
  const [ivR, setIvR] = useState('0.05');
  const [ivT, setIvT] = useState('1');
  const [ivType, setIvType] = useState('call');

  // Vol Conversion
  const vc = useEndpoint();
  const [vcVol, setVcVol] = useState('0.2');
  const [vcF, setVcF] = useState('100');
  const [vcK, setVcK] = useState('100');
  const [vcT, setVcT] = useState('1');
  const [vcDir, setVcDir] = useState('lognormal_to_normal');

  // Shifted Lognormal
  const sl = useEndpoint();
  const [slF, setSlF] = useState('0.02');
  const [slK, setSlK] = useState('0.02');
  const [slR, setSlR] = useState('0.01');
  const [slV, setSlV] = useState('0.5');
  const [slT, setSlT] = useState('1');
  const [slShift, setSlShift] = useState('0.03');
  const [slType, setSlType] = useState('call');

  return (
    <>
      <EndpointCard title="Bachelier Price" description="Normal model option pricing">
        <Row>
          <Field label="Forward"><Input value={prF} onChange={setPrF} type="number" /></Field>
          <Field label="Strike"><Input value={prK} onChange={setPrK} type="number" /></Field>
          <Field label="Rate"><Input value={prR} onChange={setPrR} type="number" /></Field>
          <Field label="Normal Vol"><Input value={prNV} onChange={setPrNV} type="number" /></Field>
          <Field label="T"><Input value={prT} onChange={setPrT} type="number" /></Field>
          <Field label="Type"><Select value={prType} onChange={setPrType} options={['call', 'put']} /></Field>
          <RunButton loading={pr.loading} onClick={() => pr.run(() => bachelierApi.price(Number(prF), Number(prK), Number(prR), Number(prNV), Number(prT), prType))} />
        </Row>
        <ResultPanel data={pr.result} error={pr.error} />
      </EndpointCard>

      <EndpointCard title="Bachelier Greeks" description="Greeks under normal model">
        <Row>
          <Field label="Forward"><Input value={grF} onChange={setGrF} type="number" /></Field>
          <Field label="Strike"><Input value={grK} onChange={setGrK} type="number" /></Field>
          <Field label="Rate"><Input value={grR} onChange={setGrR} type="number" /></Field>
          <Field label="Normal Vol"><Input value={grNV} onChange={setGrNV} type="number" /></Field>
          <Field label="T"><Input value={grT} onChange={setGrT} type="number" /></Field>
          <Field label="Type"><Select value={grType} onChange={setGrType} options={['call', 'put']} /></Field>
          <RunButton loading={gr.loading} onClick={() => gr.run(() => bachelierApi.greeks(Number(grF), Number(grK), Number(grR), Number(grNV), Number(grT), grType))} />
        </Row>
        <ResultPanel data={gr.result} error={gr.error} />
      </EndpointCard>

      <EndpointCard title="Bachelier Greeks Full" description="All Greeks including higher-order">
        <Row>
          <Field label="Forward"><Input value={gfF} onChange={setGfF} type="number" /></Field>
          <Field label="Strike"><Input value={gfK} onChange={setGfK} type="number" /></Field>
          <Field label="Rate"><Input value={gfR} onChange={setGfR} type="number" /></Field>
          <Field label="Normal Vol"><Input value={gfNV} onChange={setGfNV} type="number" /></Field>
          <Field label="T"><Input value={gfT} onChange={setGfT} type="number" /></Field>
          <Field label="Type"><Select value={gfType} onChange={setGfType} options={['call', 'put']} /></Field>
          <RunButton loading={gf.loading} onClick={() => gf.run(() => bachelierApi.greeksFull(Number(gfF), Number(gfK), Number(gfR), Number(gfNV), Number(gfT), gfType))} />
        </Row>
        <ResultPanel data={gf.result} error={gf.error} />
      </EndpointCard>

      <EndpointCard title="Bachelier Implied Vol" description="Normal implied vol from market price">
        <Row>
          <Field label="Mkt Price"><Input value={ivPrice} onChange={setIvPrice} type="number" /></Field>
          <Field label="Forward"><Input value={ivF} onChange={setIvF} type="number" /></Field>
          <Field label="Strike"><Input value={ivK} onChange={setIvK} type="number" /></Field>
          <Field label="Rate"><Input value={ivR} onChange={setIvR} type="number" /></Field>
          <Field label="T"><Input value={ivT} onChange={setIvT} type="number" /></Field>
          <Field label="Type"><Select value={ivType} onChange={setIvType} options={['call', 'put']} /></Field>
          <RunButton loading={iv.loading} onClick={() => iv.run(() => bachelierApi.impliedVol(Number(ivPrice), Number(ivF), Number(ivK), Number(ivR), Number(ivT), ivType))} />
        </Row>
        <ResultPanel data={iv.result} error={iv.error} />
      </EndpointCard>

      <EndpointCard title="Vol Conversion" description="Lognormal <-> Normal vol conversion">
        <Row>
          <Field label="Volatility"><Input value={vcVol} onChange={setVcVol} type="number" /></Field>
          <Field label="Forward"><Input value={vcF} onChange={setVcF} type="number" /></Field>
          <Field label="Strike"><Input value={vcK} onChange={setVcK} type="number" /></Field>
          <Field label="T"><Input value={vcT} onChange={setVcT} type="number" /></Field>
          <Field label="Direction"><Select value={vcDir} onChange={setVcDir} options={['lognormal_to_normal', 'normal_to_lognormal']} /></Field>
          <RunButton loading={vc.loading} onClick={() => vc.run(() => bachelierApi.volConversion(Number(vcVol), Number(vcF), Number(vcK), Number(vcT), vcDir))} />
        </Row>
        <ResultPanel data={vc.result} error={vc.error} />
      </EndpointCard>

      <EndpointCard title="Shifted Lognormal" description="Displaced diffusion model pricing">
        <Row>
          <Field label="Forward"><Input value={slF} onChange={setSlF} type="number" /></Field>
          <Field label="Strike"><Input value={slK} onChange={setSlK} type="number" /></Field>
          <Field label="Rate"><Input value={slR} onChange={setSlR} type="number" /></Field>
          <Field label="Vol"><Input value={slV} onChange={setSlV} type="number" /></Field>
          <Field label="T"><Input value={slT} onChange={setSlT} type="number" /></Field>
          <Field label="Shift"><Input value={slShift} onChange={setSlShift} type="number" /></Field>
          <Field label="Type"><Select value={slType} onChange={setSlType} options={['call', 'put']} /></Field>
          <RunButton loading={sl.loading} onClick={() => sl.run(() => bachelierApi.shiftedLognormal(Number(slF), Number(slK), Number(slR), Number(slV), Number(slT), Number(slShift), slType))} />
        </Row>
        <ResultPanel data={sl.result} error={sl.error} />
      </EndpointCard>
    </>
  );
}
