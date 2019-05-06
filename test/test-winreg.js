const fs = require('fs');
const reg = require('../src/index');
const { expect } = require('chai');

describe('get()', () => {
	it('should error if root key is not specified', () => {
		try {
			reg.get();
		} catch (e) {
			expect(e.message).to.equal('Expected root key to be a non-empty string');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key is not specified', () => {
		try {
			reg.get('HKLM');
		} catch (e) {
			expect(e.message).to.equal('Expected key to be a non-empty string');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if value name is not specified', () => {
		try {
			reg.get('HKLM', 'software');
		} catch (e) {
			expect(e.message).to.equal('Expected value name to be a non-empty string');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if root key is not valid', () => {
		try {
			reg.get('a', 'b', 'c');
		} catch (e) {
			expect(e.message).to.equal('Invalid registry root key "a"');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if key is not found', () => {
		try {
			reg.get('HKLM', 'foo', 'bar');
		} catch (e) {
			expect(e.message).to.equal('Registry key or value not found (code 2)');
			return;
		}
		throw new Error('Expected error');
	});

	it('should error if value is not found', () => {
		try {
			reg.get('HKLM', 'SOFTWARE', 'bar');
		} catch (e) {
			expect(e.message).to.equal('Registry key or value not found (code 2)');
			return;
		}
		throw new Error('Expected error');
	});

	it('should get a string value', () => {
		const value = reg.get('HKLM', 'SOFTWARE\\Microsoft\\Windows\\CurrentVersion', 'ProgramFilesDir');
		expect(value).to.be.a('string');
		expect(value).to.not.equal('');
		expect(fs.existsSync(value)).to.be.true;
	});

	it('should get an expanded string value', () => {
		const value = reg.get('HKLM', 'SOFTWARE\\Microsoft\\Windows\\CurrentVersion', 'ProgramFilesPath');
		expect(value).to.be.a('string');
		expect(value).to.not.equal('');
		expect(fs.existsSync(value)).to.be.true;
	});

	it('should get multi-string value', () => {
		const value = reg.get('HKLM', 'HARDWARE\\DESCRIPTION\\System', 'SystemBiosVersion');
		expect(value).to.be.an('array');
		expect(value).to.have.lengthOf.above(0);
		for (const v of value) {
			expect(v).to.be.a('string');
			expect(v).to.not.equal('');
		}
	});

	it('should get an 32-bit integer value', () => {
		const value = reg.get('HKLM', 'HARDWARE\\DESCRIPTION\\System', 'Capabilities');
		expect(value).to.be.a('number');
		expect(value).to.be.at.least(0);
	});

	it('should get an 64-bit integer value', () => {
		const value = reg.get('HKLM', 'Software\\Microsoft\\Windows\\CurrentVersion\\Diagnostics\\DiagTrack', 'TriggerCount');
		expect(value).to.be.a('number');
		expect(value).to.be.at.least(0);
	});

	it('should get a binary value', () => {
		const value = reg.get('HKLM', 'HARDWARE\\DESCRIPTION\\System', 'Component Information');
		expect(value).to.be.an.instanceof(Buffer);
		expect(value.length).to.be.above(0);
	});

	it('should get a full resource descriptor value', () => {
		const value = reg.get('HKLM', 'HARDWARE\\DESCRIPTION\\System', 'Configuration Data');
		expect(value).to.be.an.instanceof(Buffer);
		expect(value.length).to.be.above(0);
	});

	it('should get a resource list value', () => {
		const value = reg.get('HKLM', 'HARDWARE\\RESOURCEMAP\\System Resources\\Loader Reserved', '.Raw');
		expect(value).to.be.an.instanceof(Buffer);
		expect(value.length).to.be.above(0);
	});

	it('should get none value', () => {
		const value = reg.get('HKCR', '.mp3\\OpenWithProgids', 'mp3_auto_file');
		expect(value).to.be.null;
	});
});
