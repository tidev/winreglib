import fs from 'node:fs';
import { describe, expect, it } from 'vitest';
import winreglib from '../src/index.js';
const { spawnSync } = require('node:child_process');
import snooplogg from 'snooplogg';

const { log } = snooplogg('test:winreglib');

const reg = (...args) => {
	log(`Executing: reg ${args.join(' ')}`);
	spawnSync('reg', args, { stdio: 'ignore' });
};

describe('get()', () => {
	it('should error if key is not specified', () => {
		expect(() => {
			// biome-ignore lint/suspicious/noExplicitAny: need to test invalid input
			winreglib.get(undefined as any, undefined as any);
		}).toThrowError(new TypeError('Expected key to be a non-empty string'));
	});

	it('should error if value name is not specified', () => {
		expect(() => {
			// biome-ignore lint/suspicious/noExplicitAny: need to test invalid input
			winreglib.get('HKLM\\software', undefined as any);
		}).toThrowError(
			new TypeError('Expected value name to be a non-empty string')
		);
	});

	it('should error if key does not contain a subkey', () => {
		expect(() => {
			winreglib.get('foo', 'bar');
		}).toThrowError(
			new Error('Expected key to contain both a root and subkey')
		);
	});

	it('should error if key is not valid', () => {
		expect(() => {
			winreglib.get('foo\\bar', 'baz');
		}).toThrowError(new Error('Invalid registry root key "foo"'));
	});

	it('should error if key is not found', () => {
		expect(() => {
			winreglib.get('HKLM\\foo', 'bar');
		}).toThrowError(new Error('Registry key or value not found'));
	});

	it('should error if value is not found', () => {
		expect(() => {
			winreglib.get('HKLM\\SOFTWARE', 'bar');
		}).toThrowError(new Error('Registry key or value not found'));
	});

	it('should get a string value', () => {
		const value = winreglib.get(
			'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion',
			'ProgramFilesDir'
		) as string;
		expect(value).toBeTypeOf('string');
		expect(value).not.toBe('');
		expect(fs.existsSync(value)).toBe(true);
	});

	it('should get an expanded string value', () => {
		const value = winreglib.get(
			'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion',
			'ProgramFilesPath'
		) as string;
		expect(value).toBeTypeOf('string');
		expect(value).not.toBe('');
		expect(fs.existsSync(value)).toBe(true);
	});

	it('should get multi-string value', () => {
		const value = winreglib.get(
			'HKLM\\HARDWARE\\DESCRIPTION\\System',
			'SystemBiosVersion'
		) as string[];
		expect(value).toBeInstanceOf(Array);
		expect(value.length).toBeGreaterThan(0);
		for (const v of value) {
			expect(v).toBeTypeOf('string');
			expect(v).not.toBe('');
		}
	});

	it('should get an 32-bit integer value', () => {
		const value = winreglib.get(
			'HKLM\\HARDWARE\\DESCRIPTION\\System',
			'Capabilities'
		) as number;
		expect(value).toBeTypeOf('number');
		expect(value).toBeGreaterThanOrEqual(0);
	});

	it('should get an 64-bit integer value', async () => {
		const value = winreglib.get(
			'HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\StateRepositoryStatus',
			'MaintenanceLastPerformed'
		) as number;
		expect(value).toBeTypeOf('number');
		expect(value).toBeGreaterThan(0);
	});

	it('should get a binary value', () => {
		const value = winreglib.get(
			'HKLM\\HARDWARE\\DESCRIPTION\\System',
			'Component Information'
		) as Buffer;
		expect(value).toBeInstanceOf(Buffer);
		expect(value.length).toBeGreaterThan(0);
	});

	it('should get a full resource descriptor value', () => {
		const value = winreglib.get(
			'HKLM\\HARDWARE\\DESCRIPTION\\System',
			'Configuration Data'
		) as Buffer;
		expect(value).toBeInstanceOf(Buffer);
		expect(value.length).toBeGreaterThan(0);
	});

	it('should get a resource list value', () => {
		const value = winreglib.get(
			'HKLM\\HARDWARE\\RESOURCEMAP\\System Resources\\Loader Reserved',
			'.Raw'
		) as Buffer;
		expect(value).toBeInstanceOf(Buffer);
		expect(value.length).toBeGreaterThan(0);
	});

	it('should get none value', { timeout: 15000 }, () => {
		try {
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
			reg('add', 'HKCU\\Software\\winreglib');
			reg('add', 'HKCU\\Software\\winreglib', '/v', 'foo', '/t', 'REG_NONE');

			const value = winreglib.get('HKCU\\Software\\winreglib', 'foo');
			expect(value).toBeNull();
		} finally {
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
		}
	});
});
