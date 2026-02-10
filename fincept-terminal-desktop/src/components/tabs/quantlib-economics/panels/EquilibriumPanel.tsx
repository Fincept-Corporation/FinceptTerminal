import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { equilibriumApi } from '../quantlibEconomicsApi';

export default function EquilibriumPanel() {
  // Cobb-Douglas
  const cd = useEndpoint();
  const [cdAlphas, setCdAlphas] = useState('0.4, 0.6');
  const [cdPrices, setCdPrices] = useState('10, 20');
  const [cdWealth, setCdWealth] = useState('100');

  // CES
  const ces = useEndpoint();
  const [cesAlphas, setCesAlphas] = useState('0.4, 0.6');
  const [cesRho, setCesRho] = useState('0.5');
  const [cesPrices, setCesPrices] = useState('10, 20');
  const [cesWealth, setCesWealth] = useState('100');

  // Exchange Economy
  const ee = useEndpoint();
  const [eeEndow, setEeEndow] = useState('[[10,5],[5,10]]');
  const [eeAlphas, setEeAlphas] = useState('[[0.4,0.6],[0.5,0.5]]');

  // Walrasian
  const wal = useEndpoint();
  const [walEndow, setWalEndow] = useState('[[10,5],[5,10]]');
  const [walAlphas, setWalAlphas] = useState('[[0.4,0.6],[0.5,0.5]]');
  const [walIter, setWalIter] = useState('1000');
  const [walTol, setWalTol] = useState('0.000001');

  return (
    <>
      <EndpointCard title="Cobb-Douglas" description="Cobb-Douglas demand given alphas, prices, wealth">
        <Row>
          <Field label="Alphas" width="150px"><Input value={cdAlphas} onChange={setCdAlphas} /></Field>
          <Field label="Prices" width="150px"><Input value={cdPrices} onChange={setCdPrices} /></Field>
          <Field label="Wealth"><Input value={cdWealth} onChange={setCdWealth} type="number" /></Field>
          <RunButton loading={cd.loading} onClick={() => cd.run(() => equilibriumApi.cobbDouglas(cdAlphas.split(',').map(Number), cdPrices.split(',').map(Number), Number(cdWealth)))} />
        </Row>
        <ResultPanel data={cd.result} error={cd.error} />
      </EndpointCard>

      <EndpointCard title="CES" description="Constant Elasticity of Substitution demand">
        <Row>
          <Field label="Alphas" width="150px"><Input value={cesAlphas} onChange={setCesAlphas} /></Field>
          <Field label="Rho"><Input value={cesRho} onChange={setCesRho} type="number" /></Field>
          <Field label="Prices" width="150px"><Input value={cesPrices} onChange={setCesPrices} /></Field>
          <Field label="Wealth"><Input value={cesWealth} onChange={setCesWealth} type="number" /></Field>
          <RunButton loading={ces.loading} onClick={() => ces.run(() => equilibriumApi.ces(cesAlphas.split(',').map(Number), Number(cesRho), cesPrices.split(',').map(Number), Number(cesWealth)))} />
        </Row>
        <ResultPanel data={ces.result} error={ces.error} />
      </EndpointCard>

      <EndpointCard title="Exchange Economy" description="Compute equilibrium of a pure exchange economy">
        <Row>
          <Field label="Endowments (JSON 2D)" width="250px"><Input value={eeEndow} onChange={setEeEndow} /></Field>
          <Field label="Alphas (JSON 2D)" width="250px"><Input value={eeAlphas} onChange={setEeAlphas} /></Field>
          <RunButton loading={ee.loading} onClick={() => ee.run(() => equilibriumApi.exchangeEconomy(JSON.parse(eeEndow), JSON.parse(eeAlphas)))} />
        </Row>
        <ResultPanel data={ee.result} error={ee.error} />
      </EndpointCard>

      <EndpointCard title="Walrasian Equilibrium" description="Walrasian general equilibrium via tâtonnement">
        <Row>
          <Field label="Endowments (JSON 2D)" width="250px"><Input value={walEndow} onChange={setWalEndow} /></Field>
          <Field label="Alphas (JSON 2D)" width="250px"><Input value={walAlphas} onChange={setWalAlphas} /></Field>
          <Field label="Max Iter"><Input value={walIter} onChange={setWalIter} type="number" /></Field>
          <Field label="Tolerance"><Input value={walTol} onChange={setWalTol} type="number" /></Field>
          <RunButton loading={wal.loading} onClick={() => wal.run(() => equilibriumApi.walrasian(JSON.parse(walEndow), JSON.parse(walAlphas), Number(walIter), Number(walTol)))} />
        </Row>
        <ResultPanel data={wal.result} error={wal.error} />
      </EndpointCard>
    </>
  );
}
