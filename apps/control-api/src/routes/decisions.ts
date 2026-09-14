import { FastifyInstance } from 'fastify';

export async function registerDecisionsRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/decisions', async () => ({
    items: [],
    timestamp: new Date().toISOString(),
  }));
}
