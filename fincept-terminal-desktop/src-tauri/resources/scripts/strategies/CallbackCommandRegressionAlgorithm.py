# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-E8875612
# Category: Regression Test
# Description: Regression algorithm asserting the behavior of different callback commands call
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

class InvalidCommand():
    variable = 10

class VoidCommand():
    quantity = 0
    target = []
    parameters = {}
    targettime = None

    def run(self, algo: QCAlgorithm) -> bool | None:
        if not self.targettime or self.targettime != algo.time:
            return
        tag = self.parameters["tag"]
        algo.order(self.target[0], self.get_quantity(), tag=tag)

    def get_quantity(self):
        return self.quantity

class BoolCommand(Command):
    something_else = {}
    array_test = []
    result = False

    def run(self, algo: QCAlgorithm) -> bool | None:
        trade_ibm = self.my_custom_method()
        if trade_ibm:
            algo.debug(f"BoolCommand.run: {str(self)}")
            algo.buy("IBM", 1)
        return trade_ibm

    def my_custom_method(self):
        return self.result

### <summary>
### Regression algorithm asserting the behavior of different callback commands call
### </summary>
class CallbackCommandRegressionAlgorithm(QCAlgorithm):
    def initialize(self):
        '''Initialise the data and resolution required, as well as the cash and start-end dates for your algorithm. All algorithms must initialized.'''

        self.set_start_date(2013, 10, 7)
        self.set_end_date(2013, 10, 11)

        self.add_equity("SPY")
        self.add_equity("IBM")
        self.add_equity("BAC")

        self.add_command(VoidCommand)
        self.add_command(BoolCommand)

        threw_exception = False
        try:
            self.add_command(InvalidCommand)
        except:
            threw_exception = True
        if not threw_exception:
            raise ValueError('InvalidCommand did not throw!')

        bool_command = BoolCommand()
        bool_command.result = True
        bool_command.something_else = { "Property": 10 }
        bool_command.array_test = [ "SPY", "BTCUSD" ]
        link = self.link(bool_command)
        if "&command[array_test][0]=SPY&command[array_test][1]=BTCUSD&command[result]=True&command[something_else][Property]=10&command[$type]=BoolCommand" not in link:
            raise ValueError(f'Invalid link was generated! {link}')

        potential_command = VoidCommand()
        potential_command.target = [ "BAC" ]
        potential_command.quantity = 10
        potential_command.parameters = { "tag": "Signal X" }

        command_link = self.link(potential_command)
        if "command[parameters][tag]=Signal+X&command[quantity]=10&command[target][0]=BAC&command[$type]=VoidCommand" not in command_link:
            raise ValueError(f'Invalid link was generated! {command_link}')
        self.notify.email("email@address", "Trade Command Event", f"Signal X trade\nFollow link to trigger: {command_link}")

        untyped_command_link = self.link({ "symbol": "SPY", "parameters": { "quantity": 10 } })
        if "&command[symbol]=SPY&command[parameters][quantity]=10" not in untyped_command_link:
            raise ValueError(f'Invalid link was generated! {untyped_command_link}')
        self.notify.email("email@address", "Untyped Command Event", f"Signal Y trade\nFollow link to trigger: {untyped_command_link}")

    def on_command(self, data):
        self.debug(f"on_command: {str(data)}")
        self.buy(data.symbol, data.parameters["quantity"])
        return True # False, None
