import { FastifyInstance } from 'fastify';

export async function registerModelsRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/models', async () => ({
    items: [],
    timestamp: new Date().toISOString(),
  }));
}
