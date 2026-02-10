import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { black76Api } from '../quantlibPricingApi';

export default function Black76Panel() {
  // Price
  const pr = useEndpoint();
  const [prF, setPrF] = useState('100');
  const [prK, setPrK] = useState('100');
  const [prR, setPrR] = useState('0.05');
  const [prV, setPrV] = useState('0.2');
  const [prT, setPrT] = useState('1');
  const [prType, setPrType] = useState('call');

  // Greeks
  const gr = useEndpoint();
  const [grF, setGrF] = useState('100');
  const [grK, setGrK] = useState('100');
  const [grR, setGrR] = useState('0.05');
  const [grV, setGrV] = useState('0.2');
  const [grT, setGrT] = useState('1');
  const [grType, setGrType] = useState('call');

  // Greeks Full
  const gf = useEndpoint();
  const [gfF, setGfF] = useState('100');
  const [gfK, setGfK] = useState('100');
  const [gfR, setGfR] = useState('0.05');
  const [gfV, setGfV] = useState('0.2');
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

  // Caplet
  const cap = useEndpoint();
  const [capFR, setCapFR] = useState('0.05');
  const [capK, setCapK] = useState('0.04');
  const [capDF, setCapDF] = useState('0.95');
  const [capV, setCapV] = useState('0.2');
  const [capS, setCapS] = useState('0.5');
  const [capE, setCapE] = useState('1');
  const [capN, setCapN] = useState('1000000');

  // Floorlet
  const fl = useEndpoint();
  const [flFR, setFlFR] = useState('0.03');
  const [flK, setFlK] = useState('0.04');
  const [flDF, setFlDF] = useState('0.95');
  const [flV, setFlV] = useState('0.2');
  const [flS, setFlS] = useState('0.5');
  const [flE, setFlE] = useState('1');
  const [flN, setFlN] = useState('1000000');

  // Swaption
  const sw = useEndpoint();
  const [swFSR, setSwFSR] = useState('0.05');
  const [swK, setSwK] = useState('0.04');
  const [swA, setSwA] = useState('4.5');
  const [swV, setSwV] = useState('0.2');
  const [swT, setSwT] = useState('1');
  const [swN, setSwN] = useState('1000000');

  return (
    <>
      <EndpointCard title="Black76 Price" description="Futures/forward option price">
        <Row>
          <Field label="Forward"><Input value={prF} onChange={setPrF} type="number" /></Field>
          <Field label="Strike"><Input value={prK} onChange={setPrK} type="number" /></Field>
          <Field label="Rate"><Input value={prR} onChange={setPrR} type="number" /></Field>
          <Field label="Vol"><Input value={prV} onChange={setPrV} type="number" /></Field>
          <Field label="T"><Input value={prT} onChange={setPrT} type="number" /></Field>
          <Field label="Type"><Select value={prType} onChange={setPrType} options={['call', 'put']} /></Field>
          <RunButton loading={pr.loading} onClick={() => pr.run(() => black76Api.price(Number(prF), Number(prK), Number(prR), Number(prV), Number(prT), prType))} />
        </Row>
        <ResultPanel data={pr.result} error={pr.error} />
      </EndpointCard>

      <EndpointCard title="Black76 Greeks" description="Greeks for futures options">
        <Row>
          <Field label="Forward"><Input value={grF} onChange={setGrF} type="number" /></Field>
          <Field label="Strike"><Input value={grK} onChange={setGrK} type="number" /></Field>
          <Field label="Rate"><Input value={grR} onChange={setGrR} type="number" /></Field>
          <Field label="Vol"><Input value={grV} onChange={setGrV} type="number" /></Field>
          <Field label="T"><Input value={grT} onChange={setGrT} type="number" /></Field>
          <Field label="Type"><Select value={grType} onChange={setGrType} options={['call', 'put']} /></Field>
          <RunButton loading={gr.loading} onClick={() => gr.run(() => black76Api.greeks(Number(grF), Number(grK), Number(grR), Number(grV), Number(grT), grType))} />
        </Row>
        <ResultPanel data={gr.result} error={gr.error} />
      </EndpointCard>

      <EndpointCard title="Black76 Greeks Full" description="All Greeks including higher-order">
        <Row>
          <Field label="Forward"><Input value={gfF} onChange={setGfF} type="number" /></Field>
          <Field label="Strike"><Input value={gfK} onChange={setGfK} type="number" /></Field>
          <Field label="Rate"><Input value={gfR} onChange={setGfR} type="number" /></Field>
          <Field label="Vol"><Input value={gfV} onChange={setGfV} type="number" /></Field>
          <Field label="T"><Input value={gfT} onChange={setGfT} type="number" /></Field>
          <Field label="Type"><Select value={gfType} onChange={setGfType} options={['call', 'put']} /></Field>
          <RunButton loading={gf.loading} onClick={() => gf.run(() => black76Api.greeksFull(Number(gfF), Number(gfK), Number(gfR), Number(gfV), Number(gfT), gfType))} />
        </Row>
        <ResultPanel data={gf.result} error={gf.error} />
      </EndpointCard>

      <EndpointCard title="Black76 Implied Vol" description="Implied vol from market price">
        <Row>
          <Field label="Mkt Price"><Input value={ivPrice} onChange={setIvPrice} type="number" /></Field>
          <Field label="Forward"><Input value={ivF} onChange={setIvF} type="number" /></Field>
          <Field label="Strike"><Input value={ivK} onChange={setIvK} type="number" /></Field>
          <Field label="Rate"><Input value={ivR} onChange={setIvR} type="number" /></Field>
          <Field label="T"><Input value={ivT} onChange={setIvT} type="number" /></Field>
          <Field label="Type"><Select value={ivType} onChange={setIvType} options={['call', 'put']} /></Field>
          <RunButton loading={iv.loading} onClick={() => iv.run(() => black76Api.impliedVol(Number(ivPrice), Number(ivF), Number(ivK), Number(ivR), Number(ivT), ivType))} />
        </Row>
        <ResultPanel data={iv.result} error={iv.error} />
      </EndpointCard>

      <EndpointCard title="Caplet" description="Interest rate caplet pricing">
        <Row>
          <Field label="Fwd Rate"><Input value={capFR} onChange={setCapFR} type="number" /></Field>
          <Field label="Strike"><Input value={capK} onChange={setCapK} type="number" /></Field>
          <Field label="Disc Factor"><Input value={capDF} onChange={setCapDF} type="number" /></Field>
          <Field label="Vol"><Input value={capV} onChange={setCapV} type="number" /></Field>
          <Field label="T Start"><Input value={capS} onChange={setCapS} type="number" /></Field>
          <Field label="T End"><Input value={capE} onChange={setCapE} type="number" /></Field>
          <Field label="Notional"><Input value={capN} onChange={setCapN} type="number" /></Field>
          <RunButton loading={cap.loading} onClick={() => cap.run(() => black76Api.caplet(Number(capFR), Number(capK), Number(capDF), Number(capV), Number(capS), Number(capE), Number(capN)))} />
        </Row>
        <ResultPanel data={cap.result} error={cap.error} />
      </EndpointCard>

      <EndpointCard title="Floorlet" description="Interest rate floorlet pricing">
        <Row>
          <Field label="Fwd Rate"><Input value={flFR} onChange={setFlFR} type="number" /></Field>
          <Field label="Strike"><Input value={flK} onChange={setFlK} type="number" /></Field>
          <Field label="Disc Factor"><Input value={flDF} onChange={setFlDF} type="number" /></Field>
          <Field label="Vol"><Input value={flV} onChange={setFlV} type="number" /></Field>
          <Field label="T Start"><Input value={flS} onChange={setFlS} type="number" /></Field>
          <Field label="T End"><Input value={flE} onChange={setFlE} type="number" /></Field>
          <Field label="Notional"><Input value={flN} onChange={setFlN} type="number" /></Field>
          <RunButton loading={fl.loading} onClick={() => fl.run(() => black76Api.floorlet(Number(flFR), Number(flK), Number(flDF), Number(flV), Number(flS), Number(flE), Number(flN)))} />
        </Row>
        <ResultPanel data={fl.result} error={fl.error} />
      </EndpointCard>

      <EndpointCard title="Swaption" description="European swaption pricing (Black76)">
        <Row>
          <Field label="Fwd Swap Rate"><Input value={swFSR} onChange={setSwFSR} type="number" /></Field>
          <Field label="Strike"><Input value={swK} onChange={setSwK} type="number" /></Field>
          <Field label="Annuity"><Input value={swA} onChange={setSwA} type="number" /></Field>
          <Field label="Vol"><Input value={swV} onChange={setSwV} type="number" /></Field>
          <Field label="T Expiry"><Input value={swT} onChange={setSwT} type="number" /></Field>
          <Field label="Notional"><Input value={swN} onChange={setSwN} type="number" /></Field>
          <RunButton loading={sw.loading} onClick={() => sw.run(() => black76Api.swaption(Number(swFSR), Number(swK), Number(swA), Number(swV), Number(swT), true, Number(swN)))} />
        </Row>
        <ResultPanel data={sw.result} error={sw.error} />
      </EndpointCard>
    </>
  );
}
