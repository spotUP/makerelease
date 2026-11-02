import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    include: ['test/unit/**/*.spec.ts'],
    environment: 'node',
    // Keep the machine usable while tests run: at most two workers.
    poolOptions: {
      threads: { minThreads: 1, maxThreads: 2 },
      forks: { minForks: 1, maxForks: 2 },
    },
  },
});
