<div align="center">
  <img src="branding/chatou-logo.png" width="112" alt="Chatou 插头图标">
  <h1>Chatou 插头</h1>
  <p><strong>把你最熟悉的中文输入，接到任何语言与语气。</strong></p>
  <p>Windows 系统级 AI 表达输入法 · 输入中文 · 按 F8 · 原地变成你想说的话</p>

  [![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-2563EB?style=flat-square&logo=windows11&logoColor=white)](https://github.com/gjz1023984937-dev/chatou/releases/latest)
  [![Release](https://img.shields.io/github/v/release/gjz1023984937-dev/chatou?style=flat-square&color=7C3AED)](https://github.com/gjz1023984937-dev/chatou/releases/latest)
  [![License](https://img.shields.io/badge/License-MIT-10B981?style=flat-square)](LICENSE)

  **[下载最新版](https://github.com/gjz1023984937-dev/chatou/releases/latest)** · [隐私说明](PRIVACY.md) · [安全说明](SECURITY.md) · [English](README_EN.md)
</div>

---

> [!IMPORTANT]
> 当前发行版仅支持 **Windows 10/11 x64**。Linux、macOS 与 iOS/iPadOS 尚不能直接安装；详细计划见[平台支持](#平台支持)。

## 你照常打字，Chatou 负责换一种说法

比如你在微信里输入：

> 我们明天下午三点开会，记得把最新方案带上。

选中「商务英语」后按一下 `F8`：

> We’ll meet at 3 PM tomorrow. Please remember to bring the latest proposal.

换成「互联网黑话」，同一句话会变成：

> 明天下午三点拉齐进度，记得带上最新版方案，现场把关键链路对齐并形成闭环。

这里没有复制、切窗口、粘贴三连。文字直接留在你原本正在使用的输入框里。

<div align="center">
  <img src="docs/assets/chatou-settings-lang.png" width="860" alt="Chatou 翻译风格设置界面">
</div>

## 为什么叫“插头”

Chatou 不替代你的表达习惯，它只是给输入法多接上一层能力。

| 你已经会的 | 插上 Chatou 之后 |
|---|---|
| 用拼音快速组织中文 | 原地转换为英文、日语、粤语等语言 |
| 写出意思，但拿不准措辞 | 改成商务、学术、客服、社交媒体等口吻 |
| 在不同软件里反复复制粘贴 | 在当前输入框按 `F8` 完成转换 |

`Enter` 不属于 Chatou。它始终保留给当前软件，用来发送消息或换行。

## 平台支持

| 平台 | 当前状态 | 说明 |
|---|---|---|
| Windows 10/11 x64 | ✅ 可用 | 当前 Release 提供安装器，接入 Windows TSF 与 Weasel |
| Linux | 🧭 计划中 | 需要适配 `fcitx5-rime` 或 `ibus-rime`，Windows 安装包不能直接运行 |
| macOS | 🧭 计划中 | 需要适配 macOS 输入法前端并重做安装、设置与文本回填 |
| iOS / iPadOS | 🔬 调研中 | 需要独立键盘扩展；受系统权限限制，交互不会与 Windows 版完全相同 |

Chatou 的提示词、语言与模型调用逻辑可以复用，但当前实现中的 TSF、WinHTTP、注册表、快捷键、剪贴板回填和设置程序均依赖 Windows，因此跨平台不是重新打包即可完成。

## 它能在哪里工作

Chatou 接入 Windows 的文本服务框架，不依赖浏览器扩展。

- 浏览器：Chrome、Edge、Firefox 的网页输入框和搜索框
- 办公：Word、PowerPoint、记事本、VS Code
- 沟通：微信、飞书、钉钉、Slack、Discord
- 其他支持 Windows 输入法的常规文本输入区域

密码框、安全桌面、部分游戏和自行接管键盘的程序可能会阻止输入法或 `F8`，这属于 Windows/宿主程序的安全边界。

## 三十秒上手

1. 从 [Releases](https://github.com/gjz1023984937-dev/chatou/releases/latest) 下载 `Chatou-Installer`。
2. 运行安装器，选择安装目录并点击「安装 Chatou」。
3. 安装完成后填写自己的模型 API Key。
4. 按 `Win + Space` 切换到「Chatou 插头」。
5. 输入拼音、空格选出中文，再按 `F8`。

如果 Windows SmartScreen 提示未知发布者，可先核对 Release 页面提供的文件摘要，再选择是否运行。目前安装包尚未进行商业代码签名。

## 卸载也要清楚

在 Windows「已安装的应用」中选择 Chatou 即可卸载。默认会清除程序、输入法注册、缓存、用户字典和模型配置；如果准备以后重新安装，可以勾选「保留个人数据」。

Word、浏览器或系统输入服务可能正在使用输入法 DLL。遇到这种情况，卸载器会自动安排重启删除，并明确提示重启；重启后不需要再手工寻找残留文件。

<div align="center">
  <img src="docs/assets/chatou-installer.png" width="680" alt="Chatou 安装界面">
</div>

## 不是只有“中译英”

风格框可以直接写自然语言，不需要学习指令格式。

```text
简洁专业的英文邮件
像长期在伦敦工作的产品经理一样表达
商务日语，但不要过度敬语
适合小红书的自然口吻
严谨、克制的论文摘要风格
港片里的谈判对白
```

内置风格只是快捷入口。只要模型能理解，你就可以自己描述。

## 模型由你决定

Chatou 使用 OpenAI Chat Completions 兼容接口，预设支持：

- DeepSeek
- Moonshot
- OpenAI
- Ollama
- 自定义兼容服务

每个服务商可以保存独立 API Key，切换模型时不会相互覆盖。

<div align="center">
  <img src="docs/assets/chatou-settings-model.png" width="860" alt="Chatou 模型服务设置界面">
</div>

### 数据去向

Chatou 没有自己的中转服务器。触发转换时，当前待处理文本会直接发送到你配置的模型地址；API Key 和偏好保存在本机 `%APPDATA%\Rime` 下。

如果使用 Ollama 等本地兼容服务，文本可以只在本机模型中处理。具体数据政策仍以你选择的模型服务商为准。

为帮助排查转换问题，当前版本会在本机写入轮转诊断日志，其中可能包含截断后的输入和模型响应；Chatou 不会自动上传这些日志。API Key 也保存在本机配置文件中，并未使用系统凭据保险库加密。请不要在共享 Windows 账户中保存敏感密钥，完整说明和清理方法见 [PRIVACY.md](PRIVACY.md)。

## 一次按键背后发生了什么

```text
拼音输入
   ↓
Rime 生成中文并在本地记录待转换文本
   ↓
F8 被 Windows TSF 路由到 Chatou
   ↓
Chatou 调用你选择的模型
   ↓
译文或改写结果回到原来的输入框
```

网络请求在后台执行，输入法界面不会因为等待模型响应而整体卡死。

## 快捷键与状态

| 操作 | 结果 |
|---|---|
| `F8` | 翻译或改写最近输入的中文 |
| `Enter` | 交给当前应用发送或换行 |
| `Backspace` | 正常删除，同时同步待转换文本 |
| `Esc` | 清除本次待转换记录 |
| 纯净模式 | 暂时停用 AI，只作为普通中文输入法 |

## 从源码构建

### 需要准备

- Windows 10/11 x64
- Visual Studio C++ Desktop workload（包含 ATL）
- xmake
- Boost 1.84
- WTL
- 已构建的 librime

Boost 与 WTL 可以放在仓库同级的 `dependencies` 目录，也可以通过 `BOOST_ROOT`、`WTL_ROOT` 指定。

```powershell
git clone https://github.com/gjz1023984937-dev/chatou.git
cd chatou

# 先构建 third_party/weasel/librime，生成 dist/lib/rime.dll 与 rime.lib
./_build_weasel_xmake.ps1

cd tools/ta-settings
./_build.ps1

cd ../ta-installer
./_build.ps1
```

生成位置：

```text
third_party/weasel/output/                         输入法核心组件
tools/ta-settings/build/windows/x64/release/       设置程序
tools/ta-installer/build/windows/x64/release/      单文件安装器
```

内部仍保留 `typeanything.schema.yaml`、`typeanything_lang.txt` 等兼容名称。这是为了让原 TypeAnything 用户升级后继续使用已有 API Key、风格和词库，并不代表界面品牌未完成迁移。

## 项目结构

```text
branding/                           Chatou 品牌资源
docs/assets/                        README 界面截图
tools/ta-settings/                  风格与模型设置界面
tools/ta-installer/                 单文件安装器
third_party/weasel/                 Windows TSF 输入法框架
  └─ librime/plugins/typeanything/  AI 转换处理器与 Rime 方案
```

## 路线图

- 可自定义全局触发键
- 本地模型的一键发现与连接测试
- 翻译失败时更清晰的非侵入提示
- 默认隐藏或脱敏本地诊断日志中的输入与模型响应
- 使用 Windows 凭据保险库保存 API Key
- Windows 安装包代码签名
- Linux 输入法适配（优先评估 `fcitx5-rime`）
- macOS 输入法适配
- iOS / iPadOS 键盘扩展可行性验证

欢迎提交 Issue 和 Pull Request，参与方式见 [CONTRIBUTING.md](CONTRIBUTING.md)。与输入框兼容性有关的问题，请附上 Windows 版本、应用名称和复现步骤；安全问题请按 [SECURITY.md](SECURITY.md) 私密报告。

## 来源与许可证

Chatou 使用 [Weasel](https://github.com/rime/weasel) 与 [librime](https://github.com/rime/librime) 提供 Windows 输入法和 Rime 引擎能力。

Chatou 新增代码按 [MIT License](LICENSE) 发布。`third_party/weasel/` 中的上游代码继续遵守其 GPL v3 / BSD-3-Clause 等原始许可证与署名要求。
