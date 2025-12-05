#!/bin/bash
# Creating smaller JSON files (common, dashboard, settings, tabs) for remaining 16 languages

# Chinese (zh)
cat > public/locales/zh/common.json << 'EOF'
{
  "appName": "Fincept Terminal",
  "loading": "加载中...",
  "error": "错误",
  "success": "成功",
  "save": "保存",
  "cancel": "取消",
  "delete": "删除",
  "edit": "编辑",
  "add": "添加",
  "search": "搜索",
  "filter": "筛选",
  "back": "返回",
  "next": "下一页",
  "previous": "上一页",
  "submit": "提交",
  "close": "关闭",
  "open": "打开",
  "yes": "是",
  "no": "否",
  "ok": "确定",
  "confirm": "确认",
  "language": "语言",
  "selectLanguage": "选择语言"
}
EOF

cat > public/locales/zh/dashboard.json << 'EOF'
{
  "welcome": "欢迎使用 Fincept Terminal",
  "tabs": {
    "dashboard": "仪表板",
    "markets": "市场",
    "news": "新闻",
    "watchlist": "监视列表",
    "portfolio": "投资组合",
    "analytics": "分析",
    "economics": "经济学",
    "dbnomics": "DBnomics",
    "economicCalendar": "经济日历",
    "geopolitics": "地缘政治",
    "maritime": "海事",
    "options": "期权",
    "equityResearch": "股票研究",
    "screener": "筛选器",
    "kraken": "Kraken",
    "polygon": "Polygon.io",
    "chat": "聊天",
    "docs": "文档",
    "nodeEditor": "节点编辑器",
    "codeEditor": "代码编辑器",
    "forum": "论坛",
    "marketplace": "市场",
    "profile": "个人资料",
    "support": "支持",
    "settings": "设置"
  }
}
EOF

cat > public/locales/zh/settings.json << 'EOF'
{
  "title": "设置",
  "sections": {
    "credentials": "凭据",
    "profile": "个人资料",
    "terminal": "终端外观",
    "notifications": "通知",
    "llm": "LLM 配置",
    "dataConnections": "数据连接",
    "backtesting": "回测提供商",
    "language": "语言和地区"
  },
  "language": {
    "title": "语言设置",
    "selectLanguage": "选择语言",
    "currentLanguage": "当前语言",
    "availableLanguages": "可用语言"
  },
  "credentials": {
    "title": "API 凭据",
    "addNew": "添加新凭据",
    "serviceName": "服务名称",
    "apiKey": "API 密钥",
    "showPassword": "显示",
    "hidePassword": "隐藏",
    "save": "保存更改",
    "delete": "删除"
  },
  "terminal": {
    "title": "终端外观",
    "fontFamily": "字体系列",
    "fontSize": "字体大小",
    "fontWeight": "字体粗细",
    "theme": "颜色主题",
    "applyChanges": "应用更改",
    "resetDefaults": "重置为默认值"
  },
  "messages": {
    "saveSuccess": "设置保存成功",
    "saveError": "保存设置失败",
    "deleteSuccess": "删除成功",
    "deleteError": "删除失败"
  }
}
EOF

cat > public/locales/zh/tabs.json << 'EOF'
{
  "markets": {
    "title": "市场",
    "subtitle": "实时市场数据和分析"
  },
  "news": {
    "title": "金融新闻",
    "subtitle": "最新市场新闻和更新"
  },
  "portfolio": {
    "title": "投资组合",
    "subtitle": "跟踪和分析您的投资"
  },
  "analytics": {
    "title": "分析",
    "subtitle": "高级金融分析工具"
  },
  "chat": {
    "title": "AI 助手",
    "subtitle": "与您的AI金融助手聊天",
    "placeholder": "询问我有关市场、交易或金融的任何问题...",
    "send": "发送"
  }
}
EOF

echo "Chinese (zh) small files created"

# Japanese (ja)
cat > public/locales/ja/common.json << 'EOF'
{
  "appName": "Fincept Terminal",
  "loading": "読み込み中...",
  "error": "エラー",
  "success": "成功",
  "save": "保存",
  "cancel": "キャンセル",
  "delete": "削除",
  "edit": "編集",
  "add": "追加",
  "search": "検索",
  "filter": "フィルター",
  "back": "戻る",
  "next": "次へ",
  "previous": "前へ",
  "submit": "送信",
  "close": "閉じる",
  "open": "開く",
  "yes": "はい",
  "no": "いいえ",
  "ok": "OK",
  "confirm": "確認",
  "language": "言語",
  "selectLanguage": "言語を選択"
}
EOF

cat > public/locales/ja/dashboard.json << 'EOF'
{
  "welcome": "Fincept Terminalへようこそ",
  "tabs": {
    "dashboard": "ダッシュボード",
    "markets": "マーケット",
    "news": "ニュース",
    "watchlist": "ウォッチリスト",
    "portfolio": "ポートフォリオ",
    "analytics": "分析",
    "economics": "経済学",
    "dbnomics": "DBnomics",
    "economicCalendar": "経済カレンダー",
    "geopolitics": "地政学",
    "maritime": "海事",
    "options": "オプション",
    "equityResearch": "株式調査",
    "screener": "スクリーナー",
    "kraken": "Kraken",
    "polygon": "Polygon.io",
    "chat": "チャット",
    "docs": "ドキュメント",
    "nodeEditor": "ノードエディター",
    "codeEditor": "コードエディター",
    "forum": "フォーラム",
    "marketplace": "マーケットプレイス",
    "profile": "プロフィール",
    "support": "サポート",
    "settings": "設定"
  }
}
EOF

cat > public/locales/ja/settings.json << 'EOF'
{
  "title": "設定",
  "sections": {
    "credentials": "認証情報",
    "profile": "プロフィール",
    "terminal": "ターミナル外観",
    "notifications": "通知",
    "llm": "LLM 設定",
    "dataConnections": "データ接続",
    "backtesting": "バックテストプロバイダー",
    "language": "言語と地域"
  },
  "language": {
    "title": "言語設定",
    "selectLanguage": "言語を選択",
    "currentLanguage": "現在の言語",
    "availableLanguages": "利用可能な言語"
  },
  "credentials": {
    "title": "API 認証情報",
    "addNew": "新しい認証情報を追加",
    "serviceName": "サービス名",
    "apiKey": "API キー",
    "showPassword": "表示",
    "hidePassword": "非表示",
    "save": "変更を保存",
    "delete": "削除"
  },
  "terminal": {
    "title": "ターミナル外観",
    "fontFamily": "フォントファミリー",
    "fontSize": "フォントサイズ",
    "fontWeight": "フォントの太さ",
    "theme": "カラーテーマ",
    "applyChanges": "変更を適用",
    "resetDefaults": "デフォルトに戻す"
  },
  "messages": {
    "saveSuccess": "設定が正常に保存されました",
    "saveError": "設定の保存に失敗しました",
    "deleteSuccess": "正常に削除されました",
    "deleteError": "削除に失敗しました"
  }
}
EOF

cat > public/locales/ja/tabs.json << 'EOF'
{
  "markets": {
    "title": "マーケット",
    "subtitle": "リアルタイム市場データと分析"
  },
  "news": {
    "title": "金融ニュース",
    "subtitle": "最新の市場ニュースとアップデート"
  },
  "portfolio": {
    "title": "ポートフォリオ",
    "subtitle": "投資を追跡・分析"
  },
  "analytics": {
    "title": "分析",
    "subtitle": "高度な金融分析ツール"
  },
  "chat": {
    "title": "AI アシスタント",
    "subtitle": "AI 金融アシスタントとチャット",
    "placeholder": "市場、トレード、金融について質問してください...",
    "send": "送信"
  }
}
EOF

echo "Japanese (ja) small files created"

