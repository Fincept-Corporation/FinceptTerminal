# Geopolitics Agents - Book-Based Analysis Framework

## 📁 **Final File Structure (3 Main Guides + Agent Folders)**

```
GeopoliticsAgents/
├── README.md                              # Project overview and quick start guide
├── PRISONERS_OF_GEOGRAPHY_AGENT_GUIDE.md  # Tim Marshall's geographic determinism
│   └── geography_agents/                    # Individual agent implementations
│       ├── russia_geography_agent.py          # Russia: Plain vulnerability & warm-water ports
│       ├── china_geography_agent.py           # China: Maritime vulnerability & unity challenges
│       ├── us_geography_agent.py             # US: Geographic advantages & projection
│       ├── europe_geography_agent.py          # Europe: Fragmentation vs integration
│       ├── middle_east_geography_agent.py    # Arab World: Desert geography & resources
│       └── [other_region_agents]...        # South Asia, Africa, etc.
└── WORLD_ORDER_AGENT_GUIDE.md              # Henry Kissinger's diplomatic framework
    └── world_order_agents/                   # Individual agent implementations
        ├── westphalian_order_agent.py         # Sovereignty & balance of power
        ├── chinese_order_agent.py             # Middle Kingdom & hierarchical order
        ├── american_order_agent.py             # Liberal internationalism & exceptionalism
        ├── balance_of_power_agent.py          # Power transition & equilibrium
        ├── nuclear_order_agent.py             # Deterrence & arms control
        └── [other_order_agents]...         # Islamic, Digital Age, etc.
```

## 🗂️ **Agent Organization Guidelines**

### **Single Agent vs. Multiple Agents**
- **Single File Approach**: Each region/order system = one Python file
- **Logical Grouping**: Related agents in subfolders for organization
- **Independent Functionality**: Each agent can run standalone
- **Inter-agent Communication**: Shared insights between related agents

### **File Naming Convention**
```python
# Geography Agents (Marshall)
russia_geography_agent.py     # Russia geographic determinism
china_geography_agent.py      # China geographic determinism
us_geography_agent.py         # US geographic determinism

# World Order Agents (Kissinger)
westphalian_order_agent.py    # Westphalian sovereignty system
chinese_order_agent.py        # Chinese hierarchical order
american_order_agent.py        # American liberal order
```

### **Agent Structure Template**
```python
# File: geography_agents/russia_geography_agent.py

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - current_events (array)
#   - GEOPOLITICAL_API_KEY
#
# OUTPUT:
#   - Geographic constraints analysis
#   - Strategic imperatives based on geography
#   - Marshall thesis compliance score
#
# PARAMETERS:
#   - analysis_depth="comprehensive"
#   - framework="marshalls_determinism"
"""

from fincept_terminal.Agents.src.graph.state import AgentState, show_agent_reasoning
from fincept_terminal.Agents.src.tools.api import get_geopolitical_data
# ... other imports

def russia_geography_agent(state: AgentState):
    """
    RUSSIA GEOGRAPHY AGENT - Tim Marshall's Framework

    Core Thesis: Russian expansion driven by geographic insecurity
    - Flat plain vulnerability creates perpetual defense needs
    - Warm-water port obsession (Black Sea, Baltic, Pacific)
    - Geographic constraints DETERMINE foreign policy
    - Historical patterns repeat due to immutable geography

    Marshall Compliance Required: Geographic determinism focus
    """

    # Implementation following guide template
    # ... agent logic
```

## 📋 **Implementation Workflow**

### **1. Choose Your Book Framework**
```bash
# For Marshall's geographic determinism
cd geography_agents/  # Implement regional agents

# For Kissinger's diplomatic framework
cd world_order_agents/  # Implement order system agents
```

### **2. Follow Implementation Guide**
```bash
# Read relevant guide for framework specifics
cat PRISONERS_OF_GEOGRAPHY_AGENT_GUIDE.md    # Marshall framework
cat WORLD_ORDER_AGENT_GUIDE.md              # Kissinger framework
```

### **3. Implement Individual Agents**
```bash
# Create agents in appropriate subfolder
# Each agent focuses on specific region/order system
# Follow naming convention and template structure
```

### **4. Test & Validate**
```bash
# Test each agent with dummy data from guides
# Ensure 70%+ book thesis compliance
# Validate LLM integration and inter-agent communication
```

## 🎯 **Success Criteria**

### **Individual Agent Standards**
- ✅ **Book Thesis Compliance**: ≥70% framework alignment
- ✅ **Geographic/Order Specificity**: Deep understanding of core concepts
- ✅ **Modular Design**: Independent operation with error handling
- ✅ **LLM Integration**: Real API calls with structured prompts
- ✅ **Inter-agent Communication**: Share relevant insights between agents

### **Folder Organization Benefits**
- **Clear Structure**: Logical grouping by book framework
- **Easy Navigation**: Related agents co-located
- **Scalable**: Easy to add new regions/order systems
- **Maintainable**: Separate concerns reduce complexity

## 🎯 **Project Overview**

This folder contains comprehensive guides for converting influential geopolitics books into specialized AI agents that analyze current events through the authors' specific theoretical frameworks.

### **Supported Book Frameworks:**

1. **"Prisoners of Geography" by Tim Marshall**
   - **Core Thesis**: Geographic determinism - geography DETERMINES, not just influences, geopolitical behavior
   - **Focus Areas**: Regional constraints, historical patterns, strategic imperatives
   - **Agent Types**: Russia, China, US, Europe, Middle East, Africa, etc.

2. **"World Order" by Henry Kissinger**
   - **Core Thesis**: Realpolitik and balance of power in international order evolution
   - **Focus Areas**: Civilizational orders, diplomatic practice, historical continuity
   - **Agent Types**: Westphalian, Chinese, American, Balance of Power, Nuclear

## 🚨 **Critical Implementation Requirements**

### **Core Concepts Over Chapter Coverage**
⚠️ **IMPORTANT**: Agents must demonstrate DEEP understanding of author's core thesis, NOT superficial chapter coverage

### **Book Fidelity Standards**
- **Marshall Agents**: Must show geographic determinism, historical patterns, inevitability
- **Kissinger Agents**: Must show realpolitik approach, balance of power, historical continuity
- **Compliance Threshold**: Minimum 70% depth score on concept recognition

### **Production Requirements**
- Modular error handling
- Multi-LLM provider support (OpenAI, Anthropic)
- Agent intercommunication where frameworks interact
- Real-world data integration
- Frontend Tauri command structure

## 🧪 **Testing & Usage**

### **Quick Start Testing**
```bash
# Set up environment
export OPENAI_API_KEY="your-key-here"
export ANTHROPIC_API_KEY="your-key-here"

# Test Geography agents (Marshall framework)
python test_agents/russia_geography_test.py

# Test World Order agents (Kissinger framework)
python test_agents/westphalian_order_test.py
```

### **Usage Examples**
- **`GEOGRAPHY_AGENT_USAGE_EXAMPLE.md`**: Complete testing framework for Marshall agents
- **`WORLD_ORDER_AGENT_USAGE_EXAMPLE.md`**: Complete testing framework for Kissinger agents
- Both include dummy data, LLM integration, and compliance validation

## 📚 **Implementation Order**

1. **Study the Core Framework** - Read the relevant guide (Geography or World Order)
2. **Set Up Testing** - Follow usage example for dummy data and LLM testing
3. **Implement Agent** - Use guide templates and production patterns
4. **Validate Depth** - Ensure 70%+ compliance with author's core thesis
5. **Test Integration** - Verify agent intercommunication and frontend connectivity
6. **Production Deployment** - Integrate with Tauri commands and React frontend

## 🏗️ **Frontend Integration**

### **Command Structure**
```bash
# Geography agents (Marshall)
/geography analyze russia ukraine-conflict-2024
/geography analyze china south-china-sea-militarization
/geography network russia-china arctic-competition

# World Order agents (Kissinger)
/worldorder analyze westphalian ukraine-sovereignty
/worldorder analyze chinese south-china-sea-hierarchy
/worldorder compare westphalian chinese cyber-sovereignty
```

### **React Components**
- `GeographyAgentAnalysis`: Display geographic determinants and historical patterns
- `WorldOrderAgentAnalysis`: Display diplomatic frameworks and balance of power calculations

## 🤖 **LLM Provider Support**

Both frameworks support multiple LLM providers with structured prompts:

- **OpenAI GPT-4**: For complex geopolitical analysis
- **Anthropic Claude**: For diplomatic nuance and historical context
- **Extensible**: Easy to add additional providers

## 🎯 **Success Criteria**

### **Agent Quality Standards**
- ✅ **Core Concept Compliance**: 70%+ depth score on author's thesis
- ✅ **Historical Pattern Recognition**: Connects current events to historical precedents
- ✅ **Framework Fidelity**: Strict adherence to author's specific analytical approach
- ✅ **Production Readiness**: Error handling, modular design, LLM integration
- ✅ **Interagent Communication**: Shared insights where frameworks interact

### **Expected Outputs**
- **Geographic Determinism**: Events analyzed through constraint/imperative lens (Marshall)
- **Realpolitik Assessment**: Events analyzed through power/balance lens (Kissinger)
- **Historical Continuity**: Recognition of recurring patterns in international relations
- **Predictive Analysis**: Geographic/order-based outcome predictions

## 🚀 **Getting Started**

1. **Choose Your Framework**: Geography (Marshall) or World Order (Kissinger)
2. **Read Implementation Guide**: Detailed agent architecture and coding patterns
3. **Follow Usage Example**: Set up testing with dummy data and real LLM
4. **Build Your Agent**: Use templates and production requirements
5. **Validate & Test**: Ensure compliance with author's core thesis
6. **Deploy**: Integrate with Fincept Terminal Tauri commands

## 📞 **Support & Contribution**

This framework is designed for open-source contribution. When adding new agents:

- **Maintain Book Fidelity**: Ensure agents reflect author's core thesis
- **Follow Production Patterns**: Error handling, modular design, LLM integration
- **Include Testing**: Add dummy data and validation examples
- **Document Clearly**: Update guides with new agent patterns

**For questions or contributions, refer to the specific implementation guides and usage examples in this folder.**