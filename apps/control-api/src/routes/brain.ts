import { FastifyInstance } from 'fastify';
import {
  getBrainEvent,
  getBrainFeedStatus,
  getLiveBrainSnapshot,
  listBrainEvents,
} from '../services/liveBrainFeed.js';

/**
 * Authoritative Brain surfaces for the LIVE terminal.
 * Data originates from market-core via POST /api/pipeline/brain-snapshot.
 */
export async function registerBrainRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/brain/live', async () => {
    const snapshot = getLiveBrainSnapshot();
    const status = getBrainFeedStatus();
    return {
      snapshot,
      status,
      awaiting_market_core: snapshot === null,
      note:
        snapshot === null
          ? 'No authoritative C++ Brain snapshot received yet. UI must not invent decisions.'
          : 'Authoritative market-core Brain snapshot.',
    };
  });

  app.get('/api/brain/status', async () => getBrainFeedStatus());

  app.get<{
    Querystring: { kind?: string; limit?: string; instrument_id?: string };
  }>('/api/brain/events', async (request) => {
    const kind = (request.query.kind || 'all') as
      | 'all'
      | 'snapshot'
      | 'decision'
      | 'prediction'
      | 'trade'
      | 'position'
      | 'market'
      | 'risk'
      | 'execution'
      | 'error';
    const limit = request.query.limit ? parseInt(request.query.limit, 10) : 100;
    const instrument_id = request.query.instrument_id
      ? parseInt(request.query.instrument_id, 10)
      : undefined;
    return {
      items: listBrainEvents({ kind, limit, instrument_id }),
    };
  });

  app.get<{ Params: { id: string } }>('/api/brain/events/:id', async (request, reply) => {
    const ev = getBrainEvent(request.params.id);
    if (!ev) {
      reply.code(404);
      return { error: 'not_found' };
    }
    return { event: ev };
  });

  /** Legacy alias — prefer /api/brain/live. Returns live instruments only. */
  app.get('/api/brain/snapshots', async () => {
    const snapshot = getLiveBrainSnapshot();
    return {
      items: snapshot?.instruments ?? [],
      status: getBrainFeedStatus(),
      source: snapshot?.source ?? null,
    };
  });
}
