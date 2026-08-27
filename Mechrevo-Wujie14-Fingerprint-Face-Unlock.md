# Fingerprint & Face Unlock Solution

## Mechrevo Wujie 14 (机械革命·无界14)

| Document | Detail |
|---|---|
| Title | Fingerprint & Face Unlock Solution for Mechrevo Wujie 14 |
| Version | v1.0 |
| Author / Maintainer | [xiaomaogou66](https://github.com/xiaomaogou66) |
| Platform | Arch Linux / CachyOS (Wayland / Niri / Noctalia V5) |
| Target hardware | Mechrevo Wujie 14 and laptops with the same peripherals |
| Base solution | [NyxNiri](https://github.com/ech678/NyxNiri) (GPL-3.0, by ech678) |
| Document license | CC BY-SA 4.0 ([link](https://creativecommons.org/licenses/by-sa/4.0/)) |
| Status | Verified on this machine |

---

## 1. Introduction

The Mechrevo Wujie 14 ships with a Realtek fingerprint sensor (USB ID `3274:9011`) that has **no official Linux driver support**. This solution provides Linux driver support for it via protocol reverse engineering, and combines it with face recognition (howdy) to deliver a complete **fingerprint / face / password** three-channel unlock experience.

## 2. Target hardware baseline

| Hardware | Specification | Status |
|---|---|---|
| Fingerprint sensor | Realtek USB2.0 Finger Print Bridge (`3274:9011`, MOC family, Match-on-Chip) | No official support — reverse-engineered here ✅ |
| Camera | UVC device (/dev/video0, MJPG 1080p) | Native ✅ |
| Form factor | Built-in (sensor is an internal USB device) | — |

**Verify the sensor**:

```bash
lsusb | grep -i finger
# Bus 003 Device 003: ID 3274:9011 Generic Realtek USB2.0 Finger Print Bridge
```

## 3. Solution overview

The lock screen is Noctalia V5 (from the NyxNiri base), offering three authentication channels:

```
┌──────────────────────────────┐
│        Noctalia lock screen   │
│  ┌──────────┬──────────┬───┐ │
│  │ 🖐️ Fp     │ 👤 Face   │ ⌨️ │ │
│  │ one-touch │ press Ent │pwd│ │
│  └──────────┴──────────┴───┘ │
└──────────────────────────────┘
```

| Channel | Stack | Notes |
|---|---|---|
| Fingerprint | Noctalia D-Bus → fprintd → **patched libfprint** (reverse-engineered) | Fastest, passwordless |
| Face | Enter → PAM (`login`) → howdy → OpenCV/dlib | Contactless |
| Password | pam_unix | Fallback |

## 4. Technical highlights

### 4.1 Fingerprint driver reverse engineering (core work)

- **Protocol**: Realtek MOC proprietary bulk protocol (12-byte command header + data phase + 5-byte status phase)
- **Key finding**: the `3274:9011` firmware requires a **4-step `select_os` handshake** (param = 0→1→2→3) before the template store becomes accessible; the official driver sends it once (param=0x01, correct for 9003)
- **Template layout**: 10 slots × 35 bytes; `GET_ENROLL_NUM` returns capacity, not occupancy
- **Factory data**: pre-stored templates cause `duplicate` errors during enrollment — clear first with `delete_record(0xff)`

### 4.2 Face recognition (howdy)

- howdy 2.6.1 + pam-python-git (Python 3 PAM module)
- Requires patching pam.py's Python 2 leftover (`import ConfigParser`) and relaxing PAM runtime permissions

### 4.3 PAM & Noctalia integration

- PAM service `login` (used by the Noctalia lock screen) → `system-local-login` → single hook in `system-auth`
- Noctalia `allow_empty_password = true` makes "Enter + empty password" trigger PAM face authentication

## 5. Deployment steps

### 5.1 Prerequisites

NyxNiri desktop deployment (niri + noctalia + greetd + fish) in place.

### 5.2 Fingerprint channel

```bash
# Dependency
sudo pacman -S fprintd

# Patched libfprint (4-step handshake fix)
cd libfprint-9011
https_proxy=http://127.0.0.1:7897 makepkg -si
sudo systemd-hwdb update

# Clear factory templates (avoids "duplicate" errors)
sudo pkill -x fprintd
sudo rtk-clear

# Enroll (press finger when prompted, 3-4 scans)
fprintd-enroll $USER
fprintd-verify $USER
```

### 5.3 Face channel

```bash
paru -S howdy pam-python-git

sudo cp /usr/lib/security/howdy/config.ini /etc/howdy/config.ini
sudo sed -i 's|^device_path = none|device_path = /dev/video0|' \
     /usr/lib/security/howdy/config.ini

# Python 3 compatibility fix + permissions
sudo python3 -c "p='/usr/lib/security/howdy/pam.py'; s=open(p).read(); \
  s=s.replace('import ConfigParser','try:\n    import configparser as ConfigParser\nexcept ImportError:\n    import ConfigParser'); \
  open(p,'w').write(s)"
sudo chmod -R a+rX /usr/lib/security/howdy

# Disable snapshots (avoid auth being interrupted by permission errors)
sudo sed -i -e 's/^capture_failed = .*/capture_failed = false/' \
            -e 's/^capture_successful = .*/capture_successful = false/' \
            /usr/lib/security/howdy/config.ini

# PAM hook
sudo cp /etc/pam.d/system-auth /etc/pam.d/system-auth.bak
sudo sed -i '/pam_faillock.so.*preauth/a auth sufficient pam_python3.so /usr/lib/security/howdy/pam.py' \
     /etc/pam.d/system-auth

# Enroll face
sudo howdy add
```

### 5.4 Noctalia configuration

Append to `~/.config/noctalia/noctalia-config.toml`:

```toml
[lockscreen]
allow_empty_password = true
fingerprint = true
```

Hot reload: `noctalia msg config-reload`

## 6. Acceptance criteria

| # | Test | Expected |
|---|---|---|
| 1 | `fprintd-list $USER` | fingerprint listed |
| 2 | `fprintd-verify $USER` | match |
| 3 | `Mod+L` → finger | instant unlock |
| 4 | `Mod+L` → Enter | camera recognition → unlock |
| 5 | `Mod+L` → password | unlock |
| 6 | sudo auth | camera trigger / password usable |

## 7. Deliverables

| Artifact | Description |
|---|---|
| `realtek-9011.patch` | libfprint patch (device ID + 4-step handshake + hwdb + meson fix) |
| `PKGBUILD` | package script (`libfprint-9011`) |
| `rtk-clear` (source) | factory template clearing tool |
| `README.md` | reverse engineering record & deployment guide |

## 8. Maintenance & rollback

| Scenario | Action |
|---|---|
| Upgrade overwrote the patch | `cd libfprint-9011 && makepkg -si` |
| howdy upgraded | re-apply pam.py py3 patch + permissions |
| Rollback fingerprint driver | `sudo pacman -S libfprint` |
| Factory reset | restore system-auth.bak; remove the noctalia section |

## 9. Known issues & risks

1. **fprintd 1.94.5 bug**: segfault in the logging path when the driver reports a protocol error (does not affect normal flows)
2. **howdy upgrades revert pam.py**: re-apply the patch after upgrading
3. **Face light sensitivity**: weak/back lighting may time out — tune `certainty` / `timeout`
4. **Patch scope**: the handshake is keyed on PID, affecting only 9011; other Realtek MOC models are unaffected

## 10. Sources & license

- **Base desktop solution**: [NyxNiri](https://github.com/ech678/NyxNiri) (GPL-3.0, by ech678)
- **Driver base**: libfprint realtek driver (Realtek Corp., LGPL-2.1)
- **This reverse engineering work**: 3274:9011 driver patch (LGPL-2.1 compatible, released under libfprint's license)
- **This document**: © 2026 [xiaomaogou66](https://github.com/xiaomaogou66), licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)

## Appendix A: Troubleshooting

| Symptom | Check |
|---|---|
| `fprintd-enroll` unknown-error | factory templates cleared? check `journalctl -u fprintd` |
| Enter does nothing on lock screen | is noctalia `allow_empty_password` active (config-reload)? |
| Camera doesn't light up | check `device_path` and user video group membership |
| No fingerprint prompt | patched lib installed? (`pacman -Q libfprint` should be -2) |

---
*End of document*
