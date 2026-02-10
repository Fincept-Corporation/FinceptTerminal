import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { saccrApi } from '../quantlibRegulatoryApi';

export default function SaccrPanel() {
  const ead = useEndpoint();
  const [mtm, setMtm] = useState('50000');
  const [addon, setAddon] = useState('10000');
  const [collateral, setCollateral] = useState('0');
  const [alpha, setAlpha] = useState('1.4');

  return (
    <>
      <EndpointCard title="SA-CCR EAD" description="Standardized approach for counterparty credit risk">
        <Row>
          <Field label="MTM Value"><Input value={mtm} onChange={setMtm} type="number" /></Field>
          <Field label="Add-on"><Input value={addon} onChange={setAddon} type="number" /></Field>
          <Field label="Collateral"><Input value={collateral} onChange={setCollateral} type="number" /></Field>
          <Field label="Alpha"><Input value={alpha} onChange={setAlpha} type="number" /></Field>
          <RunButton loading={ead.loading} onClick={() => ead.run(() =>
            saccrApi.ead(Number(mtm), Number(addon), Number(collateral), Number(alpha))
          )} />
        </Row>
        <ResultPanel data={ead.result} error={ead.error} />
      </EndpointCard>
    </>
  );
}
