# Changelog

本项目所有重要变更都会记录在此文件。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

## [v0.0.0] - 2026-08-09

首个 Vatrix 版本，基于 PvZ-Portable（上游 commit `7c11939`）的独立改版。

### 新增
- 启动直达僵王关（Level 0-0）
- 新增 SpawnLogic 出怪决策层，僵王按场上局势智能出怪
- 僵王召唤引入点数机制，限制单轮出怪强度
- 僵王召唤出怪算法更新
- 僵王AI目标选择智能化为伤害最大化策略
- 调整扔房车的策略
- 调整僵王召唤点数经济与出怪成本
- 僵王召唤战术系统与0-0关丢车概率调整
- 僵王砸车飞贼固定节奏与传送带辣椒机制调整
- 开场戴夫对话
- 0-0 通关奖励改为纸条并支持自定义图片
- 日志输出始终产生
- 加上调试信息
- 加上作弊快捷键
- 僵王战小丑低血量自动爆炸与战术调整

### 变更
- 项目更名 Vatrix：窗口标题、可执行文件名、Android/iOS/WASM 显示名统一，窗口标题带版本号（Vatrix v0.0.0）
- 版本号固定为 0.0.0（CMake / Android build.gradle / PKGBUILD 三处手动维护），不再由 git describe 推导
- 存档与日志改为写入 exe 所在目录（userdata/），不再使用 %APPDATA%；-savedir 仍可覆盖
- mProdName 改为 vatrix，移除 partner.xml 对产品名的覆盖
- 上游代码改动处以 // vx: 标注，便于区分

### 修复
- 调整点数和吐球逻辑
- 僵王召唤点数经济与战术再调整
- 调整僵王算法
- 点数调整
