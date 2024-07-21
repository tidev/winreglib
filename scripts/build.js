/**
 * This file can be removed once `node-gyp-build` has merged and release this
 * pull request: https://github.com/prebuild/node-gyp-build/pull/72
 */

if (process.platform !== 'win32') {
	console.log(`Skipping build on ${process.platform}`);
	process.exit(0);
}

import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import nodeGypBuild from 'node-gyp-build/node-gyp-build.js';

const cwd = dirname(dirname(fileURLToPath(import.meta.url)));

// try to load winreglib
try {
	nodeGypBuild(cwd);
	process.exit(0);
} catch {}

// binary not found, try to build it
const pkg = JSON.parse(
	readFileSync(join(cwd, 'node_modules', 'node-gyp', 'package.json'), 'utf8')
);
console.log(`Building winreglib (${process.platform}-${process.arch})`);
const { status } = spawnSync(
	process.execPath,
	[resolve(cwd, 'node_modules', 'node-gyp', pkg.bin), 'rebuild'],
	{ stdio: 'inherit' }
);
if (status) {
	console.error(`Failed to build winreglib (code: ${status})`);
	process.exit(status);
}

// built, try loading it again
try {
	nodeGypBuild(cwd);
} catch {
	process.exit(1);
}
