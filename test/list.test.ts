import assert from 'node:assert';
import { describe, expect, it } from 'vitest';
import winreglib from '../src/index.js';

describe('list()', () => {
	it('should error if key is not specified', () => {
		expect(() => {
			winreglib.list(undefined as any);
		}).toThrowError(new TypeError('Expected key to be a non-empty string'));
	});

	it('should error if key does not contain a subkey', () => {
		const err: Error & { code?: string } = new Error(
			'Expected key to contain both a root and subkey'
		);
		err.code = 'ERR_NO_SUBKEY';
		expect(() => {
			winreglib.list('foo');
		}).toThrowError(err);
	});

	it('should error if key is not valid', () => {
		const err: Error & { code?: string } = new Error(
			'Invalid registry root key "foo"'
		);
		err.code = 'ERR_WINREG_INVALID_ROOT';
		expect(() => {
			winreglib.list('foo\\bar');
		}).toThrowError(err);
	});

	it('should error if key is not found', () => {
		const err: Error & { code?: string } = new Error(
			'Registry key or value not found'
		);
		err.code = 'ERR_WINREG_NOT_FOUND';
		expect(() => {
			winreglib.list('HKLM\\foo');
		}).toThrowError(err);
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
