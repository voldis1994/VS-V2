import { FastifyInstance } from 'fastify';

export async function registerPredictionsRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/predictions', async () => ({
    items: [],
    timestamp: new Date().toISOString(),
  }));
}
