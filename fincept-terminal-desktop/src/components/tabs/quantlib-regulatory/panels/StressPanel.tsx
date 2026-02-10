import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { stressApi } from '../quantlibRegulatoryApi';

export default function StressPanel() {
  const cp = useEndpoint();
  const [cpCap, setCpCap] = useState('80000');
  const [cpRwa, setCpRwa] = useState('500000');
  const [cpEarnings, setCpEarnings] = useState('10000, 12000, 11000');
  const [cpLosses, setCpLosses] = useState('5000, 15000, 8000');
  const [cpRwaChg, setCpRwaChg] = useState('0, 20000, -10000');

  return (
    <>
      <EndpointCard title="Capital Projection" description="Multi-period stress test capital projection">
        <Row>
          <Field label="Initial Capital"><Input value={cpCap} onChange={setCpCap} type="number" /></Field>
          <Field label="Initial RWA"><Input value={cpRwa} onChange={setCpRwa} type="number" /></Field>
          <Field label="Earnings" width="200px"><Input value={cpEarnings} onChange={setCpEarnings} /></Field>
          <Field label="Losses" width="200px"><Input value={cpLosses} onChange={setCpLosses} /></Field>
          <Field label="RWA Changes" width="200px"><Input value={cpRwaChg} onChange={setCpRwaChg} /></Field>
          <RunButton loading={cp.loading} onClick={() => cp.run(() =>
            stressApi.capitalProjection(
              Number(cpCap), Number(cpRwa),
              cpEarnings.split(',').map(Number), cpLosses.split(',').map(Number), cpRwaChg.split(',').map(Number)
            )
          )} />
        </Row>
        <ResultPanel data={cp.result} error={cp.error} />
      </EndpointCard>
    </>
  );
}
