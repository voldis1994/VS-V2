
import { StreamHub } from './websocketServer.js';

export const brainStream = new StreamHub();

export function registerBrainStream(app: { get: Function }) {
  app.get('/ws/brain', { websocket: true }, (socket: import('ws').WebSocket) => {
    brainStream.add(socket);
    socket.on('close', () => brainStream.remove(socket));
  });
}
