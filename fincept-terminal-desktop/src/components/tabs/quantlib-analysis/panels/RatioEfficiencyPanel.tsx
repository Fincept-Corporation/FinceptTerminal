import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { efficiencyRatioApi, growthRatioApi } from '../quantlibAnalysisApi';

const effRatios = [
  'assetTurnover', 'fixedAssetTurnover', 'inventoryTurnover', 'receivablesTurnover',
  'payablesTurnover', 'dso', 'dio', 'dpo', 'ccc', 'operatingCycle', 'wcTurnover', 'equityTurnover',
];

export default function RatioEfficiencyPanel() {
  // Efficiency — all 12 share same params
  const ef = useEndpoint();
  const [efRatio, setEfRatio] = useState('assetTurnover');
  const [efRev, setEfRev] = useState('1000000');
  const [efCogs, setEfCogs] = useState('600000');
  const [efAR, setEfAR] = useState('150000');
  const [efInv, setEfInv] = useState('100000');
  const [efAP, setEfAP] = useState('80000');
  const [efTA, setEfTA] = useState('2000000');
  const [efFA, setEfFA] = useState('800000');
  const [efWC, setEfWC] = useState('200000');
  const [efTE, setEfTE] = useState('800000');

  // Growth — 4 ratios with different params
  const yoy = useEndpoint();
  const [yoyCur, setYoyCur] = useState('1200000');
  const [yoyPrev, setYoyPrev] = useState('1000000');

  const cagr = useEndpoint();
  const [cagrCur, setCagrCur] = useState('1500000');
  const [cagrPrev, setCagrPrev] = useState('1000000');
  const [cagrPer, setCagrPer] = useState('3');

  const sg = useEndpoint();
  const [sgRet, setSgRet] = useState('0.6');
  const [sgRoe, setSgRoe] = useState('0.15');

  const ig = useEndpoint();
  const [igRet, setIgRet] = useState('0.6');
  const [igRoa, setIgRoa] = useState('0.08');

  const buildEffData = () => ({
    revenue: Number(efRev), cost_of_goods_sold: Number(efCogs),
    accounts_receivable: Number(efAR), inventory: Number(efInv),
    accounts_payable: Number(efAP), total_assets: Number(efTA),
    fixed_assets: Number(efFA), working_capital: Number(efWC), total_equity: Number(efTE),
  });

  return (
    <>
      <EndpointCard title="Efficiency Ratios (12)" description="Turnover, days outstanding, and cycle ratios">
        <Row>
          <Field label="Ratio"><Select value={efRatio} onChange={setEfRatio} options={effRatios} /></Field>
        </Row>
        <Row>
          <Field label="Revenue"><Input value={efRev} onChange={setEfRev} type="number" /></Field>
          <Field label="COGS"><Input value={efCogs} onChange={setEfCogs} type="number" /></Field>
          <Field label="Accts Recv"><Input value={efAR} onChange={setEfAR} type="number" /></Field>
          <Field label="Inventory"><Input value={efInv} onChange={setEfInv} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Accts Payable"><Input value={efAP} onChange={setEfAP} type="number" /></Field>
          <Field label="Total Assets"><Input value={efTA} onChange={setEfTA} type="number" /></Field>
          <Field label="Fixed Assets"><Input value={efFA} onChange={setEfFA} type="number" /></Field>
          <Field label="Working Cap"><Input value={efWC} onChange={setEfWC} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Total Equity"><Input value={efTE} onChange={setEfTE} type="number" /></Field>
          <RunButton loading={ef.loading} onClick={() => ef.run(() => (efficiencyRatioApi as any)[efRatio](buildEffData()))} />
        </Row>
        <ResultPanel data={ef.result} error={ef.error} />
      </EndpointCard>

      <EndpointCard title="YoY Growth" description="Year-over-year growth rate">
        <Row>
          <Field label="Current"><Input value={yoyCur} onChange={setYoyCur} type="number" /></Field>
          <Field label="Previous"><Input value={yoyPrev} onChange={setYoyPrev} type="number" /></Field>
          <RunButton loading={yoy.loading} onClick={() => yoy.run(() => growthRatioApi.yoy(Number(yoyCur), Number(yoyPrev)))} />
        </Row>
        <ResultPanel data={yoy.result} error={yoy.error} />
      </EndpointCard>

      <EndpointCard title="CAGR" description="Compound annual growth rate">
        <Row>
          <Field label="Current"><Input value={cagrCur} onChange={setCagrCur} type="number" /></Field>
          <Field label="Previous"><Input value={cagrPrev} onChange={setCagrPrev} type="number" /></Field>
          <Field label="Periods"><Input value={cagrPer} onChange={setCagrPer} type="number" /></Field>
          <RunButton loading={cagr.loading} onClick={() => cagr.run(() => growthRatioApi.cagr(Number(cagrCur), Number(cagrPrev), Number(cagrPer)))} />
        </Row>
        <ResultPanel data={cagr.result} error={cagr.error} />
      </EndpointCard>

      <EndpointCard title="Sustainable Growth" description="Retention ratio × ROE">
        <Row>
          <Field label="Retention Ratio"><Input value={sgRet} onChange={setSgRet} type="number" /></Field>
          <Field label="ROE"><Input value={sgRoe} onChange={setSgRoe} type="number" /></Field>
          <RunButton loading={sg.loading} onClick={() => sg.run(() => growthRatioApi.sustainable(Number(sgRet), Number(sgRoe)))} />
        </Row>
        <ResultPanel data={sg.result} error={sg.error} />
      </EndpointCard>

      <EndpointCard title="Internal Growth" description="Retention ratio × ROA">
        <Row>
          <Field label="Retention Ratio"><Input value={igRet} onChange={setIgRet} type="number" /></Field>
          <Field label="ROA"><Input value={igRoa} onChange={setIgRoa} type="number" /></Field>
          <RunButton loading={ig.loading} onClick={() => ig.run(() => growthRatioApi.internal(Number(igRet), Number(igRoa)))} />
        </Row>
        <ResultPanel data={ig.result} error={ig.error} />
      </EndpointCard>
    </>
  );
}
