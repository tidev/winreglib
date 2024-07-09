import assert from 'node:assert';
import { describe, expect, it } from 'vitest';
import winreglib from '../src/index.js';

describe('list()', () => {
	it('should error if key is not specified', () => {
		expect(() => {
			// biome-ignore lint/suspicious/noExplicitAny: need to test invalid input
			winreglib.list(undefined as any);
		}).toThrowError(new Error('Expected key to be a non-empty string'));
	});

	it('should error if key does not contain a subkey', () => {
		expect(() => {
			winreglib.list('foo');
		}).toThrowError(
			new Error('Expected key to contain both a root and subkey')
		);
	});

	it('should error if key is not valid', () => {
		expect(() => {
			winreglib.list('foo\\bar');
		}).toThrowError(new Error('Invalid registry root key "foo"'));
	});

	it('should error if key is not found', () => {
		expect(() => {
			winreglib.list('HKLM\\foo');
		}).toThrowError(new Error('Registry key or value not found'));
	});

	it('should retrieve subkeys and values', () => {
		const result = winreglib.list(
			'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion'
		);
		expect(result).toBeTypeOf('object');
		assert(result);
		expect(result).toHaveProperty('resolvedRoot', 'HKEY_LOCAL_MACHINE');
		expect(result).toHaveProperty(
			'key',
			'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion'
		);
		expect(result).toHaveProperty('subkeys');
		expect(result.subkeys).toBeInstanceOf(Array);
		for (const key of result.subkeys) {
			expect(key).toBeTypeOf('string');
			expect(key).not.toBe('');
		}
		expect(result).toHaveProperty('values');
		expect(result.values).toBeInstanceOf(Array);
		for (const value of result.values) {
			expect(value).toBeTypeOf('string');
			expect(value).not.toBe('');
		}
	});
});
