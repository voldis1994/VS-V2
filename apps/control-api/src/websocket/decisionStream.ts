
import { StreamHub } from './websocketServer.js';

export const decisionStream = new StreamHub();

export function registerDecisionStream(app: { get: Function }) {
  app.get('/ws/decision', { websocket: true }, (socket: import('ws').WebSocket) => {
    decisionStream.add(socket);
    socket.on('close', () => decisionStream.remove(socket));
  });
}
