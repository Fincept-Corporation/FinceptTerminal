import { useState, useEffect } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow
} from '@/components/ui/table';

const DashboardScreen = () => {
  const [currentTime, setCurrentTime] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Custom Bloomberg Terminal Styling
  const terminalStyles = `
    .terminal-bg {
      background: #000000 !important;
      border: 1px solid #FF6600 !important;
      border-radius: 0 !important;
    }
    .terminal-header {
      background: #FF6600 !important;
      color: #000000 !important;
      border-radius: 0 !important;
      font-family: 'Courier New', monospace !important;
      font-weight: bold !important;
      padding: 2px 4px !important;
      margin: 0 !important;
    }
    .terminal-text {
      font-family: 'Courier New', monospace !important;
      font-size: 11px !important;
      line-height: 1.2 !important;
      color: #FFFFFF !important;
    }
    .terminal-orange { color: #FF6600 !important; }
    .terminal-green { color: #00FF00 !important; }
    .terminal-red { color: #FF0000 !important; }
    .terminal-yellow { color: #FFFF00 !important; }
    .terminal-cyan { color: #00FFFF !important; }
    .terminal-gray { color: #808080 !important; }
    .terminal-table {
      border-collapse: collapse !important;
      width: 100% !important;
      font-family: 'Courier New', monospace !important;
      font-size: 10px !important;
    }
    .terminal-table th {
      background: #333333 !important;
      color: #FFFF00 !important;
      border: 1px solid #808080 !important;
      padding: 2px 4px !important;
      text-align: left !important;
    }
    .terminal-table td {
      border: 1px solid #333333 !important;
      padding: 1px 4px !important;
      color: #FFFFFF !important;
    }
    .terminal-button {
      background: #000000 !important;
      border: 1px solid #FFFF00 !important;
      color: #FFFF00 !important;
      border-radius: 0 !important;
      font-family: 'Courier New', monospace !important;
      font-size: 9px !important;
      padding: 1px 6px !important;
      margin: 1px !important;
      height: auto !important;
    }
    .terminal-button:hover {
      background: #FFFF00 !important;
      color: #000000 !important;
    }
    .india-border {
      border: 2px solid #00FF00 !important;
      background: #000000 !important;
    }
    .india-header {
      background: #00FF00 !important;
      color: #000000 !important;
      text-align: center !important;
    }
  `;

  const countries = {
    "India": { gdp_trillion: 3.9, military_budget_billion: 83.6, political_stability: 6.8, current_risk: "Medium", market_influence: 7.8 },
    "United States": { gdp_trillion: 27.4, military_budget_billion: 842, political_stability: 7.0, current_risk: "Low", market_influence: 9.5 },
    "China": { gdp_trillion: 18.2, military_budget_billion: 296, political_stability: 6.5, current_risk: "Medium", market_influence: 8.9 },
    "Russia": { gdp_trillion: 2.0, military_budget_billion: 109, political_stability: 4.0, current_risk: "High", market_influence: 6.2 }
  };

  const intelligenceFeeds = [
    { time: "16:15", priority: "HIGH", source: "HUMINT", classification: "SECRET", event: "India-China border talks scheduled; infrastructure buildout accelerating" },
    { time: "16:02", priority: "CRITICAL", source: "ECONINT", classification: "TOP SECRET", event: "Significant FDI inflows to India's semiconductor sector detected" },
    { time: "15:48", priority: "MEDIUM", source: "OSINT", classification: "CONFIDENTIAL", event: "India-Middle East-Europe Corridor gaining momentum among traders" }
  ];

  const marketData = [
    { asset: "India Nifty 50", impact: +0.95, reason: "FDI Strong" },
    { asset: "S&P 500", impact: -1.8, reason: "Geopolitical Risk" },
    { asset: "Energy Sector", impact: +3.8, reason: "Supply Risk" },
    { asset: "Tech Stocks", impact: -2.9, reason: "US-China Tension" },
    { asset: "Gold", impact: +2.1, reason: "Safe Haven" }
  ];

  return (
    <>
      <style>{terminalStyles}</style>
      <div className="min-h-screen bg-black text-white p-2 terminal-text">

        {/* Bloomberg Header */}
        <Card className="terminal-bg mb-2">
          <CardHeader className="terminal-header p-2">
            <div className="text-sm font-bold">
              ████████ BLOOMBERG TERMINAL ████████ GEOPOLITICAL INTELLIGENCE SYSTEM ████████
            </div>
          </CardHeader>
          <CardContent className="p-2">
            <div className="terminal-text text-xs">
              <span className="terminal-orange">GLOBAL RISK: </span>
              <span className="terminal-red">7.2/10 </span>
              <span className="terminal-gray">| </span>
              <span className="terminal-orange">CONFLICTS: </span>
              <span className="terminal-red">15 </span>
              <span className="terminal-gray">| </span>
              <span className="terminal-orange">SANCTIONS: </span>
              <span className="terminal-yellow">3124 </span>
              <span className="terminal-gray">| </span>
              <span className="terminal-white">{currentTime.toLocaleString()}</span>
            </div>
            <div className="terminal-text text-xs mt-1">
              <span className="terminal-gray">INDIA NIFTY: </span>
              <span className="terminal-green">24,850 </span>
              <span className="terminal-gray">| INR/USD: </span>
              <span className="terminal-cyan">83.12 </span>
              <span className="terminal-gray">| OIL: </span>
              <span className="terminal-green">$82.30 </span>
              <span className="terminal-gray">| ALERTS: </span>
              <span className="terminal-red">52 ACTIVE</span>
            </div>
          </CardContent>
        </Card>

        {/* Function Keys */}
        <div className="mb-2 flex gap-1 flex-wrap">
          {["F1:INDIA", "F2:CHINA", "F3:PAKISTAN", "F4:USA", "F5:RUSSIA", "F6:LAC", "F7:QUAD", "F8:BRICS"].map(key => (
            <Button key={key} className="terminal-button">
              {key}
            </Button>
          ))}
        </div>

        {/* Main Dashboard Grid */}
        <div className="grid grid-cols-3 gap-2 mb-4">

          {/* Intelligence Alerts */}
          <Card className="terminal-bg">
            <CardHeader className="terminal-header p-1">
              <CardTitle className="text-xs">■ REAL-TIME INTELLIGENCE ALERTS ■</CardTitle>
            </CardHeader>
            <CardContent className="p-2">
              {intelligenceFeeds.map((alert, idx) => (
                <div key={idx} className="mb-3 text-xs terminal-text">
                  <div className="flex gap-2">
                    <span className="terminal-gray">{alert.time}</span>
                    <span className={alert.priority === 'CRITICAL' ? 'terminal-red' : alert.priority === 'HIGH' ? 'terminal-orange' : 'terminal-yellow'}>
                      [{alert.priority}]
                    </span>
                    <span className="terminal-cyan">[{alert.source}]</span>
                  </div>
                  <div className="mt-1">{alert.event}</div>
                  <div className="terminal-yellow text-xs">CLASSIFICATION: {alert.classification}</div>
                  <div className="border-b border-gray-600 mt-1"></div>
                </div>
              ))}
            </CardContent>
          </Card>

          {/* Country Risk Matrix */}
          <Card className="terminal-bg">
            <CardHeader className="terminal-header p-1">
              <CardTitle className="text-xs">■ COUNTRY RISK MATRIX ■</CardTitle>
            </CardHeader>
            <CardContent className="p-2">
              <Table className="terminal-table">
                <TableHeader>
                  <TableRow>
                    <TableHead className="text-xs">COUNTRY</TableHead>
                    <TableHead className="text-xs">RISK</TableHead>
                    <TableHead className="text-xs">STAB</TableHead>
                    <TableHead className="text-xs">MIL($B)</TableHead>
                    <TableHead className="text-xs">GDP</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {Object.entries(countries).map(([country, data]) => (
                    <TableRow key={country}>
                      <TableCell className="text-xs">{country.substring(0,8)}</TableCell>
                      <TableCell className={`text-xs ${
                        data.current_risk === 'Low' ? 'terminal-green' :
                        data.current_risk === 'Medium' ? 'terminal-yellow' : 'terminal-red'
                      }`}>
                        {data.current_risk.substring(0,3).toUpperCase()}
                      </TableCell>
                      <TableCell className="text-xs terminal-cyan">{data.political_stability}</TableCell>
                      <TableCell className="text-xs terminal-red">${data.military_budget_billion}</TableCell>
                      <TableCell className="text-xs terminal-green">${data.gdp_trillion}T</TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </CardContent>
          </Card>

          {/* Market Impact */}
          <Card className="terminal-bg">
            <CardHeader className="terminal-header p-1">
              <CardTitle className="text-xs">■ MARKET IMPACT SUMMARY ■</CardTitle>
            </CardHeader>
            <CardContent className="p-2">
              <Table className="terminal-table">
                <TableHeader>
                  <TableRow>
                    <TableHead className="text-xs">ASSET</TableHead>
                    <TableHead className="text-xs">IMPACT</TableHead>
                    <TableHead className="text-xs">REASON</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {marketData.map((item, idx) => (
                    <TableRow key={idx}>
                      <TableCell className="text-xs">{item.asset.substring(0,12)}</TableCell>
                      <TableCell className={`text-xs ${item.impact > 0 ? 'terminal-green' : 'terminal-red'}`}>
                        {item.impact > 0 ? '+' : ''}{item.impact}%
                      </TableCell>
                      <TableCell className="text-xs terminal-gray">{item.reason}</TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </CardContent>
          </Card>
        </div>

        {/* India-Specific Dashboard */}
        <Card className="india-border mb-4">
          <CardHeader className="india-header p-2">
            <CardTitle className="text-sm font-bold">
              ████ INDIA GEOPOLITICAL DASHBOARD - REAL-TIME ANALYSIS ████
            </CardTitle>
          </CardHeader>
          <CardContent className="p-2">
            <div className="grid grid-cols-4 gap-2">

              <Card className="terminal-bg">
                <CardHeader className="p-1">
                  <CardTitle className="text-xs terminal-yellow">INDIA: KEY METRICS</CardTitle>
                </CardHeader>
                <CardContent className="p-2">
                  <Table className="terminal-table">
                    <TableBody>
                      <TableRow>
                        <TableCell className="text-xs">GDP:</TableCell>
                        <TableCell className="text-xs terminal-green">$3.9T</TableCell>
                      </TableRow>
                      <TableRow>
                        <TableCell className="text-xs">Population:</TableCell>
                        <TableCell className="text-xs terminal-cyan">1428M</TableCell>
                      </TableRow>
                      <TableRow>
                        <TableCell className="text-xs">Military:</TableCell>
                        <TableCell className="text-xs terminal-red">$83.6B</TableCell>
                      </TableRow>
                      <TableRow>
                        <TableCell className="text-xs">Risk Level:</TableCell>
                        <TableCell className="text-xs terminal-yellow">MEDIUM</TableCell>
                      </TableRow>
                    </TableBody>
                  </Table>
                </CardContent>
              </Card>

              <Card className="terminal-bg">
                <CardHeader className="p-1">
                  <CardTitle className="text-xs terminal-yellow">STRATEGIC DEVELOPMENTS</CardTitle>
                </CardHeader>
                <CardContent className="p-2 text-xs">
                  <div className="terminal-green">✓ G20 Presidency Success</div>
                  <div className="terminal-green">✓ Quad Alliance Active</div>
                  <div className="terminal-green">✓ Digital India Growth</div>
                  <div className="terminal-orange">⚠ LAC Border Tensions</div>
                  <div className="terminal-orange">⚠ Pakistan Relations</div>
                  <div className="terminal-green">✓ Manufacturing Hub</div>
                </CardContent>
              </Card>

              <Card className="terminal-bg">
                <CardHeader className="p-1">
                  <CardTitle className="text-xs terminal-yellow">THREAT ASSESSMENT</CardTitle>
                </CardHeader>
                <CardContent className="p-2">
                  <Table className="terminal-table">
                    <TableBody>
                      {[
                        {threat: "China Border", level: 7.8, category: "HIGH"},
                        {threat: "Pakistan Terror", level: 6.9, category: "MEDIUM"},
                        {threat: "Cyber Threats", level: 6.5, category: "MEDIUM"},
                        {threat: "Energy Security", level: 5.8, category: "MEDIUM"}
                      ].map((item, idx) => (
                        <TableRow key={idx}>
                          <TableCell className="text-xs">{item.threat.substring(0,10)}:</TableCell>
                          <TableCell className={`text-xs ${item.level >= 7.0 ? 'terminal-orange' : 'terminal-yellow'}`}>
                            {item.level}
                          </TableCell>
                          <TableCell className={`text-xs ${item.level >= 7.0 ? 'terminal-orange' : 'terminal-yellow'}`}>
                            [{item.category.substring(0,3)}]
                          </TableCell>
                        </TableRow>
                      ))}
                    </TableBody>
                  </Table>
                </CardContent>
              </Card>

              <Card className="terminal-bg">
                <CardHeader className="p-1">
                  <CardTitle className="text-xs terminal-yellow">STRATEGIC OPPORTUNITIES</CardTitle>
                </CardHeader>
                <CardContent className="p-2 text-xs terminal-green">
                  <div>• Global Manufacturing Hub</div>
                  <div>• Semiconductor Production</div>
                  <div>• Renewable Energy Leader</div>
                  <div>• Defense Exports Growth</div>
                  <div>• IMEC Trade Corridor</div>
                  <div>• Digital Economy Expansion</div>
                </CardContent>
              </Card>
            </div>
          </CardContent>
        </Card>

        {/* Status Bar */}
        <Card className="terminal-bg">
          <CardContent className="p-2 terminal-header">
            <div className="flex justify-between text-xs font-bold">
              <div>SYSTEM STATUS: OPERATIONAL | DATA FEEDS: ACTIVE | INTEL PROCESSING: REAL-TIME</div>
              <div>Last Update: {currentTime.toLocaleTimeString()} | Classification: CONFIDENTIAL</div>
            </div>
          </CardContent>
        </Card>
      </div>
    </>
  );
};

export default DashboardScreen;