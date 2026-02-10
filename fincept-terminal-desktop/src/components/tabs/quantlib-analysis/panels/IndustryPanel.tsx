import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { industryApi } from '../quantlibAnalysisApi';

const sampleBanking = '{"net_interest_income":500000,"total_assets":10000000,"total_loans":6000000,"total_deposits":8000000,"non_performing_loans":120000,"loan_loss_provisions":60000,"operating_income":700000,"operating_expenses":400000,"total_equity":1200000,"risk_weighted_assets":7000000,"tier1_capital":1000000,"tier2_capital":200000}';
const sampleInsurance = '{"net_premiums_written":2000000,"net_premiums_earned":1800000,"losses_incurred":1100000,"underwriting_expenses":400000,"investment_income":200000,"net_income":300000,"total_assets":5000000,"total_equity":1500000,"policy_reserves":2500000,"claims_paid":1000000}';
const sampleReits = '{"net_income":500000,"depreciation":200000,"amortization":50000,"gains_on_sales":30000,"total_revenue":1000000,"total_assets":8000000,"total_debt":3000000,"total_equity":4000000,"shares_outstanding":1000000,"dividends_paid":400000}';
const sampleUtilities = '{"total_revenue":3000000,"operating_expenses":2200000,"depreciation":300000,"interest_expense":150000,"net_income":250000,"total_assets":10000000,"total_debt":4000000,"total_equity":5000000,"capital_expenditures":800000,"rate_base":8000000}';

export default function IndustryPanel() {
  const bank = useEndpoint();
  const [bankData, setBankData] = useState(sampleBanking);

  const ins = useEndpoint();
  const [insData, setInsData] = useState(sampleInsurance);

  const reit = useEndpoint();
  const [reitData, setReitData] = useState(sampleReits);

  const util = useEndpoint();
  const [utilData, setUtilData] = useState(sampleUtilities);

  return (
    <>
      <EndpointCard title="Banking Analysis" description="NIM, efficiency, NPL, capital adequacy, and banking ratios">
        <Row>
          <Field label="Banking Data (JSON)" width="700px"><Input value={bankData} onChange={setBankData} /></Field>
          <RunButton loading={bank.loading} onClick={() => bank.run(() => industryApi.banking(JSON.parse(bankData)))} />
        </Row>
        <ResultPanel data={bank.result} error={bank.error} />
      </EndpointCard>

      <EndpointCard title="Insurance Analysis" description="Combined ratio, loss ratio, expense ratio, ROE, reserve adequacy">
        <Row>
          <Field label="Insurance Data (JSON)" width="700px"><Input value={insData} onChange={setInsData} /></Field>
          <RunButton loading={ins.loading} onClick={() => ins.run(() => industryApi.insurance(JSON.parse(insData)))} />
        </Row>
        <ResultPanel data={ins.result} error={ins.error} />
      </EndpointCard>

      <EndpointCard title="REITs Analysis" description="FFO, AFFO, NAV, cap rate, dividend coverage">
        <Row>
          <Field label="REITs Data (JSON)" width="700px"><Input value={reitData} onChange={setReitData} /></Field>
          <RunButton loading={reit.loading} onClick={() => reit.run(() => industryApi.reits(JSON.parse(reitData)))} />
        </Row>
        <ResultPanel data={reit.result} error={reit.error} />
      </EndpointCard>

      <EndpointCard title="Utilities Analysis" description="Rate base, allowed ROE, regulatory asset base, coverage">
        <Row>
          <Field label="Utilities Data (JSON)" width="700px"><Input value={utilData} onChange={setUtilData} /></Field>
          <RunButton loading={util.loading} onClick={() => util.run(() => industryApi.utilities(JSON.parse(utilData)))} />
        </Row>
        <ResultPanel data={util.result} error={util.error} />
      </EndpointCard>
    </>
  );
}
