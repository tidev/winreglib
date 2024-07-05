import { EventEmitter } from 'node:events';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import snooplogg, { type Logger } from 'snooplogg';

const { default: nodeGypBuild } = await import(
	'node-gyp-build/node-gyp-build.js'
);
const binding = nodeGypBuild(dirname(fileURLToPath(import.meta.url)));

const logger = snooplogg('winreglib');

/**
 * A handle for watching a registry key for changes.
 */
export class WinRegLibWatchHandle extends EventEmitter {
	key: string;

	constructor(key: string) {
		super();
		this.key = key;
		this.emit = this.emit.bind(this);
		binding.watch(key, this.emit);
	}

	stop() {
		binding.unwatch(this.key, this.emit);
	}
}

export type RegistryKey = {
	root: string;
	path: string;
	subkeys: string[];
	values: Record<string, unknown>;
};

/**
 * The `winreglib` API and debug log emitter.
 *
 * @type {EventEmitter}
 * @emits {log} Emits a debug log message.
 */
class WinRegLib extends EventEmitter {
	nss: Record<string, Logger> = {};

	constructor() {
		super();

		binding.init((ns: string, msg: string) => {
			this.emit('log', msg);
			if (ns) {
				if (!this.nss[ns]) {
					this.nss[ns] = logger(ns);
				}
				this.nss[ns].log(msg);
			} else {
				logger.log(msg);
			}
		});
	}

	/**
	 * Gets the value for a specific key value.
	 *
	 * @param {String} key - The key.
	 * @param {String} value - The name of the value to get.
	 * @returns {*} The value reflects the data type from the registry.
	 */
	get(key: string, value: string): unknown {
		if (!key || typeof key !== 'string') {
			throw new TypeError('Expected key to be a non-empty string');
		}

		if (!value || typeof value !== 'string') {
			throw new TypeError('Expected value name to be a non-empty string');
		}

		return binding.get(key, value);
	}

	/**
	 * Lists all subkeys and values for a specific key.
	 *
	 * @param {String} key - The key to list.
	 * @returns {Object} Contains the resolved `root`, `path`, `subkeys`, and `values`.
	 */
	list(key: string): RegistryKey {
		if (!key || typeof key !== 'string') {
			throw new TypeError('Expected key to be a non-empty string');
		}

		return binding.list(key);
	}

	/**
	 * Watches a key for changes to subkeys and values.
	 *
	 * @param {String} key - The key to watch.
	 * @returns {EventEmitter} The handle to wire up listeners and stop watching.
	 * @emits {change} Emits an event object containing the `key` that changed.
	 */
	watch(key: string): WinRegLibWatchHandle {
		if (!key || typeof key !== 'string') {
			throw new TypeError('Expected key to be a non-empty string');
		}

		return new WinRegLibWatchHandle(key);
	}
}

export default new WinRegLib();
