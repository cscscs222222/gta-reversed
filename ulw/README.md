# Ultimate Living World 0.6.0

Bu geliştirme sürümü klasik GTA San Andreas Hoodlum 1.0 US içindir.

| Sistem | Bu paketteki değişiklik |
| --- | --- |
| Siviller | Silah sesine ve yakındaki yangına mesafe, görüş ve yakın geçmişe göre tepki verir. |
| Polis | Yaya devriyeler ve polis otomobilleri son görüş konumunu paylaşabilir. Kapsanan araç takibinde polis, görüş kaybından sonra kaydedilen noktaya doğru yol seçer ve yaklaştığında daha geniş bir alanı tarar. |
| Hava desteği | Serbest dolaşımda polis helikopteri görüş bilgisini devriyelerle paylaşır. Görüş kesilince son bildirilen bölgeyi tarar. Gece projektörü tarama konumuna yönelir. |
| Barikatlar | Oyunun dinamik barikat adayları yol genişliği, araç hızı, takip bilgisi ve kamera görüşüne göre elenir. Oyun yol geometrisini ve barikat araçlarını kendi yönetir. |
| Trafik | Sivil sürücüler yağmur, sis, gece, çarpışma veya arkadan yaklaşan siren nedeniyle yavaşlayabilir. Ağır çarpışmadan sonra duran araçlar aynı seviyede arkadan yaklaşan trafiği de yavaşlatabilir. Mevcut kırmızı ışık ve trafik sınırları uygulanmaya devam eder. |
| Otomobiller | Islak yol ve ağır hasar yol tutuşunu azaltır. Uzun süreli sert frenleme fren etkisini, ağır hasar da aracın çekişini düşürebilir. |
| Çatışma | Uygun serbest dolaşım karşılaşmalarında oyuncuya ateş eden polis ve çete üyelerinin isabeti mesafe, aydınlık, hava ve can durumuna göre değişir. Açık havada yakın çatışmalar zorlaşır. |
| Şehir | Trafik ve sivil yaya yoğunluğu saat, gün, bölge ve hava durumuna göre değişir. Mevcut durakların, bankların, yiyecek noktalarının ve yağmur barınaklarının kullanım sıklığı bu koşullara uyarlanır. |
| Görevler | ULW görev işaretlerini ve motorun çalışan görev betiğini izler. Görevlerde, ara sahnelerde ve görev sonu toparlanma sırasında yeni dünya davranışlarını askıya alır. |

## Test kopyasına kurulum

1. Oyunun ayrı bir test kopyasını kullan. Bu sürüm, `gta_sa.exe` dosyası SHA-256 değeri `A559AA772FD136379155EFA71F00C47AAD34BBFEAE6196B0FE1047D0645CBD26` olan Hoodlum 1.0 US ve `InitializeASI` çağıran Ultimate ASI Loader gerektirir. İstersen `Verify-Game.ps1` aracını test kopyasının yolu ile çalıştırarak kontrol edebilirsin.
2. GTA kapalıyken `UltimateLivingWorld.asi` ve `UltimateLivingWorld.ini` dosyalarını test kopyasının `scripts` klasörüne koy. İlk denemede test kopyasındaki `gta_reversed.asi` dosyasını devre dışı bırak. Bu iki eklentinin aynı oyun sürecinde davranışı henüz doğrulanmadı.
3. GTA açıldığında `scripts\UltimateLivingWorld.log` dosyasındaki `Installed` ve `Skipped` satırlarından hangi sistemlerin etkinleştiğini kontrol edebilirsin.

Paketi kaldırmak için oyun kapalıyken kopyaladığın iki dosyayı kaldır. `Enabled=0` tüm eklentiyi devre dışı bırakır. `[Modules]` bölümünde bir sistemin değerini `0` yaparak sonraki açılışta o sistemi kapatabilirsin.

## Ultimate Chaos Missions

Test kopyasında zaten kullandığın Chaos Missions kampanyasıyla aynı kurulumu yap. Pakette görev dosyası yoktur. `Verify-Game.ps1`, orijinal `main.scm` ile tanınan Chaos Missions Zor ve Acımasız sürümlerini ayırt eder ve görev bloklarının motor sınırlarını kontrol eder. Farklı bir kampanya dosyası da oyun içindeki genel görev korumasını kullanır.

| Oyunda açılacak görev | Günlükte beklenen görev adı |
| --- | --- |
| Wrong Side of the Tracks | `SMOKE3` |
| Just Business | `DRUGS1` |
| Supply Lines... | `ZERO2` |
| End of the Line | Üç aşama için `FINALEA`, `FINALEB`, `FINALEC` |

Görevi başlattığında `scripts\UltimateLivingWorld.log` içinde `Mission protection` veya `Cutscene protection` satırını ara. Görev sona erince kısa bir beklemenin ardından `Free roam: world systems resumed.` görünür. Bu süre, tren kovalamacası, betikli araç takibi, uzaktan kumandalı uçuş ve final görevleri için uzatılır.

## Kontroller

Windows derlemesi sivil tepkileri, hava ve kara devriyeleri arasındaki konum paylaşımını, arama rotalarını, barikat adaylarını, trafik kazası algılamayı, fren ve tutuşu, isabet profillerini, kentteki kullanım saatlerini ve serbest dolaşımla görevler arasındaki geçişleri otomatik olarak test eder. Windows test süreci ASI’yi yükler ve bağlantıların oyun dışı bir süreçte devre dışı kaldığını kontrol eder.

Oyun içi doğrulama için gece serbest dolaşımda helikopter takibini başlat ve kapalı bir alana girerek görüş hattını kes. Araçla kovalamacada açık görüş alanında barikatların yerleşimini ve köşeyi döndükten sonra polislerin yönünü karşılaştır. Arama ve dinamik barikat kuralları serbest dolaşımda çalışır. Görevlerin, kayıt yüklemenin ve diğer eklentilerle birlikte çalışmanın oyun içi testleri bekliyor.

Günlük oyun zamanı ilerledikçe NPC kaçışı, devriye rotaları, hava araması, barikat yerleşim denemeleri, kazaya bağlı yavaşlama ve etkinlik noktalarının kullanımını sayar. `AirSupport` ve `Roadblocks` ayarları yeni sistemleri ayrı ayrı kapatır. Farklı bir ASI aynı oyun çağrısını önceden değiştirmişse Ultimate Living World ilgili bağlantıyı atlar ve durumu kaydeder. ASI değişikliklerini oyun kapalıyken yap.

## Derleme

`build.cmd` var olan MSVC x86 ortamını kullanır ve paketi `bin/ulw/dist` altında üretir. GitHub Actions’taki **Build** iş akışı aynı adımları çalıştırır. Başarılı çalışmanın **Artifacts** bölümünde **UltimateLivingWorld** paketi çıkar. Arşivde ASI, INI, doğrulama betiği ve bu kılavuz bulunur.

Bireysel NPC iş ve ev rotaları, yeni şehir etkinlikleri, ekonomi, ayrıntılı yakalama taktikleri, diğer taşıtların fiziği ve Chaos Missions görevlerinin oyun içi uyumluluk testleri tam sürüm için bekliyor.
