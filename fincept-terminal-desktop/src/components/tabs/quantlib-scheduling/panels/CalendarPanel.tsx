import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { calendarApi, adjustmentApi } from '../quantlibSchedulingApi';

export default function CalendarPanel() {
  // Is Business Day
  const isBD = useEndpoint();
  const [ibdDate, setIbdDate] = useState('2025-01-15');
  const [ibdCal, setIbdCal] = useState('US');

  // Next Business Day
  const nbd = useEndpoint();
  const [nbdDate, setNbdDate] = useState('2025-01-18');
  const [nbdCal, setNbdCal] = useState('US');

  // Previous Business Day
  const pbd = useEndpoint();
  const [pbdDate, setPbdDate] = useState('2025-01-18');
  const [pbdCal, setPbdCal] = useState('US');

  // Add Business Days
  const abd = useEndpoint();
  const [abdDate, setAbdDate] = useState('2025-01-15');
  const [abdDays, setAbdDays] = useState('5');
  const [abdCal, setAbdCal] = useState('US');

  // Business Days Between
  const bdb = useEndpoint();
  const [bdbStart, setBdbStart] = useState('2025-01-01');
  const [bdbEnd, setBdbEnd] = useState('2025-01-31');
  const [bdbCal, setBdbCal] = useState('US');

  // List Calendars
  const lc = useEndpoint();

  // Adjust Date
  const adj = useEndpoint();
  const [adjDate, setAdjDate] = useState('2025-01-18');
  const [adjAdj, setAdjAdj] = useState('Modified Following');
  const [adjCal, setAdjCal] = useState('Weekend');

  // Batch Adjust
  const ba = useEndpoint();
  const [baDates, setBaDates] = useState('2025-01-18, 2025-01-19, 2025-03-29');
  const [baAdj, setBaAdj] = useState('Modified Following');

  // List Methods
  const lm = useEndpoint();

  return (
    <>
      <EndpointCard title="Is Business Day" description="Check if date is a business day">
        <Row>
          <Field label="Date"><Input value={ibdDate} onChange={setIbdDate} /></Field>
          <Field label="Calendar"><Input value={ibdCal} onChange={setIbdCal} /></Field>
          <RunButton loading={isBD.loading} onClick={() => isBD.run(() => calendarApi.isBusinessDay(ibdDate, ibdCal))} />
        </Row>
        <ResultPanel data={isBD.result} error={isBD.error} />
      </EndpointCard>

      <EndpointCard title="Next Business Day" description="Find next business day from date">
        <Row>
          <Field label="Date"><Input value={nbdDate} onChange={setNbdDate} /></Field>
          <Field label="Calendar"><Input value={nbdCal} onChange={setNbdCal} /></Field>
          <RunButton loading={nbd.loading} onClick={() => nbd.run(() => calendarApi.nextBusinessDay(nbdDate, nbdCal))} />
        </Row>
        <ResultPanel data={nbd.result} error={nbd.error} />
      </EndpointCard>

      <EndpointCard title="Previous Business Day" description="Find previous business day from date">
        <Row>
          <Field label="Date"><Input value={pbdDate} onChange={setPbdDate} /></Field>
          <Field label="Calendar"><Input value={pbdCal} onChange={setPbdCal} /></Field>
          <RunButton loading={pbd.loading} onClick={() => pbd.run(() => calendarApi.previousBusinessDay(pbdDate, pbdCal))} />
        </Row>
        <ResultPanel data={pbd.result} error={pbd.error} />
      </EndpointCard>

      <EndpointCard title="Add Business Days" description="Add N business days to date">
        <Row>
          <Field label="Date"><Input value={abdDate} onChange={setAbdDate} /></Field>
          <Field label="Days"><Input value={abdDays} onChange={setAbdDays} type="number" /></Field>
          <Field label="Calendar"><Input value={abdCal} onChange={setAbdCal} /></Field>
          <RunButton loading={abd.loading} onClick={() => abd.run(() => calendarApi.addBusinessDays(abdDate, Number(abdDays), abdCal))} />
        </Row>
        <ResultPanel data={abd.result} error={abd.error} />
      </EndpointCard>

      <EndpointCard title="Business Days Between" description="Count business days between two dates">
        <Row>
          <Field label="Start Date"><Input value={bdbStart} onChange={setBdbStart} /></Field>
          <Field label="End Date"><Input value={bdbEnd} onChange={setBdbEnd} /></Field>
          <Field label="Calendar"><Input value={bdbCal} onChange={setBdbCal} /></Field>
          <RunButton loading={bdb.loading} onClick={() => bdb.run(() => calendarApi.businessDaysBetween(bdbStart, bdbEnd, bdbCal))} />
        </Row>
        <ResultPanel data={bdb.result} error={bdb.error} />
      </EndpointCard>

      <EndpointCard title="List Calendars" description="Available calendar names">
        <Row>
          <RunButton loading={lc.loading} onClick={() => lc.run(() => calendarApi.list())} />
        </Row>
        <ResultPanel data={lc.result} error={lc.error} />
      </EndpointCard>

      <EndpointCard title="Adjust Date" description="Apply business day adjustment to a date">
        <Row>
          <Field label="Date"><Input value={adjDate} onChange={setAdjDate} /></Field>
          <Field label="Adjustment"><Select value={adjAdj} onChange={setAdjAdj} options={['Modified Following', 'Following', 'Preceding', 'Modified Preceding', 'Unadjusted']} /></Field>
          <Field label="Calendar"><Input value={adjCal} onChange={setAdjCal} /></Field>
          <RunButton loading={adj.loading} onClick={() => adj.run(() => adjustmentApi.adjustDate(adjDate, adjAdj, adjCal))} />
        </Row>
        <ResultPanel data={adj.result} error={adj.error} />
      </EndpointCard>

      <EndpointCard title="Batch Adjust Dates" description="Adjust multiple dates at once">
        <Row>
          <Field label="Dates (comma-sep)" width="350px"><Input value={baDates} onChange={setBaDates} /></Field>
          <Field label="Adjustment"><Select value={baAdj} onChange={setBaAdj} options={['Modified Following', 'Following', 'Preceding', 'Modified Preceding']} /></Field>
          <RunButton loading={ba.loading} onClick={() => ba.run(() => adjustmentApi.batchAdjust(baDates.split(',').map(s => s.trim()), baAdj))} />
        </Row>
        <ResultPanel data={ba.result} error={ba.error} />
      </EndpointCard>

      <EndpointCard title="List Adjustment Methods" description="Available adjustment conventions">
        <Row>
          <RunButton loading={lm.loading} onClick={() => lm.run(() => adjustmentApi.methods())} />
        </Row>
        <ResultPanel data={lm.result} error={lm.error} />
      </EndpointCard>
    </>
  );
}
