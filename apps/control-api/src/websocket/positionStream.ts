
import { StreamHub } from './websocketServer.js';

export const positionStream = new StreamHub();

export function registerPositionStream(app: { get: Function }) {
  app.get('/ws/position', { websocket: true }, (socket: import('ws').WebSocket) => {
    positionStream.add(socket);
    socket.on('close', () => positionStream.remove(socket));
  });
}
