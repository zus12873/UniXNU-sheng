# macOS first-boot verification

Device: Xiaomi Pad 6S Pro (sheng), 8 GiB configuration.

| Input | Value |
| --- | --- |
| OS | macOS 27.3, 26C5035r |
| Boot image SHA-256 | `258b3dee34704ac1736d0c922725a4559407db27b2abfa788081c496df8912b1` |
| Selected firmware SHA-256 | `c9fcaadf405eb131f20b4cdbb9976688f98a05cbcbc9388d6ff450447d9cdc33` |
| DarwinBoot PE SHA-256 | `9dee24298f823d38bb0d6d49e74b7e060b63bcaf29d02571e47ab3ac892990e4` |
| Template SHA-256 | `8fed0304acdfac65421f8523d6be727311ee5f6f8826ac20736735de0e533124` |

The image was temporarily started with `fastboot boot boot/boot.img`.
The device operator selected the Data initialization entry, confirmed it,
and reported reaching the desktop. Ordinary Boot on the same fresh userdata
had shown a cursor on a black screen. Its saved log reported Data requests
as not ready, and a read-only export found a blank persistent-store header.

The compatibility path preserves the template's firmware transport and
replaces DarwinBoot with the current build. Repacking through the integrated
`unixnu bootimg` path reproduced the tested image byte for byte.

QEMU checks verified that an ordinary boot leaves blank Data unchanged;
explicit initialization enabled writes, completed 64,978,944 bytes of Data
I/O without persistence errors, and reached launchd mount phases 1 and 2.
The old frontend passed the confirmation argument to the new loader. With
`timeout=-1`, its four-entry menu stayed open without input.

This checkpoint covers initialization and desktop entry. Reboot persistence,
Wi-Fi, Bluetooth and sustained GPU operation were not evaluated in this run.
