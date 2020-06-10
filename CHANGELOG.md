# v2.0.1 (Jun 10, 2020)

 * fix: Added `node-gyp` to fix install issues.

# v2.0.0 (Jun 5, 2020)

 * BREAKING CHANGE: Drop support for Node 8.
 * chore: Updated dependencies.

# v1.0.5 (Jul 15, 2020)

 * fix: Added back install script.

# v1.0.4 (Jul 12, 2019)

 * fix: Removed `install` script that was forcing Visual Studio to be installed.
 * chore: Updated dependencies.

# v1.0.3 (Jul 1, 2019)

 * fix: Fixed bug with removing a watch node listener would remove all listeners.
 * fix: Fixed typo in error code value when watch node fails invoking listener.
 * chore: Made all global functions static.
 * chore: Misc code cleanup.
 * chore: Updated dependencies.

# v1.0.2 (May 28, 2019)

 * fix: Added missing double quote in install script.

# v1.0.1 (May 28, 2019)

 * fix: Added null gyp target for non-win32 platforms.
 * fix: Added `prepublishOnly` script to rebuild the prebuilds.
 * fix: Fixed install script to only run `node-gyp-build` on win32.

# v1.0.0 (May 24, 2019)

 - Initial release.
