import fs from 'node:fs';
import { describe, expect, it } from 'vitest';
import * as winreglib from '../src/index.js';
const { spawnSync } = require('node:child_process');
import snooplogg from 'snooplogg';

const { log } = snooplogg('test:winreglib');

const reg = (...args) => {
	log(`Executing: reg ${args.join(' ')}`);
	spawnSync('reg', args, { stdio: 'ignore' });
};

describe('get()', () => {
	it('should error if key is not specified', () => {
		try {
			winreglib.get();
		} catch (e) {
			expect(e.message).to.equal('Expected key to be a non-empty string');
			return;
		}
		throw new Error('Expected error');
	});

	// 	it('should error if value name is not specified', () => {
	// 		try {
	// 			winreglib.get('HKLM\\software');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Expected value name to be a non-empty string');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if key does not contain a subkey', () => {
	// 		try {
	// 			winreglib.get('foo', 'bar');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Expected key to contain both a root and subkey');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if key is not valid', () => {
	// 		try {
	// 			winreglib.get('foo\\bar', 'baz');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Invalid registry root key "foo"');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if key is not found', () => {
	// 		try {
	// 			winreglib.get('HKLM\\foo', 'bar');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Registry key or value not found');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if value is not found', () => {
	// 		try {
	// 			winreglib.get('HKLM\\SOFTWARE', 'bar');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Registry key or value not found');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should get a string value', () => {
	// 		const value = winreglib.get('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion', 'ProgramFilesDir');
	// 		expect(value).to.be.a('string');
	// 		expect(value).to.not.equal('');
	// 		expect(fs.existsSync(value)).to.be.true;
	// 	});

	// 	it('should get an expanded string value', () => {
	// 		const value = winreglib.get('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion', 'ProgramFilesPath');
	// 		expect(value).to.be.a('string');
	// 		expect(value).to.not.equal('');
	// 		expect(fs.existsSync(value)).to.be.true;
	// 	});

	// 	it('should get multi-string value', () => {
	// 		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'SystemBiosVersion');
	// 		expect(value).to.be.an('array');
	// 		expect(value).to.have.lengthOf.above(0);
	// 		for (const v of value) {
	// 			expect(v).to.be.a('string');
	// 			expect(v).to.not.equal('');
	// 		}
	// 	});

	// 	it('should get an 32-bit integer value', () => {
	// 		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'Capabilities');
	// 		expect(value).to.be.a('number');
	// 		expect(value).to.be.at.least(0);
	// 	});

	// 	it('should get an 64-bit integer value', () => {
	// 		const value = winreglib.get('HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Diagnostics\\DiagTrack', 'TriggerCount');
	// 		expect(value).to.be.a('number');
	// 		expect(value).to.be.at.least(0);
	// 	});

	// 	it('should get a binary value', () => {
	// 		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'Component Information');
	// 		expect(value).to.be.an.instanceof(Buffer);
	// 		expect(value.length).to.be.above(0);
	// 	});

	// 	it('should get a full resource descriptor value', () => {
	// 		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'Configuration Data');
	// 		expect(value).to.be.an.instanceof(Buffer);
	// 		expect(value.length).to.be.above(0);
	// 	});

	// 	it('should get a resource list value', () => {
	// 		const value = winreglib.get('HKLM\\HARDWARE\\RESOURCEMAP\\System Resources\\Loader Reserved', '.Raw');
	// 		expect(value).to.be.an.instanceof(Buffer);
	// 		expect(value.length).to.be.above(0);
	// 	});

	// 	it('should get none value', function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		try {
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 			reg('add', 'HKCU\\Software\\winreglib');
	// 			reg('add', 'HKCU\\Software\\winreglib', '/v', 'foo', '/t', 'REG_NONE');

	// 			const value = winreglib.get('HKCU\\Software\\winreglib', 'foo');
	// 			expect(value).to.be.null;
	// 		} finally {
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});
	// });

	// describe('list()', () => {
	// 	it('should error if key is not specified', () => {
	// 		try {
	// 			winreglib.list();
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Expected key to be a non-empty string');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if key does not contain a subkey', () => {
	// 		try {
	// 			winreglib.list('foo');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Expected key to contain both a root and subkey');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if key is not valid', () => {
	// 		try {
	// 			winreglib.list('foo\\bar');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Invalid registry root key "foo"');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if key is not found', () => {
	// 		try {
	// 			winreglib.list('HKLM\\foo');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Registry key or value not found');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should retrieve subkeys and values', () => {
	// 		const result = winreglib.list('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion');

	// 		expect(result).to.be.an('object');
	// 		expect(result).to.have.keys('resolvedRoot', 'key', 'subkeys', 'values');

	// 		expect(result.resolvedRoot).to.equal('HKEY_LOCAL_MACHINE');
	// 		expect(result.key).to.equal('HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion');

	// 		expect(result.subkeys).to.be.an('array');
	// 		for (const key of result.subkeys) {
	// 			expect(key).to.be.a('string');
	// 			expect(key).to.not.equal('');
	// 		}

	// 		expect(result.values).to.be.an('array');
	// 		for (const value of result.values) {
	// 			expect(value).to.be.a('string');
	// 			expect(value).to.not.equal('');
	// 		}
	// 	});
	// });

	// describe('watch()', () => {
	// 	it('should error if key is not specified', () => {
	// 		try {
	// 			winreglib.watch();
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Expected key to be a non-empty string');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if key does not contain a subkey', () => {
	// 		try {
	// 			winreglib.watch('HKLM');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Expected key to contain both a root and subkey');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should error if root key is not valid', () => {
	// 		try {
	// 			winreglib.watch('foo\\bar');
	// 		} catch (e) {
	// 			expect(e.message).to.equal('Invalid registry root key "foo"');
	// 			return;
	// 		}
	// 		throw new Error('Expected error');
	// 	});

	// 	it('should watch existing key for new subkey', async function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		reg('add', 'HKCU\\Software\\winreglib');

	// 		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib');

	// 		try {
	// 			let counter = 0;

	// 			await new Promise((resolve, reject) => {
	// 				handle.on('change', evt => {
	// 					// console.log('CHANGE!', evt);
	// 					try {
	// 						expect(evt).to.be.an('object');
	// 						switch (counter++) {
	// 							case 0:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f');
	// 								break;

	// 							case 1:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								resolve();
	// 								break;
	// 						}
	// 					} catch (e) {
	// 						reject(e);
	// 					}
	// 				});

	// 				setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo'), 500);
	// 			});
	// 			handle.stop();
	// 		} finally {
	// 			// also test stop() being called twice
	// 			handle.stop();
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});

	// 	it('should watch existing key for value change', async function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		reg('add', 'HKCU\\Software\\winreglib');

	// 		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib');

	// 		try {
	// 			let counter = 0;

	// 			await new Promise((resolve, reject) => {
	// 				handle.on('change', evt => {
	// 					try {
	// 						expect(evt).to.be.an('object');
	// 						switch (counter++) {
	// 							case 0:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								reg('delete', 'HKCU\\Software\\winreglib', '/f', '/v', 'foo');
	// 								break;

	// 							case 1:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								resolve();
	// 								break;
	// 						}
	// 					} catch (e) {
	// 						reject(e);
	// 					}
	// 				});

	// 				setTimeout(() => reg('add', 'HKCU\\Software\\winreglib', '/v', 'foo', '/t', 'REG_SZ', '/d', 'bar'), 500);
	// 			});
	// 		} finally {
	// 			handle.stop();
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});

	// 	it('should watch a key that does not exist', async function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		reg('add', 'HKCU\\Software\\winreglib');

	// 		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo');

	// 		try {
	// 			let counter = 0;

	// 			await new Promise((resolve, reject) => {
	// 				handle.on('change', evt => {
	// 					// console.log('CHANGE!!', counter, evt);
	// 					try {
	// 						expect(evt).to.be.an('object');
	// 						switch (counter++) {
	// 							case 0:
	// 								expect(evt).to.deep.equal({
	// 									type: 'add',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								reg('add', 'HKCU\\Software\\winreglib\\foo', '/v', 'bar', '/t', 'REG_SZ', '/d', 'baz');
	// 								break;

	// 							case 1:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});

	// 								reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f', '/v', 'bar');
	// 								break;

	// 							case 2:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});

	// 								setTimeout(() => reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f'), 500);
	// 								break;

	// 							case 3:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo'), 500);
	// 								break;

	// 							case 4:
	// 								expect(evt).to.deep.equal({
	// 									type: 'add',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								resolve();
	// 								break;
	// 						}
	// 					} catch (e) {
	// 						reject(e);
	// 					}
	// 				});

	// 				setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo'), 500);
	// 			});
	// 		} finally {
	// 			handle.stop();
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});

	// 	it('should watch a key whose parent does not exist', async function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		reg('add', 'HKCU\\Software\\winreglib');

	// 		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo\\bar\\baz');

	// 		try {
	// 			let counter = 0;

	// 			await new Promise((resolve, reject) => {
	// 				handle.on('change', evt => {
	// 					// console.log('CHANGE!!', counter, evt);
	// 					try {
	// 						expect(evt).to.be.an('object');
	// 						switch (counter++) {
	// 							case 0:
	// 								expect(evt).to.deep.equal({
	// 									type: 'add',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});
	// 								reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz', '/v', 'val1', '/t', 'REG_SZ', '/d', 'hello1');
	// 								break;

	// 							case 1:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});

	// 								// this next line should not trigger anything
	// 								reg('add', 'HKCU\\Software\\winreglib\\foo', '/v', 'val2', '/t', 'REG_SZ', '/d', 'hello2');

	// 								setTimeout(() => {
	// 									reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz', '/v', 'val3', '/t', 'REG_SZ', '/d', 'hello3');
	// 								}, 1000);
	// 								break;

	// 							case 2:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});

	// 								setTimeout(() => {
	// 									reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 								}, 1000);
	// 								break;

	// 							case 3:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});
	// 								resolve();
	// 								break;
	// 						}
	// 					} catch (e) {
	// 						reject(e);
	// 					}
	// 				});

	// 				setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo'), 500);
	// 				setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz\\wiz'), 2000);
	// 			});
	// 		} finally {
	// 			handle.stop();
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});

	// 	it('should watch a key that gets deleted', async function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		reg('add', 'HKCU\\Software\\winreglib\\foo');

	// 		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo');

	// 		try {
	// 			let counter = 0;

	// 			await new Promise((resolve, reject) => {
	// 				handle.on('change', evt => {
	// 					// console.log('CHANGE!', evt);
	// 					try {
	// 						expect(evt).to.be.an('object');
	// 						switch (counter++) {
	// 							case 0:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								resolve();
	// 								break;
	// 						}
	// 					} catch (e) {
	// 						reject(e);
	// 					}
	// 				});

	// 				setTimeout(() => reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f'), 500);
	// 			});
	// 			handle.stop();
	// 		} finally {
	// 			// also test stop() being called twice
	// 			handle.stop();
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});

	// 	it('should watch a key whose parent gets deleted', async function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz');

	// 		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo\\bar\\baz');

	// 		try {
	// 			let counter = 0;

	// 			await new Promise((resolve, reject) => {
	// 				handle.on('change', evt => {
	// 					// console.log('CHANGE!', evt);
	// 					try {
	// 						expect(evt).to.be.an('object');
	// 						switch (counter++) {
	// 							case 0:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});
	// 								resolve();
	// 								break;
	// 						}
	// 					} catch (e) {
	// 						reject(e);
	// 					}
	// 				});

	// 				setTimeout(() => reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f'), 500);
	// 			});
	// 			handle.stop();
	// 		} finally {
	// 			// also test stop() being called twice
	// 			handle.stop();
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});

	// 	it('should survive the gauntlet', async function () {
	// 		this.timeout(15000);
	// 		this.slow(14000);

	// 		reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		reg('add', 'HKCU\\Software\\winreglib');

	// 		const winreglibHandle = winreglib.watch('HKCU\\SOFTWARE\\winreglib');
	// 		const fooHandle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo');
	// 		const barHandle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo\\bar');
	// 		const bazHandle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo\\bar\\baz');

	// 		try {
	// 			let counter = 0;

	// 			await new Promise((resolve, reject) => {
	// 				const fn = evt => {
	// 					// console.log('CHANGE!', evt);
	// 					try {
	// 						expect(evt).to.be.an('object');
	// 						switch (counter++) {
	// 							case 0:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								break;

	// 							case 1:
	// 								expect(evt).to.deep.equal({
	// 									type: 'add',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								break;

	// 							case 2:
	// 								expect(evt).to.deep.equal({
	// 									type: 'add',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
	// 								});
	// 								reg('delete', 'HKCU\\Software\\winreglib\\foo\\bar', '/f');
	// 								break;

	// 							case 3:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								break;

	// 							case 4:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
	// 								});
	// 								reg('add', 'HKCU\\Software\\winreglib', '/v', 'foo', '/t', 'REG_SZ', '/d', 'bar');
	// 								break;

	// 							case 5:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\wiz');
	// 								break;

	// 							case 6:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								break;

	// 							case 7:
	// 								expect(evt).to.deep.equal({
	// 									type: 'add',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
	// 								});
	// 								reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz');
	// 								break;

	// 							case 8:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
	// 								});
	// 								break;

	// 							case 9:
	// 								expect(evt).to.deep.equal({
	// 									type: 'add',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});
	// 								reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz', '/v', 'foo', '/t', 'REG_SZ', '/d', 'bar');
	// 								break;

	// 							case 10:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});
	// 								reg('delete', 'HKCU\\Software\\winreglib', '/f', '/v', 'foo');
	// 								break;

	// 							case 11:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f');
	// 								break;

	// 							case 12:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
	// 								});
	// 								break;

	// 							case 13:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
	// 								});
	// 								break;

	// 							case 14:
	// 								expect(evt).to.deep.equal({
	// 									type: 'delete',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
	// 								});
	// 								break;

	// 							case 15:
	// 								expect(evt).to.deep.equal({
	// 									type: 'change',
	// 									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
	// 								});
	// 								resolve();
	// 								break;
	// 						}
	// 					} catch (e) {
	// 						reject(e);
	// 					}
	// 				};

	// 				winreglibHandle.on('change', fn);
	// 				fooHandle.on('change', fn);
	// 				barHandle.on('change', fn);
	// 				bazHandle.on('change', fn);

	// 				setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo\\bar'), 500);
	// 			});
	// 		} finally {
	// 			winreglibHandle.stop();
	// 			fooHandle.stop();
	// 			barHandle.stop();
	// 			bazHandle.stop();
	// 			reg('delete', 'HKCU\\Software\\winreglib', '/f');
	// 		}
	// 	});
});
