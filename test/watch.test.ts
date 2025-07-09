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

describe('watch()', () => {
	it('should error if key is not specified', () => {
		expect(() => {
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
			const key = `HKEY_CURRENT_USER\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
			reg('add', key);

			const handle = winreglib.watch(key);

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
										key
									});
									reg('delete', `${key}\\foo`, '/f');
									break;
								case 1:
									expect(evt).toMatchObject({
										type: 'change',
										key
									});
									resolve();
									break;
							}
						} catch (e) {
							reject(e);
						}
					});
					setTimeout(() => reg('add', `${key}\\foo`), 250);
				});

				handle.stop();
			} finally {
				// also test stop() being called twice
				handle.stop();
				reg('delete', key, '/f');
			}
		},
		15000
	);

	it(
		'should watch existing key for value change',
		async () => {
			const key = `HKEY_CURRENT_USER\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
			reg('add', key);

			const handle = winreglib.watch(key);

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
										key
									});
									reg(
										'delete',
										key,
										'/f',
										'/v',
										'foo'
									);
									break;
								case 1:
									expect(evt).toMatchObject({
										type: 'change',
										key
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
								key,
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
				reg('delete', key, '/f');
			}
		},
		15000
	);

	it('should watch a key that does not exist', async () => {
		const key = `HKEY_CURRENT_USER\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
		reg('add', key);

		const handle = winreglib.watch(`${key}\\foo`);

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
									key: `${key}\\foo`
								});
								reg(
									'add',
									`${key}\\foo`,
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
									key: `${key}\\foo`
								});
								reg(
									'delete',
									`${key}\\foo`,
									'/f',
									'/v',
									'bar'
								);
								break;
							case 2:
								expect(evt).toMatchObject({
									type: 'change',
									key: `${key}\\foo`
								});
								setTimeout(
									() => reg('delete', `${key}\\foo`, '/f'),
									250
								);
								break;
							case 3:
								expect(evt).toMatchObject({
									type: 'delete',
									key: `${key}\\foo`
								});
								setTimeout(
									() => reg('add', `${key}\\foo`),
									250
								);
								break;
							case 4:
								expect(evt).toMatchObject({
									type: 'add',
									key: `${key}\\foo`
								});
								resolve();
								break;
						}
					} catch (e) {
						reject(e);
					}
				});
				setTimeout(() => reg('add', `${key}\\foo`), 250);
			});
		} finally {
			handle.stop();
			reg('delete', key, '/f');
		}
	}, 15000);

	it(
		'should watch a key whose parent does not exist',
		async () => {
			const key = `HKEY_CURRENT_USER\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
			reg('add', key);

			const handle = winreglib.watch(
				`${key}\\foo\\bar\\baz`
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
										key: `${key}\\foo\\bar\\baz`
									});
									reg(
										'add',
										`${key}\\foo\\bar\\baz`,
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
										key: `${key}\\foo\\bar\\baz`
									});
									// this next line should not trigger anything
									reg(
										'add',
										`${key}\\foo`,
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
											`${key}\\foo\\bar\\baz`,
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
										key: `${key}\\foo\\bar\\baz`
									});
									setTimeout(() => {
										reg('delete', key, '/f');
									}, 250);
									break;
								case 3:
									expect(evt).toMatchObject({
										type: 'delete',
										key: `${key}\\foo\\bar\\baz`
									});
									resolve();
									break;
							}
						} catch (e) {
							reject(e);
						}
					});

					setTimeout(() => reg('add', `${key}\\foo`), 250);
					setTimeout(
						() => reg('add', `${key}\\foo\\bar\\baz\\wiz`),
						250
					);
				});
			} finally {
				handle.stop();
				reg('delete', key, '/f');
			}
		},
		15000
	);

	it('should watch a key that gets deleted', async () => {
		const key = `HKEY_CURRENT_USER\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
		reg('add', `${key}\\foo`);

		const handle = winreglib.watch(`${key}\\foo`);

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
									key: `${key}\\foo`
								});
								resolve();
								break;
						}
					} catch (e) {
						reject(e);
					}
				});
				setTimeout(
					() => reg('delete', `${key}\\foo`, '/f'),
					250
				);
			});
			handle.stop();
		} finally {
			// also test stop() being called twice
			handle.stop();
			reg('delete', key, '/f');
		}
	}, 15000);

	it(
		'should watch a key whose parent gets deleted',
		async () => {
			const key = `HKEY_CURRENT_USER\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
			reg('add', `${key}\\foo\\bar\\baz`);

			const handle = winreglib.watch(
				`${key}\\foo\\bar\\baz`
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
										key: `${key}\\foo\\bar\\baz`
									});
									resolve();
									break;
							}
						} catch (e) {
							reject(e);
						}
					});
					setTimeout(
						() => reg('delete', `${key}\\foo`, '/f'),
						250
					);
				});
				handle.stop();
			} finally {
				// also test stop() being called twice
				handle.stop();
				reg('delete', key, '/f');
			}
		},
		15000
	);

	it('should survive the gauntlet', async () => {
		const key = `HKEY_CURRENT_USER\\Software\\winreglib\\test-${randomBytes(4).toString('hex')}`;
		reg('add', key);

		const winreglibHandle = winreglib.watch(key);
		const fooHandle = winreglib.watch(`${key}\\foo`);
		const barHandle = winreglib.watch(`${key}\\foo\\bar`);
		const bazHandle = winreglib.watch(
			`${key}\\foo\\bar\\baz`
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
									key
								});
								break;
							case 1:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: `${key}\\foo`
								});
								break;
							case 2:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: `${key}\\foo\\bar`
								});
								reg('delete', `${key}\\foo\\bar`, '/f');
								break;
							case 3:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: `${key}\\foo\\bar`
								});
								break;
							case 4:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: `${key}\\foo`
								});
								reg(
									'add',
									key,
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
									key
								});
								reg('add', `${key}\\foo\\bar\\wiz`);
								break;
							case 6:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: `${key}\\foo`
								});
								break;
							case 7:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: `${key}\\foo\\bar`
								});
								reg('add', `${key}\\foo\\bar\\baz`);
								break;
							case 8:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key: `${key}\\foo\\bar`
								});
								break;
							case 9:
								expect(handleName).toBe('bazHandle');
								expect(evt).toMatchObject({
									type: 'add',
									key: `${key}\\foo\\bar\\baz`
								});
								reg(
									'add',
									`${key}\\foo\\bar\\baz`,
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
									key: `${key}\\foo\\bar\\baz`
								});
								reg('delete', key, '/f', '/v', 'foo');
								break;
							case 11:
								expect(handleName).toBe('winreglibHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key
								});
								reg('delete', `${key}\\foo`, '/f');
								break;
							case 12:
								expect(handleName).toBe('bazHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: `${key}\\foo\\bar\\baz`
								});
								break;
							case 13:
								expect(handleName).toBe('barHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: `${key}\\foo\\bar`
								});
								break;
							case 14:
								expect(handleName).toBe('fooHandle');
								expect(evt).toMatchObject({
									type: 'delete',
									key: `${key}\\foo`
								});
								break;
							case 15:
								expect(handleName).toBe('winreglibHandle');
								expect(evt).toMatchObject({
									type: 'change',
									key
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
					() => reg('add', `${key}\\foo\\bar`),
					250
				);
			});
		} finally {
			winreglibHandle.stop();
			fooHandle.stop();
			barHandle.stop();
			bazHandle.stop();
			reg('delete', key, '/f');
		}
	}, 15000);
});
