# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-A18CA8E6
# Category: Template
# Description: Basic Template Library Class ### Library classes are snippets of code/classes you can reuse between projects. They ar...
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Basic Template Library Class
###
### Library classes are snippets of code/classes you can reuse between projects. They are
### added to projects on compile. This can be useful for reusing indicators, math functions,
### risk modules etc. Make sure you import the class in your algorithm. You need
### to name the file the module you'll be importing (not main.cs).
### importing.
### </summary>
class BasicTemplateLibrary:

    '''
    To use this library place this at the top:
    from BasicTemplateLibrary import BasicTemplateLibrary

    Then instantiate the function:
    x = BasicTemplateLibrary()
    x.add(1,2)
    '''
    def add(self, a, b):
        return a + b

    def subtract(self, a, b):
        return a - b
