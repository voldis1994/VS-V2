
import { StreamHub } from './websocketServer.js';

export const marketStream = new StreamHub();

export function registerMarketStream(app: { get: Function }) {
  app.get('/ws/market', { websocket: true }, (socket: import('ws').WebSocket) => {
    marketStream.add(socket);
    socket.on('close', () => marketStream.remove(socket));
  });
}
