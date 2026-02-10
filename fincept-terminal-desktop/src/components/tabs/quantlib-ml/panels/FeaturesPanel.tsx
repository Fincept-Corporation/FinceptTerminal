import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { featuresApi } from '../quantlibMlApi';

export default function FeaturesPanel() {
  // Technical Indicators
  const tech = useEndpoint();
  const [techPrices, setTechPrices] = useState('100, 102, 101, 103, 105, 104, 106, 108, 107, 110');
  const [techInd, setTechInd] = useState('sma');
  const [techPeriod, setTechPeriod] = useState('5');

  // Rolling Stats
  const roll = useEndpoint();
  const [rollData, setRollData] = useState('100, 102, 101, 103, 105, 104, 106, 108, 107, 110');
  const [rollWin, setRollWin] = useState('3');
  const [rollStat, setRollStat] = useState('mean');

  // Calendar Features
  const cal = useEndpoint();
  const [calDates, setCalDates] = useState('2024-01-01, 2024-03-15, 2024-07-04, 2024-12-25');

  // Cross-Sectional
  const cs = useEndpoint();
  const [csData, setCsData] = useState('[[1,2,3],[4,5,6],[7,8,9]]');
  const [csMethod, setCsMethod] = useState('rank');

  // Financial Ratios
  const fr = useEndpoint();
  const [frData, setFrData] = useState('{"revenue": 1000, "net_income": 150, "total_assets": 5000, "total_equity": 2000, "current_assets": 1500, "current_liabilities": 800}');

  // Lags
  const lag = useEndpoint();
  const [lagData, setLagData] = useState('100, 102, 101, 103, 105, 104, 106, 108, 107, 110');
  const [lagN, setLagN] = useState('3');
  const [lagLeads, setLagLeads] = useState('false');
  const [lagRets, setLagRets] = useState('true');
  const [lagLogRets, setLagLogRets] = useState('false');

  return (
    <>
      <EndpointCard title="Technical Indicators" description="SMA, EMA, RSI, MACD, Bollinger Bands, etc.">
        <Row>
          <Field label="Prices (comma-sep)" width="350px"><Input value={techPrices} onChange={setTechPrices} /></Field>
          <Field label="Indicator"><Select value={techInd} onChange={setTechInd} options={['sma', 'ema', 'rsi', 'macd', 'bollinger', 'atr', 'obv']} /></Field>
          <Field label="Period"><Input value={techPeriod} onChange={setTechPeriod} type="number" /></Field>
          <RunButton loading={tech.loading} onClick={() => tech.run(() => featuresApi.technical(techPrices.split(',').map(Number), techInd, Number(techPeriod)))} />
        </Row>
        <ResultPanel data={tech.result} error={tech.error} />
      </EndpointCard>

      <EndpointCard title="Rolling Statistics" description="Rolling mean / std / sharpe / beta / drawdown">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={rollData} onChange={setRollData} /></Field>
          <Field label="Window"><Input value={rollWin} onChange={setRollWin} type="number" /></Field>
          <Field label="Statistic"><Select value={rollStat} onChange={setRollStat} options={['mean', 'std', 'sharpe', 'max_drawdown', 'skew', 'kurtosis']} /></Field>
          <RunButton loading={roll.loading} onClick={() => roll.run(() => featuresApi.rolling(rollData.split(',').map(Number), Number(rollWin), rollStat))} />
        </Row>
        <ResultPanel data={roll.result} error={roll.error} />
      </EndpointCard>

      <EndpointCard title="Calendar Features" description="Day-of-week, month, quarter, holiday flags">
        <Row>
          <Field label="Dates (comma-sep ISO)" width="450px"><Input value={calDates} onChange={setCalDates} /></Field>
          <RunButton loading={cal.loading} onClick={() => cal.run(() => featuresApi.calendar(calDates.split(',').map(s => s.trim())))} />
        </Row>
        <ResultPanel data={cal.result} error={cal.error} />
      </EndpointCard>

      <EndpointCard title="Cross-Sectional" description="Cross-sectional ranking / z-score / percentile">
        <Row>
          <Field label="Data (JSON 2D)" width="300px"><Input value={csData} onChange={setCsData} /></Field>
          <Field label="Method"><Select value={csMethod} onChange={setCsMethod} options={['rank', 'zscore', 'percentile']} /></Field>
          <RunButton loading={cs.loading} onClick={() => cs.run(() => featuresApi.crossSectional(JSON.parse(csData), csMethod))} />
        </Row>
        <ResultPanel data={cs.result} error={cs.error} />
      </EndpointCard>

      <EndpointCard title="Financial Ratios" description="Compute standard financial ratios">
        <Row>
          <Field label="Data (JSON object)" width="500px"><Input value={frData} onChange={setFrData} /></Field>
          <RunButton loading={fr.loading} onClick={() => fr.run(() => featuresApi.financialRatios(JSON.parse(frData)))} />
        </Row>
        <ResultPanel data={fr.result} error={fr.error} />
      </EndpointCard>

      <EndpointCard title="Lag Features" description="Generate lag/lead features and returns">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={lagData} onChange={setLagData} /></Field>
          <Field label="N Lags"><Input value={lagN} onChange={setLagN} type="number" /></Field>
          <Field label="Leads"><Select value={lagLeads} onChange={setLagLeads} options={['false', 'true']} /></Field>
          <Field label="Returns"><Select value={lagRets} onChange={setLagRets} options={['true', 'false']} /></Field>
          <Field label="Log Returns"><Select value={lagLogRets} onChange={setLagLogRets} options={['false', 'true']} /></Field>
          <RunButton loading={lag.loading} onClick={() => lag.run(() => featuresApi.lags(lagData.split(',').map(Number), Number(lagN), lagLeads === 'true', lagRets === 'true', lagLogRets === 'true'))} />
        </Row>
        <ResultPanel data={lag.result} error={lag.error} />
      </EndpointCard>
    </>
  );
}
