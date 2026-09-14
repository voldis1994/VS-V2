
import { WebSocket } from 'ws';

export type WsClient = WebSocket;

export class StreamHub {
  private clients = new Set<WsClient>();

  add(client: WsClient) { this.clients.add(client); }
  remove(client: WsClient) { this.clients.delete(client); }

  broadcast(payload: unknown) {
    const msg = JSON.stringify(payload);
    for (const c of this.clients) {
      if (c.readyState === 1) c.send(msg);
    }
  }
}
