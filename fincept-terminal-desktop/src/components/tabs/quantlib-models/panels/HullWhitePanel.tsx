import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { hullWhiteApi } from '../quantlibModelsApi';

export default function HullWhitePanel() {
  const cal = useEndpoint();
  const [calKappa, setCalKappa] = useState('0.1');
  const [calSigma, setCalSigma] = useState('0.01');
  const [calR0, setCalR0] = useState('0.03');
  const [calTenors, setCalTenors] = useState('1, 2, 3, 5, 7, 10');
  const [calRates, setCalRates] = useState('0.03, 0.032, 0.034, 0.038, 0.04, 0.042');

  return (
    <>
      <EndpointCard title="Hull-White Calibrate" description="Calibrate Hull-White to market rates">
        <Row>
          <Field label="Kappa"><Input value={calKappa} onChange={setCalKappa} type="number" /></Field>
          <Field label="Sigma"><Input value={calSigma} onChange={setCalSigma} type="number" /></Field>
          <Field label="r0"><Input value={calR0} onChange={setCalR0} type="number" /></Field>
          <Field label="Market Tenors" width="250px"><Input value={calTenors} onChange={setCalTenors} /></Field>
          <Field label="Market Rates" width="250px"><Input value={calRates} onChange={setCalRates} /></Field>
          <RunButton loading={cal.loading} onClick={() => cal.run(() =>
            hullWhiteApi.calibrate(Number(calKappa), Number(calSigma), Number(calR0), calTenors.split(',').map(Number), calRates.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={cal.result} error={cal.error} />
      </EndpointCard>
    </>
  );
}
