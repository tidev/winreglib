import { defineConfig } from 'vitest/config';

export default defineConfig({
	test: {
		coverage: {
			include: ['src/**/*.ts'],
			reporter: ['html', 'lcov', 'text']
		},
		environment: 'node',
		environmentOptions: {
			node: {
				env: {
					SNOOPLOGG: '*'
				}
			}
		},
		globals: false,
		include: ['test/**/*.test.ts'],
		poolOptions: {
 			forks: {
 				singleFork: true
 			}
 		},
		watch: false
	}
});
