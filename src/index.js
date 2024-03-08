const binding = require('node-gyp-build')(`${__dirname}/..`);
const { EventEmitter } = require('events');
const logger = require('snooplogg').default('winreglib');
const nss = {};

/**
 * The `winreglib` API and debug log emitter.
 *
 * @type {EventEmitter}
 * @emits {log} Emits a debug log message.
 */
const api = module.exports = new EventEmitter();

binding.init((ns, msg) => {
	api.emit('log', msg);
	if (ns) {
		(nss[ns] || (nss[ns] = logger(ns))).log(msg);
	} else {
		logger.log(msg);
	}
});

/**
 * Gets the value for a specific key value.
 *
 * @param {String} key - The key.
 * @param {String} value - The name of the value to get.
 * @returns {*} The value reflects the data type from the registry.
 */
api.get = function get(key, value) {
	if (!key || typeof key !== 'string') {
		throw new TypeError('Expected key to be a non-empty string');
	}

	if (typeof value !== 'string') {
		throw new TypeError('Expected value name to be a non-empty string');
	}

	return binding.get(key, value);
};

/**
 * Lists all subkeys and values for a specific key.
 *
 * @param {String} key - The key to list.
 * @returns {Object} Contains the resolved `root`, `path`, `subkeys`, and `values`.
 */
api.list = function list(key) {
	if (!key || typeof key !== 'string') {
		throw new TypeError('Expected key to be a non-empty string');
	}

	return binding.list(key);
};

/**
 * Watches a key for changes to subkeys and values.
 *
 * @param {String} key - The key to watch.
 * @returns {EventEmitter} The handle to wire up listeners and stop watching.
 * @emits {change} Emits an event object containing the `key` that changed.
 */
api.watch = function watch(key) {
	if (!key || typeof key !== 'string') {
		throw new TypeError('Expected key to be a non-empty string');
	}

	const handle = new EventEmitter();
	const emit = handle.emit.bind(handle);

	binding.watch(key, emit);
	handle.stop = function stop() {
		binding.unwatch(key, emit);
	};

	return handle;
};
