# QUANTCONNECT.COM - Democratizing Finance, Empowering Individuals.
# Lean Algorithmic Trading Engine v2.0. Copyright 2014 QuantConnect Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Redistributed by Fincept Corporation in the Fincept Terminal with header/format changes only; the code is unchanged.

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
