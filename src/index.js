const binding = require('node-gyp-build')(`${__dirname}/..`);
const { EventEmitter } = require('events');

exports.get = function get(key, value) {
	if (!key || typeof key !== 'string') {
		throw new TypeError('Expected key to be a non-empty string');
	}

	if (!value || typeof value !== 'string') {
		throw new TypeError('Expected value name to be a non-empty string');
	}

	return binding.get(key, value);
};

exports.list = function list(key) {
	if (!key || typeof key !== 'string') {
		throw new TypeError('Expected key to be a non-empty string');
	}

	return binding.list(key);
};

exports.watch = function watch(key) {
	if (!key || typeof key !== 'string') {
		throw new TypeError('Expected key to be a non-empty string');
	}

	const handle = new EventEmitter();
	handle.id = binding.watch(key, result => handle.emit('change', result));
	console.log('watch(): watch id =', handle.id);
	handle.stop = function stop() {
		binding.unwatch(handle.id);
	};

	return handle;
};
