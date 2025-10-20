# Finceptターミナル✨

<div align="center">

[![License: MIT](https://img.shields.io/badge/license-MIT-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE.txt)[![Tauri](https://img.shields.io/badge/Tauri-2.0-FFC131?logo=tauri)](https://tauri.app/)[![React](https://img.shields.io/badge/React-19-61DAFB?logo=react)](https://react.dev/)[![TypeScript](https://img.shields.io/badge/TypeScript-5.6-3178C6?logo=typescript)](https://www.typescriptlang.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

[英語](README.md)\|[スペイン語](README.es.md)\|[中文](README.zh-CN.md)\|[日本語](README.ja.md)\|[フランス語](README.fr.md)\|[ドイツ語](README.de.md)\|[韓国人](README.ko.md)\|[ヒンディー語](README.hi.md)

### _プロフェッショナルな財務分析プラットフォーム_

**ブルームバーグレベルの洞察を誰にでも。オープンソース。クロスプラットフォーム。**

[📥 ダウンロード](#-getting-started)•[✨ 特徴](#-features)•[📸 スクリーンショット](#-platform-preview) • [🤝 貢献する](#-contributing)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Geopolitics.png)

</div>

* * *

## 🎯 What is Fincept Terminal?

**フィンセプトターミナル**は、次の機能を備えた最新のクロスプラットフォーム金融端末です。**苦難**,**反応する**、 そして**TypeScript**。完全に無料でオープンソースの機関グレードの財務分析ツールを個人投資家やトレーダーに提供します。

ブルームバーグとリフィニティブからインスピレーションを得た Fincept ターミナルは、リアルタイムの市場データ、高度な分析、AI を活用した洞察、プロフェッショナルなインターフェイスをすべてエンタープライズ価格なしで提供します。

### 🌟 Fincept ターミナルを選ぶ理由?

| 従来のプラットフォーム       | フィンセプトターミナル             |
| ----------------- | ----------------------- |
| 💸 年間 20,000 ドル以上 | 🆓**無料＆オープンソース**        |
| 🏢 エンタープライズのみ     | 👤**誰でも利用可能**           |
| 🔒 ベンダーロックイン      | 💻**クロスプラットフォームデスクトップ** |
| ⚙️限定的なカスタマイズ      | 🎨**完全にカスタマイズ可能**       |

**技術スタック:**Tauri (Rust) • React 19 • TypeScript • TailwindCSS

* * *

## 🚀 Getting Started

### **オプション 1: Microsoft Store からダウンロードする**🎉

<div align="center">

[![Get it from Microsoft Store](https://get.microsoft.com/images/en-us%20dark.svg)](https://apps.microsoft.com/detail/XPDDZR13CXS466?hl=en-US&gl=IN&ocid=pdpshare)

**最も簡単なインストール • 自動アップデート • Windows 10/11**

</div>

### **オプション 2: インストーラーをダウンロードする**

**Windows:**

-   📦[MSI インストーラーをダウンロード](http://product.fincept.in/FinceptTerminalV2Alpha.msi)(Windows 10/11)

**macOS と Linux:**

-   事前に構築されたインストーラーは近日公開予定です。

### **オプション 3: ソースからビルドする**

#### 🚀**クイックセットアップ（自動）**

**Windowsの場合:**

```bash
# 1. Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal

# 2. Run setup script (as Administrator)
setup.bat
```

**Linux/macOS の場合:**

```bash
# 1. Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal

# 2. Make script executable and run
chmod +x setup.sh
./setup.sh
```

自動セットアップ スクリプトは次のことを行います。

-   ✅ Node.js LTS (v22.14.0) をインストールする
-   ✅ Rust (最新の安定版) をインストールする
-   ✅ プロジェクトの依存関係をインストールする
-   ✅ すべてを自動的にセットアップします

#### ⚙️**手動セットアップ**

**前提条件:**Node.js 18 以降、Rust 1.70 以降、Git

```bash
# Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-terminal-desktop

# Install dependencies
npm install

# Run in development
npm run tauri dev

# Build for production
npm run tauri build
```

* * *

## ✨ 特徴

<table>
<tr>
<td width="50%">

### 📊 マーケットインテリジェンス

🌍 グローバルなカバレッジ (株式、外国為替、仮想通貨、商品)<br>📈 リアルタイムのデータとストリーミング更新<br>📰 マルチソースニュースの統合<br>📉 カスタムウォッチリスト

### 🧠 AI を活用した分析

🤖 GenAIチャットインターフェース<br>📊 リアルタイムの感情分析<br>💡 AI 主導の洞察と推奨事項<br>🎯 自動パターン認識

</td>
<td width="50%">

### 📈 プロフェッショナルツール

📊 高度なチャート作成 (50 以上のインジケーター)<br>💼 会社の財務と調査<br>📋 マルチアカウントポートフォリオ追跡<br>⚡ バックテスト戦略<br>🔔 カスタム価格と技術アラート

### 🌐 グローバルな洞察

🏛️ 経済データ (金利、GDP、インフレ)<br>🗺️ 貿易ルートと海上物流<br>🌍 地政学的リスク評価

</td>
</tr>
</table>

**ユーザーエクスペリエンス:**ブルームバーグ スタイルの UI • ファンクション キーのショートカット (F1 ～ F12) • ダーク モード • タブベースのワークフロー

* * *

## 🎬 プラットフォームのプレビュー

<div align="center">

|                                              チャットモジュール                                              |                                                    ダッシュボード                                                    |
| :-------------------------------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------------------: |
| ![Chat](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Chat.png) | ![Dashboard](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png) |
|                                           AI を活用した財務アシスタント                                          |                                                  リアルタイムの市場概要                                                  |

|                                                     経済                                                    |                                                   株式調査                                                  |
| :-------------------------------------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------------: |
| ![Economy](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Economy.png) | ![Equity](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png) |
|                                                   世界経済指標                                                  |                                                 包括的な株式分析                                                |

|                                                 フォーラム                                                 |                                                        地政学                                                        |
| :---------------------------------------------------------------------------------------------------: | :---------------------------------------------------------------------------------------------------------------: |
| ![Forum](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Forum.png) | ![Geopolitics](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Geopolitics.png) |
|                                            コミュニティのディスカッション                                            |                                                   グローバルリスクモニタリング                                                  |

|                                                        世界貿易                                                       |                                                     市場                                                    |
| :---------------------------------------------------------------------------------------------------------------: | :-------------------------------------------------------------------------------------------------------: |
| ![GlobalTrade](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/GlobalTrade.png) | ![Markets](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Markets.png) |
|                                                      国際貿易の流れ                                                      |                                                ライブマルチアセット市場                                               |

|                                                          貿易分析                                                         |
| :-------------------------------------------------------------------------------------------------------------------: |
| ![TradeAnalysis](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/TradeAnalysis.png) |
|                                                      高度な分析とバックテスト                                                     |

</div>

* * *

## 🛣️ロードマップ

### **現在の状況**

-   ✅ Tauri アプリケーションフレームワーク
-   ✅ 認証システム（ゲスト+登録済み）
-   ✅ ブルームバーグスタイルのUI
-   ✅ 支払いの統合
-   ✅ フォーラム機能
-   🚧 リアルタイムの市場データ
-   🚧 高度なグラフ作成
-   🚧 AIアシスタント

### **近日公開 (2025 年第 2 四半期)**

-   📊 完全な市場データのストリーミング
-   📈 50以上のインジケーターを備えたインタラクティブなチャート
-   🤖 プロダクション AI 機能
-   💼 ポートフォリオ管理
-   🔔 警報システム

### **未来**

-   🌍 多言語サポート
-   🏢 ブローカーの統合
-   📱 モバイルコンパニオンアプリ
-   🔌プラグインシステム
-   🎨 テーママーケットプレイス

* * *

## 🤝 貢献しています

開発者、トレーダー、金融専門家からの貢献を歓迎します。

**貢献する方法:**

-   🐛 バグや問題を報告する
-   💡 新機能を提案する
-   🔧 コードを送信する (React、Rust、TypeScript)
-   📚 ドキュメントを改善する
-   🎨 デザインへの貢献

**クイックリンク:**

-   [貢献ガイドライン](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/CONTRIBUTING.md)
-   [バグを報告する](https://github.com/Fincept-Corporation/FinceptTerminal/issues/new?template=bug_report.md)
-   [リクエスト機能](https://github.com/Fincept-Corporation/FinceptTerminal/issues/new?template=feature_request.md)
-   [ディスカッション](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

**開発セットアップ:**

```bash
# Fork and clone
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal

# Automated setup (recommended for first-time contributors)
# Windows: run setup.bat as Administrator
# Linux/macOS: chmod +x setup.sh && ./setup.sh

# Or manual setup
cd fincept-terminal-desktop
npm install
npm run dev          # Start Vite dev server
npm run tauri dev    # Start Tauri app
```

* * *

## 🏗️ プロジェクトの構造

    fincept-terminal-desktop/
    ├── src/
    │   ├── components/      # React components (auth, dashboard, tabs, ui)
    │   ├── contexts/        # React Context (Auth, Theme)
    │   ├── services/        # API service layers
    │   └── hooks/           # Custom React hooks
    ├── src-tauri/
    │   ├── src/             # Rust backend
    │   ├── Cargo.toml       # Rust dependencies
    │   └── tauri.conf.json  # Tauri config
    └── package.json         # Node dependencies

* * *

## 📊 技術的な詳細

**パフォーマンス：**

-   バイナリサイズ: ~15MB
-   メモリ: ~150MB (Electron の場合は 500MB+)
-   起動: &lt;2秒

**プラットフォームのサポート:**

-   ✅ Windows 10/11 (x64、ARM64)
-   ✅ macOS 11+ (インテル、Apple シリコン)
-   ✅ Linux (Ubuntu、Debian、Fedora)

**安全：**

-   Tauri サンドボックス環境
-   Node.js ランタイムなし
-   暗号化された認証情報のストレージ
-   HTTPS のみの API 呼び出し

* * *

## 📈 スターの歴史

**⭐ リポにスターを付けてプロジェクトを共有しましょう ❤️**

<div align="center">
<a href="https://star-history.com/#Fincept-Corporation/FinceptTerminal&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
 </picture>
</a>
</div>

* * *

## 🌐 私たちとつながりましょう

<div align="center">

[![GitHub Discussions](https://img.shields.io/badge/GitHub-Discussions-181717?style=for-the-badge&logo=github)](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)[![Email Support](https://img.shields.io/badge/Email-dev@fincept.in-D14836?style=for-the-badge&logo=gmail&logoColor=white)](mailto:dev@fincept.in)[![Contact Form](https://img.shields.io/badge/Contact-Form-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://forms.gle/DUsDHwxBNRVstYMi6)

**コミュニティによってコミュニティのために構築される**_誰もが専門的な財務分析を利用できるようにする_

⭐**私たちにスターを付けてください**• 🔄**共有**• 🤝**貢献する**

</div>

* * *

## 📜ライセンス

MIT ライセンス - を参照[ライセンス.txt](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE.txt)

* * *

## 🙏 謝辞

構築されたもの:[苦難](https://tauri.app/)•[反応する](https://react.dev/)•[さび](https://www.rust-lang.org/)•[TailwindCSS](https://tailwindcss.com/)•[基数UI](https://www.radix-ui.com/)

* * *

**注記：**以前のバージョンは Python/DearPyGUI で構築され、次の場所にアーカイブされています。`legacy-python-depreciated/`。現在の Tauri ベースのアプリケーションは、最新のテクノロジーを使用して完全に書き直されました。

* * *

<div align="center">

**© 2024-2025 Fincept Corporation • オープンソース • MIT ライセンス**

</div>
