import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { leverageRatioApi, cfRatioApi } from '../quantlibAnalysisApi';

const levRatios = [
  'debtToEquity', 'debtToAssets', 'debtToCapital', 'equityRatio', 'equityMultiplier',
  'interestCoverage', 'ebitdaInterestCoverage', 'cashCoverage', 'netDebtToEbitda',
  'ltdToEquity', 'financialLeverage', 'capitalization', 'dscr',
];

const cfRatios = [
  'ocfToDebt', 'ocfRatio', 'fcf', 'cashConversionQuality',
  'capexToOcf', 'capexToDepreciation', 'cashDividendCoverage', 'reinvestment',
];

export default function RatioLeverageCfPanel() {
  // Leverage — 13 ratios share same params
  const lev = useEndpoint();
  const [levRatio, setLevRatio] = useState('debtToEquity');
  const [levTD, setLevTD] = useState('700000');
  const [levTE, setLevTE] = useState('800000');
  const [levTA, setLevTA] = useState('2000000');
  const [levTL, setLevTL] = useState('1200000');
  const [levLTD, setLevLTD] = useState('500000');
  const [levSTD, setLevSTD] = useState('200000');
  const [levEbit, setLevEbit] = useState('200000');
  const [levEbitda, setLevEbitda] = useState('250000');
  const [levIntExp, setLevIntExp] = useState('20000');
  const [levOCF, setLevOCF] = useState('220000');
  const [levTC, setLevTC] = useState('1500000');

  // Cashflow — 8 batch + 2 special
  const cf = useEndpoint();
  const [cfRatio, setCfRatio] = useState('ocfToDebt');
  const [cfOCF, setCfOCF] = useState('220000');
  const [cfICF, setCfICF] = useState('-80000');
  const [cfFCF, setCfFCF] = useState('-50000');
  const [cfNI, setCfNI] = useState('150000');
  const [cfTD, setCfTD] = useState('700000');
  const [cfCL, setCfCL] = useState('300000');
  const [cfCapex, setCfCapex] = useState('80000');
  const [cfDiv, setCfDiv] = useState('30000');
  const [cfDepr, setCfDepr] = useState('50000');
  const [cfTA, setCfTA] = useState('2000000');

  // OCF Margin (special)
  const ocfm = useEndpoint();
  const [ocfmOCF, setOcfmOCF] = useState('220000');
  const [ocfmRev, setOcfmRev] = useState('1000000');

  // FCF Margin (special)
  const fcfm = useEndpoint();
  const [fcfmOCF, setFcfmOCF] = useState('220000');
  const [fcfmCapex, setFcfmCapex] = useState('80000');
  const [fcfmRev, setFcfmRev] = useState('1000000');

  const buildLevData = () => ({
    total_debt: Number(levTD), total_equity: Number(levTE), total_assets: Number(levTA),
    total_liabilities: Number(levTL), long_term_debt: Number(levLTD), short_term_debt: Number(levSTD),
    ebit: Number(levEbit), ebitda: Number(levEbitda), interest_expense: Number(levIntExp),
    operating_cash_flow: Number(levOCF), total_capital: Number(levTC),
  });

  const buildCfData = () => ({
    operating_cash_flow: Number(cfOCF), investing_cash_flow: Number(cfICF),
    financing_cash_flow: Number(cfFCF), net_income: Number(cfNI),
    total_debt: Number(cfTD), current_liabilities: Number(cfCL),
    capital_expenditures: Number(cfCapex), dividends: Number(cfDiv),
    depreciation: Number(cfDepr), total_assets: Number(cfTA),
  });

  return (
    <>
      <EndpointCard title="Leverage Ratios (13)" description="Debt, coverage, and capitalization ratios">
        <Row>
          <Field label="Ratio"><Select value={levRatio} onChange={setLevRatio} options={levRatios} /></Field>
        </Row>
        <Row>
          <Field label="Total Debt"><Input value={levTD} onChange={setLevTD} type="number" /></Field>
          <Field label="Total Equity"><Input value={levTE} onChange={setLevTE} type="number" /></Field>
          <Field label="Total Assets"><Input value={levTA} onChange={setLevTA} type="number" /></Field>
          <Field label="Total Liab"><Input value={levTL} onChange={setLevTL} type="number" /></Field>
        </Row>
        <Row>
          <Field label="LT Debt"><Input value={levLTD} onChange={setLevLTD} type="number" /></Field>
          <Field label="ST Debt"><Input value={levSTD} onChange={setLevSTD} type="number" /></Field>
          <Field label="EBIT"><Input value={levEbit} onChange={setLevEbit} type="number" /></Field>
          <Field label="EBITDA"><Input value={levEbitda} onChange={setLevEbitda} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Interest Exp"><Input value={levIntExp} onChange={setLevIntExp} type="number" /></Field>
          <Field label="Op Cash Flow"><Input value={levOCF} onChange={setLevOCF} type="number" /></Field>
          <Field label="Total Capital"><Input value={levTC} onChange={setLevTC} type="number" /></Field>
          <RunButton loading={lev.loading} onClick={() => lev.run(() => (leverageRatioApi as any)[levRatio](buildLevData()))} />
        </Row>
        <ResultPanel data={lev.result} error={lev.error} />
      </EndpointCard>

      <EndpointCard title="Cashflow Ratios (8)" description="OCF, FCF, coverage, and reinvestment ratios">
        <Row>
          <Field label="Ratio"><Select value={cfRatio} onChange={setCfRatio} options={cfRatios} /></Field>
        </Row>
        <Row>
          <Field label="Op CF"><Input value={cfOCF} onChange={setCfOCF} type="number" /></Field>
          <Field label="Inv CF"><Input value={cfICF} onChange={setCfICF} type="number" /></Field>
          <Field label="Fin CF"><Input value={cfFCF} onChange={setCfFCF} type="number" /></Field>
          <Field label="Net Income"><Input value={cfNI} onChange={setCfNI} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Total Debt"><Input value={cfTD} onChange={setCfTD} type="number" /></Field>
          <Field label="Current Liab"><Input value={cfCL} onChange={setCfCL} type="number" /></Field>
          <Field label="CapEx"><Input value={cfCapex} onChange={setCfCapex} type="number" /></Field>
          <Field label="Dividends"><Input value={cfDiv} onChange={setCfDiv} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Depreciation"><Input value={cfDepr} onChange={setCfDepr} type="number" /></Field>
          <Field label="Total Assets"><Input value={cfTA} onChange={setCfTA} type="number" /></Field>
          <RunButton loading={cf.loading} onClick={() => cf.run(() => (cfRatioApi as any)[cfRatio](buildCfData()))} />
        </Row>
        <ResultPanel data={cf.result} error={cf.error} />
      </EndpointCard>

      <EndpointCard title="OCF Margin" description="Operating cash flow / revenue">
        <Row>
          <Field label="Op Cash Flow"><Input value={ocfmOCF} onChange={setOcfmOCF} type="number" /></Field>
          <Field label="Revenue"><Input value={ocfmRev} onChange={setOcfmRev} type="number" /></Field>
          <RunButton loading={ocfm.loading} onClick={() => ocfm.run(() => cfRatioApi.ocfMargin(Number(ocfmOCF), Number(ocfmRev)))} />
        </Row>
        <ResultPanel data={ocfm.result} error={ocfm.error} />
      </EndpointCard>

      <EndpointCard title="FCF Margin" description="Free cash flow / revenue">
        <Row>
          <Field label="Op Cash Flow"><Input value={fcfmOCF} onChange={setFcfmOCF} type="number" /></Field>
          <Field label="CapEx"><Input value={fcfmCapex} onChange={setFcfmCapex} type="number" /></Field>
          <Field label="Revenue"><Input value={fcfmRev} onChange={setFcfmRev} type="number" /></Field>
          <RunButton loading={fcfm.loading} onClick={() => fcfm.run(() => cfRatioApi.fcfMargin(Number(fcfmOCF), Number(fcfmCapex), Number(fcfmRev)))} />
        </Row>
        <ResultPanel data={fcfm.result} error={fcfm.error} />
      </EndpointCard>
    </>
  );
}
