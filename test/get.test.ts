import fs from 'node:fs';
import { describe, expect, it } from 'vitest';
import winreglib from '../src/index.js';
import { spawnSync } from 'node:child_process';
import snooplogg from 'snooplogg';
import { randomBytes } from 'node:crypto';

const { log } = snooplogg('test:winreglib');

const reg = (...args) => {
	log(`Executing: reg ${args.join(' ')}`);
	spawnSync('reg', args, { stdio: 'ignore' });
};

process.on('beforeExit', () => {
	reg('delete', 'HKCU\\Software\\winreglib', '/f');
});

describe('get()', () => {
	it('should error if key is not specified', () => {
		expect(() => {
			winreglib.get(undefined as any, undefined as any);
		}).toThrowError(new TypeError('Expected key to be a non-empty string'));
	});

	it('should error if value name is not specified', () => {
		expect(() => {
			winreglib.get('HKLM\\software', undefined as any);
		}).toThrowError(
			new TypeError('Expected value name to be a non-empty string')
		);
	});

	it('should error if key does not contain a subkey', () => {
		const err: Error & { code?: string } = new Error(
			'Expected key to contain both a root and subkey'
		);
		err.code = 'ERR_NO_SUBKEY';
		expect(() => {
			winreglib.get('foo', 'bar');
		}).toThrowError(err);
	});

	it('should error if key is not valid', () => {
		const err: Error & { code?: string } = new Error(
			'Invalid registry root key "foo"'
		);
		err.code = 'ERR_WINREG_INVALID_ROOT';
		expect(() => {
			winreglib.get('foo\\bar', 'baz');
		}).toThrowError(err);
	});

	it('should error if key is not found', () => {
		const err: Error & { code?: string } = new Error(
			'Registry key or value not found'
		);
		err.code = 'ERR_WINREG_NOT_FOUND';
		expect(() => {
			winreglib.get('HKLM\\foo', 'bar');
		}).toThrowError(err);
	});

	it('should error if value is not found', () => {
		const err: Error & { code?: string } = new Error(
			'Registry key or value not found'
		);
		err.code = 'ERR_WINREG_NOT_FOUND';
		expect(() => {
			winreglib.get('HKLM\\SOFTWARE', 'bar');
		}).toThrowError(err);
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
		const key = `HKCU\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
		try {
			reg('add', key);
			reg('add', key, '/v', 'foo', '/t', 'REG_NONE');

			const value = winreglib.get(key, 'foo');
			expect(value).toBeNull();
		} finally {
			reg('delete', key, '/f');
		}
	});
});
