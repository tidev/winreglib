<br>
<div align="center">
	<img width="640" height="240" src="media/winreglib.webp" alt="winreglib">
</div>
<br>

`winreglib` is a native C++ addon for Node.js for querying and watching Windows
Registry keys. It provides a read-only, synchronous API for accessing the
Windows Registry.

## Features

  - Get, list, and watch registry keys _without_ spawning `reg.exe`
  - Written in TypeScript
  - Packaged as an ESM module
  - Supports x64 (64-bit) CPU architectures (arm/arm64 untested)

## Requirements

`winreglib` requires Node.js >=20.18.1 (Node-API 8) and ships with pre-built
binaries for x64 and ia32 architectures. ARM-based architectures are not
officially supported, but should technically work as long as you have the
build tools installed.

## Example

```javascript
import winreglib from 'winreglib';

const key = 'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion';
const results = winreglib.list(key);

console.log(`Resolved ${results.resolvedRoot}`);
console.log(`${results.key} has ${results.subkeys.length} subkeys`);

console.log('  Subkeys:');
for (const subkey of results.subkeys) {
	console.log(`    ${subkey}`);
}

console.log('  Values:');
for (const valueName of results.values) {
	console.log(`    ${valueName} = ${winreglib.get(key, valueName)}`);
}
```

## Supported Root Keys

| Name                               | Abbreviation |
| ---------------------------------- | ------------ |
| `HKEY_CLASSES_ROOT`                | `HKCR`       |
| `HKEY_CURRENT_CONFIG`              | `HKCC`       |
| `HKEY_CURRENT_USER`                | `HKCU`       |
| `HKEY_CURRENT_USER_LOCAL_SETTINGS` |              |
| `HKEY_LOCAL_MACHINE`               | `HKLM`       |
| `HKEY_PERFORMANCE_DATA`            |              |
| `HKEY_PERFORMANCE_NLSTEXT`         |              |
| `HKEY_PERFORMANCE_TEXT`            |              |
| `HKEY_USERS`                       | `HKU`        |

## API

### `get(key, valueName)`

Get a value for the given key and value name.

| Argument    | Type   | Description                      |
| ----------- | ------ | -------------------------------- |
| `key`       | String | The key beginning with the root. |
| `valueName` | String | The name of the value to get.    |

Returns a `String`, `Number`, `Buffer`, `Array.<String>`, or `null`
depending on the value.

If `key` or `valueName` is not found, an `Error` is thrown.

```js
const value = winreglib.get(
  'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion',
  'ProgramFilesDir'
);

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

Returns an `RegistryKey` object:

```
type RegistryKey = {
	resolvedRoot: string;
	key: string;
	subkeys: string[];
	values: unknown[];
};
```

If `key` is not found, an `Error` is thrown.

```js
const result = winreglib.list(
  'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup
);

console.log(result);
```

```js
{ resolvedRoot: 'HKEY_LOCAL_MACHINE',
  key:
    'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup',
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

Returns a handle (`WinRegLibWatchHandle` which extends `EventEmitter`) that
emits `"change"` events. Call `handle.stop()` to stop watching the key.

```js
const handle = winreglib.watch('HKLM\\SOFTWARE');
handle.on('change', evt => {
	console.log(`Got change! type=${evt.type} key=${evt.key}`);
	handle.stop();
});
```

The `"change"` event object contains a change `"type"` and the affected
`"key"`.

| Event Type | Description                        |
| ---------- | ---------------------------------- |
| `add`      | The `key` was added.               |
| `change`   | A subkey or value was added, changed, deleted, or permissions modified, but we don't know exactly what. |
| `delete`   | The `key` was deleted.             |

`watch()` can track keys that do not exist and when they are created, a
change event will be emitted. You can watch the same key multiple times,
however each returned handle is unique and you must call `handle.stop()` for
each.

Due to limitations of the Win32 API, `watch()` is unable to determine what
actually changed during a `change` event type. You will need to call `list()`
and cache the subkeys and values, then call `list()` again when a change is
emitted and compare the before and after.

Note that `watch()` does not support recursively watching for changes. The
Win32 API does support recursive watching, so this could be added in the
future.

## Advanced

### Debug Logging

`winreglib` exposes an event emitter that emits debug log messages. This is
intended to help debug issues under the hood. The average user will never
need to use this, however it would be handy when filing a bug.

```js
winreglib.on('log', msg => console.log(msg));
```

Alternatively, `winreglib` uses the amazing [`snooplogg`][2] debug logger
where you simply set the `SNOOPLOGG` environment variable to `winreglib` (or
`*`) and it will print the debug log to `stderr`.

```bash
$ SNOOPLOGG=winreglib node myapp.js
  6.150s winreglib:Watchman Initializing async work (thread 16764576586047274673)
  6.150s winreglib:Watchman::run Initializing run loop (thread 12502165600786624632)
  6.151s winreglib:Watchman::run Populating active copy (count=0)
  6.151s winreglib:Watchman::run Waiting on 2 objects...
  6.152s winreglib:list key="HKLM" subkey="SOFTWARE\Microsoft\Windows\CurrentVersion"
  6.152s winreglib:list 170 keys (max 30), 11 values (max 24)
```

### Building `winreglib`

`winreglib` has two components: the public interface written in TypeScript and
the native addon written in C++.

To compile the C++ code, you will need the Microsoft Visual Studio (not VSCode)
or the Microsoft Build Tools. You may also need Python 3 installed.

| Command              | Description |
| -------------------- | ----------- |
| `pnpm build`         | Compiles the TypeScript and the Node.js native C++ addon for the current architecture |
| `pnpm build:bundle`  | Compiles only the TypeScript code |
| `pnpm build:local`   | Compiles only the Node.js native C++ addon |
| `pnpm rebuild:local` | Cleans and re-compiles only the Node.js native C++ addon |

When publishing, the native C++ addon is prebuilt for x64 and ia32
architectures. Generally you shouldn't need be concerned with the prebuilt
binaries, however the following commands will compile the prebuilds:

| Command               | Description                    |
| --------------------- | ------------------------------ |
| `pnpm build:prebuild` | Prebuild for x64 and ia32      |
| `pnpm prebuild-arm`   | Prebuild for arm (32-bit)      |
| `pnpm prebuild-arm64` | Prebuild for arm64             |
| `pnpm prebuild-ia32`  | Prebuild for ia32 (x86 32-bit) |
| `pnpm prebuild-x64`   | Prebuild for x64               |

### Architecture

When `winreglib` is imported, it immediately spawns a background thread in the
event the app is going to watch a key. If a key is added/changed/deleted, the
background thread sends a message to the main thread which emits the change
event.

## Legal

Titanium is a registered trademark of TiDev Inc. All Titanium trademark and patent rights were transferred and assigned to TiDev Inc. on 4/7/2022. Please see the LEGAL information about using our trademarks, privacy policy, terms of usage and other legal information at https://tidev.io/legal.

[1]: https://github.com/tidev/winreglib/blob/master/LICENSE
[2]: https://www.npmjs.com/package/snooplogg
