# フィンセプトターミナル

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![ImGui](https://img.shields.io/badge/ImGui-Docking-blue)](https://github.com/ocornut/imgui)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **あなたの思考だけが限界です。データはそうではありません。**

CFA レベルの分析、AI 自動化、無制限のデータ接続を備えた最先端の金融インテリジェンス プラットフォーム。

[📥 ダウンロード](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 ドキュメント](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 ディスカッション](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬不協和音](https://discord.gg/ae87a8ygbN)·[🤝パートナー](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png)

</div>

* * *

## について

**Fincept ターミナル vch**は純粋なネイティブ C++20 デスクトップ アプリケーションであり、以前の Tauri/React/Rust スタックから完全に書き直されました。使用します**親愛なるイムグイ様**GPU アクセラレーション UI の場合、**GLFW + OpenGL**レンダリング用、埋め込み用**パイソン**分析用に提供され、単一のネイティブ バイナリでブルームバーグ ターミナル クラスのパフォーマンスを実現します。

* * *

## テクノロジースタック

| 層            | テクノロジー                           |
| ------------ | -------------------------------- |
| **言語**       | C++20 (MSVC / GCC / Clang)       |
| **UI**       | ImGui (ドッキング ブランチ) + ImPlot 様    |
| **レイアウト**    | ヨガ (フレックスボックス エンジン)              |
| **レンダリング**   | GLFW 3 + OpenGL 3.3+             |
| **ネットワーキング** | libcurl + OpenSSL                |
| **データベース**   | SQLite 3                         |
| **連載**       | nlohmann/json                    |
| **ロギング**     | spdlog                           |
| **オーディオ**    | ミニオーディオ                          |
| **ビデオ**      | libmpv (オプション)                   |
| **分析**       | 組み込み Python 3.11+ (100 以上のスクリプト) |
| **建てる**      | CMake 3.20+ / vcpkg              |

* * *

## 特徴

| **特徴**              | **説明**                                                             |
| ------------------- | ------------------------------------------------------------------ |
| 📊**CFAレベルの分析**     | DCF モデル、ポートフォリオの最適化、リスク指標 (VaR、Sharpe)、組み込み Python によるデリバティブ価格設定   |
| 🤖**AIエージェント**      | 20 人以上の投資家ペルソナ (バフェット、ダリオ、グラハム)、ヘッジファンド戦略、ローカル LLM サポート            |
| 🌐**100以上のデータコネクタ** | DBnomics、Polygon、Kraken、Yahoo Finance、FRED、IMF、世界銀行、AkShare、政府 API |
| 📈**リアルタイム取引**      | 暗号 (Kraken/HyperLiquid WebSocket)、株式、アルゴ取引、ペーパー取引エンジン              |
| 🔬**クアントリブ スイート**   | 18 個の定量分析モジュール — 価格設定、リスク、確率論、ボラティリティ、債券                           |
| 🚢**グローバルインテリジェンス** | 海洋追跡、地政学的分析、関係マッピング、衛星データ                                          |
| 🎨**ビジュアルワークフロー**   | 自動化パイプライン用のノードエディター、MCPツール統合                                       |
| 🧠**AI Quant Lab**  | ML モデル、因子発見、HFT、強化学習取引                                             |

* * *

## 40以上のスクリーン

| カテゴリ        | スクリーン                                                                           |
| ----------- | ------------------------------------------------------------------------------- |
| **コア**      | ダッシュボード、市場、ニュース、ウォッチリスト                                                         |
| **トレーディング** | 仮想通貨取引、株式取引、アルゴ取引、バックテスト、取引可視化                                                  |
| **研究**      | 株式リサーチ、スクリーナー、ポートフォリオ、表面分析、M&A分析、デリバティブ、オルタナティブ投資                               |
| **クアントリブ**  | コア、分析、曲線、経済学、計測器、ML、モデル、数値、物理学、ポートフォリオ、価格設定、規制、リスク、スケジューリング、ソルバー、統計、確率論、ボラティリティ |
| **AI/ML**   | AI Quant Lab、エージェント スタジオ、AI チャット、アルファ アリーナ                                      |
| **経済**      | 経済学、DBnomics、AkShare、アジア市場                                                      |
| **地政学**     | 地政学、政府データ、関係地図、海事、ポリマーケット                                                       |
| **ツール**     | コード エディター、ノード エディター、Excel、レポート ビルダー、メモ、データ ソース、データ マッピング、MCP サーバー               |
| **コミュニティ**  | フォーラム、プロフィール、設定、サポート、ドキュメント、概要                                                  |

* * *

## ソースからビルドする

### 前提条件

-   **CMake**3.20+
-   **vcpkg**(依存関係管理用)
-   **C++20コンパイラ**(MSVC 2022、GCC 12+、または Clang 15+)
-   **パイソン**3.11+ (分析スクリプト用)

### 建てる

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-cpp

# Windows (MSVC)
cmake --preset=default
cmake --build build --config Release

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 走る

```bash
./build/FinceptTerminal          # Linux / macOS
.\build\Release\FinceptTerminal  # Windows
```

### vcpkg の依存関係

glfw3、curl、nlohmann-json、sqlite3、openssl、imgui (ドッキング + フリータイプ)、ヨガ、stb、implot、spdlog、miniaudio

* * *

## 当社の特徴

**フィンセプトターミナル**は、従来のソフトウェアによる制限を拒否する人のために構築されたオープンソースの金融プラットフォームです。私たちは次の点で競争します**分析の深さ**そして**データへのアクセシビリティ**— インサイダー情報や独占フィードではありません。

-   **ネイティブパフォーマンス**— C++ と GPU アクセラレーション ImGui、Electron/Web オーバーヘッドなし
-   **単一バイナリ**— Node.js、ブラウザ ランタイム、JavaScript バンドラーなし
-   **CFAレベルの分析**— Python モジュールを介してカリキュラムを完全にカバー
-   **100以上のデータコネクタ**— Yahoo Financeから政府データベースまで
-   **無料＆オープンソース**(AGPL-3.0) 商用ライセンスも利用可能

* * *

## ロードマップ

**2026 年第 1 四半期:**C++ の移行が完了し、リアルタイム ストリーミングが強化され、高度なバックテストが追加されました。**2026年:**オプション戦略ビルダー、マルチポートフォリオ管理、50 人以上の AI エージェント**未来：**モバイル コンパニオン、組織向け機能、プログラマティック API、ML トレーニング UI

* * *

## 貢献する

私たちは財務分析の未来を一緒に構築しています。

**貢献する：**新しいデータ コネクタ、AI エージェント、分析モジュール、C++ 画面、ドキュメント

-   [貢献ガイド](docs/CONTRIBUTING.md)
-   [C++ 貢献ガイド](fincept-cpp/CONTRIBUTING.md)
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
