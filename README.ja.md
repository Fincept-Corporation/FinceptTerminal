# フィンセプトターミナル

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **あなたの思考だけが限界です。データはそうではありません。**

CFA レベルの分析、AI 自動化、無制限のデータ接続を備えた最先端の金融インテリジェンス プラットフォーム。

[📥 ダウンロード](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 ドキュメント](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 ディスカッション](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬不協和音](https://discord.gg/ae87a8ygbN)·[🤝パートナー](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png)

</div>

* * *

## について

**Fincept ターミナル vch**は純粋なネイティブ C++20 デスクトップ アプリケーションであり、以前の Tauri/React/Rust スタックから完全に書き直されました。使用します**Qt6**UI とレンダリング用、埋め込み**パイソン**分析用に提供され、単一のネイティブ バイナリでブルームバーグ ターミナル クラスのパフォーマンスを実現します。

* * *

## 特徴

| **特徴**              | **説明**                                                                                                                              |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| 📊**CFAレベルの分析**     | DCF モデル、ポートフォリオの最適化、リスク指標 (VaR、Sharpe)、組み込み Python によるデリバティブ価格設定                                                                    |
| 🤖**AIエージェント**      | 20 人以上の投資家ペルソナ (バフェット、ダリオ、グラハム)、ヘッジファンド戦略、ローカル LLM サポート、マルチプロバイダー (OpenAI、Anthropic、Gemini、Groq、DeepSeek、MiniMax、OpenRouter、Ollama) |
| 🌐**100以上のデータコネクタ** | DBnomics、Polygon、Kraken、Yahoo Finance、FRED、IMF、世界銀行、AkShare、政府 API                                                                  |
| 📈**リアルタイム取引**      | 暗号 (Kraken/HyperLiquid WebSocket)、株式、アルゴ取引、ペーパー取引エンジン                                                                               |
| 🔬**クアントリブ スイート**   | 18 個の定量分析モジュール — 価格設定、リスク、確率論、ボラティリティ、債券                                                                                            |
| 🚢**グローバルインテリジェンス** | 海洋追跡、地政学的分析、関係マッピング、衛星データ                                                                                                           |
| 🎨**ビジュアルワークフロー**   | 自動化パイプライン用のノードエディター、MCPツール統合                                                                                                        |
| 🧠**AI Quant Lab**  | ML モデル、因子発見、HFT、強化学習取引                                                                                                              |

* * *

## インストール

### オプション 1 — ビルド済みバイナリをダウンロードする (推奨)

事前に構築されたバイナリは、[リリースページ](https://github.com/Fincept-Corporation/FinceptTerminal/releases). No build tools required — just extract and run.

| プラットフォーム            | ダウンロード                                   | 走る                                                   |
| ------------------- | ---------------------------------------- | ---------------------------------------------------- |
| **Windows x64**     | `FinceptTerminal-Windows-x64.zip`        | 抽出→`FinceptTerminal.exe`                             |
| **Windows ARM64**   | `FinceptTerminal-Windows-arm64.zip`      | 抽出→`FinceptTerminal.exe`                             |
| **Linux x64**       | `FinceptTerminal-Linux-x86_64.AppImage`  | `chmod +x`→`./FinceptTerminal-Linux-x86_64.AppImage` |
| **macOS（アップルシリコン）** | `FinceptTerminal-macOS-arm64.tar.gz`     | 抽出→`./FinceptTerminal`                               |
| **macOS (インテル)**    | `FinceptTerminal-macOS-x64.tar.gz`       | 抽出→`./FinceptTerminal`                               |
| **macOS (ユニバーサル)**  | `FinceptTerminal-macOS-universal.tar.gz` | 抽出→`./FinceptTerminal`                               |

* * *

### オプション 2 — クイック スタート (ワンクリック ビルド)

セットアップ スクリプトをクローンして実行します。これにより、すべての依存関係がインストールされ、アプリが自動的に構築されます。

```bash
# Linux / macOS
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
chmod +x setup.sh && ./setup.sh
```

```bat
# Windows — run from Developer Command Prompt for VS 2022
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
setup.bat
```

このスクリプトは、コンパイラ チェック、CMake、Qt6、Python、ビルド、起動を処理します。

* * *

### オプション 3 — ドッカー

```bash
# Pull and run
docker pull ghcr.io/fincept-corporation/fincept-terminal:latest
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
    ghcr.io/fincept-corporation/fincept-terminal:latest

# Or build from source
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
docker build -t fincept-terminal .
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix fincept-terminal
```

> **注記：**Docker は主に Linux を対象としています。 macOS および Windows では、追加の XServer 構成が必要です。

* * *

### オプション 4 — ソースからビルドする (手動)

#### 前提条件

| 道具           | バージョン | 窓                                                            | Linux                 | macOS                              |
| ------------ | ----- | ------------------------------------------------------------ | --------------------- | ---------------------------------- |
| **Git**      | 最新    | `winget install Git.Git`                                     | `apt install git`     | `brew install git`                 |
| **CMake**    | 3.20+ | `winget install Kitware.CMake`                               | `apt install cmake`   | `brew install cmake`               |
| **C++コンパイラ** | C++20 | MSVC 2022 ([ビジュアルスタジオ](https://visualstudio.microsoft.com/)) | `apt install g++`     | Xcode CLT:`xcode-select --install` |
| **Qt6**      | 6.5+  | 以下を参照してください                                                  | 以下を参照してください           | 以下を参照してください                        |
| **パイソン**     | 3.11+ | [python.org](https://www.python.org/downloads/)              | `apt install python3` | `brew install python`              |

#### Qt6をインストールする

**Windows:**

```powershell
# Via Qt online installer (recommended — includes windeployqt)
# Download from https://www.qt.io/download-qt-installer
# Select: Qt 6.x > MSVC 2022 64-bit

# Or via winget
winget install Qt.QtCreator
```

**Linux (Ubuntu/Debian):**

```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

**macOS:**

```bash
brew install qt
```

#### 建てる

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-qt

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (from Developer Command Prompt for VS 2022)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release --parallel
```

#### 走る

```bash
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

* * *

## 当社の特徴

**フィンセプトターミナル**は、従来のソフトウェアによる制限を拒否する人のために構築されたオープンソースの金融プラットフォームです。私たちは次の点で競争します**分析の深さ**そして**データへのアクセシビリティ**— インサイダー情報や独占フィードではありません。

-   **ネイティブパフォーマンス**— Qt6 を使用した C++20、Electron/Web オーバーヘッドなし
-   **単一バイナリ**— Node.js、ブラウザ ランタイム、JavaScript バンドラーなし
-   **CFAレベルの分析**— Python モジュールを介してカリキュラムを完全にカバー
-   **100以上のデータコネクタ**— Yahoo Financeから政府データベースまで
-   **無料＆オープンソース**(AGPL-3.0) 商用ライセンスも利用可能

* * *

## ロードマップ

| タイムライン            | マイルストーン                                    |
| ----------------- | ------------------------------------------ |
| **2026 年第 1 四半期** | リアルタイムストリーミング、高度なバックテスト、ブローカー統合            |
| **嘔吐 2026**       | オプション戦略ビルダー、マルチポートフォリオ管理、50 人以上の AI エージェント |
| **Kz 2026**       | プログラマティック API、ML トレーニング UI、制度的機能           |
| **未来**            | モバイル コンパニオン、クラウド同期、コミュニティ マーケットプレイス        |

* * *

## 貢献する

私たちは財務分析の未来を一緒に構築しています。

**貢献する：**新しいデータ コネクタ、AI エージェント、分析モジュール、C++ 画面、ドキュメント

-   [貢献ガイド](docs/CONTRIBUTING.md)
-   [C++ 貢献ガイド](fincept-qt/CONTRIBUTING.md)
-   [Python 貢献者ガイド](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [バグを報告する](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [リクエスト機能](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## 大学および教育者向け

**プロレベルの財務分析を教室に導入しましょう。**

-   **$799/月**20アカウントの場合
-   Fincept データと API へのフルアクセス
-   金融、経済、データ サイエンスのコースに最適
-   CFA カリキュラム分析の組み込み

**興味がある？**電子メール**[support@fincept.in](mailto:support@fincept.in)**あなたの機関名を付けてください。

[大学ライセンスの詳細](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## ライセンス

**デュアルライセンス: AGPL-3.0 (オープンソース) + 商用**

### オープンソース (AGPL-3.0)

-   個人、教育、非営利目的での使用は無料
-   ネットワークサービスとして配布または使用する場合は、共有の変更が必要です
-   ソースコードの完全な透明性

### 商用ライセンス

-   ビジネスで使用する場合、または商業的に Fincept データ/API にアクセスする場合に必要
-   接触：**[support@fincept.in](mailto:support@fincept.in)**
-   詳細：[商用ライセンスガイド](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### 商標

「Fincept Terminal」および「Fincept」はFincept Corporationの商標です。

© 2025-2026 Fincept Corporation.無断転載を禁じます。

* * *

<div align="center">

### **あなたの思考だけが限界です。データはそうではありません。**

<div align="center">
<a href="https://star-history.com/#Fincept-Corporation/FinceptTerminal&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
 </picture>
</a>
</div>

[![Email](https://img.shields.io/badge/Email-support@fincept.in-blue)](mailto:support@fincept.in)

⭐**星**· 🔄**共有**· 🤝**貢献する**

</div>
