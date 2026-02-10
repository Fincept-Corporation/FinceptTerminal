import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { mmApi, equityApi, commodityApi, fxApi } from '../quantlibInstrumentsApi';

const sampleReturns = '0.01, -0.02, 0.03, -0.01, 0.005, -0.03, 0.02, 0.01, -0.005, 0.015, -0.025, 0.008, -0.012, 0.018, -0.007, 0.022, -0.015, 0.009, -0.004, 0.013';

export default function MarketsPanel() {
  // Deposit
  const dep = useEndpoint();
  const [depStart, setDepStart] = useState('2024-01-15');
  const [depMat, setDepMat] = useState('2024-04-15');
  const [depNot, setDepNot] = useState('1000000');
  const [depRate, setDepRate] = useState('0.045');

  // T-Bill
  const tb = useEndpoint();
  const [tbSettle, setTbSettle] = useState('2024-01-15');
  const [tbMat, setTbMat] = useState('2024-07-15');
  const [tbFace, setTbFace] = useState('100');
  const [tbDisc, setTbDisc] = useState('0.05');

  // Repo
  const repo = useEndpoint();
  const [repoStart, setRepoStart] = useState('2024-01-15');
  const [repoEnd, setRepoEnd] = useState('2024-01-22');
  const [repoCol, setRepoCol] = useState('10000000');
  const [repoRate, setRepoRate] = useState('0.04');
  const [repoHair, setRepoHair] = useState('0.02');

  // Variance Swap
  const vs = useEndpoint();
  const [vsStrike, setVsStrike] = useState('0.04');
  const [vsRet, setVsRet] = useState(sampleReturns);
  const [vsNot, setVsNot] = useState('100000');

  // Volatility Swap
  const vols = useEndpoint();
  const [volStrike, setVolStrike] = useState('0.2');
  const [volRet, setVolRet] = useState(sampleReturns);
  const [volNot, setVolNot] = useState('100000');

  // Commodity Future
  const cf = useEndpoint();
  const [cfType, setCfType] = useState('crude_oil');
  const [cfSpot, setCfSpot] = useState('75');
  const [cfDel, setCfDel] = useState('2024-06-15');
  const [cfTrade, setCfTrade] = useState('2024-01-15');
  const [cfFut, setCfFut] = useState('77');

  // FX Forward
  const fxf = useEndpoint();
  const [fxSpot, setFxSpot] = useState('1.08');
  const [fxBase, setFxBase] = useState('0.04');
  const [fxQuote, setFxQuote] = useState('0.05');
  const [fxT, setFxT] = useState('0.5');
  const [fxNot, setFxNot] = useState('1000000');
  const [fxPair, setFxPair] = useState('EURUSD');

  // FX Garman-Kohlhagen
  const gk = useEndpoint();
  const [gkSpot, setGkSpot] = useState('1.08');
  const [gkK, setGkK] = useState('1.10');
  const [gkDom, setGkDom] = useState('0.05');
  const [gkFor, setGkFor] = useState('0.04');
  const [gkVol, setGkVol] = useState('0.1');
  const [gkT, setGkT] = useState('0.5');
  const [gkType, setGkType] = useState('call');

  return (
    <>
      <EndpointCard title="Deposit" description="Money market deposit valuation">
        <Row>
          <Field label="Start Date"><Input value={depStart} onChange={setDepStart} /></Field>
          <Field label="Maturity"><Input value={depMat} onChange={setDepMat} /></Field>
          <Field label="Notional"><Input value={depNot} onChange={setDepNot} type="number" /></Field>
          <Field label="Rate"><Input value={depRate} onChange={setDepRate} type="number" /></Field>
          <RunButton loading={dep.loading} onClick={() => dep.run(() => mmApi.deposit(depStart, depMat, Number(depNot), Number(depRate)))} />
        </Row>
        <ResultPanel data={dep.result} error={dep.error} />
      </EndpointCard>

      <EndpointCard title="T-Bill" description="Treasury bill valuation">
        <Row>
          <Field label="Settlement"><Input value={tbSettle} onChange={setTbSettle} /></Field>
          <Field label="Maturity"><Input value={tbMat} onChange={setTbMat} /></Field>
          <Field label="Face Value"><Input value={tbFace} onChange={setTbFace} type="number" /></Field>
          <Field label="Discount Rate"><Input value={tbDisc} onChange={setTbDisc} type="number" /></Field>
          <RunButton loading={tb.loading} onClick={() => tb.run(() => mmApi.tbill(tbSettle, tbMat, Number(tbFace), Number(tbDisc)))} />
        </Row>
        <ResultPanel data={tb.result} error={tb.error} />
      </EndpointCard>

      <EndpointCard title="Repo" description="Repurchase agreement valuation">
        <Row>
          <Field label="Start Date"><Input value={repoStart} onChange={setRepoStart} /></Field>
          <Field label="End Date"><Input value={repoEnd} onChange={setRepoEnd} /></Field>
          <Field label="Collateral"><Input value={repoCol} onChange={setRepoCol} type="number" /></Field>
          <Field label="Repo Rate"><Input value={repoRate} onChange={setRepoRate} type="number" /></Field>
          <Field label="Haircut"><Input value={repoHair} onChange={setRepoHair} type="number" /></Field>
          <RunButton loading={repo.loading} onClick={() => repo.run(() => mmApi.repo(repoStart, repoEnd, Number(repoCol), Number(repoRate), Number(repoHair)))} />
        </Row>
        <ResultPanel data={repo.result} error={repo.error} />
      </EndpointCard>

      <EndpointCard title="Variance Swap" description="Variance swap P&L">
        <Row>
          <Field label="Strike Var"><Input value={vsStrike} onChange={setVsStrike} type="number" /></Field>
          <Field label="Realized Returns" width="350px"><Input value={vsRet} onChange={setVsRet} /></Field>
          <Field label="Notional"><Input value={vsNot} onChange={setVsNot} type="number" /></Field>
          <RunButton loading={vs.loading} onClick={() => vs.run(() => equityApi.varianceSwap(Number(vsStrike), vsRet.split(',').map(Number), Number(vsNot)))} />
        </Row>
        <ResultPanel data={vs.result} error={vs.error} />
      </EndpointCard>

      <EndpointCard title="Volatility Swap" description="Volatility swap P&L">
        <Row>
          <Field label="Strike Vol"><Input value={volStrike} onChange={setVolStrike} type="number" /></Field>
          <Field label="Realized Returns" width="350px"><Input value={volRet} onChange={setVolRet} /></Field>
          <Field label="Notional"><Input value={volNot} onChange={setVolNot} type="number" /></Field>
          <RunButton loading={vols.loading} onClick={() => vols.run(() => equityApi.volatilitySwap(Number(volStrike), volRet.split(',').map(Number), Number(volNot)))} />
        </Row>
        <ResultPanel data={vols.result} error={vols.error} />
      </EndpointCard>

      <EndpointCard title="Commodity Future" description="Commodity futures valuation">
        <Row>
          <Field label="Type"><Select value={cfType} onChange={setCfType} options={['crude_oil', 'gold', 'silver', 'natural_gas', 'wheat', 'corn']} /></Field>
          <Field label="Spot"><Input value={cfSpot} onChange={setCfSpot} type="number" /></Field>
          <Field label="Delivery"><Input value={cfDel} onChange={setCfDel} /></Field>
          <Field label="Trade Date"><Input value={cfTrade} onChange={setCfTrade} /></Field>
          <Field label="Futures Price"><Input value={cfFut} onChange={setCfFut} type="number" /></Field>
          <RunButton loading={cf.loading} onClick={() => cf.run(() => commodityApi.future(cfType, Number(cfSpot), cfDel, cfTrade, Number(cfFut)))} />
        </Row>
        <ResultPanel data={cf.result} error={cf.error} />
      </EndpointCard>

      <EndpointCard title="FX Forward" description="FX forward valuation via CIP">
        <Row>
          <Field label="Spot Rate"><Input value={fxSpot} onChange={setFxSpot} type="number" /></Field>
          <Field label="Base Rate"><Input value={fxBase} onChange={setFxBase} type="number" /></Field>
          <Field label="Quote Rate"><Input value={fxQuote} onChange={setFxQuote} type="number" /></Field>
          <Field label="TTM"><Input value={fxT} onChange={setFxT} type="number" /></Field>
          <Field label="Notional"><Input value={fxNot} onChange={setFxNot} type="number" /></Field>
          <Field label="Pair"><Input value={fxPair} onChange={setFxPair} /></Field>
          <RunButton loading={fxf.loading} onClick={() => fxf.run(() => fxApi.forward(Number(fxSpot), Number(fxBase), Number(fxQuote), Number(fxT), Number(fxNot), fxPair))} />
        </Row>
        <ResultPanel data={fxf.result} error={fxf.error} />
      </EndpointCard>

      <EndpointCard title="FX Garman-Kohlhagen" description="FX option pricing">
        <Row>
          <Field label="Spot"><Input value={gkSpot} onChange={setGkSpot} type="number" /></Field>
          <Field label="Strike"><Input value={gkK} onChange={setGkK} type="number" /></Field>
          <Field label="Dom Rate"><Input value={gkDom} onChange={setGkDom} type="number" /></Field>
          <Field label="For Rate"><Input value={gkFor} onChange={setGkFor} type="number" /></Field>
          <Field label="Volatility"><Input value={gkVol} onChange={setGkVol} type="number" /></Field>
          <Field label="TTX"><Input value={gkT} onChange={setGkT} type="number" /></Field>
          <Field label="Type"><Select value={gkType} onChange={setGkType} options={['call', 'put']} /></Field>
          <RunButton loading={gk.loading} onClick={() => gk.run(() => fxApi.garmanKohlhagen(Number(gkSpot), Number(gkK), Number(gkDom), Number(gkFor), Number(gkVol), Number(gkT), gkType))} />
        </Row>
        <ResultPanel data={gk.result} error={gk.error} />
      </EndpointCard>
    </>
  );
}
