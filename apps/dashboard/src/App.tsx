import { Routes, Route, Navigate } from 'react-router-dom';
import { useWebSocket } from './hooks/useWebSocket';
import { ControlLayout } from './control/ControlLayout';
import { ControlMainPage } from './control/ControlMainPage';
import { ControlClientsPage } from './control/ControlClientsPage';
import {
  ControlAiPage,
  ControlErrorsPage,
  ControlFeedPage,
  ControlNewsPage,
  ControlSystemPage,
} from './control/ControlExtraPages';
import { ClientPanelPage } from './pages/ClientPanelPage';

/**
 * VS SYSTEM — Control Panel is the ONLY admin UI.
 * Public client site stays at /client.
 * All legacy "Tactical Desk" routes redirect into /control/*.
 */
export default function App() {
  useWebSocket();

  return (
    <Routes>
      {/* Public client web (separate product surface) */}
      <Route path="/client" element={<ClientPanelPage />} />
      <Route path="/client/*" element={<ClientPanelPage />} />

      {/* Admin Control Panel — sole operator UI */}
      <Route path="/control" element={<ControlLayout />}>
        <Route index element={<ControlMainPage />} />
        <Route path="clients" element={<ControlClientsPage />} />
        <Route path="ai" element={<ControlAiPage />} />
        <Route path="errors" element={<ControlErrorsPage />} />
        <Route path="feed" element={<ControlFeedPage />} />
        <Route path="news" element={<ControlNewsPage />} />
        <Route path="system" element={<ControlSystemPage />} />
      </Route>

      <Route path="/" element={<Navigate to="/control" replace />} />

      {/* Legacy Tactical Desk → Control Panel (no dual UI) */}
      <Route path="/brokers" element={<Navigate to="/control/clients" replace />} />
      <Route path="/clients" element={<Navigate to="/control/clients" replace />} />
      <Route path="/trading" element={<Navigate to="/control/clients" replace />} />
      <Route path="/trades" element={<Navigate to="/control/clients" replace />} />
      <Route path="/brain" element={<Navigate to="/control/ai" replace />} />
      <Route path="/robot" element={<Navigate to="/control/ai" replace />} />
      <Route path="/feeds" element={<Navigate to="/control/feed" replace />} />
      <Route path="/live" element={<Navigate to="/control" replace />} />
      <Route path="/overview" element={<Navigate to="/control" replace />} />
      <Route path="/markets" element={<Navigate to="/control/feed" replace />} />
      <Route path="/market" element={<Navigate to="/control/feed" replace />} />
      <Route path="/market-legacy" element={<Navigate to="/control/feed" replace />} />
      <Route path="/structure" element={<Navigate to="/control/ai" replace />} />
      <Route path="/patterns" element={<Navigate to="/control/ai" replace />} />
      <Route path="/scenarios" element={<Navigate to="/control/ai" replace />} />
      <Route path="/predictions" element={<Navigate to="/control/ai" replace />} />
      <Route path="/decisions" element={<Navigate to="/control/ai" replace />} />
      <Route path="/positions" element={<Navigate to="/control" replace />} />
      <Route path="/risk" element={<Navigate to="/control/clients" replace />} />
      <Route path="/execution" element={<Navigate to="/control" replace />} />
      <Route path="/learning" element={<Navigate to="/control/ai" replace />} />
      <Route path="/models" element={<Navigate to="/control/ai" replace />} />
      <Route path="/diagnostics" element={<Navigate to="/control/errors" replace />} />
      <Route path="/system" element={<Navigate to="/control/system" replace />} />
      <Route path="/logs" element={<Navigate to="/control/errors" replace />} />
      <Route path="/settings" element={<Navigate to="/control/system" replace />} />

      <Route path="*" element={<Navigate to="/control" replace />} />
    </Routes>
  );
}
