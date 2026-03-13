# ios1-jb

Untethered jailbreak for iOS 1.x devices (S5L8900). macOS only.

## Build

```
make
```

## Usage

Put your device in Recovery mode, then run:

```
./src/ios1-jb
```

### Options

| Flag | Description |
|------|-------------|
| `-h, --help` | Show help message |
| `-s` | Enable serial output |
| `-n` | Normal boot (skip ramdisk) |

Default behavior (no flags) jailbreaks the device.

## What it does

- Remounts rootfs as read-write
- Enables syslogging to `/var/log/syslog`
- Hacktivates the device
- Installs a minimal bootstrap (basic cli tools, no package manager)
- Enables SSH (connect via device IP, USB tunneling not supported)
- SSH password: `alpine`

## Included utilities
- `inject`: inject a dylib into a running process
- `screenshot`: capture a screenshot of the device's display
- SSH/SCP/SFTP
- Basic coreutils
- nano, tar, unzip, top, killall, ifconfig, nvram, du, base64, md5sum, sw_vers
