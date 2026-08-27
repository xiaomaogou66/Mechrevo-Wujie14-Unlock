<div align="right">
  <a href="README.md">English</a> | <strong>简体中文</strong>
</div>

# 机械革命 无界 14 2026 指纹与面部解锁

面向 **Realtek USB2.0 Finger Print Bridge（USB ID `3274:9011`）** 的 Linux（Arch / CachyOS，Wayland）解锁方案——该传感器**无官方 libfprint 驱动**，本方案通过逆向工程实现驱动支持，并集成 howdy 面部解锁。

![status](https://img.shields.io/badge/status-working-green) ![license](https://img.shields.io/badge/license-LGPL--2.1-orange)
![release](https://img.shields.io/github/v/release/xiaomaogou66/Mechrevo-Wujie14-Unlock)

> 桌面基础方案：[NyxNiri](https://github.com/ech678/NyxNiri)（GPL-3.0）——Arch/CachyOS 上的 Niri + Noctalia V5。

## 功能

| 通道 | 实现方式 |
|---|---|
| 🖐️ 指纹 | Noctalia 锁屏 → fprintd D-Bus → **补丁版 libfprint**（逆向驱动） |
| 👤 人脸 | 锁屏按回车 → PAM（login 栈）→ howdy → OpenCV/dlib |
| ⌨️ 密码 | pam_unix 兜底 |

## 硬件

- 传感器：`3274:9011` Realtek USB2.0 Finger Print Bridge（Match-on-Chip，内置 USB）
- 摄像头：UVC（`/dev/video0`，MJPG 1080p）
- 已验证机型：Mechrevo Wujie 14（机械革命 无界14），CachyOS + niri + Noctalia V5

检测你的传感器：

```bash
lsusb | grep -i finger   # 应显示 "3274:9011 Generic Realtek USB2.0 Finger Print Bridge"
```

## 逆向工程核心发现

传感器属于 Realtek MOC（Match-on-Chip）家族，但固件与已支持的 `3274:9003` 存在差异：

- **`3274:9011` 固件要求 4 步 `select_os` 握手（param = 0→1→2→3），模板存储区才可访问。**
  官方驱动只发送一次（param=0x01，适用于 9003）；缺少握手时 `GET_TEMPLATE` 被固件拒绝（状态 `0x01`）。
- 补丁以 PID 区分握手逻辑，不影响其他 Realtek MOC 型号。
- 传感器出厂/前任用户预置模板 → 需先清空（`delete_record(0xff)`），否则录入报 `duplicate`。

## 安装

### 1. 指纹驱动（补丁版 libfprint）

```bash
sudo pacman -S fprintd
git clone https://github.com/xiaomaogou66/Mechrevo-Wujie14-Unlock
cd Mechrevo-Wujie14-Unlock
makepkg -si          # 构建 libfprint-9011（与官方 libfprint 冲突，会替换）
sudo systemd-hwdb update
```

### 2. 清空出厂模板并录入

```bash
sudo pkill -x fprintd
cd tools && make && sudo ./rtk-clear     # 清空传感器模板存储
fprintd-enroll $USER                     # 提示后按手指，扫 3~4 次
fprintd-verify $USER
```

### 3. 面部解锁（howdy）

```bash
paru -S howdy pam-python-git
sudo cp /usr/lib/security/howdy/config.ini /etc/howdy/config.ini
sudo sed -i 's|^device_path = none|device_path = /dev/video0|' /usr/lib/security/howdy/config.ini

# pam.py 的 Python 3 兼容补丁（2.6.1 自带的是 Python 2 import）
sudo python3 -c "p='/usr/lib/security/howdy/pam.py'; s=open(p).read(); s=s.replace('import ConfigParser','try:\n    import configparser as ConfigParser\nexcept ImportError:\n    import ConfigParser'); open(p,'w').write(s)"
sudo chmod -R a+rX /usr/lib/security/howdy   # 锁屏 PAM 以普通用户运行，必须可读

sudo sed -i -e 's/^capture_failed = .*/capture_failed = false/' \
            -e 's/^capture_successful = .*/capture_successful = false/' \
            /usr/lib/security/howdy/config.ini

# PAM 挂接（覆盖锁屏 / 登录 / sudo）
sudo cp /etc/pam.d/system-auth /etc/pam.d/system-auth.bak
sudo sed -i '/pam_faillock.so.*preauth/a auth sufficient pam_python3.so /usr/lib/security/howdy/pam.py' /etc/pam.d/system-auth

sudo howdy add    # 对着摄像头
```

### 4. Noctalia 锁屏配置

`~/.config/noctalia/noctalia-config.toml`：

```toml
[lockscreen]
allow_empty_password = true   # 回车 + 空密码触发 PAM（人脸认证）
fingerprint = true            # 原生指纹解锁
```

热重载：`noctalia msg config-reload`

## 使用（锁屏时）

- **手指放传感器** → 秒解锁
- **按回车** → 摄像头人脸识别 → 解锁
- **输入密码** → 兜底

## 验收

| 测试 | 预期 |
|---|---|
| `fprintd-list $USER` | 显示已录入指纹 |
| `fprintd-verify $USER` | 匹配通过 |
| `Mod+L` → 手指 | 解锁 |
| `Mod+L` → 回车 | 摄像头识别 → 解锁 |

## 仓库结构

```
├── realtek-9011.patch                        # libfprint 补丁（设备 ID + 4 步握手 + hwdb + meson 修复）
├── PKGBUILD                                  # 打包脚本（libfprint-9011）
├── README.md                                 # 本文件（英文）
├── README.zh-CN.md                           # 本文件（中文）
├── Mechrevo-Wujie-14-指纹与面部解锁方案.md   # 完整方案文档（中文，CC BY-SA 4.0）
├── Mechrevo-Wujie14-Fingerprint-Face-Unlock.md  # 完整方案文档（English，CC BY-SA 4.0）
└── tools/
    ├── rtk-clear.c                           # 清空出厂模板
    └── rtk-test*.c                           # 协议探测工具（libusb）
```

## 已知问题

- fprintd 1.94.5 在记录某些驱动错误时会段错误（仅影响错误日志路径，正常流程不受影响）
- howdy 升级会还原 `pam.py`——升级后需重新打 Python 3 补丁
- 人脸识别对光照敏感——可调整 howdy 的 `certainty` / `timeout`

## 致谢与许可

- [NyxNiri](https://github.com/ech678/NyxNiri) — 基础桌面方案（GPL-3.0，ech678）
- [libfprint](https://gitlab.freedesktop.org/libfprint/libfprint) realtek 驱动 — Realtek Corp.（LGPL-2.1）
- 本补丁：LGPL-2.1-or-later（与 libfprint 一致）
- 文档：© 2026 [xiaomaogou66](https://github.com/xiaomaogou66)，CC BY-SA 4.0

上游合并进展：*待定* —— 本仓库在合并前承载该补丁。
