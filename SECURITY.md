# Security Policy

## Supported versions

Chatou 目前处于早期开发阶段。安全修复仅保证进入最新 Release；发现问题后请先升级到最新版再复现。

## Reporting a vulnerability

请不要为未修复的漏洞创建公开 Issue，也不要在公开日志中粘贴 API Key、私人文字或完整接口响应。

请使用仓库的 **Security → Report a vulnerability** 私密提交以下信息：

- 受影响的 Chatou 版本和 Windows 版本；
- 问题类型及潜在影响；
- 最小复现步骤或概念验证；
- 相关日志中已经脱敏的必要片段。

维护者会尽快确认报告。在修复发布前，请避免公开可直接利用的细节。

## Sensitive local data

API Key、用户偏好及诊断日志保存在用户本机。当前存储与日志行为详见 [PRIVACY.md](PRIVACY.md)。
