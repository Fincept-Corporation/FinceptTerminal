import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { valRatioApi, qualityRatioApi } from '../quantlibAnalysisApi';

const valRatios = [
  'pe', 'forwardPe', 'peg', 'priceToBook', 'priceToSales', 'priceToCashFlow',
  'evToEbitda', 'evToSales', 'evToFcf', 'dividendYield', 'earningsYield', 'fcfYield', 'priceToTangible',
];

const qualRatios = [
  'accruals', 'sloan', 'noa', 'earningsPersistence', 'earningsSmooth', 'cashEarnings',
];

export default function RatioValQualPanel() {
  // Valuation — 13 ratios
  const vr = useEndpoint();
  const [vrRatio, setVrRatio] = useState('pe');
  const [vrData, setVrData] = useState('{"share_price":50,"earnings_per_share":3.5,"book_value_per_share":25,"revenue_per_share":40,"cash_flow_per_share":5,"dividends_per_share":1.5,"shares_outstanding":1000000,"market_cap":50000000,"enterprise_value":55000000,"ebitda":5000000,"net_income":3500000,"revenue":40000000,"free_cash_flow":4000000,"tangible_book_value_per_share":22,"forward_eps":4,"earnings_growth":0.1}');

  // Quality — 6 ratios
  const qr = useEndpoint();
  const [qrRatio, setQrRatio] = useState('accruals');
  const [qrData, setQrData] = useState('{"net_income":150000,"operating_cash_flow":220000,"total_assets":2000000,"total_equity":800000,"net_operating_assets":600000,"prior_net_operating_assets":550000,"current_net_income":150000,"prior_net_income":130000,"depreciation":50000,"revenue":1000000,"prior_revenue":900000}');

  return (
    <>
      <EndpointCard title="Valuation Ratios (13)" description="P/E, P/B, EV/EBITDA, yields, and market multiples">
        <Row>
          <Field label="Ratio"><Select value={vrRatio} onChange={setVrRatio} options={valRatios} /></Field>
        </Row>
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={vrData} onChange={setVrData} /></Field>
          <RunButton loading={vr.loading} onClick={() => vr.run(() => (valRatioApi as any)[vrRatio](JSON.parse(vrData)))} />
        </Row>
        <ResultPanel data={vr.result} error={vr.error} />
      </EndpointCard>

      <EndpointCard title="Quality Ratios (6)" description="Accruals, Sloan, NOA, persistence, smoothness, cash earnings">
        <Row>
          <Field label="Ratio"><Select value={qrRatio} onChange={setQrRatio} options={qualRatios} /></Field>
        </Row>
        <Row>
          <Field label="Data (JSON)" width="700px"><Input value={qrData} onChange={setQrData} /></Field>
          <RunButton loading={qr.loading} onClick={() => qr.run(() => (qualityRatioApi as any)[qrRatio](JSON.parse(qrData)))} />
        </Row>
        <ResultPanel data={qr.result} error={qr.error} />
      </EndpointCard>
    </>
  );
}
