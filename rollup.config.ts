import typescript from '@rollup/plugin-typescript';
import { defineConfig } from 'rollup';
import { minify as esbuildMinifyPlugin } from 'rollup-plugin-esbuild';

export default defineConfig([
	{
		input: './src/index.ts',
		output: {
			dir: './dist',
			externalLiveBindings: false,
			format: 'es',
			freeze: false,
			preserveModules: false,
			sourcemap: true
		},
		plugins: [
			esbuildMinifyPlugin({
				minify: true,
				minifySyntax: true
			}),
			typescript({
				tsconfig: './tsconfig.build.json'
			})
		]
	}
]);
