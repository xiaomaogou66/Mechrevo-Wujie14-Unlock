<div align="right">
  <strong>English</strong> | <a href="README.zh-CN.md">简体中文</a>
</div>

# Fingerprint & Face Unlock for Mechrevo Wujie 14

Linux (Arch / CachyOS, Wayland) unlock solution for the **Realtek USB2.0 Finger Print Bridge (USB ID `3274:9011`)** — a sensor with **no official libfprint driver** — plus face unlock via howdy.

![status](https://img.shields.io/badge/status-working-green) ![license](https://img.shields.io/badge/license-LGPL--2.1-orange)

> Desktop base: [NyxNiri](https://github.com/ech678/NyxNiri) (GPL-3.0) — Niri + Noctalia V5 on Arch/CachyOS.

## Features

| Channel | How it works | 
|---|---|
| 🖐️ Fingerprint | Noctalia lock screen → fprintd D-Bus → **patched libfprint** (reverse-engineered driver) |
| 👤 Face | Press Enter on lock screen → PAM (`login` stack) → howdy → OpenCV/dlib |
| ⌨️ Password | pam_unix fallback |

## Hardware

- Sensor: `3274:9011` Realtek USB2.0 Finger Print Bridge (Match-on-Chip, built-in USB)
- Camera: UVC (`/dev/video0`, MJPG 1080p)
- Confirmed on: Mechrevo Wujie 14 (机械革命 无界14), CachyOS + niri + Noctalia V5

Check your sensor:

```bash
lsusb | grep -i finger   # expect "3274:9011 Generic Realtek USB2.0 Finger Print Bridge"
```

## The Reverse-Engineering Finding

The sensor is a Realtek MOC (Match-on-Chip) device, but its firmware differs from the supported `3274:9003`:

- **`3274:9011` requires a 4-step `select_os` handshake (param = 0→1→2→3) before the template store becomes accessible.**
  The official driver sends it once (param=0x01, correct for 9003); without the handshake, `GET_TEMPLATE` is rejected (status `0x01`).
- The patch keys the handshake on the PID, so other Realtek MOC devices are unaffected.
- Sensors ship with factory/pre-owner templates → clear them first (`delete_record(0xff)`) or enrollment reports `duplicate`.

## Install

### 1. Fingerprint driver (patched libfprint)

```bash
sudo pacman -S fprintd
git clone https://github.com/xiaomaogou66/Mechrevo-Wujie14-Unlock
cd Mechrevo-Wujie14-Unlock
makepkg -si          # builds libfprint-9011 (conflicts with stock libfprint)
sudo systemd-hwdb update
```

### 2. Clear factory templates & enroll

```bash
sudo pkill -x fprintd
cd tools && make && sudo ./rtk-clear     # clears the sensor template store
fprintd-enroll $USER                     # press finger when prompted, 3-4 scans
fprintd-verify $USER
```

### 3. Face unlock (howdy)

```bash
paru -S howdy pam-python-git
sudo cp /usr/lib/security/howdy/config.ini /etc/howdy/config.ini
sudo sed -i 's|^device_path = none|device_path = /dev/video0|' /usr/lib/security/howdy/config.ini

# Python 3 compatibility patch for pam.py (2.6.1 ships a Python 2 import)
sudo python3 -c "p='/usr/lib/security/howdy/pam.py'; s=open(p).read(); s=s.replace('import ConfigParser','try:\n    import configparser as ConfigParser\nexcept ImportError:\n    import ConfigParser'); open(p,'w').write(s)"
sudo chmod -R a+rX /usr/lib/security/howdy   # lock screen PAM runs as the user

sudo sed -i -e 's/^capture_failed = .*/capture_failed = false/' \
            -e 's/^capture_successful = .*/capture_successful = false/' \
            /usr/lib/security/howdy/config.ini

# PAM hook (covers lock screen / login / sudo)
sudo cp /etc/pam.d/system-auth /etc/pam.d/system-auth.bak
sudo sed -i '/pam_faillock.so.*preauth/a auth sufficient pam_python3.so /usr/lib/security/howdy/pam.py' /etc/pam.d/system-auth

sudo howdy add    # look into the camera
```

### 4. Noctalia lock screen config

`~/.config/noctalia/noctalia-config.toml`:

```toml
[lockscreen]
allow_empty_password = true   # Enter + empty password triggers PAM (face auth)
fingerprint = true            # native fingerprint unlock
```

Reload: `noctalia msg config-reload`

## Usage (lock screen)

- **Finger on sensor** → instant unlock
- **Press Enter** → camera face recognition → unlock
- **Type password** → fallback

## Verification

| Test | Expected |
|---|---|
| `fprintd-list $USER` | enrolled fingerprint listed |
| `fprintd-verify $USER` | match |
| `Mod+L` → finger | unlock |
| `Mod+L` → Enter | camera → unlock |

## Repository layout

```
├── realtek-9011.patch                              # libfprint patch (ID + 4-step handshake + hwdb + meson fix)
├── PKGBUILD                                        # AUR-style package (libfprint-9011)
├── README.md                                       # this file
├── Mechrevo-Wujie-14-指纹与面部解锁方案.md         # full solution doc (中文, CC BY-SA 4.0)
├── Mechrevo-Wujie14-Fingerprint-Face-Unlock.md     # full solution doc (English, CC BY-SA 4.0)
└── tools/
    ├── rtk-clear.c                                 # clear factory templates
    └── rtk-test*.c                                 # protocol probe tools (libusb)
```

## Known issues

- fprintd 1.94.5 segfaults when logging certain driver errors (cosmetic; normal flows unaffected)
- howdy upgrades restore `pam.py` — re-apply the Python 3 patch after upgrading
- Face recognition is light-sensitive — tune `certainty` / `timeout` in howdy config

## Credits & License

- [NyxNiri](https://github.com/ech678/NyxNiri) — base desktop solution (GPL-3.0, ech678)
- [libfprint](https://gitlab.freedesktop.org/libfprint/libfprint) realtek driver — Realtek Corp. (LGPL-2.1)
- This patch: LGPL-2.1-or-later (same as libfprint)
- Documents: © 2026 [xiaomaogou66](https://github.com/xiaomaogou66), CC BY-SA 4.0

Upstream MR: *pending* — this repository hosts the patch until merged.
