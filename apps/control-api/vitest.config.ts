import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    env: { ALLOW_INSECURE_ADMIN: 'true' },
    include: ['src/**/*.test.ts'],
  },
});
