# Windows Registry Utility Library

A library for querying and watching the Windows Registry.

## Prerequisites

`winreglib` requires N-API version 3 and the following Node.js versions:

* v10.2.0 or newer

## Installation

	npm install winreglib

## Introduction

`winreglib` is a native Node.js addon for querying the Windows Registry. The API is synchronous.

Currently `winreglib` only supports read operations. It can support write operations someday if
need and time exists.

## Example

```js
import winreglib from 'winreglib';

const key = 'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion';
const results = winreglib.list(key);

console.log(`${results.root}\\${results.path}`);

console.log('  Subkeys:');
for (const subkey of results.subkeys) {
	console.log(`    ${subkey}`);
}

console.log('  Values:');
for (const valueName of results.values) {
	console.log(`    ${valueName} = ${winreglib.get(key, valueName)}`);
}
```

## API

### `get(key, valueName)`

Get a value for the given key and value name.

| Argument    | Type   | Description                      |
| ----------- | ------ | -------------------------------- |
| `key`       | String | The key beginning with the root. |
| `valueName` | String | The name of the value to get.    |

Returns a `String`, `Number`, `Buffer`, `Array.<String>`, or `null` depending on the value.

If `key` or `valueName` is not found, an `Error` is thrown.

```js
const value = winreglib.get('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion', 'ProgramFilesDir');

console.log(value);
```

```
C:\Program Files
```

### `list(key)`

Retreives all subkeys and value names for a give key.

| Argument | Type   | Description                      |
| -------- | ------ | -------------------------------- |
| `key`    | String | The key beginning with the root. |

Returns an `Object` with the resolved `resolvedRoot` (String), `key` (String), `subkeys`
(Array[String]), and `values` (Array[String]).

If `key` is not found, an `Error` is thrown.

```js
const result = winreglib.list('HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup');

console.log(result);
```

```js
{ resolvedRoot: 'HKEY_LOCAL_MACHINE',
  key: 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup',
  subkeys:
   [ 'DPI',
     'ImageServicingData',
     'OC Manager',
     'OOBE',
     'PnpLockdownFiles',
     'PnpResources',
     'State',
     'Sysprep',
     'SysPrepExternal',
     'WindowsFeatures' ],
  values: [ 'LogLevel', 'BootDir' ] }
```

### `watch(key)`

Watches a key for changes in subkeys or values.

| Argument | Type   | Description                      |
| -------- | ------ | -------------------------------- |
| `key`    | String | The key beginning with the root. |

Returns a handle (`EventEmitter`) that emits `"change"` events. Call `handle.stop()` to stop watching
the key.

```js
const handle = winreglib.watch('HKLM\\SOFTWARE');
handle.on('change', evt => {
	console.log(`Got change! type=${evt.type} key=${evt.key}`);
	handle.stop();
});
```

The `"change"` event object contains a change `"type"` and the affected `"key"`.

| Event Type | Description                        |
| ---------- | ---------------------------------- |
| `add`      | The `key` was added.               |
| `change`   | A subkey or value was added, changed, deleted, or permissions modified, but we don't know exactly what. |
| `delete`   | The `key` was deleted.             |

`watch()` can track keys that do not exist and when they are created, a change event will be
emitted. You can watch the same key multiple times, however each returned handle is unique and you
must call `handle.stop()` for each.

Due to limitations of the Win32 API, `watch()` is unable to determine what actually changed during
a `change` event type. You will need to call `list()` and cache the subkeys and values, then call
`list()` again when a change is emitted and compare the before and after.

Note that `watch()` does not support recursively watching for changes.

## Advanced

### Debug Logging

`winreglib` exposes an event emitter that emits debug log messages. This is intended to help debug
issues under the hood. The average user will never need to use this, however it would be handy when
filing a bug.

```js
winreglib.on('log', msg => console.log(msg));
```

Alternatively, `winreglib` uses the amazing [`snooplogg`][2] debug logger where you simply set the
`SNOOPLOGG` environment variable to `winreglib` (or `*`) and it will print the debug log to stdout.

## Legal

Titanium is a registered trademark of TiDev Inc. All Titanium trademark and patent rights were transferred and assigned to TiDev Inc. on 4/7/2022. Please see the LEGAL information about using our trademarks, privacy policy, terms of usage and other legal information at https://tidev.io/legal.

[1]: https://github.com/appcelerator/winreglib/blob/master/LICENSE
[2]: https://www.npmjs.com/package/snooplogg
