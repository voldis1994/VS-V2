import { ReactNode } from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import { Layout } from './components/Layout';
import { useWebSocket } from './hooks/useWebSocket';

import { OverviewPage } from './pages/Overview';
import { LiveTerminalPage } from './pages/LiveTerminal';
import { MarketsPage } from './pages/Markets';
import { BrainPage } from './pages/Brain';
import { StructurePage } from './pages/Structure';
import { PatternsPage } from './pages/Patterns';
import { ScenariosPage } from './pages/Scenarios';
import { PredictionsPage } from './pages/Predictions';
import { DecisionsPage } from './pages/Decisions';
import { PositionsPage } from './pages/Positions';
import { RiskPage } from './pages/Risk';
import { ExecutionPage } from './pages/Execution';
import { LearningPage } from './pages/Learning';
import { ModelsPage } from './pages/Models';
import { DiagnosticsPage } from './pages/Diagnostics';

import { MarketReaderPage } from './pages/MarketReaderPage';
import { TradingPage } from './pages/TradingPage';
import { ClientsPage } from './pages/ClientsPage';
import { BrokersPage } from './pages/BrokersPage';
import { TradesPage } from './pages/TradesPage';
import { FeedsPage } from './pages/FeedsPage';
import { SystemPage } from './pages/SystemPage';
import { LogsPage } from './pages/LogsPage';
import { SettingsPage } from './pages/SettingsPage';
import { RobotDeskPage } from './pages/RobotDeskPage';
import { ClientPanelPage } from './pages/ClientPanelPage';

function Desk({ children }: { children: ReactNode }) {
  return <Layout>{children}</Layout>;
}

export default function App() {
  useWebSocket();

  return (
    <Routes>
      <Route path="/robot" element={<RobotDeskPage />} />
      <Route path="/client" element={<ClientPanelPage />} />

      <Route path="/" element={<LiveTerminalPage />} />
      <Route path="/overview" element={<Desk><OverviewPage /></Desk>} />
      <Route path="/markets" element={<Desk><MarketsPage /></Desk>} />
      <Route path="/brain" element={<Desk><BrainPage /></Desk>} />
      <Route path="/structure" element={<Desk><StructurePage /></Desk>} />
      <Route path="/patterns" element={<Desk><PatternsPage /></Desk>} />
      <Route path="/scenarios" element={<Desk><ScenariosPage /></Desk>} />
      <Route path="/predictions" element={<Desk><PredictionsPage /></Desk>} />
      <Route path="/decisions" element={<Desk><DecisionsPage /></Desk>} />
      <Route path="/positions" element={<Desk><PositionsPage /></Desk>} />
      <Route path="/risk" element={<Desk><RiskPage /></Desk>} />
      <Route path="/execution" element={<Desk><ExecutionPage /></Desk>} />
      <Route path="/learning" element={<Desk><LearningPage /></Desk>} />
      <Route path="/models" element={<Desk><ModelsPage /></Desk>} />
      <Route path="/diagnostics" element={<Desk><DiagnosticsPage /></Desk>} />

      {/* Legacy VS aliases */}
      <Route path="/market" element={<Navigate to="/markets" replace />} />
      <Route path="/market-legacy" element={<Desk><MarketReaderPage /></Desk>} />
      <Route path="/trading" element={<Desk><TradingPage /></Desk>} />
      <Route path="/clients" element={<Desk><ClientsPage /></Desk>} />
      <Route path="/brokers" element={<Desk><BrokersPage /></Desk>} />
      <Route path="/trades" element={<Desk><TradesPage /></Desk>} />
      <Route path="/feeds" element={<Desk><FeedsPage /></Desk>} />
      <Route path="/system" element={<Desk><SystemPage /></Desk>} />
      <Route path="/logs" element={<Desk><LogsPage /></Desk>} />
      <Route path="/settings" element={<Desk><SettingsPage /></Desk>} />
    </Routes>
  );
}
