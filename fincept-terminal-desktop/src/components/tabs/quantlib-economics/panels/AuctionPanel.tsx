import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { auctionApi } from '../quantlibEconomicsApi';

const auctionTypes = ['first_price', 'second_price', 'english', 'dutch'];

export default function AuctionPanel() {
  // Run Auction
  const ra = useEndpoint();
  const [raType, setRaType] = useState('first_price');
  const [raN, setRaN] = useState('5');
  const [raVals, setRaVals] = useState('10, 15, 12, 8, 20');

  // Equilibrium Bid
  const eb = useEndpoint();
  const [ebType, setEbType] = useState('first_price');
  const [ebN, setEbN] = useState('5');
  const [ebVals, setEbVals] = useState('10, 15, 12, 8, 20');

  // Expected Revenue
  const er = useEndpoint();
  const [erType, setErType] = useState('first_price');
  const [erN, setErN] = useState('5');

  // Simulate
  const sim = useEndpoint();
  const [simType, setSimType] = useState('first_price');
  const [simN, setSimN] = useState('5');
  const [simSims, setSimSims] = useState('10000');
  const [simDist, setSimDist] = useState('uniform');

  return (
    <>
      <EndpointCard title="Run Auction" description="Run a single auction with given valuations">
        <Row>
          <Field label="Type"><Select value={raType} onChange={setRaType} options={auctionTypes} /></Field>
          <Field label="N Bidders"><Input value={raN} onChange={setRaN} type="number" /></Field>
          <Field label="Valuations" width="250px"><Input value={raVals} onChange={setRaVals} /></Field>
          <RunButton loading={ra.loading} onClick={() => ra.run(() => auctionApi.run(raType, Number(raN), raVals.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ra.result} error={ra.error} />
      </EndpointCard>

      <EndpointCard title="Equilibrium Bid" description="Compute equilibrium bidding strategies">
        <Row>
          <Field label="Type"><Select value={ebType} onChange={setEbType} options={auctionTypes} /></Field>
          <Field label="N Bidders"><Input value={ebN} onChange={setEbN} type="number" /></Field>
          <Field label="Valuations" width="250px"><Input value={ebVals} onChange={setEbVals} /></Field>
          <RunButton loading={eb.loading} onClick={() => eb.run(() => auctionApi.equilibriumBid(ebType, Number(ebN), ebVals.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={eb.result} error={eb.error} />
      </EndpointCard>

      <EndpointCard title="Expected Revenue" description="Theoretical expected revenue by auction type">
        <Row>
          <Field label="Type"><Select value={erType} onChange={setErType} options={auctionTypes} /></Field>
          <Field label="N Bidders"><Input value={erN} onChange={setErN} type="number" /></Field>
          <RunButton loading={er.loading} onClick={() => er.run(() => auctionApi.expectedRevenue(erType, Number(erN)))} />
        </Row>
        <ResultPanel data={er.result} error={er.error} />
      </EndpointCard>

      <EndpointCard title="Simulate Auctions" description="Monte Carlo auction simulation">
        <Row>
          <Field label="Type"><Select value={simType} onChange={setSimType} options={auctionTypes} /></Field>
          <Field label="N Bidders"><Input value={simN} onChange={setSimN} type="number" /></Field>
          <Field label="Simulations"><Input value={simSims} onChange={setSimSims} type="number" /></Field>
          <Field label="Distribution"><Select value={simDist} onChange={setSimDist} options={['uniform', 'normal', 'exponential']} /></Field>
          <RunButton loading={sim.loading} onClick={() => sim.run(() => auctionApi.simulate(simType, Number(simN), Number(simSims), simDist))} />
        </Row>
        <ResultPanel data={sim.result} error={sim.error} />
      </EndpointCard>
    </>
  );
}
