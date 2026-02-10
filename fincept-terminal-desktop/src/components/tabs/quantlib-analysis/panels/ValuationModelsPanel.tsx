import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { creditApi, predictiveApi, residualIncomeApi, startupApi, valuationOtherApi } from '../quantlibAnalysisApi';

const creditModels = ['mertonModel', 'spreadFromPd', 'distanceToDefault', 'expectedLoss', 'ratingPd'];
const predictiveModels = ['altmanZ', 'piotroskiF', 'beneishM', 'ohlsonO', 'springate', 'zmijewski'];
const riModels = ['eva', 'ri', 'riValuation'];
const startupModels = ['vcMethod', 'berkus', 'firstChicago', 'dilution'];

export default function ValuationModelsPanel() {
  // Credit — 5 endpoints
  const cr = useEndpoint();
  const [crModel, setCrModel] = useState('mertonModel');
  const [crData, setCrData] = useState('{"asset_value":1000000,"debt_face_value":500000,"asset_volatility":0.3,"risk_free_rate":0.03,"time_to_maturity":1,"recovery_rate":0.4}');

  // Predictive — 6 endpoints
  const pd = useEndpoint();
  const [pdModel, setPdModel] = useState('altmanZ');
  const [pdData, setPdData] = useState('{"working_capital":200000,"retained_earnings":400000,"ebit":200000,"market_cap":2000000,"total_liabilities":1200000,"revenue":1000000,"total_assets":2000000,"current_assets":500000,"current_liabilities":300000,"net_income":150000,"total_debt":700000,"operating_cash_flow":220000,"depreciation":50000}');

  // Residual Income — 3 endpoints
  const ri = useEndpoint();
  const [riModel, setRiModel] = useState('eva');
  const [riData, setRiData] = useState('{"net_income":150000,"cost_of_capital":0.1,"invested_capital":1500000,"book_value":800000,"roe":0.15,"growth_rate":0.03,"required_return":0.1}');

  // Startup — 4 endpoints
  const su = useEndpoint();
  const [suModel, setSuModel] = useState('vcMethod');
  const [suData, setSuData] = useState('{"expected_exit_value":10000000,"years_to_exit":5,"discount_rate":0.4,"investment_amount":1000000,"pre_money_valuation":3000000,"sound_idea":true,"prototype":true,"quality_management":true,"strategic_relationships":true,"product_rollout":true,"scenarios":{"base":{"probability":0.5,"exit_value":8000000},"upside":{"probability":0.3,"exit_value":15000000},"downside":{"probability":0.2,"exit_value":2000000}}}');

  // Factor Models
  const fm = useEndpoint();
  const [fmData, setFmData] = useState('{"risk_free_rate":0.03,"market_premium":0.06,"beta":1.2,"smb_factor":0.02,"hml_factor":0.03,"smb_loading":0.5,"hml_loading":0.3}');

  // Proforma
  const pf = useEndpoint();
  const [pfData, setPfData] = useState('{"revenue":1000000,"growth_rate":0.1,"margin":0.15,"capex_pct":0.08,"depreciation_pct":0.05,"tax_rate":0.25,"years":5}');

  // SOTP
  const sotp = useEndpoint();
  const [sotpData, setSotpData] = useState('{"segments":[{"name":"Division A","ebitda":500000,"multiple":8},{"name":"Division B","ebitda":300000,"multiple":6}],"net_debt":1000000,"shares_outstanding":1000000}');

  // Comparable
  const comp = useEndpoint();
  const [compData, setCompData] = useState('{"target":{"ebitda":500000,"revenue":2000000,"net_income":200000},"comparables":[{"name":"Peer A","ev_ebitda":8,"price_sales":2,"pe":15},{"name":"Peer B","ev_ebitda":10,"price_sales":2.5,"pe":18}]}');

  // Screen
  const scr = useEndpoint();
  const [scrData, setScrData] = useState('{"criteria":{"min_market_cap":1000000,"max_pe":20,"min_roe":0.1,"min_dividend_yield":0.02},"universe":"us_equities"}');

  return (
    <>
      <EndpointCard title="Credit Models (5)" description="Merton, spread-from-PD, distance-to-default, expected loss, rating PD">
        <Row>
          <Field label="Model"><Select value={crModel} onChange={setCrModel} options={creditModels} /></Field>
        </Row>
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={crData} onChange={setCrData} /></Field>
          <RunButton loading={cr.loading} onClick={() => cr.run(() => (creditApi as any)[crModel](JSON.parse(crData)))} />
        </Row>
        <ResultPanel data={cr.result} error={cr.error} />
      </EndpointCard>

      <EndpointCard title="Predictive Models (6)" description="Altman Z, Piotroski F, Beneish M, Ohlson O, Springate, Zmijewski">
        <Row>
          <Field label="Model"><Select value={pdModel} onChange={setPdModel} options={predictiveModels} /></Field>
        </Row>
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={pdData} onChange={setPdData} /></Field>
          <RunButton loading={pd.loading} onClick={() => pd.run(() => (predictiveApi as any)[pdModel](JSON.parse(pdData)))} />
        </Row>
        <ResultPanel data={pd.result} error={pd.error} />
      </EndpointCard>

      <EndpointCard title="Residual Income (3)" description="EVA, residual income, RI valuation">
        <Row>
          <Field label="Model"><Select value={riModel} onChange={setRiModel} options={riModels} /></Field>
        </Row>
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={riData} onChange={setRiData} /></Field>
          <RunButton loading={ri.loading} onClick={() => ri.run(() => (residualIncomeApi as any)[riModel](JSON.parse(riData)))} />
        </Row>
        <ResultPanel data={ri.result} error={ri.error} />
      </EndpointCard>

      <EndpointCard title="Startup Valuation (4)" description="VC method, Berkus, First Chicago, dilution analysis">
        <Row>
          <Field label="Model"><Select value={suModel} onChange={setSuModel} options={startupModels} /></Field>
        </Row>
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={suData} onChange={setSuData} /></Field>
          <RunButton loading={su.loading} onClick={() => su.run(() => (startupApi as any)[suModel](JSON.parse(suData)))} />
        </Row>
        <ResultPanel data={su.result} error={su.error} />
      </EndpointCard>

      <EndpointCard title="Factor Models" description="CAPM, Fama-French, multi-factor asset pricing">
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={fmData} onChange={setFmData} /></Field>
          <RunButton loading={fm.loading} onClick={() => fm.run(() => valuationOtherApi.factorModels(JSON.parse(fmData)))} />
        </Row>
        <ResultPanel data={fm.result} error={fm.error} />
      </EndpointCard>

      <EndpointCard title="Pro Forma Adjustments" description="Forward-looking financial projections">
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={pfData} onChange={setPfData} /></Field>
          <RunButton loading={pf.loading} onClick={() => pf.run(() => valuationOtherApi.proforma(JSON.parse(pfData)))} />
        </Row>
        <ResultPanel data={pf.result} error={pf.error} />
      </EndpointCard>

      <EndpointCard title="Sum-of-the-Parts" description="Segment-based SOTP valuation">
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={sotpData} onChange={setSotpData} /></Field>
          <RunButton loading={sotp.loading} onClick={() => sotp.run(() => valuationOtherApi.sotp(JSON.parse(sotpData)))} />
        </Row>
        <ResultPanel data={sotp.result} error={sotp.error} />
      </EndpointCard>

      <EndpointCard title="Comparable Analysis" description="Peer-based multiples valuation">
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={compData} onChange={setCompData} /></Field>
          <RunButton loading={comp.loading} onClick={() => comp.run(() => valuationOtherApi.comparable(JSON.parse(compData)))} />
        </Row>
        <ResultPanel data={comp.result} error={comp.error} />
      </EndpointCard>

      <EndpointCard title="Valuation Screen" description="Screen stocks by valuation criteria">
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={scrData} onChange={setScrData} /></Field>
          <RunButton loading={scr.loading} onClick={() => scr.run(() => valuationOtherApi.screen(JSON.parse(scrData)))} />
        </Row>
        <ResultPanel data={scr.result} error={scr.error} />
      </EndpointCard>
    </>
  );
}
