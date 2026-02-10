import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../shared';
import { conventionsApi } from '../quantlibCoreApi';

export default function ConventionsPanel() {
  const fmtDate = useEndpoint();
  const [fmtStr, setFmtStr] = useState('2024-06-15');
  const [fmtFmt, setFmtFmt] = useState('iso');

  const parseD = useEndpoint();
  const [parseDateStr, setParseDateStr] = useState('June 15, 2024');

  const dToY = useEndpoint();
  const [dtyVal, setDtyVal] = useState('365');
  const [dtyConv, setDtyConv] = useState('actual');

  const yToD = useEndpoint();
  const [ytdVal, setYtdVal] = useState('1');
  const [ytdConv, setYtdConv] = useState('actual');

  const normRate = useEndpoint();
  const [nrVal, setNrVal] = useState('5');
  const [nrUnit, setNrUnit] = useState('percent');

  const normVol = useEndpoint();
  const [nvVal, setNvVal] = useState('25');
  const [nvType, setNvType] = useState('lognormal');
  const [nvPct, setNvPct] = useState(true);

  return (
    <>
      <EndpointCard title="Format Date" description="POST /quantlib/core/conventions/format-date">
        <Row>
          <Field label="Date String"><Input value={fmtStr} onChange={setFmtStr} /></Field>
          <Field label="Format"><Select value={fmtFmt} onChange={setFmtFmt} options={['iso', 'us', 'eu', 'long']} /></Field>
          <RunButton loading={fmtDate.loading} onClick={() => fmtDate.run(() => conventionsApi.formatDate(fmtStr, fmtFmt))} />
        </Row>
        <ResultPanel data={fmtDate.result} error={fmtDate.error} />
      </EndpointCard>

      <EndpointCard title="Parse Date" description="POST /quantlib/core/conventions/parse-date">
        <Row>
          <Field label="Date String" width="240px"><Input value={parseDateStr} onChange={setParseDateStr} /></Field>
          <RunButton loading={parseD.loading} onClick={() => parseD.run(() => conventionsApi.parseDate(parseDateStr))} />
        </Row>
        <ResultPanel data={parseD.result} error={parseD.error} />
      </EndpointCard>

      <EndpointCard title="Days → Years" description="POST /quantlib/core/conventions/days-to-years">
        <Row>
          <Field label="Days"><Input value={dtyVal} onChange={setDtyVal} type="number" /></Field>
          <Field label="Convention"><Select value={dtyConv} onChange={setDtyConv} options={['actual', '30/360', 'ACT/365', 'ACT/360']} /></Field>
          <RunButton loading={dToY.loading} onClick={() => dToY.run(() => conventionsApi.daysToYears(Number(dtyVal), dtyConv))} />
        </Row>
        <ResultPanel data={dToY.result} error={dToY.error} />
      </EndpointCard>

      <EndpointCard title="Years → Days" description="POST /quantlib/core/conventions/years-to-days">
        <Row>
          <Field label="Years"><Input value={ytdVal} onChange={setYtdVal} type="number" /></Field>
          <Field label="Convention"><Select value={ytdConv} onChange={setYtdConv} options={['actual', '30/360', 'ACT/365', 'ACT/360']} /></Field>
          <RunButton loading={yToD.loading} onClick={() => yToD.run(() => conventionsApi.yearsToDays(Number(ytdVal), ytdConv))} />
        </Row>
        <ResultPanel data={yToD.result} error={yToD.error} />
      </EndpointCard>

      <EndpointCard title="Normalize Rate" description="POST /quantlib/core/conventions/normalize-rate">
        <Row>
          <Field label="Value"><Input value={nrVal} onChange={setNrVal} type="number" /></Field>
          <Field label="Unit"><Select value={nrUnit} onChange={setNrUnit} options={['decimal', 'percent', 'bps']} /></Field>
          <RunButton loading={normRate.loading} onClick={() => normRate.run(() => conventionsApi.normalizeRate(Number(nrVal), nrUnit))} />
        </Row>
        <ResultPanel data={normRate.result} error={normRate.error} />
      </EndpointCard>

      <EndpointCard title="Normalize Volatility" description="POST /quantlib/core/conventions/normalize-volatility">
        <Row>
          <Field label="Value"><Input value={nvVal} onChange={setNvVal} type="number" /></Field>
          <Field label="Vol Type"><Select value={nvType} onChange={setNvType} options={['lognormal', 'normal', 'shifted']} /></Field>
          <Field label="Is Percent">
            <Select value={nvPct ? 'true' : 'false'} onChange={v => setNvPct(v === 'true')} options={['true', 'false']} />
          </Field>
          <RunButton loading={normVol.loading} onClick={() => normVol.run(() => conventionsApi.normalizeVol(Number(nvVal), nvType, nvPct))} />
        </Row>
        <ResultPanel data={normVol.result} error={normVol.error} />
      </EndpointCard>
    </>
  );
}
