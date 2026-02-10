import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../shared';
import { typesApi } from '../quantlibCoreApi';

export default function TypesPanel() {
  const tenor = useEndpoint();
  const [tenorVal, setTenorVal] = useState('6M');
  const [tenorDate, setTenorDate] = useState(new Date().toISOString().split('T')[0]);

  const rate = useEndpoint();
  const [rateVal, setRateVal] = useState('0.05');
  const [rateUnit, setRateUnit] = useState('decimal');

  const money = useEndpoint();
  const [moneyAmt, setMoneyAmt] = useState('1000000');
  const [moneyCcy, setMoneyCcy] = useState('USD');

  const mConvert = useEndpoint();
  const [mcAmt, setMcAmt] = useState('1000000');
  const [mcFrom, setMcFrom] = useState('USD');
  const [mcTo, setMcTo] = useState('EUR');
  const [mcRate, setMcRate] = useState('0.92');

  const spread = useEndpoint();
  const [bps, setBps] = useState('50');

  const notSch = useEndpoint();
  const [nsType, setNsType] = useState('bullet');
  const [nsNotional, setNsNotional] = useState('10000000');
  const [nsPeriods, setNsPeriods] = useState('10');
  const [nsRate, setNsRate] = useState('0.05');

  const currencies = useEndpoint();
  const frequencies = useEndpoint();

  return (
    <>
      <EndpointCard title="Tenor → Add to Date" description="POST /quantlib/core/types/tenor/add-to-date">
        <Row>
          <Field label="Tenor"><Input value={tenorVal} onChange={setTenorVal} placeholder="6M, 1Y, 3M" /></Field>
          <Field label="Start Date"><Input value={tenorDate} onChange={setTenorDate} type="date" /></Field>
          <RunButton loading={tenor.loading} onClick={() => tenor.run(() => typesApi.tenorAddToDate(tenorVal, tenorDate))} />
        </Row>
        <ResultPanel data={tenor.result} error={tenor.error} />
      </EndpointCard>

      <EndpointCard title="Rate Convert" description="POST /quantlib/core/types/rate/convert">
        <Row>
          <Field label="Value"><Input value={rateVal} onChange={setRateVal} type="number" /></Field>
          <Field label="Unit"><Select value={rateUnit} onChange={setRateUnit} options={['decimal', 'percent', 'bps']} /></Field>
          <RunButton loading={rate.loading} onClick={() => rate.run(() => typesApi.rateConvert(Number(rateVal), rateUnit))} />
        </Row>
        <ResultPanel data={rate.result} error={rate.error} />
      </EndpointCard>

      <EndpointCard title="Money Create" description="POST /quantlib/core/types/money/create">
        <Row>
          <Field label="Amount"><Input value={moneyAmt} onChange={setMoneyAmt} type="number" /></Field>
          <Field label="Currency"><Input value={moneyCcy} onChange={setMoneyCcy} /></Field>
          <RunButton loading={money.loading} onClick={() => money.run(() => typesApi.moneyCreate(Number(moneyAmt), moneyCcy))} />
        </Row>
        <ResultPanel data={money.result} error={money.error} />
      </EndpointCard>

      <EndpointCard title="Money Convert" description="POST /quantlib/core/types/money/convert">
        <Row>
          <Field label="Amount"><Input value={mcAmt} onChange={setMcAmt} type="number" /></Field>
          <Field label="From"><Input value={mcFrom} onChange={setMcFrom} /></Field>
          <Field label="To"><Input value={mcTo} onChange={setMcTo} /></Field>
          <Field label="Rate"><Input value={mcRate} onChange={setMcRate} type="number" /></Field>
          <RunButton loading={mConvert.loading} onClick={() => mConvert.run(() => typesApi.moneyConvert(Number(mcAmt), mcFrom, mcTo, Number(mcRate)))} />
        </Row>
        <ResultPanel data={mConvert.result} error={mConvert.error} />
      </EndpointCard>

      <EndpointCard title="Spread from BPS" description="POST /quantlib/core/types/spread/from-bps">
        <Row>
          <Field label="BPS"><Input value={bps} onChange={setBps} type="number" /></Field>
          <RunButton loading={spread.loading} onClick={() => spread.run(() => typesApi.spreadFromBps(Number(bps)))} />
        </Row>
        <ResultPanel data={spread.result} error={spread.error} />
      </EndpointCard>

      <EndpointCard title="Notional Schedule" description="POST /quantlib/core/types/notional-schedule">
        <Row>
          <Field label="Type"><Select value={nsType} onChange={setNsType} options={['bullet', 'amortizing', 'linear', 'custom']} /></Field>
          <Field label="Notional"><Input value={nsNotional} onChange={setNsNotional} type="number" /></Field>
          <Field label="Periods"><Input value={nsPeriods} onChange={setNsPeriods} type="number" /></Field>
          <Field label="Rate"><Input value={nsRate} onChange={setNsRate} type="number" /></Field>
          <RunButton loading={notSch.loading} onClick={() => notSch.run(() => typesApi.notionalSchedule(nsType, Number(nsNotional), Number(nsPeriods), Number(nsRate)))} />
        </Row>
        <ResultPanel data={notSch.result} error={notSch.error} />
      </EndpointCard>

      <EndpointCard title="List Currencies" description="GET /quantlib/core/types/currencies">
        <Row>
          <RunButton loading={currencies.loading} onClick={() => currencies.run(() => typesApi.listCurrencies())} label="Fetch" />
        </Row>
        <ResultPanel data={currencies.result} error={currencies.error} />
      </EndpointCard>

      <EndpointCard title="List Frequencies" description="GET /quantlib/core/types/frequencies">
        <Row>
          <RunButton loading={frequencies.loading} onClick={() => frequencies.run(() => typesApi.listFrequencies())} label="Fetch" />
        </Row>
        <ResultPanel data={frequencies.result} error={frequencies.error} />
      </EndpointCard>
    </>
  );
}
