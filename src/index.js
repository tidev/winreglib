const binding = require('node-gyp-build')(`${__dirname}/..`);

exports.get = function get(root, key, value) {
    if (!root || typeof root !== 'string') {
        throw new TypeError('Expected root key to be a non-empty string');
    }

    if (!key || typeof key !== 'string') {
        throw new TypeError('Expected key to be a non-empty string');
    }

    if (!value || typeof value !== 'string') {
        throw new TypeError('Expected value name to be a non-empty string');
    }

    return binding.get(root, key, value);
};

exports.list = function list(root, key) {
    if (!root || typeof root !== 'string') {
        throw new TypeError('Expected root key to be a non-empty string');
    }

    if (!key || typeof key !== 'string') {
        throw new TypeError('Expected key to be a non-empty string');
    }

    return binding.list(root, key);
};
