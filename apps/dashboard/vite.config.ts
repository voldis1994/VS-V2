import path from 'node:path';
import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';
import { allowTunnelHosts } from './vite.allow-tunnels';

export default defineConfig(({ mode }) => {
  // Monorepo root .env (Install.bat / V2.bat) — not only apps/dashboard/.env
  const repoRoot = path.resolve(__dirname, '../..');
  const env = loadEnv(mode, repoRoot, '');
  const adminToken =
    process.env.API_ADMIN_TOKEN ||
    env.API_ADMIN_TOKEN ||
    process.env.VITE_API_ADMIN_TOKEN ||
    '';
  // Windows: prefer 127.0.0.1 — `localhost` can resolve to ::1 and miss the API
  const apiTarget = (
    process.env.CONTROL_API_URL ||
    env.CONTROL_API_URL ||
    'http://127.0.0.1:3000'
  ).replace(/\/$/, '');

  const injectAdmin = (proxy: { on: Function }) => {
    proxy.on('proxyReq', (proxyReq: { setHeader: (k: string, v: string) => void }) => {
      if (adminToken && adminToken !== 'CHANGE_ME_ADMIN_TOKEN') {
        proxyReq.setHeader('x-admin-token', adminToken);
      }
    });
  };

  return {
    plugins: [allowTunnelHosts(), react()],
    // Always same-origin /api via Vite proxy. A .env VITE_API_URL=http://localhost:3000
    // breaks Windows (localhost -> ::1) and bypasses admin-token injection.
    define: {
      'import.meta.env.VITE_API_URL': JSON.stringify(''),
    },
    server: {
      port: 5173,
      host: '127.0.0.1',
      allowedHosts: true,
      proxy: {
        '/api': {
          target: apiTarget,
          timeout: 600_000,
          proxyTimeout: 600_000,
          configure: injectAdmin,
        },
        '/health': {
          target: apiTarget,
          configure: injectAdmin,
        },
        '/ws': {
          target: apiTarget.replace(/^http/, 'ws'),
          ws: true,
          configure: injectAdmin,
        },
      },
    },
  };
});
