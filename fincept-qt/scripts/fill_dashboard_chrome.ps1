# Hand-fills dashboard chrome translations (toolbar / statusbar / tickerbar)
# for all 10 .ts files. Run from repo root.
#
# This intentionally covers only the always-visible chrome — widget content,
# widget registry, and templates are left as unfinished (English fallback)
# for translators to fill in. The 1400-string scope of a full dashboard
# translation is too large for a single hand-fill pass.
$ErrorActionPreference = 'Stop'

# Per-language translation tables. Keys are English source strings, values
# are the localized text. Add more entries here over time.

$trans = @{
  'es_ES' = @{
    # DashboardToolBar
    'LIVE'                                       = 'EN VIVO'
    'OFFLINE'                                    = 'DESCONECTADO'
    'Click to toggle UTC / local time'           = 'Clic para alternar UTC / hora local'
    '%1 WIDGETS'                                 = '%1 WIDGETS'
    'COMPACT'                                    = 'COMPACTO'
    'PULSE'                                      = 'PULSO'
    'Force-refresh all live data on the dashboard' = 'Forzar la actualizacion de todos los datos en vivo'
    '+ ADD'                                      = '+ ANADIR'
    ' UTC'                                       = ' UTC'
    ' LOC'                                       = ' LOC'
    # DashboardStatusBar
    'SESSION:'                                   = 'SESION:'
    'LAYOUT:'                                    = 'DISENO:'
    'FEEDS:'                                     = 'FUENTES:'
    'ACTIVE'                                     = 'ACTIVO'
    'EMPTY'                                      = 'VACIO'
    'CONNECTED'                                  = 'CONECTADO'
    'DISCONNECTED'                               = 'DESCONECTADO'
    'READY'                                      = 'LISTO'
    'MEM: ---'                                   = 'MEM: ---'
    'MEM: %1 MB'                                 = 'MEM: %1 MB'
    'LAT: ---'                                   = 'LAT: ---'
    'LAT: ERR'                                   = 'LAT: ERR'
    'LAT: %1ms'                                  = 'LAT: %1ms'
    # TickerBar
    'SYMBOLS:'                                   = 'SIMBOLOS:'
    'AAPL, MSFT, ^GSPC, BTC-USD ...'             = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK'                                         = 'OK'
    'Edit Symbols...'                            = 'Editar simbolos...'
    # MarketPulsePanel section headers
    'MARKET PULSE'                               = 'PULSO DEL MERCADO'
    'FEAR & GREED INDEX'                         = 'INDICE DE MIEDO Y CODICIA'
    'MARKET BREADTH'                             = 'AMPLITUD DEL MERCADO'
    'TOP GAINERS'                                = 'MAYORES SUBIDAS'
    'TOP LOSERS'                                 = 'MAYORES BAJADAS'
    'GLOBAL SNAPSHOT'                            = 'INSTANTANEA GLOBAL'
    'MARKET HOURS'                               = 'HORARIO DE MERCADO'
    'LOADING...'                                 = 'CARGANDO...'
    'EXTREME FEAR'                               = 'MIEDO EXTREMO'
    'FEAR'                                       = 'MIEDO'
    'NEUTRAL'                                    = 'NEUTRAL'
    'GREED'                                      = 'CODICIA'
    'EXTREME GREED'                              = 'CODICIA EXTREMA'
    'OPEN'                                       = 'ABIERTO'
    'CLOSED'                                     = 'CERRADO'
    'PRE'                                        = 'PRE'
    # AddWidgetDialog
    '%1 AVAILABLE'                               = '%1 DISPONIBLE'
    'Search widgets...'                          = 'Buscar widgets...'
    'CANCEL'                                     = 'CANCELAR'
    'All'                                        = 'Todos'
    # TemplatePicker
    'Choose Dashboard Template'                  = 'Elegir plantilla del panel'
    'CHOOSE TEMPLATE'                            = 'ELEGIR PLANTILLA'
    'Select a template to reset your dashboard. Current layout will be replaced.' = 'Selecciona una plantilla para reiniciar el panel. El diseno actual se reemplazara.'
    'APPLY'                                      = 'APLICAR'
    # BaseWidget
    'Configure widget'                           = 'Configurar widget'
    'Refresh widget data'                        = 'Actualizar datos del widget'
    'Close widget'                               = 'Cerrar widget'
    'No data yet --- click refresh to retry'     = 'Sin datos --- haz clic en actualizar para reintentar'
  }
  'fr_FR' = @{
    'LIVE' = 'EN DIRECT'; 'OFFLINE' = 'HORS LIGNE'
    'Click to toggle UTC / local time' = "Cliquez pour basculer UTC / heure locale"
    '%1 WIDGETS' = '%1 WIDGETS'; 'COMPACT' = 'COMPACT'; 'PULSE' = 'POULS'
    'Force-refresh all live data on the dashboard' = "Forcer l'actualisation de toutes les donnees en direct"
    '+ ADD' = '+ AJOUTER'; ' UTC' = ' UTC'; ' LOC' = ' LOC'
    'SESSION:' = 'SESSION :'; 'LAYOUT:' = 'AGENCEMENT :'; 'FEEDS:' = 'FLUX :'
    'ACTIVE' = 'ACTIF'; 'EMPTY' = 'VIDE'; 'CONNECTED' = 'CONNECTE'; 'DISCONNECTED' = 'DECONNECTE'
    'READY' = 'PRET'; 'MEM: ---' = 'MEM : ---'; 'MEM: %1 MB' = 'MEM : %1 Mo'
    'LAT: ---' = 'LAT : ---'; 'LAT: ERR' = 'LAT : ERR'; 'LAT: %1ms' = 'LAT : %1ms'
    'SYMBOLS:' = 'SYMBOLES :'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = 'OK'; 'Edit Symbols...' = 'Modifier les symboles...'
    'MARKET PULSE' = 'POULS DU MARCHE'; 'FEAR & GREED INDEX' = 'INDICE PEUR ET AVIDITE'
    'MARKET BREADTH' = 'AMPLEUR DU MARCHE'; 'TOP GAINERS' = 'PRINCIPALES HAUSSES'
    'TOP LOSERS' = 'PRINCIPALES BAISSES'; 'GLOBAL SNAPSHOT' = 'APERCU GLOBAL'
    'MARKET HOURS' = 'HORAIRES DU MARCHE'; 'LOADING...' = 'CHARGEMENT...'
    'EXTREME FEAR' = 'PEUR EXTREME'; 'FEAR' = 'PEUR'; 'NEUTRAL' = 'NEUTRE'
    'GREED' = 'AVIDITE'; 'EXTREME GREED' = 'AVIDITE EXTREME'
    'OPEN' = 'OUVERT'; 'CLOSED' = 'FERME'; 'PRE' = 'PRE'
    '%1 AVAILABLE' = '%1 DISPONIBLE'; 'Search widgets...' = 'Rechercher des widgets...'
    'CANCEL' = 'ANNULER'; 'All' = 'Tous'
    'Choose Dashboard Template' = 'Choisir un modele de tableau de bord'
    'CHOOSE TEMPLATE' = 'CHOISIR UN MODELE'
    'Select a template to reset your dashboard. Current layout will be replaced.' = "Selectionnez un modele pour reinitialiser le tableau de bord. L'agencement actuel sera remplace."
    'APPLY' = 'APPLIQUER'
    'Configure widget' = 'Configurer le widget'; 'Refresh widget data' = 'Actualiser les donnees du widget'
    'Close widget' = 'Fermer le widget'
  }
  'it_IT' = @{
    'LIVE' = 'IN DIRETTA'; 'OFFLINE' = 'OFFLINE'
    'Click to toggle UTC / local time' = 'Clic per alternare UTC / ora locale'
    '%1 WIDGETS' = '%1 WIDGET'; 'COMPACT' = 'COMPATTO'; 'PULSE' = 'POLSO'
    'Force-refresh all live data on the dashboard' = "Forza l'aggiornamento di tutti i dati in tempo reale"
    '+ ADD' = '+ AGGIUNGI'; ' UTC' = ' UTC'; ' LOC' = ' LOC'
    'SESSION:' = 'SESSIONE:'; 'LAYOUT:' = 'LAYOUT:'; 'FEEDS:' = 'FEED:'
    'ACTIVE' = 'ATTIVO'; 'EMPTY' = 'VUOTO'; 'CONNECTED' = 'CONNESSO'; 'DISCONNECTED' = 'DISCONNESSO'
    'READY' = 'PRONTO'; 'MEM: ---' = 'MEM: ---'; 'MEM: %1 MB' = 'MEM: %1 MB'
    'LAT: ---' = 'LAT: ---'; 'LAT: ERR' = 'LAT: ERR'; 'LAT: %1ms' = 'LAT: %1ms'
    'SYMBOLS:' = 'SIMBOLI:'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = 'OK'; 'Edit Symbols...' = 'Modifica simboli...'
    'MARKET PULSE' = 'POLSO DEL MERCATO'; 'FEAR & GREED INDEX' = 'INDICE DI PAURA E AVIDITA'
    'MARKET BREADTH' = 'AMPIEZZA DEL MERCATO'; 'TOP GAINERS' = 'MIGLIORI GUADAGNI'
    'TOP LOSERS' = 'PEGGIORI PERDITE'; 'GLOBAL SNAPSHOT' = 'PANORAMICA GLOBALE'
    'MARKET HOURS' = 'ORARIO DI MERCATO'; 'LOADING...' = 'CARICAMENTO...'
    'EXTREME FEAR' = 'PAURA ESTREMA'; 'FEAR' = 'PAURA'; 'NEUTRAL' = 'NEUTRALE'
    'GREED' = 'AVIDITA'; 'EXTREME GREED' = 'AVIDITA ESTREMA'
    'OPEN' = 'APERTO'; 'CLOSED' = 'CHIUSO'; 'PRE' = 'PRE'
    '%1 AVAILABLE' = '%1 DISPONIBILE'; 'Search widgets...' = 'Cerca widget...'
    'CANCEL' = 'ANNULLA'; 'All' = 'Tutti'
    'Choose Dashboard Template' = 'Scegli modello dashboard'
    'CHOOSE TEMPLATE' = 'SCEGLI MODELLO'
    'Select a template to reset your dashboard. Current layout will be replaced.' = 'Seleziona un modello per reimpostare la dashboard. Il layout attuale verra sostituito.'
    'APPLY' = 'APPLICA'
    'Configure widget' = 'Configura widget'; 'Refresh widget data' = 'Aggiorna dati widget'
    'Close widget' = 'Chiudi widget'
  }
  'de_DE' = @{
    'LIVE' = 'LIVE'; 'OFFLINE' = 'OFFLINE'
    'Click to toggle UTC / local time' = 'Klicken zum Wechseln UTC / Ortszeit'
    '%1 WIDGETS' = '%1 WIDGETS'; 'COMPACT' = 'KOMPAKT'; 'PULSE' = 'PULS'
    'Force-refresh all live data on the dashboard' = 'Aktualisierung aller Live-Daten erzwingen'
    '+ ADD' = '+ HINZUFUEGEN'; ' UTC' = ' UTC'; ' LOC' = ' LOC'
    'SESSION:' = 'SITZUNG:'; 'LAYOUT:' = 'LAYOUT:'; 'FEEDS:' = 'FEEDS:'
    'ACTIVE' = 'AKTIV'; 'EMPTY' = 'LEER'; 'CONNECTED' = 'VERBUNDEN'; 'DISCONNECTED' = 'GETRENNT'
    'READY' = 'BEREIT'; 'MEM: ---' = 'MEM: ---'; 'MEM: %1 MB' = 'MEM: %1 MB'
    'LAT: ---' = 'LAT: ---'; 'LAT: ERR' = 'LAT: FEHLER'; 'LAT: %1ms' = 'LAT: %1ms'
    'SYMBOLS:' = 'SYMBOLE:'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = 'OK'; 'Edit Symbols...' = 'Symbole bearbeiten...'
    'MARKET PULSE' = 'MARKTPULS'; 'FEAR & GREED INDEX' = 'ANGST & GIER INDEX'
    'MARKET BREADTH' = 'MARKTBREITE'; 'TOP GAINERS' = 'TOP-GEWINNER'
    'TOP LOSERS' = 'TOP-VERLIERER'; 'GLOBAL SNAPSHOT' = 'GLOBALE UEBERSICHT'
    'MARKET HOURS' = 'HANDELSZEITEN'; 'LOADING...' = 'LADEN...'
    'EXTREME FEAR' = 'EXTREME ANGST'; 'FEAR' = 'ANGST'; 'NEUTRAL' = 'NEUTRAL'
    'GREED' = 'GIER'; 'EXTREME GREED' = 'EXTREME GIER'
    'OPEN' = 'OFFEN'; 'CLOSED' = 'GESCHLOSSEN'; 'PRE' = 'VOR'
    '%1 AVAILABLE' = '%1 VERFUEGBAR'; 'Search widgets...' = 'Widgets suchen...'
    'CANCEL' = 'ABBRECHEN'; 'All' = 'Alle'
    'Choose Dashboard Template' = 'Dashboard-Vorlage auswaehlen'
    'CHOOSE TEMPLATE' = 'VORLAGE WAEHLEN'
    'Select a template to reset your dashboard. Current layout will be replaced.' = 'Waehlen Sie eine Vorlage, um Ihr Dashboard zuruecksetzen. Das aktuelle Layout wird ersetzt.'
    'APPLY' = 'ANWENDEN'
    'Configure widget' = 'Widget konfigurieren'; 'Refresh widget data' = 'Widget-Daten aktualisieren'
    'Close widget' = 'Widget schliessen'
  }
  'pt_BR' = @{
    'LIVE' = 'AO VIVO'; 'OFFLINE' = 'OFFLINE'
    'Click to toggle UTC / local time' = 'Clique para alternar UTC / hora local'
    '%1 WIDGETS' = '%1 WIDGETS'; 'COMPACT' = 'COMPACTO'; 'PULSE' = 'PULSO'
    'Force-refresh all live data on the dashboard' = 'Forcar a atualizacao de todos os dados ao vivo'
    '+ ADD' = '+ ADICIONAR'; ' UTC' = ' UTC'; ' LOC' = ' LOC'
    'SESSION:' = 'SESSAO:'; 'LAYOUT:' = 'LAYOUT:'; 'FEEDS:' = 'FEEDS:'
    'ACTIVE' = 'ATIVO'; 'EMPTY' = 'VAZIO'; 'CONNECTED' = 'CONECTADO'; 'DISCONNECTED' = 'DESCONECTADO'
    'READY' = 'PRONTO'; 'MEM: ---' = 'MEM: ---'; 'MEM: %1 MB' = 'MEM: %1 MB'
    'LAT: ---' = 'LAT: ---'; 'LAT: ERR' = 'LAT: ERRO'; 'LAT: %1ms' = 'LAT: %1ms'
    'SYMBOLS:' = 'SIMBOLOS:'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = 'OK'; 'Edit Symbols...' = 'Editar simbolos...'
    'MARKET PULSE' = 'PULSO DO MERCADO'; 'FEAR & GREED INDEX' = 'INDICE DE MEDO E GANANCIA'
    'MARKET BREADTH' = 'AMPLITUDE DO MERCADO'; 'TOP GAINERS' = 'MAIORES ALTAS'
    'TOP LOSERS' = 'MAIORES BAIXAS'; 'GLOBAL SNAPSHOT' = 'PANORAMA GLOBAL'
    'MARKET HOURS' = 'HORARIO DE MERCADO'; 'LOADING...' = 'CARREGANDO...'
    'EXTREME FEAR' = 'MEDO EXTREMO'; 'FEAR' = 'MEDO'; 'NEUTRAL' = 'NEUTRO'
    'GREED' = 'GANANCIA'; 'EXTREME GREED' = 'GANANCIA EXTREMA'
    'OPEN' = 'ABERTO'; 'CLOSED' = 'FECHADO'; 'PRE' = 'PRE'
    '%1 AVAILABLE' = '%1 DISPONIVEL'; 'Search widgets...' = 'Buscar widgets...'
    'CANCEL' = 'CANCELAR'; 'All' = 'Todos'
    'Choose Dashboard Template' = 'Escolher modelo do painel'
    'CHOOSE TEMPLATE' = 'ESCOLHER MODELO'
    'Select a template to reset your dashboard. Current layout will be replaced.' = 'Selecione um modelo para redefinir seu painel. O layout atual sera substituido.'
    'APPLY' = 'APLICAR'
    'Configure widget' = 'Configurar widget'; 'Refresh widget data' = 'Atualizar dados do widget'
    'Close widget' = 'Fechar widget'
  }
  'tr_TR' = @{
    'LIVE' = 'CANLI'; 'OFFLINE' = 'COVRIMDISI'
    'Click to toggle UTC / local time' = 'UTC / yerel saati degistirmek icin tiklayin'
    '%1 WIDGETS' = '%1 WIDGET'; 'COMPACT' = 'KOMPAKT'; 'PULSE' = 'NABIZ'
    'Force-refresh all live data on the dashboard' = 'Tum canli verileri zorla yenile'
    '+ ADD' = '+ EKLE'; ' UTC' = ' UTC'; ' LOC' = ' LOC'
    'SESSION:' = 'OTURUM:'; 'LAYOUT:' = 'YERLESIM:'; 'FEEDS:' = 'AKISI:'
    'ACTIVE' = 'AKTIF'; 'EMPTY' = 'BOS'; 'CONNECTED' = 'BAGLI'; 'DISCONNECTED' = 'BAGLANTI YOK'
    'READY' = 'HAZIR'; 'MEM: ---' = 'MEM: ---'; 'MEM: %1 MB' = 'MEM: %1 MB'
    'LAT: ---' = 'LAT: ---'; 'LAT: ERR' = 'LAT: HATA'; 'LAT: %1ms' = 'LAT: %1ms'
    'SYMBOLS:' = 'SEMBOLLER:'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = 'TAMAM'; 'Edit Symbols...' = 'Sembolleri duzenle...'
    'MARKET PULSE' = 'PIYASA NABZI'; 'FEAR & GREED INDEX' = 'KORKU VE ACGOZLULUK ENDEKSI'
    'MARKET BREADTH' = 'PIYASA GENISLIGI'; 'TOP GAINERS' = 'EN COK KAZANANLAR'
    'TOP LOSERS' = 'EN COK KAYBEDENLER'; 'GLOBAL SNAPSHOT' = 'KUERESEL OZET'
    'MARKET HOURS' = 'PIYASA SAATLERI'; 'LOADING...' = 'YUKLENIYOR...'
    'EXTREME FEAR' = 'ASIRI KORKU'; 'FEAR' = 'KORKU'; 'NEUTRAL' = 'NOTR'
    'GREED' = 'ACGOZLULUK'; 'EXTREME GREED' = 'ASIRI ACGOZLULUK'
    'OPEN' = 'ACIK'; 'CLOSED' = 'KAPALI'; 'PRE' = 'ON'
    '%1 AVAILABLE' = '%1 KULLANILABILIR'; 'Search widgets...' = 'Widget ara...'
    'CANCEL' = 'IPTAL'; 'All' = 'Tumu'
    'Choose Dashboard Template' = 'Pano sablonu sec'
    'CHOOSE TEMPLATE' = 'SABLON SEC'
    'Select a template to reset your dashboard. Current layout will be replaced.' = 'Panoyu sifirlamak icin bir sablon secin. Mevcut duzen degistirilecek.'
    'APPLY' = 'UYGULA'
    'Configure widget' = "Widget'i yapilandir"; 'Refresh widget data' = 'Widget verilerini yenile'
    'Close widget' = "Widget'i kapat"
  }
  'vi_VN' = @{
    'LIVE' = 'TRUC TIEP'; 'OFFLINE' = 'NGOAI TUYEN'
    'Click to toggle UTC / local time' = 'Nhap de chuyen UTC / gio dia phuong'
    '%1 WIDGETS' = '%1 WIDGET'; 'COMPACT' = 'GON'; 'PULSE' = 'NHIP'
    'Force-refresh all live data on the dashboard' = 'Buoc lam moi moi du lieu truc tiep'
    '+ ADD' = '+ THEM'; ' UTC' = ' UTC'; ' LOC' = ' LOC'
    'SESSION:' = 'PHIEN:'; 'LAYOUT:' = 'BO CUC:'; 'FEEDS:' = 'NGUON:'
    'ACTIVE' = 'HOAT DONG'; 'EMPTY' = 'TRONG'; 'CONNECTED' = 'DA KET NOI'; 'DISCONNECTED' = 'MAT KET NOI'
    'READY' = 'SAN SANG'; 'MEM: ---' = 'MEM: ---'; 'MEM: %1 MB' = 'MEM: %1 MB'
    'LAT: ---' = 'LAT: ---'; 'LAT: ERR' = 'LAT: LOI'; 'LAT: %1ms' = 'LAT: %1ms'
    'SYMBOLS:' = 'MA:'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = 'OK'; 'Edit Symbols...' = 'Chinh sua ma...'
    'MARKET PULSE' = 'NHIP DAP THI TRUONG'; 'FEAR & GREED INDEX' = 'CHI SO SO HAI VA THAM LAM'
    'MARKET BREADTH' = 'DO RONG THI TRUONG'; 'TOP GAINERS' = 'TANG MANH NHAT'
    'TOP LOSERS' = 'GIAM MANH NHAT'; 'GLOBAL SNAPSHOT' = 'TONG QUAN TOAN CAU'
    'MARKET HOURS' = 'GIO GIAO DICH'; 'LOADING...' = 'DANG TAI...'
    'EXTREME FEAR' = 'SO HAI CUC DO'; 'FEAR' = 'SO HAI'; 'NEUTRAL' = 'TRUNG LAP'
    'GREED' = 'THAM LAM'; 'EXTREME GREED' = 'THAM LAM CUC DO'
    'OPEN' = 'MO CUA'; 'CLOSED' = 'DONG CUA'; 'PRE' = 'TRUOC'
    '%1 AVAILABLE' = '%1 KHA DUNG'; 'Search widgets...' = 'Tim widget...'
    'CANCEL' = 'HUY'; 'All' = 'Tat ca'
    'Choose Dashboard Template' = 'Chon mau bang dieu khien'
    'CHOOSE TEMPLATE' = 'CHON MAU'
    'Select a template to reset your dashboard. Current layout will be replaced.' = 'Chon mot mau de dat lai bang dieu khien. Bo cuc hien tai se duoc thay the.'
    'APPLY' = 'AP DUNG'
    'Configure widget' = 'Cau hinh widget'; 'Refresh widget data' = 'Lam moi du lieu widget'
    'Close widget' = 'Dong widget'
  }
  'id_ID' = @{
    'LIVE' = 'LANGSUNG'; 'OFFLINE' = 'LURING'
    'Click to toggle UTC / local time' = 'Klik untuk beralih UTC / waktu lokal'
    '%1 WIDGETS' = '%1 WIDGET'; 'COMPACT' = 'RINGKAS'; 'PULSE' = 'DENYUT'
    'Force-refresh all live data on the dashboard' = 'Paksa muat ulang semua data langsung'
    '+ ADD' = '+ TAMBAH'; ' UTC' = ' UTC'; ' LOC' = ' LOC'
    'SESSION:' = 'SESI:'; 'LAYOUT:' = 'TATA LETAK:'; 'FEEDS:' = 'UMPAN:'
    'ACTIVE' = 'AKTIF'; 'EMPTY' = 'KOSONG'; 'CONNECTED' = 'TERHUBUNG'; 'DISCONNECTED' = 'TERPUTUS'
    'READY' = 'SIAP'; 'MEM: ---' = 'MEM: ---'; 'MEM: %1 MB' = 'MEM: %1 MB'
    'LAT: ---' = 'LAT: ---'; 'LAT: ERR' = 'LAT: ERR'; 'LAT: %1ms' = 'LAT: %1ms'
    'SYMBOLS:' = 'SIMBOL:'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = 'OK'; 'Edit Symbols...' = 'Edit simbol...'
    'MARKET PULSE' = 'DENYUT PASAR'; 'FEAR & GREED INDEX' = 'INDEKS KETAKUTAN DAN KESERAKAHAN'
    'MARKET BREADTH' = 'KELUASAN PASAR'; 'TOP GAINERS' = 'KENAIKAN TERTINGGI'
    'TOP LOSERS' = 'PENURUNAN TERTINGGI'; 'GLOBAL SNAPSHOT' = 'GAMBARAN GLOBAL'
    'MARKET HOURS' = 'JAM PASAR'; 'LOADING...' = 'MEMUAT...'
    'EXTREME FEAR' = 'KETAKUTAN EKSTREM'; 'FEAR' = 'KETAKUTAN'; 'NEUTRAL' = 'NETRAL'
    'GREED' = 'KESERAKAHAN'; 'EXTREME GREED' = 'KESERAKAHAN EKSTREM'
    'OPEN' = 'BUKA'; 'CLOSED' = 'TUTUP'; 'PRE' = 'PRA'
    '%1 AVAILABLE' = '%1 TERSEDIA'; 'Search widgets...' = 'Cari widget...'
    'CANCEL' = 'BATAL'; 'All' = 'Semua'
    'Choose Dashboard Template' = 'Pilih template dasbor'
    'CHOOSE TEMPLATE' = 'PILIH TEMPLATE'
    'Select a template to reset your dashboard. Current layout will be replaced.' = 'Pilih template untuk mengatur ulang dasbor Anda. Tata letak saat ini akan diganti.'
    'APPLY' = 'TERAPKAN'
    'Configure widget' = 'Konfigurasi widget'; 'Refresh widget data' = 'Muat ulang data widget'
    'Close widget' = 'Tutup widget'
  }
  'zh_CN' = @{
    'LIVE' = '实时'; 'OFFLINE' = '离线'
    'Click to toggle UTC / local time' = '点击切换 UTC / 本地时间'
    '%1 WIDGETS' = '%1 个组件'; 'COMPACT' = '紧凑'; 'PULSE' = '脉搏'
    'Force-refresh all live data on the dashboard' = '强制刷新仪表板上的所有实时数据'
    '+ ADD' = '+ 添加'; ' UTC' = ' UTC'; ' LOC' = ' 本地'
    'SESSION:' = '会话：'; 'LAYOUT:' = '布局：'; 'FEEDS:' = '数据源：'
    'ACTIVE' = '活跃'; 'EMPTY' = '空'; 'CONNECTED' = '已连接'; 'DISCONNECTED' = '已断开'
    'READY' = '就绪'; 'MEM: ---' = '内存: ---'; 'MEM: %1 MB' = '内存: %1 MB'
    'LAT: ---' = '延迟: ---'; 'LAT: ERR' = '延迟: 错误'; 'LAT: %1ms' = '延迟: %1ms'
    'SYMBOLS:' = '代码：'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = '确定'; 'Edit Symbols...' = '编辑代码...'
    'MARKET PULSE' = '市场脉搏'; 'FEAR & GREED INDEX' = '恐惧与贪婪指数'
    'MARKET BREADTH' = '市场广度'; 'TOP GAINERS' = '涨幅榜'
    'TOP LOSERS' = '跌幅榜'; 'GLOBAL SNAPSHOT' = '全球概览'
    'MARKET HOURS' = '交易时段'; 'LOADING...' = '加载中...'
    'EXTREME FEAR' = '极度恐惧'; 'FEAR' = '恐惧'; 'NEUTRAL' = '中性'
    'GREED' = '贪婪'; 'EXTREME GREED' = '极度贪婪'
    'OPEN' = '开市'; 'CLOSED' = '休市'; 'PRE' = '盘前'
    '%1 AVAILABLE' = '%1 可用'; 'Search widgets...' = '搜索组件...'
    'CANCEL' = '取消'; 'All' = '全部'
    'Choose Dashboard Template' = '选择仪表板模板'
    'CHOOSE TEMPLATE' = '选择模板'
    'Select a template to reset your dashboard. Current layout will be replaced.' = '选择模板以重置仪表板。当前布局将被替换。'
    'APPLY' = '应用'
    'Configure widget' = '配置组件'; 'Refresh widget data' = '刷新组件数据'
    'Close widget' = '关闭组件'
  }
  'zh_HK' = @{
    'LIVE' = '即時'; 'OFFLINE' = '離線'
    'Click to toggle UTC / local time' = '點擊切換 UTC / 本地時間'
    '%1 WIDGETS' = '%1 個小工具'; 'COMPACT' = '精簡'; 'PULSE' = '脈搏'
    'Force-refresh all live data on the dashboard' = '強制重新整理儀表板上所有即時數據'
    '+ ADD' = '+ 新增'; ' UTC' = ' UTC'; ' LOC' = ' 本地'
    'SESSION:' = '工作階段：'; 'LAYOUT:' = '版面：'; 'FEEDS:' = '資料來源：'
    'ACTIVE' = '使用中'; 'EMPTY' = '空白'; 'CONNECTED' = '已連線'; 'DISCONNECTED' = '已中斷'
    'READY' = '就緒'; 'MEM: ---' = '記憶體: ---'; 'MEM: %1 MB' = '記憶體: %1 MB'
    'LAT: ---' = '延遲: ---'; 'LAT: ERR' = '延遲: 錯誤'; 'LAT: %1ms' = '延遲: %1ms'
    'SYMBOLS:' = '代號：'; 'AAPL, MSFT, ^GSPC, BTC-USD ...' = 'AAPL, MSFT, ^GSPC, BTC-USD ...'
    'OK' = '確定'; 'Edit Symbols...' = '編輯代號...'
    'MARKET PULSE' = '市場脈搏'; 'FEAR & GREED INDEX' = '恐懼與貪婪指數'
    'MARKET BREADTH' = '市場廣度'; 'TOP GAINERS' = '升幅榜'
    'TOP LOSERS' = '跌幅榜'; 'GLOBAL SNAPSHOT' = '全球概覽'
    'MARKET HOURS' = '交易時段'; 'LOADING...' = '載入中...'
    'EXTREME FEAR' = '極度恐懼'; 'FEAR' = '恐懼'; 'NEUTRAL' = '中性'
    'GREED' = '貪婪'; 'EXTREME GREED' = '極度貪婪'
    'OPEN' = '開市'; 'CLOSED' = '休市'; 'PRE' = '盤前'
    '%1 AVAILABLE' = '%1 可用'; 'Search widgets...' = '搜尋小工具...'
    'CANCEL' = '取消'; 'All' = '全部'
    'Choose Dashboard Template' = '選擇儀表板範本'
    'CHOOSE TEMPLATE' = '選擇範本'
    'Select a template to reset your dashboard. Current layout will be replaced.' = '選擇範本以重設儀表板。目前版面將被取代。'
    'APPLY' = '套用'
    'Configure widget' = '設定小工具'; 'Refresh widget data' = '重新整理小工具資料'
    'Close widget' = '關閉小工具'
  }
}

function Escape-XmlAttr([string]$s) {
  return $s.Replace('&','&amp;').Replace('<','&lt;').Replace('>','&gt;')
}

foreach ($lang in $trans.Keys) {
  $ts_path = "translations/fincept_${lang}.ts"
  if (-not (Test-Path $ts_path)) { continue }

  $content = Get-Content $ts_path -Raw -Encoding utf8
  $count = 0
  foreach ($source in $trans[$lang].Keys) {
    $translation = $trans[$lang][$source]
    # Source/translation may contain XML metacharacters
    $src_xml = Escape-XmlAttr $source
    $trans_xml = Escape-XmlAttr $translation
    # Pattern: matches the multiline message block where the source is exactly src_xml
    # and the translation is `type="unfinished"` (may have empty body or heuristic body).
    $pattern = '(<message>\s*<source>' + [regex]::Escape($src_xml) + '</source>\s*<translation\s+type="unfinished">)[^<]*(</translation>\s*</message>)'
    $replacement = '<message>' + "`n" + '        <source>' + $src_xml + '</source>' + "`n" + '        <translation>' + $trans_xml + '</translation>' + "`n" + '    </message>'
    $new = [regex]::Replace($content, $pattern, $replacement)
    if ($new -ne $content) {
      $count++
      $content = $new
    }
  }
  Set-Content -Path $ts_path -Value $content -Encoding utf8 -NoNewline
  Write-Output "${lang}: $count translations filled"
}
