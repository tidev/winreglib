const fs = require('fs');
const winreglib = require('../src/index');
const { expect } = require('chai');
const { spawnSync } = require('child_process');

const reg = (...args) => {
	console.log('Executing: reg ' + args.join(' '));
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

	it('should error if value name is not specified', () => {
		try {
			winreglib.get('HKLM\\software');
		} catch (e) {
			expect(e.message).to.equal('Expected value name to be a non-empty string');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key does not contain a subkey', () => {
		try {
			winreglib.get('foo', 'bar');
		} catch (e) {
			expect(e.message).to.equal('Expected key to contain both a root and subkey');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key is not valid', () => {
		try {
			winreglib.get('foo\\bar', 'baz');
		} catch (e) {
			expect(e.message).to.equal('Invalid registry root key "foo"');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key is not found', () => {
		try {
			winreglib.get('HKLM\\foo', 'bar');
		} catch (e) {
			expect(e.message).to.equal('Registry key or value not found');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if value is not found', () => {
		try {
			winreglib.get('HKLM\\SOFTWARE', 'bar');
		} catch (e) {
			expect(e.message).to.equal('Registry key or value not found');
			return;
		}
		throw new Error('Expected error');
	});

	it('should get a string value', () => {
		const value = winreglib.get('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion', 'ProgramFilesDir');
		expect(value).to.be.a('string');
		expect(value).to.not.equal('');
		expect(fs.existsSync(value)).to.be.true;
	});

	it('should get an expanded string value', () => {
		const value = winreglib.get('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion', 'ProgramFilesPath');
		expect(value).to.be.a('string');
		expect(value).to.not.equal('');
		expect(fs.existsSync(value)).to.be.true;
	});

	it('should get multi-string value', () => {
		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'SystemBiosVersion');
		expect(value).to.be.an('array');
		expect(value).to.have.lengthOf.above(0);
		for (const v of value) {
			expect(v).to.be.a('string');
			expect(v).to.not.equal('');
		}
	});

	it('should get an 32-bit integer value', () => {
		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'Capabilities');
		expect(value).to.be.a('number');
		expect(value).to.be.at.least(0);
	});

	it('should get an 64-bit integer value', () => {
		const value = winreglib.get('HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Diagnostics\\DiagTrack', 'TriggerCount');
		expect(value).to.be.a('number');
		expect(value).to.be.at.least(0);
	});

	it('should get a binary value', () => {
		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'Component Information');
		expect(value).to.be.an.instanceof(Buffer);
		expect(value.length).to.be.above(0);
	});

	it('should get a full resource descriptor value', () => {
		const value = winreglib.get('HKLM\\HARDWARE\\DESCRIPTION\\System', 'Configuration Data');
		expect(value).to.be.an.instanceof(Buffer);
		expect(value.length).to.be.above(0);
	});

	it('should get a resource list value', () => {
		const value = winreglib.get('HKLM\\HARDWARE\\RESOURCEMAP\\System Resources\\Loader Reserved', '.Raw');
		expect(value).to.be.an.instanceof(Buffer);
		expect(value.length).to.be.above(0);
	});

	it('should get none value', () => {
		const value = winreglib.get('HKCR\\.mp3\\OpenWithProgids', 'mp3_auto_file');
		expect(value).to.be.null;
	});
});

describe('list()', () => {
	it('should error if key is not specified', () => {
		try {
			winreglib.list();
		} catch (e) {
			expect(e.message).to.equal('Expected key to be a non-empty string');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key does not contain a subkey', () => {
		try {
			winreglib.list('foo');
		} catch (e) {
			expect(e.message).to.equal('Expected key to contain both a root and subkey');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key is not valid', () => {
		try {
			winreglib.list('foo\\bar');
		} catch (e) {
			expect(e.message).to.equal('Invalid registry root key "foo"');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key is not found', () => {
		try {
			winreglib.list('HKLM\\foo');
		} catch (e) {
			expect(e.message).to.equal('Registry key or value not found');
			return;
		}
		throw new Error('Expected error');
	});

	it('should retrieve subkeys and values', () => {
		const result = winreglib.list('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion');

		expect(result).to.be.an('object');
		expect(result).to.have.keys('root', 'path', 'subkeys', 'values');

		expect(result.root).to.equal('HKEY_LOCAL_MACHINE');
		expect(result.path).to.equal('SOFTWARE\\Microsoft\\Windows\\CurrentVersion');

		expect(result.subkeys).to.be.an('array');
		for (const key of result.subkeys) {
			expect(key).to.be.a('string');
			expect(key).to.not.equal('');
		}

		expect(result.values).to.be.an('array');
		for (const value of result.values) {
			expect(value).to.be.a('string');
			expect(value).to.not.equal('');
		}
	});
});

describe('watch()', () => {
	it('should error if key is not specified', () => {
		try {
			winreglib.watch();
		} catch (e) {
			expect(e.message).to.equal('Expected key to be a non-empty string');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key does not contain a subkey', () => {
		try {
			winreglib.watch('HKLM');
		} catch (e) {
			expect(e.message).to.equal('Expected key to contain both a root and subkey');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if root key is not valid', () => {
		try {
			winreglib.watch('foo\\bar');
		} catch (e) {
			expect(e.message).to.equal('Invalid registry root key "foo"');
			return;
		}
		throw new Error('Expected error');
	});

	it.only('should watch existing key', async function () {
		this.timeout(15000);
		this.slow(14000);

		reg('delete', 'HKCU\\Software\\winreglib', '/f');
		reg('add', 'HKCU\\Software\\winreglib');

		try {
			const handle = winreglib.watch('HKCU\\SOFTWARE\\winreglib');
			let counter = 0;

			await new Promise((resolve, reject) => {
				handle.on('change', evt => {
					try {
						console.log('CHANGE EVENT!', evt);
						expect(evt).to.be.an('object');
						switch (counter++) {
							case 0:
								expect(evt.action).to.equal('add');
								reg('delete', 'HKCU\\Software\\winreglib\\foo', '/f', '/va');
								break;

							case 1:
								expect(evt.action).to.equal('delete');
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
			reg('delete', 'HKCU\\Software\\winreglib', '/f');
		}
	});
});
