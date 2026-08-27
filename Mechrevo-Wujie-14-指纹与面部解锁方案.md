# 指纹 + 面部解锁解决方案

## Mechrevo Wujie 14（机械革命·无界14）

| 文档属性 | 内容 |
|---|---|
| 文档名称 | Fingerprint & Face Unlock Solution for Mechrevo Wujie 14 |
| 版本 | v1.0 |
| 作者 / 维护者 | [xiaomaogou66](https://github.com/xiaomaogou66) |
| 适用平台 | Arch Linux / CachyOS（Wayland / Niri / Noctalia V5） |
| 目标机型 | Mechrevo Wujie 14（机械革命 无界14）及搭载相同外设的机型 |
| 基础方案 | [NyxNiri](https://github.com/ech678/NyxNiri)（GPL-3.0, ech678） |
| 文档许可 | CC BY-SA 4.0（[链接](https://creativecommons.org/licenses/by-sa/4.0/deed.zh)） |
| 文档状态 | 已在本机验证通过 |

---

## 1. 引言

Mechrevo Wujie 14 内置的 Realtek 指纹传感器（USB ID `3274:9011`）在 Linux 生态中**无官方驱动支持**。本方案通过协议逆向工程为其实现 Linux 驱动支持，并结合面部识别（howdy），为该机型提供完整的 **指纹 / 人脸 / 密码** 三通道锁屏解锁能力。

## 2. 目标机型与硬件基线

| 硬件 | 规格 | 状态 |
|---|---|---|
| 指纹传感器 | Realtek USB2.0 Finger Print Bridge（`3274:9011`，MOC 族，Match-on-Chip） | 官方不支持，本方案逆向驱动 ✅ |
| 摄像头 | UVC 设备（/dev/video0，MJPG 1080p） | 原生可用 ✅ |
| 显示屏/键盘 | 内置（指纹传感器为内置 USB 设备） | — |

**驱动能力确认**：
```bash
lsusb | grep -i finger
# Bus 003 Device 003: ID 3274:9011 Generic Realtek USB2.0 Finger Print Bridge
```

## 3. 方案概述

解锁界面为 Noctalia V5 锁屏（NyxNiri 方案内置），提供三通道认证：

```
┌──────────────────────────────┐
│        Noctalia 锁屏          │
│  ┌──────────┬──────────┬───┐ │
│  │ 🖐️ 指纹   │ 👤 人脸   │ ⌨️ │ │
│  │ 按一下秒解 │ 回车触发  │ 密码│ │
│  └──────────┴──────────┴───┘ │
└──────────────────────────────┘
```

| 通道 | 技术栈 | 特点 |
|---|---|---|
| 指纹 | noctalia D-Bus → fprintd → **补丁版 libfprint**（逆向驱动） | 最快，无密码 |
| 人脸 | 回车 → PAM(login) → howdy → OpenCV/dlib 识别 | 无接触 |
| 密码 | pam_unix | 兜底 |

## 4. 技术实现要点

### 4.1 指纹驱动逆向（核心成果）

- **协议**：Realtek MOC 私有 bulk 协议（12 字节命令头 + 数据相 + 5 字节状态相）
- **关键发现**：`3274:9011` 固件要求 **4 步 select_os 握手**（param = 0→1→2→3）后模板存储区才可访问；官方驱动仅发送一次（param=0x01，适用于 9003）
- **模板布局**：10 槽 × 35 字节；`GET_ENROLL_NUM` 返回容量而非占用数
- **出厂数据**：传感器预置模板会导致 enroll 报 duplicate，需先执行 `delete_record(0xff)` 清空

### 4.2 人脸识别（howdy）

- howdy 2.6.1 + pam-python-git（Python 3 版 PAM 模块）
- 需修复 pam.py 的 Python 2 残留（`import ConfigParser`）并放行 PAM 运行权限

### 4.3 PAM 与 noctalia 集成

- PAM 服务 `login`（noctalia 锁屏使用）→ `system-local-login` → `system-auth` 单点挂接
- noctalia `allow_empty_password = true` 使"回车 + 空密码"触发 PAM 人脸认证

## 5. 部署步骤

### 5.1 前置条件

已按 NyxNiri 完成桌面部署（niri + noctalia + greetd + fish）。

### 5.2 指纹通道

```bash
# 依赖
sudo pacman -S fprintd

# 补丁版 libfprint（含 4 步握手修复）
cd libfprint-9011
https_proxy=http://127.0.0.1:7897 makepkg -si
sudo systemd-hwdb update

# 清空传感器出厂模板（避免 duplicate 错误）
sudo pkill -x fprintd
sudo rtk-clear

# 录入指纹（提示后按手指，扫 3~4 次）
fprintd-enroll $USER
fprintd-verify $USER   # 验证
```

### 5.3 人脸通道

```bash
paru -S howdy pam-python-git

sudo cp /usr/lib/security/howdy/config.ini /etc/howdy/config.ini
sudo sed -i 's|^device_path = none|device_path = /dev/video0|' \
     /usr/lib/security/howdy/config.ini

# py3 兼容修复 + 权限
sudo python3 -c "p='/usr/lib/security/howdy/pam.py'; s=open(p).read(); \
  s=s.replace('import ConfigParser','try:\n    import configparser as ConfigParser\nexcept ImportError:\n    import ConfigParser'); \
  open(p,'w').write(s)"
sudo chmod -R a+rX /usr/lib/security/howdy

# 关闭快照（防止权限中断认证）
sudo sed -i -e 's/^capture_failed = .*/capture_failed = false/' \
            -e 's/^capture_successful = .*/capture_successful = false/' \
            /usr/lib/security/howdy/config.ini

# PAM 挂接
sudo cp /etc/pam.d/system-auth /etc/pam.d/system-auth.bak
sudo sed -i '/pam_faillock.so.*preauth/a auth sufficient pam_python3.so /usr/lib/security/howdy/pam.py' \
     /etc/pam.d/system-auth

# 录入人脸
sudo howdy add
```

### 5.4 noctalia 配置

`~/.config/noctalia/noctalia-config.toml` 追加：

```toml
[lockscreen]
allow_empty_password = true
fingerprint = true
```

热重载：`noctalia msg config-reload`

## 6. 验收标准

| # | 测试项 | 预期结果 |
|---|---|---|
| 1 | `fprintd-list $USER` | 显示已录入指纹 |
| 2 | `fprintd-verify $USER` | 手指匹配通过 |
| 3 | `Mod+L` → 手指 | 秒解锁 |
| 4 | `Mod+L` → 回车 | 摄像头识别 → 解锁 |
| 5 | `Mod+L` → 密码 | 正常解锁 |
| 6 | sudo 认证 | 摄像头触发 / 密码可用 |

## 7. 交付物清单

| 交付物 | 说明 |
|---|---|
| `realtek-9011.patch` | libfprint 补丁（设备 ID + 4 步握手 + hwdb + meson 修复） |
| `PKGBUILD` | 打包脚本（`libfprint-9011`） |
| `rtk-clear`（源码） | 传感器出厂模板清理工具 |
| `README.md` | 逆向工程记录与部署文档 |

## 8. 维护与回滚

| 场景 | 操作 |
|---|---|
| 系统升级覆盖补丁库 | `cd libfprint-9011 && makepkg -si` |
| howdy 升级后 | 重打 pam.py py3 补丁 + 权限 |
| 回滚指纹驱动 | `sudo pacman -S libfprint` |
| 出厂重置 | 恢复 system-auth.bak；删除 noctalia 配置段 |

## 9. 已知问题与风险

1. **fprintd 1.94.5 缺陷**：驱动返回协议错误时，fprintd 日志打印路径存在段错误（不影响正常认证流程）
2. **howdy 升级还原 pam.py**：升级后需重打补丁
3. **人脸光照敏感**：弱光/逆光可能超时，可通过 `certainty`、`timeout` 调整
4. **逆向驱动适用范围**：补丁以 PID 区分握手逻辑，仅影响 9011；其他 Realtek MOC 型号不受影响

## 10. 来源与许可

- **基础桌面方案**：[NyxNiri](https://github.com/ech678/NyxNiri)（GPL-3.0，作者 ech678）
- **驱动基础**：libfprint realtek 驱动（Realtek Corp., LGPL-2.1）
- **本方案逆向成果**：3274:9011 驱动补丁（LGPL-2.1 兼容，随 libfprint 许可发布）
- **本文档**：© 2026 [xiaomaogou66](https://github.com/xiaomaogou66)，以 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/deed.zh) 许可发布

## 附录 A：故障排查

| 现象 | 排查 |
|---|---|
| `fprintd-enroll` 报 unknown-error | 检查是否已清空出厂模板；查看 `journalctl -u fprintd` |
| 锁屏回车无反应 | 确认 noctalia `allow_empty_password` 已生效（config-reload） |
| 摄像头不亮 | 确认 `device_path` 与用户视频组权限 |
| 指纹无提示 | 确认补丁库已安装（`pacman -Q libfprint` 应为 -2） |

---
*文档结束*
