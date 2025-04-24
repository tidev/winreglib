import { describe, expect, it } from 'vitest';
import winreglib from '../src/index.js';
const { spawnSync } = require('node:child_process');
import snooplogg from 'snooplogg';

const { log } = snooplogg('test:winreglib');

const reg = (...args) => {
	log(`Executing: reg ${args.join(' ')}`);
	spawnSync('reg', args, { stdio: 'ignore' });
};

describe('watch()', () => {
	it('should error if key is not specified', () => {
		expect(() => {
			// biome-ignore lint/suspicious/noExplicitAny: need to test invalid input
			winreglib.watch(undefined as any);
		}).toThrowError(new TypeError('Expected key to be a non-empty string'));
	});

	it('should error if key does not contain a subkey', () => {
		const err: Error & { code?: string } = new Error(
			'Expected key to contain both a root and subkey'
		);
		err.code = 'ERR_NO_SUBKEY';
		expect(() => {
			winreglib.watch('HKLM');
		}).toThrowError(err);
	});

	it('should error if root key is not valid', () => {
		const err: Error & { code?: string } = new Error(
			'Invalid registry root key "foo"'
		);
		err.code = 'ERR_WINREG_INVALID_ROOT';
		expect(() => {
			winreglib.watch('foo\\bar');
		}).toThrowError(err);
	});

	it(
		'should watch existing key for new subkey',
		async () => {
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
			reg('add', 'HKCU\\Software\\winreglib');

			const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib');

			try {
				let counter = 0;
				await new Promise<void>((resolve, reject) => {
					handle.on('change', evt => {
						// log('CHANGE!', evt);
						try {
							expect(evt).toBeInstanceOf(Object);
							switch (counter++) {
								case 0:
									expect(evt).toMatchObject({
										type: 'change',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
									});
									reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f');
									break;
								case 1:
									expect(evt).toMatchObject({
										type: 'change',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
									});
									resolve();
									break;
							}
						} catch (e) {
							reject(e);
						}
					});
					setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo'), 250);
				});

				handle.stop();
			} finally {
				// also test stop() being called twice
				handle.stop();
				reg('delete', 'HKCU\\Software\\winreglib', '/f');
			}
		},
		15000
	);

	it(
		'should watch existing key for value change',
		async () => {
			reg('delete', 'HKCU\\Software\\winreglib2', '/f');
			reg('add', 'HKCU\\Software\\winreglib2');

			const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib2');

			try {
				let counter = 0;
				await new Promise<void>((resolve, reject) => {
					handle.on('change', evt => {
						// log('CHANGE!', evt);
						try {
							expect(evt).toBeTypeOf('object');
							switch (counter++) {
								case 0:
									expect(evt).toMatchObject({
										type: 'change',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib2'
									});
									reg(
										'delete',
										'HKCU\\Software\\winreglib2',
										'/f',
										'/v',
										'foo'
									);
									break;
								case 1:
									expect(evt).toMatchObject({
										type: 'change',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib2'
									});
									resolve();
									break;
							}
						} catch (e) {
							reject(e);
						}
					});
					setTimeout(
						() =>
							reg(
								'add',
								'HKCU\\Software\\winreglib2',
								'/v',
								'foo',
								'/t',
								'REG_SZ',
								'/d',
								'bar'
							),
						250
					);
				});
			} finally {
				handle.stop();
				reg('delete', 'HKCU\\Software\\winreglib2', '/f');
			}
		},
		15000
	);

	it('should watch a key that does not exist', async () => {
		reg('delete', 'HKCU\\Software\\winreglib', '/f');
		reg('add', 'HKCU\\Software\\winreglib');

		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo');

		try {
			let counter = 0;
			await new Promise<void>((resolve, reject) => {
				handle.on('change', evt => {
					// console.log('CHANGE!!', counter, evt);
					try {
						expect(evt).toBeTypeOf('object');
						switch (counter++) {
							case 0:
								expect(evt).toMatchObject({
									type: 'add',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								reg(
									'add',
									'HKCU\\Software\\winreglib\\foo',
									'/v',
									'bar',
									'/t',
									'REG_SZ',
									'/d',
									'baz'
								);
								break;
							case 1:
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								reg(
									'delete',
									'HKCU\\Software\\winreglib\\foo',
									'/f',
									'/v',
									'bar'
								);
								break;
							case 2:
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								setTimeout(
									() => reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f'),
									250
								);
								break;
							case 3:
								expect(evt).toMatchObject({
									type: 'delete',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								setTimeout(
									() => reg('add', 'HKCU\\Software\\winreglib\\foo'),
									250
								);
								break;
							case 4:
								expect(evt).toMatchObject({
									type: 'add',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								resolve();
								break;
						}
					} catch (e) {
						reject(e);
					}
				});
				setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo'), 250);
			});
		} finally {
			handle.stop();
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
		}
	}, 15000);

	it(
		'should watch a key whose parent does not exist',
		async () => {
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
			reg('add', 'HKCU\\Software\\winreglib');

			const handle = winreglib.watch(
				'HKCU\\SOFTWARE\\winreglib\\foo\\bar\\baz'
			);

			try {
				let counter = 0;
				await new Promise<void>((resolve, reject) => {
					handle.on('change', evt => {
						// console.log('CHANGE!!', counter, evt);
						try {
							expect(evt).toBeTypeOf('object');
							switch (counter++) {
								case 0:
									expect(evt).toMatchObject({
										type: 'add',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
									});
									reg(
										'add',
										'HKCU\\Software\\winreglib\\foo\\bar\\baz',
										'/v',
										'val1',
										'/t',
										'REG_SZ',
										'/d',
										'hello1'
									);
									break;
								case 1:
									expect(evt).toMatchObject({
										type: 'change',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
									});
									// this next line should not trigger anything
									reg(
										'add',
										'HKCU\\Software\\winreglib\\foo',
										'/v',
										'val2',
										'/t',
										'REG_SZ',
										'/d',
										'hello2'
									);
									setTimeout(() => {
										reg(
											'add',
											'HKCU\\Software\\winreglib\\foo\\bar\\baz',
											'/v',
											'val3',
											'/t',
											'REG_SZ',
											'/d',
											'hello3'
										);
									}, 250);
									break;
								case 2:
									expect(evt).toMatchObject({
										type: 'change',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
									});
									setTimeout(() => {
										reg('delete', 'HKCU\\Software\\winreglib', '/f');
									}, 250);
									break;
								case 3:
									expect(evt).toMatchObject({
										type: 'delete',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
									});
									resolve();
									break;
							}
						} catch (e) {
							reject(e);
						}
					});

					setTimeout(() => reg('add', 'HKCU\\Software\\winreglib\\foo'), 250);
					setTimeout(
						() => reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz\\wiz'),
						250
					);
				});
			} finally {
				handle.stop();
				reg('delete', 'HKCU\\Software\\winreglib', '/f');
			}
		},
		15000
	);

	it('should watch a key that gets deleted', async () => {
		reg('delete', 'HKCU\\Software\\winreglib', '/f');
		reg('add', 'HKCU\\Software\\winreglib\\foo');

		const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo');

		try {
			let counter = 0;
			await new Promise<void>((resolve, reject) => {
				handle.on('change', evt => {
					// console.log('CHANGE!', evt);
					try {
						expect(evt).toBeTypeOf('object');
						switch (counter++) {
							case 0:
								expect(evt).toMatchObject({
									type: 'delete',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								resolve();
								break;
						}
					} catch (e) {
						reject(e);
					}
				});
				setTimeout(
					() => reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f'),
					250
				);
			});
			handle.stop();
		} finally {
			// also test stop() being called twice
			handle.stop();
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
		}
	}, 15000);

	it(
		'should watch a key whose parent gets deleted',
		async () => {
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
			reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz');

			const handle = winreglib.watch(
				'HKCU\\SOFTWARE\\winreglib\\foo\\bar\\baz'
			);

			try {
				let counter = 0;
				await new Promise<void>((resolve, reject) => {
					handle.on('change', evt => {
						// console.log('CHANGE!', evt);
						try {
							expect(evt).toBeTypeOf('object');
							switch (counter++) {
								case 0:
									expect(evt).toMatchObject({
										type: 'delete',
										key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
									});
									resolve();
									break;
							}
						} catch (e) {
							reject(e);
						}
					});
					setTimeout(
						() => reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f'),
						250
					);
				});
				handle.stop();
			} finally {
				// also test stop() being called twice
				handle.stop();
				reg('delete', 'HKCU\\Software\\winreglib', '/f');
			}
		},
		15000
	);

	it('should survive the gauntlet', async () => {
		reg('delete', 'HKCU\\Software\\winreglib', '/f');
		reg('add', 'HKCU\\Software\\winreglib');

		const winreglibHandle = winreglib.watch('HKCU\\SOFTWARE\\winreglib');
		const fooHandle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo');
		const barHandle = winreglib.watch('HKCU\\SOFTWARE\\winreglib\\foo\\bar');
		const bazHandle = winreglib.watch(
			'HKCU\\SOFTWARE\\winreglib\\foo\\bar\\baz'
		);

		try {
			let counter = 0;
			await new Promise<void>((resolve, reject) => {
				const fn = (evt, handleName) => {
					log('CHANGE!', counter, handleName, evt);
					try {
						expect(evt).toBeTypeOf('object');
						switch (counter++) {
							case 0:
								expect(handleName).toBe('winreglibHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
								});
								break;
							case 1:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								break;
							case 2:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
								});
								reg('delete', 'HKCU\\Software\\winreglib\\foo\\bar', '/f');
								break;
							case 3:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
								});
								break;
							case 4:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								reg(
									'add',
									'HKCU\\Software\\winreglib',
									'/v',
									'foo',
									'/t',
									'REG_SZ',
									'/d',
									'bar'
								);
								break;
							case 5:
								expect(handleName).toBe('winreglibHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
								});
								reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\wiz');
								break;
							case 6:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								break;
							case 7:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
								});
								reg('add', 'HKCU\\Software\\winreglib\\foo\\bar\\baz');
								break;
							case 8:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
								});
								break;
							case 9:
								expect(handleName).toBe('bazHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
								});
								reg(
									'add',
									'HKCU\\Software\\winreglib\\foo\\bar\\baz',
									'/v',
									'foo',
									'/t',
									'REG_SZ',
									'/d',
									'bar'
								);
								break;
							case 10:
								expect(handleName).toBe('bazHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
								});
								reg('delete', 'HKCU\\Software\\winreglib', '/f', '/v', 'foo');
								break;
							case 11:
								expect(handleName).toBe('winreglibHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
								});
								reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f');
								break;
							case 12:
								expect(handleName).toBe('bazHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar\\baz'
								});
								break;
							case 13:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo\\bar'
								});
								break;
							case 14:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib\\foo'
								});
								break;
							case 15:
								expect(handleName).toBe('winreglibHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: 'HKEY_CURRENT_USER\\SOFTWARE\\winreglib'
								});
								resolve();
								break;
						}
					} catch (e) {
						reject(e);
					}
				};
				winreglibHandle.on('change', evt => fn(evt, 'winreglibHandle'));
				fooHandle.on('change', evt => fn(evt, 'fooHandle'));
				barHandle.on('change', evt => fn(evt, 'barHandle'));
				bazHandle.on('change', evt => fn(evt, 'bazHandle'));
				setTimeout(
					() => reg('add', 'HKCU\\Software\\winreglib\\foo\\bar'),
					250
				);
			});
		} finally {
			winreglibHandle.stop();
			fooHandle.stop();
			barHandle.stop();
			bazHandle.stop();
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
		}
	}, 15000);
});
