> [!IMPORTANT]
> **エディションは 2 つ。** **[Enterprise](https://fincept.in/enterprise)** はファンドやリサーチデスク向けのプライベート（クローズドソース）ビルドです。41 モジュール、独自データ、ライブ・ブローカールーティング、SSO を備え、**1 ユーザー月額 99 ドル**から。**このリポジトリ**は学習・学術利用向けの無償 AGPL-3.0 版で、月 1 回リリースされます。
> [比較](https://fincept.in/comparison) · [料金](https://fincept.in/pricing)

# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)

### **限界はあなたの思考だけ。データではない。**

機関投資家水準の金融分析、AI による自動化、無制限のデータ接続を備えた最先端の金融インテリジェンス・プラットフォーム。

[📥 ダウンロード](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [🏢 Enterprise](https://fincept.in/enterprise) · [💳 料金](https://fincept.in/pricing) · [📖 マニュアル](https://fincept.in/manual) · [💬 Discord](https://discord.gg/ae87a8ygbN)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/FinceptBanner.png)

</div>

---

## 概要

**Fincept Terminal** は金融リサーチのためのネイティブ C++20 デスクトップ・ターミナルです。UI は Qt6、分析は組み込み Python 3.11、単一バイナリで動作し、Electron は使用しません。

共通のデータコア上に 2 つのエディションがあります。**[Enterprise](https://fincept.in/enterprise)** はチームが日々開発しているプライベート（クローズドソース）ビルドで、ファンド、ファミリーオフィス、リサーチデスク向けです。**このリポジトリ**は無償の AGPL-3.0 版で、学習・個人利用・学術研究向けに月 1 回リリースされます。

学生・趣味・研究者ならオープン版を。企業である場合、あるいはターミナルが収益の手段である場合は Enterprise を選んでください。AGPL のコピーレフトが適用されず、オープン版の実質的なコストは自前のデータ費と LLM 費（トークン単位・上限なしで課金）だからです。

| | オープンソース | **Enterprise** |
|---|---|---|
| **ライセンス** | AGPL-3.0 — 強いコピーレフト | プロプライエタリ — コピーレフト義務なし |
| **費用** | 無償、ただしデータ費と LLM 費は自己負担 | 1 ユーザー月額 99／199／299 ドル |
| **データ** | 無償の公開フィード、キーは自前 | 独自データセット、長い履歴、ポイント・イン・タイム |
| **AI** | 自前の LLM キー | 400〜5,000 クレジット込み · マルチエージェント・リサーチ · プライベート・データルーム |
| **取引** | ペーパートレード + ブローカー連携 | ライブ・ブローカールーティング + ライブ・アルゴ配備 |
| **統制** | — | SSO/SAML、監査ログ、RBAC、SLA に基づくサポート |

[**Enterprise を見る →**](https://fincept.in/enterprise) · [詳細比較](https://fincept.in/comparison) · [料金](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## Enterprise

6 つのデスクにまたがる 41 モジュール — エージェンティック・リサーチ、クオンツラボとバックテスト、詳細なファンダメンタル分析、マーケットと執行、マクロとグローバル・インテリジェンス、そして自分専用のワークスペース。すべて [700 ページのマニュアル](https://fincept.in/manual)に収録されています。

| | **Exclusive** | **Exclusive+** ★ | **Exclusive Pro** |
|---|---|---|---|
| | **月額 99 ドル**／ユーザー | **月額 199 ドル**／ユーザー | **月額 299 ドル**／ユーザー |
| AI クレジット／月 | 400 | 2,000 | 5,000 |
| ディープリサーチ + エージェントチーム | — | ✓ | ✓ |
| ライブ取引 + アルゴ | — | — | ✓ |

月次請求、縛りなし、最低シート数なし、四半期払いで 15% 割引 — 年間で **1 ユーザーあたり 1,188〜3,588 ドル**、Bloomberg の 1 シート約 27,000 ドルに対しての価格です。**大学向け:** Exclusive Pro 5 シートを **月額 699 ドル**。これが料金表のすべてで、個別交渉価格も、別売りの商用ライセンスもありません。

Enterprise には専用アカウントが必要です。無償の Fincept アカウントではログインできません。

[**アカウント作成**](https://fincept.in/enterprise/signup) · [**デモを予約**](https://calendly.com/nikultilak/fincept-terminal-demo)

---

## インストール

**Windows x64**、**Linux x64**（`.run` / `.deb` / `.rpm`）、**macOS（Apple シリコン）**向けのインストーラーは[リリースページ](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)にあります。

**ソースからのビルド** — Linux/macOS: `git clone … && ./setup.sh`。Windows、手動ビルド、固定されたツールチェーン（**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**）、トラブルシューティングは **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)** を参照してください。バージョンは固定です。これより新しい／古いものはサポートされません。

> Enterprise ビルドをお探しですか。Windows、macOS、Linux 向けの署名済みインストーラーが Enterprise ログインの内側にあります — [こちらから入手](https://fincept.in/enterprise)。

---

## オープン版の内容

- **分析** — DCF、ポートフォリオ最適化、VaR／シャープ、デリバティブ・プライシング、債券、オルタナティブ、加えて 18 モジュールの QuantLib スイート
- **AI** — トレーダー／投資家、経済、地政学にわたる 37 エージェント。キーは自前（OpenAI、Anthropic、Gemini、Groq、DeepSeek、OpenRouter、Ollama）
- **データ** — 100 以上のコネクタ: FRED、IMF、世界銀行、DBnomics、AkShare、Polygon、Kraken、Yahoo Finance、政府 API
- **取引** — 暗号資産・株式フィード、ペーパートレード・エンジン、16 のブローカー連携
- **自動化** — ビジュアル・ノードエディタ、MCP ツール、AI Quant Lab（機械学習、ファクター探索、強化学習）
- **グローバル・インテリジェンス** — 海上輸送トラッキング、地政学分析、関係マッピング

ネイティブ C++20 · Qt6 · 組み込み Python 3.11 · 単一バイナリ · Node.js もブラウザランタイムも不要。

---

## 本リポジトリの保守方針

本リポジトリは**公開のまま維持され、削除されません**。すでにリリースされたものはリリースされたままです。

継続的な開発ではなく、現在は**月 1 回のリリース**となります。チームの日常的な作業が Enterprise にあるためです。Issue とプルリクエストは引き続きレビューされ、修正は月次サイクルで反映されます。セキュリティ報告は [support@fincept.in](mailto:support@fincept.in) へ。

---

## コントリビュート

新しいデータコネクタ、AI エージェント、分析モジュール、C++ 画面、ドキュメント — いずれも歓迎します。

[コントリビュートガイド](../CONTRIBUTING.md) · [C++ ガイド](../CPP_CONTRIBUTOR_GUIDE.md) · [Python ガイド](../PYTHON_CONTRIBUTOR_GUIDE.md) · [アーキテクチャ](../ARCHITECTURE.md) · [バグ報告](https://github.com/Fincept-Corporation/FinceptTerminal/issues) · [機能リクエスト](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## Fincept の他のプロダクト

- **[Fincept Data API](https://docs.fincept.in)** — 500 以上の REST エンドポイント、423,000 以上の銘柄、2,000 以上のソース。どのアカウントにも無償枠が付属します。
- **[Quantcept](https://quantcept.io)** — オープンソースの AI 搭載コマンドライン金融ターミナル（Apache-2.0）。

---

## ライセンス

**AGPL-3.0-or-later** — 全文は [LICENSE](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE) にあります。

個人利用、学習、学術研究には無償です。AGPL-3.0 は**寛容型ではなく、強いコピーレフト**です。改変したビルドを配布する場合、あるいは他者がアクセスするサービスとして稼働させる場合、改変内容を同じライセンスで公開しなければなりません。多くの法務部門にとって議論はこの一点で決着します。だからこそ企業は、プロプライエタリで管理すべきコピーレフト義務のない **[Enterprise](https://fincept.in/enterprise)** を選びます。配布を伴わない個人利用には義務は生じません。

Fincept は本リポジトリについて、個別の商用ライセンスや学術ライセンスの販売を終了しました。商用・法人・大学の用途は、上記の公開価格による **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** が対応します。

**商標。** 「Fincept」「Fincept Terminal」および Fincept ロゴは Fincept Corporation の商標です。フォーク、派生物、リブランド製品、商用製品での使用には事前の書面による許可が必要です。

お問い合わせ: [support@fincept.in](mailto:support@fincept.in) · [利用規約](https://fincept.in/terms) · [プライバシー](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. All rights reserved.

---

<div align="center">

### **限界はあなたの思考だけ。データではない。**

⭐ **スター** · 🔄 **シェア** · 🤝 **コントリビュート**

<sub>英語原文: <a href="../../README.md">README.md</a></sub>

</div>
