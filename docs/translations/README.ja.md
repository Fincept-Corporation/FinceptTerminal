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

> [!IMPORTANT]
> **Fincept Terminal には 2 つのエディションがあります。**
>
> 🏢 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** — ファンド、ファミリーオフィス、リサーチデスク向けのプライベート（クローズドソース）ビルド。41 モジュール、独自データセット、マルチエージェント・リサーチ、ライブ・ブローカールーティング、プライベート・データルーム、SSO、SLA を備え、**1 ユーザー月額 99 ドル**から。チームが日々開発しているのはこちらであり、ターミナルで収益を上げるならこのエディションです。
>
> 📖 **このリポジトリ** — AGPL-3.0 の無償オープンソース版。学習、個人利用、学術研究向けです。公開のまま維持され、削除されることはなく、**月 1 回のリリース**を行います。
>
> [**どちらを選ぶべきか →**](#どちらのエディションを選ぶべきか) · [比較](https://fincept.in/comparison) · [料金](https://fincept.in/pricing)

---

## 概要

**Fincept Terminal** は金融リサーチのためのネイティブ C++20 デスクトップ・ターミナルです。UI は Qt6、分析は組み込み Python 3.11、単一バイナリで動作し、Electron は使用しません。

**共通のデータコア上に 2 つのエディション**があります。

| | **オープンソース** — 本リポジトリ | **Enterprise** — [fincept.in/enterprise](https://fincept.in/enterprise) |
|---|---|---|
| **対象** | 学習、個人利用、学術研究 | ファンド、ファミリーオフィス、リサーチデスク |
| **価格** | 無償 · AGPL-3.0 | **月額 99 ドル**／ユーザー〜 |
| **リリース頻度** | 月 1 回の更新 | 継続的 |

---

## どちらのエディションを選ぶべきか

| あなたが… | 選ぶべきは | 理由 |
|---|---|---|
| 学生・趣味・独学の方 | **オープンソース** | 本当に無償で、今後もそのままです |
| 学術研究者 | **オープンソース** | 学術利用は無償。管理されたシートが必要な大学は下記の学術バンドルをご覧ください |
| ファンド、ファミリーオフィス、自己勘定デスク、銀行、フィンテック | **Enterprise** | AGPL のコピーレフトが適用されず、独自データ、ライブルーティング、SSO、SLA が得られます |
| これを職業として、収益のために行っている方 | **Enterprise** | 実質的に安く、日々開発が進む唯一のエディションです |

> **率直に言えば。** オープン版は手を抜いたデモではなく、実用に足る製品です。ただし無償の公開フィード、**あなたの** API キー、**あなたの** LLM キーで動作し、AI の呼び出しはすべてトークン単位で上限なくあなたに課金されます。コミュニティの Issue はベストエフォートで、応答の保証はありません。ターミナルが収益の手段であるなら、Enterprise のほうが安全で結果的に安く済みます。

[**Enterprise を見る →**](https://fincept.in/enterprise) · [詳細比較](https://fincept.in/comparison) · [料金](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## オープンソース版と Enterprise の比較

| | オープンソース | **Enterprise** |
|---|---|---|
| **ライセンス** | AGPL-3.0 — 強いコピーレフト。配布・ホスティングすれば改変内容の公開義務が生じます | プロプライエタリ — 管理すべきコピーレフト義務なし |
| **費用** | 無償 — データ費と LLM 費は自己負担 | 1 ユーザー月額 99／199／299 ドル、すべて込み |
| **モジュール** | コア・ターミナル | **6 デスク・41 モジュール** |
| **データ** | 無償の公開フィード、レート制限あり、キーは自前 | 独自・専有データセット — 長い履歴、速い更新 |
| **価格履歴** | 無償枠で許される範囲 | 1 年／5 年／無制限 · 正確なバックテストのための**ポイント・イン・タイム** |
| **AI 予算** | 自前の LLM キー、トークン単位で自己負担 | 月あたり 400／2,000／5,000 クレジット込み |
| **AI リサーチ** | 基本的なマーケット・アシスタント | **計画し委任する**マルチエージェント・リサーチ · Agent Studio · 最大 53 エージェント · 6 チームモード |
| **プライベート・データルーム** | — | 自社の開示資料やモデルを、自分のエージェントのみが参照。他社と共有されず、学習にも使われません |
| **取引** | ペーパートレード＋自前キーによるブローカー連携 | **ライブ・ブローカールーティング＋ライブ・アルゴ配備** |
| **クオンツ** | コミュニティ版バックテスト | Quant Lab、Alpha Arena、ボラティリティ分析、ポイント・イン・タイム・バックテスト |
| **セキュリティ／コンプライアンス** | — | SSO/SAML、監査ログ、ロールベース・アクセス、データ分離 |
| **サポート** | GitHub Issue、ベストエフォート、応答保証なし | SLA に基づく優先対応 |
| **ドキュメント** | 本リポジトリ | [**700 ページのマニュアル**](https://fincept.in/manual) — 41 ガイド、472 セクション |
| **対応 OS** | Windows、macOS、Linux ＋ ホスト型 Web ターミナル | Windows 10/11、macOS 13+（Apple シリコン）、Linux |

Enterprise には**別アカウント**が必要です。無償の Fincept アカウントではログインできず、サブスクリプションが紐づくまでアプリはロックされたままです。[Enterprise アカウントを作成 →](https://fincept.in/enterprise/signup)

---

## Enterprise — 6 デスク、41 モジュール

| デスク | 内容 |
|---|---|
| **Agentic Research** · 6 | エージェントが作業を計画し、専門エージェントに委任し、ライブデータと自社データルームを読み、出典付きのノートを返します |
| **Quant Lab & Backtesting** · 4 | シグナル研究、ポイント・イン・タイム・バックテスト、ボラティリティ・サーフェス |
| **Deep Fundamental Research** · 8 | 株式分析、バリュエーション、デリバティブ、M&A 分析 |
| **Markets & Execution** · 7 | 株式・暗号資産・予測市場のライブ取引、ブローカールーティング、アルゴ配備 |
| **Macro & Global Intelligence** · 7 | 統計、政府データ、地政学、海上航路 |
| **自分専用のワークスペース** · 9 | ダッシュボード、表計算、ノート、ファイル、コード、レポートビルダー |

[製品一覧を見る →](https://fincept.in/products)

### 料金

| | **Exclusive** | **Exclusive+** ★ 一番人気 | **Exclusive Pro** |
|---|---|---|---|
| | **月額 99 ドル**／ユーザー | **月額 199 ドル**／ユーザー | **月額 299 ドル**／ユーザー |
| AI クレジット／月 | 400 | 2,000 | 5,000 |
| ポートフォリオ · ウォッチリスト | 1 · 3 | 10 · 25 | 無制限 |
| 価格履歴 | 1 年 | 5 年 | 無制限 |
| 含まれるシート数 | 1 | 1 | 2 |
| ディープリサーチ＋エージェントチーム | — | ✓ | ✓ |
| ブローカー口座連携 | — | ✓ | ✓ |
| ライブ取引＋アルゴ配備 | — | — | ✓ |

月次請求 · 年間契約の縛りなし · 最低シート数なし · **四半期払いで 15% 割引**。年間では **1 ユーザーあたり 1,188〜3,588 ドル**で、Bloomberg の 1 シート約 **27,000 ドル**と比べられます — [詳細な比較を見る](https://fincept.in/comparison)。

**大学・学術機関向け:** 一律のバンドル — **Exclusive Pro 5 シートを月額 699 ドル**（定価 1,495 ドル）。[support@fincept.in](mailto:support@fincept.in) までご連絡ください。

この 3 プランと学術バンドルが料金表のすべてです。個別交渉によるエンタープライズ価格はなく、別途購入する商用ライセンスもありません。

[**Enterprise アカウントを作成**](https://fincept.in/enterprise/signup) · [**デモを予約**](https://calendly.com/nikultilak/fincept-terminal-demo) · [マニュアルを読む](https://fincept.in/manual)

---

## オープンソース版のインストール

**Windows x64**、**Linux x64**（`.run` / `.deb` / `.rpm`）、**macOS（Apple シリコン）**向けのインストーラーは[リリースページ](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)にあります。

**ソースからのビルド** — Linux/macOS: `git clone … && ./setup.sh`。Windows、手動ビルド、固定されたツールチェーン（**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**）、トラブルシューティングは **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)** を参照してください。バージョンは固定です。これより新しい／古いものはサポートされません。

> Enterprise ビルドをお探しですか。Windows、macOS、Linux 向けに署名済みインストーラーが用意されており、Enterprise ログインの内側にあります — [こちらから入手](https://fincept.in/enterprise)。

---

## オープン版の内容

| | |
|---|---|
| **マルチアセット分析** | DCF、ポートフォリオ最適化、VaR／シャープ、デリバティブ・プライシング、債券、オルタナティブ — 組み込み Python 経由 |
| **QuantLib スイート** | 18 の定量モジュール — プライシング、リスク、確率過程、ボラティリティ、債券 |
| **AI エージェント** | トレーダー／投資家、経済、地政学の各フレームワークにわたる 37 エージェント。キーは自前（OpenAI、Anthropic、Gemini、Groq、DeepSeek、OpenRouter、Ollama） |
| **100 以上のデータコネクタ** | DBnomics、FRED、IMF、世界銀行、AkShare、Polygon、Kraken、Yahoo Finance、政府 API |
| **取引** | 暗号資産・株式フィード、ペーパートレード・エンジン、16 のブローカー連携 |
| **自動化** | ビジュアル・ノードエディタ、MCP ツール連携、AI Quant Lab（ML、ファクター探索、強化学習） |
| **グローバル・インテリジェンス** | 海上輸送トラッキング、地政学分析、関係マッピング |

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

Fincept は本リポジトリについて、**個別の商用ライセンスや学術ライセンスの販売を終了しました**。商用・機関・大学の用途は、上記の公開価格による **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** が対応します。

**商標。** 「Fincept」「Fincept Terminal」および Fincept ロゴは Fincept Corporation の商標です。フォーク、派生物、リブランド製品、商用製品での使用には事前の書面による許可が必要です。

お問い合わせ: [support@fincept.in](mailto:support@fincept.in) · [利用規約](https://fincept.in/terms) · [プライバシー](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. All rights reserved.

---

<div align="center">

### **限界はあなたの思考だけ。データではない。**

⭐ **スター** · 🔄 **シェア** · 🤝 **コントリビュート**

<sub>英語原文: <a href="../../README.md">README.md</a></sub>

</div>
