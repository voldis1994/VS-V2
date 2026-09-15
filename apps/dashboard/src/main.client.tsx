import React from 'react';
import ReactDOM from 'react-dom/client';
import { BrowserRouter } from 'react-router-dom';
import { ClientPanelPage } from './pages/ClientPanelPage';
import './styles/global.css';

/** Public client website entry (deploy dist-client). */
ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <BrowserRouter>
      <ClientPanelPage />
    </BrowserRouter>
  </React.StrictMode>
);
