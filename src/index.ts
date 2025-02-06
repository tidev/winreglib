import { EventEmitter } from 'node:events';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import nodeGypBuild from 'node-gyp-build';
import snooplogg, { type Logger } from 'snooplogg';

const cwd = dirname(dirname(fileURLToPath(import.meta.url)));
const binding = nodeGypBuild(cwd);

const logger = snooplogg('winreglib');

/**
 * A handle for watching a registry key for changes.
 */
export class WinRegLibWatchHandle extends EventEmitter {
	key: string;
	stop: () => void;

	constructor(key: string) {
		super();
		this.key = key;

		const emitter = this.emit.bind(this);
		binding.watch(key, emitter);

		this.stop = () => binding.unwatch(this.key, emitter);
	}
}

export type RegistryKey = {
	resolvedRoot: string;
	key: string;
	subkeys: string[];
	values: unknown[];
};

/**
 * Loads the `winreglib` native module and provides an API for interacting
 * with the Windows registry.
 */
export class WinRegLib extends EventEmitter {
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
	 * @param {String} valueName - The name of the value to get.
	 * @returns {*} The value reflects the data type from the registry.
	 */
	get(key: string, valueName: string): unknown {
		if (!key || typeof key !== 'string') {
			throw new TypeError('Expected key to be a non-empty string');
		}

		if (!valueName || typeof valueName !== 'string') {
			throw new TypeError('Expected value name to be a non-empty string');
		}

		return binding.get(key, valueName);
	}

	/**
	 * Lists all subkeys and values for a specific key.
	 *
	 * @param {String} key - The key to list.
	 * @returns {RegistryKey} Contains the resolved `resolvedRoot`, `key`, `subkeys`, and `values`.
	 */
	list(key: string): RegistryKey | undefined {
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

/**
 * The `winreglib` API and debug log emitter.
 *
 * @type {EventEmitter}
 * @emits {log} Emits a debug log message.
 */
export default new WinRegLib();
