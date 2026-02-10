import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { dcfApi } from '../quantlibAnalysisApi';

export default function ValuationDcfPanel() {
  // FCFF DCF
  const fcff = useEndpoint();
  const [fcffFlows, setFcffFlows] = useState('100000,110000,121000,133100,146410');
  const [fcffWacc, setFcffWacc] = useState('0.1');
  const [fcffTg, setFcffTg] = useState('0.02');
  const [fcffShares, setFcffShares] = useState('1000000');
  const [fcffDebt, setFcffDebt] = useState('500000');

  // Gordon Growth
  const gg = useEndpoint();
  const [ggDiv, setGgDiv] = useState('2.5');
  const [ggReq, setGgReq] = useState('0.1');
  const [ggGr, setGgGr] = useState('0.03');

  // Two-Stage
  const ts = useEndpoint();
  const [tsFlows, setTsFlows] = useState('100000,115000,132250,152087,174900');
  const [tsWacc, setTsWacc] = useState('0.1');
  const [tsTg, setTsTg] = useState('0.02');

  // DDM
  const ddm = useEndpoint();
  const [ddmData, setDdmData] = useState('{"dividends":[2.0,2.1,2.2,2.3,2.4],"required_return":0.1,"terminal_growth":0.03}');

  // WACC
  const wacc = useEndpoint();
  const [waccData, setWaccData] = useState('{"equity_weight":0.6,"debt_weight":0.4,"cost_of_equity":0.1,"cost_of_debt":0.05,"tax_rate":0.25}');

  // Cost of Equity
  const coe = useEndpoint();
  const [coeData, setCoeData] = useState('{"risk_free_rate":0.03,"beta":1.2,"market_premium":0.06,"method":"capm"}');

  // Terminal Value
  const tv = useEndpoint();
  const [tvData, setTvData] = useState('{"final_cash_flow":146410,"growth_rate":0.02,"discount_rate":0.1,"method":"gordon"}');

  return (
    <>
      <EndpointCard title="FCFF DCF Valuation" description="Discounted free cash flow to firm">
        <Row>
          <Field label="FCFF (comma-sep)" width="300px"><Input value={fcffFlows} onChange={setFcffFlows} /></Field>
          <Field label="WACC"><Input value={fcffWacc} onChange={setFcffWacc} type="number" /></Field>
          <Field label="Terminal Growth"><Input value={fcffTg} onChange={setFcffTg} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Shares Outstanding"><Input value={fcffShares} onChange={setFcffShares} type="number" /></Field>
          <Field label="Net Debt"><Input value={fcffDebt} onChange={setFcffDebt} type="number" /></Field>
          <RunButton loading={fcff.loading} onClick={() => fcff.run(() => dcfApi.fcff(fcffFlows.split(',').map(Number), Number(fcffWacc), Number(fcffTg), Number(fcffShares), Number(fcffDebt)))} />
        </Row>
        <ResultPanel data={fcff.result} error={fcff.error} />
      </EndpointCard>

      <EndpointCard title="Gordon Growth Model" description="Constant-growth dividend discount model">
        <Row>
          <Field label="Dividend"><Input value={ggDiv} onChange={setGgDiv} type="number" /></Field>
          <Field label="Required Return"><Input value={ggReq} onChange={setGgReq} type="number" /></Field>
          <Field label="Growth Rate"><Input value={ggGr} onChange={setGgGr} type="number" /></Field>
          <RunButton loading={gg.loading} onClick={() => gg.run(() => dcfApi.gordonGrowth(Number(ggDiv), Number(ggReq), Number(ggGr)))} />
        </Row>
        <ResultPanel data={gg.result} error={gg.error} />
      </EndpointCard>

      <EndpointCard title="Two-Stage DCF" description="High-growth + terminal-growth DCF">
        <Row>
          <Field label="FCFF (comma-sep)" width="300px"><Input value={tsFlows} onChange={setTsFlows} /></Field>
          <Field label="WACC"><Input value={tsWacc} onChange={setTsWacc} type="number" /></Field>
          <Field label="Terminal Growth"><Input value={tsTg} onChange={setTsTg} type="number" /></Field>
          <RunButton loading={ts.loading} onClick={() => ts.run(() => dcfApi.twoStage(tsFlows.split(',').map(Number), Number(tsWacc), Number(tsTg)))} />
        </Row>
        <ResultPanel data={ts.result} error={ts.error} />
      </EndpointCard>

      <EndpointCard title="Dividend Discount Model" description="Multi-period DDM valuation">
        <Row>
          <Field label="DDM Data (JSON)" width="700px"><Input value={ddmData} onChange={setDdmData} /></Field>
          <RunButton loading={ddm.loading} onClick={() => ddm.run(() => dcfApi.ddm(JSON.parse(ddmData)))} />
        </Row>
        <ResultPanel data={ddm.result} error={ddm.error} />
      </EndpointCard>

      <EndpointCard title="WACC Calculation" description="Weighted average cost of capital for DCF">
        <Row>
          <Field label="WACC Data (JSON)" width="700px"><Input value={waccData} onChange={setWaccData} /></Field>
          <RunButton loading={wacc.loading} onClick={() => wacc.run(() => dcfApi.wacc(JSON.parse(waccData)))} />
        </Row>
        <ResultPanel data={wacc.result} error={wacc.error} />
      </EndpointCard>

      <EndpointCard title="Cost of Equity" description="CAPM, Fama-French, or build-up method">
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={coeData} onChange={setCoeData} /></Field>
          <RunButton loading={coe.loading} onClick={() => coe.run(() => dcfApi.costOfEquity(JSON.parse(coeData)))} />
        </Row>
        <ResultPanel data={coe.result} error={coe.error} />
      </EndpointCard>

      <EndpointCard title="Terminal Value" description="Gordon growth or exit multiple terminal value">
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={tvData} onChange={setTvData} /></Field>
          <RunButton loading={tv.loading} onClick={() => tv.run(() => dcfApi.terminalValue(JSON.parse(tvData)))} />
        </Row>
        <ResultPanel data={tv.result} error={tv.error} />
      </EndpointCard>
    </>
  );
}
