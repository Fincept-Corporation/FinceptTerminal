import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { profitRatioApi, liquidityRatioApi } from '../quantlibAnalysisApi';

const profitRatios = [
  { key: 'grossMargin', label: 'Gross Margin' }, { key: 'operatingMargin', label: 'Operating Margin' },
  { key: 'netMargin', label: 'Net Margin' }, { key: 'ebitdaMargin', label: 'EBITDA Margin' },
  { key: 'roa', label: 'ROA' }, { key: 'roe', label: 'ROE' },
  { key: 'roic', label: 'ROIC' }, { key: 'roce', label: 'ROCE' },
  { key: 'basicEarningPower', label: 'Basic Earning Power' },
  { key: 'cashRoa', label: 'Cash ROA' }, { key: 'cashRoe', label: 'Cash ROE' }, { key: 'cashRoic', label: 'Cash ROIC' },
] as const;

const liqRatios = [
  { key: 'currentRatio', label: 'Current Ratio' }, { key: 'quickRatio', label: 'Quick Ratio' },
  { key: 'cashRatio', label: 'Cash Ratio' }, { key: 'absoluteLiquidity', label: 'Absolute Liquidity' },
  { key: 'defensiveInterval', label: 'Defensive Interval' },
  { key: 'workingCapital', label: 'Working Capital' }, { key: 'workingCapitalRatio', label: 'WC Ratio' },
] as const;

export default function RatioProfitPanel() {
  // Profitability — all 12 share same params
  const pr = useEndpoint();
  const [prRatio, setPrRatio] = useState<string>('grossMargin');
  const [prRev, setPrRev] = useState('1000000');
  const [prCogs, setPrCogs] = useState('600000');
  const [prOpInc, setPrOpInc] = useState('200000');
  const [prEbit, setPrEbit] = useState('200000');
  const [prEbitda, setPrEbitda] = useState('250000');
  const [prNI, setPrNI] = useState('150000');
  const [prTA, setPrTA] = useState('2000000');
  const [prTE, setPrTE] = useState('800000');
  const [prIC, setPrIC] = useState('1500000');
  const [prOCF, setPrOCF] = useState('220000');

  // Liquidity — all 7 share same params
  const lq = useEndpoint();
  const [lqRatio, setLqRatio] = useState<string>('currentRatio');
  const [lqCA, setLqCA] = useState('500000');
  const [lqCL, setLqCL] = useState('300000');
  const [lqCash, setLqCash] = useState('200000');
  const [lqMS, setLqMS] = useState('50000');
  const [lqAR, setLqAR] = useState('150000');
  const [lqInv, setLqInv] = useState('100000');
  const [lqOpEx, setLqOpEx] = useState('800000');

  const buildProfitData = () => ({
    revenue: Number(prRev), cost_of_goods_sold: Number(prCogs), gross_profit: Number(prRev) - Number(prCogs),
    operating_income: Number(prOpInc), ebit: Number(prEbit), ebitda: Number(prEbitda),
    net_income: Number(prNI), total_assets: Number(prTA), total_equity: Number(prTE),
    invested_capital: Number(prIC), operating_cash_flow: Number(prOCF),
  });

  const buildLiqData = () => ({
    current_assets: Number(lqCA), current_liabilities: Number(lqCL), cash: Number(lqCash),
    marketable_securities: Number(lqMS), accounts_receivable: Number(lqAR),
    inventory: Number(lqInv), operating_expenses: Number(lqOpEx),
  });

  return (
    <>
      <EndpointCard title="Profitability Ratios (12)" description="Margin, return, and cash-based profitability ratios">
        <Row>
          <Field label="Ratio"><Select value={prRatio} onChange={setPrRatio} options={profitRatios.map(r => r.key)} /></Field>
        </Row>
        <Row>
          <Field label="Revenue"><Input value={prRev} onChange={setPrRev} type="number" /></Field>
          <Field label="COGS"><Input value={prCogs} onChange={setPrCogs} type="number" /></Field>
          <Field label="Op Income"><Input value={prOpInc} onChange={setPrOpInc} type="number" /></Field>
          <Field label="EBIT"><Input value={prEbit} onChange={setPrEbit} type="number" /></Field>
        </Row>
        <Row>
          <Field label="EBITDA"><Input value={prEbitda} onChange={setPrEbitda} type="number" /></Field>
          <Field label="Net Income"><Input value={prNI} onChange={setPrNI} type="number" /></Field>
          <Field label="Total Assets"><Input value={prTA} onChange={setPrTA} type="number" /></Field>
          <Field label="Total Equity"><Input value={prTE} onChange={setPrTE} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Invested Capital"><Input value={prIC} onChange={setPrIC} type="number" /></Field>
          <Field label="Op Cash Flow"><Input value={prOCF} onChange={setPrOCF} type="number" /></Field>
          <RunButton loading={pr.loading} onClick={() => pr.run(() => (profitRatioApi as any)[prRatio](buildProfitData()))} />
        </Row>
        <ResultPanel data={pr.result} error={pr.error} />
      </EndpointCard>

      <EndpointCard title="Liquidity Ratios (7)" description="Current, quick, cash, and working capital ratios">
        <Row>
          <Field label="Ratio"><Select value={lqRatio} onChange={setLqRatio} options={liqRatios.map(r => r.key)} /></Field>
        </Row>
        <Row>
          <Field label="Current Assets"><Input value={lqCA} onChange={setLqCA} type="number" /></Field>
          <Field label="Current Liab"><Input value={lqCL} onChange={setLqCL} type="number" /></Field>
          <Field label="Cash"><Input value={lqCash} onChange={setLqCash} type="number" /></Field>
          <Field label="Mkt Securities"><Input value={lqMS} onChange={setLqMS} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Accts Receivable"><Input value={lqAR} onChange={setLqAR} type="number" /></Field>
          <Field label="Inventory"><Input value={lqInv} onChange={setLqInv} type="number" /></Field>
          <Field label="Op Expenses"><Input value={lqOpEx} onChange={setLqOpEx} type="number" /></Field>
          <RunButton loading={lq.loading} onClick={() => lq.run(() => (liquidityRatioApi as any)[lqRatio](buildLiqData()))} />
        </Row>
        <ResultPanel data={lq.result} error={lq.error} />
      </EndpointCard>
    </>
  );
}
