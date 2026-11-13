<div align="center">
  <img src="branding/chatou-logo.png" width="112" alt="Chatou logo">
  <h1>Chatou</h1>
  <p><strong>Keep typing in Chinese. Express it in any language or tone.</strong></p>
  <p>A system-wide AI input method for Windows — type Chinese, press F8, and transform the text in place.</p>

  [![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-2563EB?style=flat-square&logo=windows11&logoColor=white)](https://github.com/gjz1023984937-dev/chatou/releases/latest)
  [![Release](https://img.shields.io/github/v/release/gjz1023984937-dev/chatou?style=flat-square&color=7C3AED)](https://github.com/gjz1023984937-dev/chatou/releases/latest)
  [![License](https://img.shields.io/badge/License-MIT-10B981?style=flat-square)](LICENSE)

  **[Download the latest release](https://github.com/gjz1023984937-dev/chatou/releases/latest)** · [中文](README.md) · [Privacy](PRIVACY.md) · [Security](SECURITY.md)
</div>

---

> [!IMPORTANT]
> The current release supports **Windows 10/11 x64 only**. Linux, macOS, iOS, and iPadOS are not supported yet.

## Type normally. Transform in place.

Type this in WeChat, Word, a browser, or another regular Windows text field:

> 我们明天下午三点开会，记得把最新方案带上。

Choose “Business English” and press `F8`:

> We’ll meet at 3 PM tomorrow. Please remember to bring the latest proposal.

There is no copy-switch-paste loop. Chatou replaces the text in the field you are already using.

<div align="center">
  <img src="docs/assets/chatou-settings-lang.png" width="860" alt="Chatou language and style settings">
</div>

## Quick start

1. Download `Chatou-Installer` from the [latest release](https://github.com/gjz1023984937-dev/chatou/releases/latest).
2. Run the installer and select “Install Chatou”.
3. Enter your own model API key after installation.
4. Press `Win + Space` and select Chatou.
5. Type Chinese, commit the text with Space, then press `F8`.

The installer is not commercially code-signed yet, so Windows SmartScreen may show an unknown-publisher warning. Verify the SHA-256 digest shown on the Release page before running it.

## Model providers

Chatou uses OpenAI Chat Completions-compatible endpoints and includes presets for:

- DeepSeek
- Moonshot
- OpenAI
- Ollama
- Custom compatible services

Chatou does not operate a relay server. When you press `F8`, the pending text is sent directly to the model endpoint you configured. With a local service such as Ollama, processing can remain on your machine.

The current version stores API keys in local configuration files without OS credential-vault encryption. Local diagnostic logs may contain truncated input and model responses. Read [PRIVACY.md](PRIVACY.md) before using Chatou with sensitive content.

## Platform status

| Platform | Status | Notes |
|---|---|---|
| Windows 10/11 x64 | Available | Uses Windows TSF and Weasel |
| Linux | Planned | Requires a port to `fcitx5-rime` or `ibus-rime` |
| macOS | Planned | Requires a macOS input-method frontend and new installer/settings integration |
| iOS / iPadOS | Research | Requires a separate keyboard extension and cannot fully reproduce the Windows F8 workflow |

The prompt, language, and model-call logic can be reused. The current TSF integration, WinHTTP client, registry handling, hotkey flow, text-edit transactions, installer, and settings app are Windows-specific.

After an AI transformation, press `Ctrl + Z` immediately to restore the original text. If no valid Chatou transformation is available, the shortcut is passed through to the current application unchanged.

## Build from source

Requirements:

- Windows 10/11 x64
- Visual Studio C++ Desktop workload with ATL
- xmake
- Boost 1.84
- WTL
- a built copy of librime

```powershell
git clone https://github.com/gjz1023984937-dev/chatou.git
cd chatou
./_build_weasel_xmake.ps1

cd tools/ta-settings
./_build.ps1

cd ../ta-installer
./_build.ps1
```

See [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. Report vulnerabilities privately according to [SECURITY.md](SECURITY.md).

## License and upstream projects

Chatou continues the work of [TypeAnything](https://github.com/A-cat-with-carrots/TypeAnything) and uses [Weasel](https://github.com/rime/weasel) and [librime](https://github.com/rime/librime). New Chatou code is released under the [MIT License](LICENSE); vendored upstream code retains its original licenses.
