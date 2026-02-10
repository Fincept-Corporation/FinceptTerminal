import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { fundamentalsApi } from '../quantlibAnalysisApi';

const sampleIncome = '{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,"interest_expense":20000}';
const sampleBalance = '{"total_assets":2000000,"total_equity":800000,"total_liabilities":1200000,"current_assets":500000,"current_liabilities":300000,"total_debt":700000,"inventory":100000,"accounts_receivable":150000,"cash":200000}';
const sampleCF = '{"operating_cash_flow":220000,"investing_cash_flow":-80000,"financing_cash_flow":-50000,"capital_expenditures":80000,"depreciation":50000}';

export default function FundamentalsPanel() {
  // Profitability
  const prof = useEndpoint();
  const [profInc, setProfInc] = useState(sampleIncome);
  const [profBal, setProfBal] = useState(sampleBalance);

  // Liquidity
  const liq = useEndpoint();
  const [liqInc, setLiqInc] = useState(sampleIncome);
  const [liqBal, setLiqBal] = useState(sampleBalance);

  // Efficiency
  const eff = useEndpoint();
  const [effInc, setEffInc] = useState(sampleIncome);
  const [effBal, setEffBal] = useState(sampleBalance);

  // Solvency
  const sol = useEndpoint();
  const [solInc, setSolInc] = useState(sampleIncome);
  const [solBal, setSolBal] = useState(sampleBalance);

  // DuPont
  const dup = useEndpoint();
  const [dupInc, setDupInc] = useState(sampleIncome);
  const [dupBal, setDupBal] = useState(sampleBalance);

  // Comprehensive
  const comp = useEndpoint();
  const [compInc, setCompInc] = useState(sampleIncome);
  const [compBal, setCompBal] = useState(sampleBalance);
  const [compCF, setCompCF] = useState(sampleCF);

  // WACC
  const wacc = useEndpoint();
  const [wDebt, setWDebt] = useState('700000');
  const [wEquity, setWEquity] = useState('800000');
  const [wTax, setWTax] = useState('0.25');
  const [wCod, setWCod] = useState('0.05');
  const [wCoe, setWCoe] = useState('0.1');

  // Optimal Capital
  const opt = useEndpoint();
  const [oDebt, setODebt] = useState('700000');
  const [oEquity, setOEquity] = useState('800000');
  const [oTax, setOTax] = useState('0.25');

  return (
    <>
      <EndpointCard title="Profitability Analysis" description="Comprehensive profitability metrics">
        <Row>
          <Field label="Income Statement (JSON)" width="400px"><Input value={profInc} onChange={setProfInc} /></Field>
          <Field label="Balance Sheet (JSON)" width="400px"><Input value={profBal} onChange={setProfBal} /></Field>
          <RunButton loading={prof.loading} onClick={() => prof.run(() => fundamentalsApi.profitability(JSON.parse(profInc), JSON.parse(profBal)))} />
        </Row>
        <ResultPanel data={prof.result} error={prof.error} />
      </EndpointCard>

      <EndpointCard title="Liquidity Analysis" description="Liquidity position assessment">
        <Row>
          <Field label="Income (JSON)" width="400px"><Input value={liqInc} onChange={setLiqInc} /></Field>
          <Field label="Balance (JSON)" width="400px"><Input value={liqBal} onChange={setLiqBal} /></Field>
          <RunButton loading={liq.loading} onClick={() => liq.run(() => fundamentalsApi.liquidity(JSON.parse(liqInc), JSON.parse(liqBal)))} />
        </Row>
        <ResultPanel data={liq.result} error={liq.error} />
      </EndpointCard>

      <EndpointCard title="Efficiency Analysis" description="Operational efficiency metrics">
        <Row>
          <Field label="Income (JSON)" width="400px"><Input value={effInc} onChange={setEffInc} /></Field>
          <Field label="Balance (JSON)" width="400px"><Input value={effBal} onChange={setEffBal} /></Field>
          <RunButton loading={eff.loading} onClick={() => eff.run(() => fundamentalsApi.efficiency(JSON.parse(effInc), JSON.parse(effBal)))} />
        </Row>
        <ResultPanel data={eff.result} error={eff.error} />
      </EndpointCard>

      <EndpointCard title="Solvency Analysis" description="Long-term solvency assessment">
        <Row>
          <Field label="Income (JSON)" width="400px"><Input value={solInc} onChange={setSolInc} /></Field>
          <Field label="Balance (JSON)" width="400px"><Input value={solBal} onChange={setSolBal} /></Field>
          <RunButton loading={sol.loading} onClick={() => sol.run(() => fundamentalsApi.solvency(JSON.parse(solInc), JSON.parse(solBal)))} />
        </Row>
        <ResultPanel data={sol.result} error={sol.error} />
      </EndpointCard>

      <EndpointCard title="DuPont Analysis" description="3-step and 5-step DuPont decomposition">
        <Row>
          <Field label="Income (JSON)" width="400px"><Input value={dupInc} onChange={setDupInc} /></Field>
          <Field label="Balance (JSON)" width="400px"><Input value={dupBal} onChange={setDupBal} /></Field>
          <RunButton loading={dup.loading} onClick={() => dup.run(() => fundamentalsApi.dupont(JSON.parse(dupInc), JSON.parse(dupBal)))} />
        </Row>
        <ResultPanel data={dup.result} error={dup.error} />
      </EndpointCard>

      <EndpointCard title="Comprehensive Analysis" description="Full fundamental analysis">
        <Row>
          <Field label="Income (JSON)" width="300px"><Input value={compInc} onChange={setCompInc} /></Field>
          <Field label="Balance (JSON)" width="300px"><Input value={compBal} onChange={setCompBal} /></Field>
          <Field label="Cashflow (JSON)" width="300px"><Input value={compCF} onChange={setCompCF} /></Field>
          <RunButton loading={comp.loading} onClick={() => comp.run(() => fundamentalsApi.comprehensive(JSON.parse(compInc), JSON.parse(compBal), JSON.parse(compCF)))} />
        </Row>
        <ResultPanel data={comp.result} error={comp.error} />
      </EndpointCard>

      <EndpointCard title="WACC" description="Weighted average cost of capital">
        <Row>
          <Field label="Debt"><Input value={wDebt} onChange={setWDebt} type="number" /></Field>
          <Field label="Equity"><Input value={wEquity} onChange={setWEquity} type="number" /></Field>
          <Field label="Tax Rate"><Input value={wTax} onChange={setWTax} type="number" /></Field>
          <Field label="Cost of Debt"><Input value={wCod} onChange={setWCod} type="number" /></Field>
          <Field label="Cost of Equity"><Input value={wCoe} onChange={setWCoe} type="number" /></Field>
          <RunButton loading={wacc.loading} onClick={() => wacc.run(() => fundamentalsApi.wacc(Number(wDebt), Number(wEquity), Number(wTax), Number(wCod), Number(wCoe)))} />
        </Row>
        <ResultPanel data={wacc.result} error={wacc.error} />
      </EndpointCard>

      <EndpointCard title="Optimal Capital Structure" description="Find optimal debt/equity mix">
        <Row>
          <Field label="Debt"><Input value={oDebt} onChange={setODebt} type="number" /></Field>
          <Field label="Equity"><Input value={oEquity} onChange={setOEquity} type="number" /></Field>
          <Field label="Tax Rate"><Input value={oTax} onChange={setOTax} type="number" /></Field>
          <RunButton loading={opt.loading} onClick={() => opt.run(() => fundamentalsApi.optimalCapital(Number(oDebt), Number(oEquity), Number(oTax)))} />
        </Row>
        <ResultPanel data={opt.result} error={opt.error} />
      </EndpointCard>
    </>
  );
}
