import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { binomialApi, monteCarloApi, pdeApi } from '../quantlibPricingApi';

export default function NumericalPanel() {
  // Binomial Price
  const bp = useEndpoint();
  const [bpS, setBpS] = useState('100');
  const [bpK, setBpK] = useState('100');
  const [bpR, setBpR] = useState('0.05');
  const [bpV, setBpV] = useState('0.2');
  const [bpT, setBpT] = useState('1');
  const [bpSteps, setBpSteps] = useState('100');
  const [bpType, setBpType] = useState('call');
  const [bpExer, setBpExer] = useState('european');

  // Binomial Greeks
  const bg = useEndpoint();
  const [bgS, setBgS] = useState('100');
  const [bgK, setBgK] = useState('100');
  const [bgR, setBgR] = useState('0.05');
  const [bgV, setBgV] = useState('0.2');
  const [bgT, setBgT] = useState('1');
  const [bgSteps, setBgSteps] = useState('100');
  const [bgType, setBgType] = useState('call');
  const [bgExer, setBgExer] = useState('american');

  // Binomial Convergence
  const bc = useEndpoint();
  const [bcS, setBcS] = useState('100');
  const [bcK, setBcK] = useState('100');
  const [bcR, setBcR] = useState('0.05');
  const [bcV, setBcV] = useState('0.2');
  const [bcT, setBcT] = useState('1');
  const [bcMax, setBcMax] = useState('200');
  const [bcType, setBcType] = useState('call');

  // MC Price
  const mc = useEndpoint();
  const [mcS, setMcS] = useState('100');
  const [mcK, setMcK] = useState('100');
  const [mcR, setMcR] = useState('0.05');
  const [mcV, setMcV] = useState('0.2');
  const [mcT, setMcT] = useState('1');
  const [mcN, setMcN] = useState('100000');
  const [mcType, setMcType] = useState('call');

  // MC Barrier
  const mb = useEndpoint();
  const [mbS, setMbS] = useState('100');
  const [mbK, setMbK] = useState('100');
  const [mbB, setMbB] = useState('80');
  const [mbR, setMbR] = useState('0.05');
  const [mbV, setMbV] = useState('0.2');
  const [mbT, setMbT] = useState('1');
  const [mbN, setMbN] = useState('100000');
  const [mbSteps, setMbSteps] = useState('252');
  const [mbBType, setMbBType] = useState('down-and-out');
  const [mbOType, setMbOType] = useState('call');

  // PDE Price
  const pp = useEndpoint();
  const [ppS, setPpS] = useState('100');
  const [ppK, setPpK] = useState('100');
  const [ppR, setPpR] = useState('0.05');
  const [ppV, setPpV] = useState('0.2');
  const [ppT, setPpT] = useState('1');
  const [ppNS, setPpNS] = useState('100');
  const [ppNT, setPpNT] = useState('100');
  const [ppType, setPpType] = useState('call');
  const [ppExer, setPpExer] = useState('european');

  // PDE Greeks
  const pg = useEndpoint();
  const [pgS, setPgS] = useState('100');
  const [pgK, setPgK] = useState('100');
  const [pgR, setPgR] = useState('0.05');
  const [pgV, setPgV] = useState('0.2');
  const [pgT, setPgT] = useState('1');
  const [pgNS, setPgNS] = useState('100');
  const [pgNT, setPgNT] = useState('100');
  const [pgType, setPgType] = useState('call');
  const [pgExer, setPgExer] = useState('european');

  return (
    <>
      <EndpointCard title="Binomial Tree Price" description="CRR binomial tree option pricing">
        <Row>
          <Field label="Spot"><Input value={bpS} onChange={setBpS} type="number" /></Field>
          <Field label="Strike"><Input value={bpK} onChange={setBpK} type="number" /></Field>
          <Field label="Rate"><Input value={bpR} onChange={setBpR} type="number" /></Field>
          <Field label="Vol"><Input value={bpV} onChange={setBpV} type="number" /></Field>
          <Field label="T"><Input value={bpT} onChange={setBpT} type="number" /></Field>
          <Field label="Steps"><Input value={bpSteps} onChange={setBpSteps} type="number" /></Field>
          <Field label="Type"><Select value={bpType} onChange={setBpType} options={['call', 'put']} /></Field>
          <Field label="Exercise"><Select value={bpExer} onChange={setBpExer} options={['european', 'american']} /></Field>
          <RunButton loading={bp.loading} onClick={() => bp.run(() => binomialApi.price(Number(bpS), Number(bpK), Number(bpR), Number(bpV), Number(bpT), Number(bpSteps), bpType, bpExer))} />
        </Row>
        <ResultPanel data={bp.result} error={bp.error} />
      </EndpointCard>

      <EndpointCard title="Binomial Greeks" description="Greeks via binomial tree">
        <Row>
          <Field label="Spot"><Input value={bgS} onChange={setBgS} type="number" /></Field>
          <Field label="Strike"><Input value={bgK} onChange={setBgK} type="number" /></Field>
          <Field label="Rate"><Input value={bgR} onChange={setBgR} type="number" /></Field>
          <Field label="Vol"><Input value={bgV} onChange={setBgV} type="number" /></Field>
          <Field label="T"><Input value={bgT} onChange={setBgT} type="number" /></Field>
          <Field label="Steps"><Input value={bgSteps} onChange={setBgSteps} type="number" /></Field>
          <Field label="Type"><Select value={bgType} onChange={setBgType} options={['call', 'put']} /></Field>
          <Field label="Exercise"><Select value={bgExer} onChange={setBgExer} options={['european', 'american']} /></Field>
          <RunButton loading={bg.loading} onClick={() => bg.run(() => binomialApi.greeks(Number(bgS), Number(bgK), Number(bgR), Number(bgV), Number(bgT), Number(bgSteps), bgType, bgExer))} />
        </Row>
        <ResultPanel data={bg.result} error={bg.error} />
      </EndpointCard>

      <EndpointCard title="Binomial Convergence" description="Price convergence as steps increase">
        <Row>
          <Field label="Spot"><Input value={bcS} onChange={setBcS} type="number" /></Field>
          <Field label="Strike"><Input value={bcK} onChange={setBcK} type="number" /></Field>
          <Field label="Rate"><Input value={bcR} onChange={setBcR} type="number" /></Field>
          <Field label="Vol"><Input value={bcV} onChange={setBcV} type="number" /></Field>
          <Field label="T"><Input value={bcT} onChange={setBcT} type="number" /></Field>
          <Field label="Max Steps"><Input value={bcMax} onChange={setBcMax} type="number" /></Field>
          <Field label="Type"><Select value={bcType} onChange={setBcType} options={['call', 'put']} /></Field>
          <RunButton loading={bc.loading} onClick={() => bc.run(() => binomialApi.convergence(Number(bcS), Number(bcK), Number(bcR), Number(bcV), Number(bcT), Number(bcMax), bcType))} />
        </Row>
        <ResultPanel data={bc.result} error={bc.error} />
      </EndpointCard>

      <EndpointCard title="Monte Carlo Price" description="European option via MC simulation">
        <Row>
          <Field label="Spot"><Input value={mcS} onChange={setMcS} type="number" /></Field>
          <Field label="Strike"><Input value={mcK} onChange={setMcK} type="number" /></Field>
          <Field label="Rate"><Input value={mcR} onChange={setMcR} type="number" /></Field>
          <Field label="Vol"><Input value={mcV} onChange={setMcV} type="number" /></Field>
          <Field label="T"><Input value={mcT} onChange={setMcT} type="number" /></Field>
          <Field label="Simulations"><Input value={mcN} onChange={setMcN} type="number" /></Field>
          <Field label="Type"><Select value={mcType} onChange={setMcType} options={['call', 'put']} /></Field>
          <RunButton loading={mc.loading} onClick={() => mc.run(() => monteCarloApi.price(Number(mcS), Number(mcK), Number(mcR), Number(mcV), Number(mcT), Number(mcN), mcType))} />
        </Row>
        <ResultPanel data={mc.result} error={mc.error} />
      </EndpointCard>

      <EndpointCard title="Monte Carlo Barrier" description="Barrier option via MC simulation">
        <Row>
          <Field label="Spot"><Input value={mbS} onChange={setMbS} type="number" /></Field>
          <Field label="Strike"><Input value={mbK} onChange={setMbK} type="number" /></Field>
          <Field label="Barrier"><Input value={mbB} onChange={setMbB} type="number" /></Field>
          <Field label="Rate"><Input value={mbR} onChange={setMbR} type="number" /></Field>
          <Field label="Vol"><Input value={mbV} onChange={setMbV} type="number" /></Field>
          <Field label="T"><Input value={mbT} onChange={setMbT} type="number" /></Field>
          <Field label="Sims"><Input value={mbN} onChange={setMbN} type="number" /></Field>
          <Field label="Steps"><Input value={mbSteps} onChange={setMbSteps} type="number" /></Field>
          <Field label="Barrier Type"><Select value={mbBType} onChange={setMbBType} options={['down-and-out', 'down-and-in', 'up-and-out', 'up-and-in']} /></Field>
          <Field label="Opt Type"><Select value={mbOType} onChange={setMbOType} options={['call', 'put']} /></Field>
          <RunButton loading={mb.loading} onClick={() => mb.run(() => monteCarloApi.barrier(Number(mbS), Number(mbK), Number(mbB), Number(mbR), Number(mbV), Number(mbT), Number(mbN), Number(mbSteps), mbBType, mbOType))} />
        </Row>
        <ResultPanel data={mb.result} error={mb.error} />
      </EndpointCard>

      <EndpointCard title="PDE / Finite Difference Price" description="Option pricing via finite difference">
        <Row>
          <Field label="Spot"><Input value={ppS} onChange={setPpS} type="number" /></Field>
          <Field label="Strike"><Input value={ppK} onChange={setPpK} type="number" /></Field>
          <Field label="Rate"><Input value={ppR} onChange={setPpR} type="number" /></Field>
          <Field label="Vol"><Input value={ppV} onChange={setPpV} type="number" /></Field>
          <Field label="T"><Input value={ppT} onChange={setPpT} type="number" /></Field>
          <Field label="N Spot"><Input value={ppNS} onChange={setPpNS} type="number" /></Field>
          <Field label="N Time"><Input value={ppNT} onChange={setPpNT} type="number" /></Field>
          <Field label="Type"><Select value={ppType} onChange={setPpType} options={['call', 'put']} /></Field>
          <Field label="Exercise"><Select value={ppExer} onChange={setPpExer} options={['european', 'american']} /></Field>
          <RunButton loading={pp.loading} onClick={() => pp.run(() => pdeApi.price(Number(ppS), Number(ppK), Number(ppR), Number(ppV), Number(ppT), Number(ppNS), Number(ppNT), ppType, ppExer))} />
        </Row>
        <ResultPanel data={pp.result} error={pp.error} />
      </EndpointCard>

      <EndpointCard title="PDE Greeks" description="Greeks via finite difference method">
        <Row>
          <Field label="Spot"><Input value={pgS} onChange={setPgS} type="number" /></Field>
          <Field label="Strike"><Input value={pgK} onChange={setPgK} type="number" /></Field>
          <Field label="Rate"><Input value={pgR} onChange={setPgR} type="number" /></Field>
          <Field label="Vol"><Input value={pgV} onChange={setPgV} type="number" /></Field>
          <Field label="T"><Input value={pgT} onChange={setPgT} type="number" /></Field>
          <Field label="N Spot"><Input value={pgNS} onChange={setPgNS} type="number" /></Field>
          <Field label="N Time"><Input value={pgNT} onChange={setPgNT} type="number" /></Field>
          <Field label="Type"><Select value={pgType} onChange={setPgType} options={['call', 'put']} /></Field>
          <Field label="Exercise"><Select value={pgExer} onChange={setPgExer} options={['european', 'american']} /></Field>
          <RunButton loading={pg.loading} onClick={() => pg.run(() => pdeApi.greeks(Number(pgS), Number(pgK), Number(pgR), Number(pgV), Number(pgT), Number(pgNS), Number(pgNT), pgType, pgExer))} />
        </Row>
        <ResultPanel data={pg.result} error={pg.error} />
      </EndpointCard>
    </>
  );
}
