# Ultimate Living World 0.4.0

Bu geliştirme sürümü klasik GTA San Andreas Hoodlum 1.0 US içindir.

| Sistem | Bu paketteki değişiklik |
| --- | --- |
| Siviller | Silah sesine ve yakındaki yangına mesafe, görüş ve yakın geçmişe göre tepki verir. |
| Polis | Yaya devriyeler ve polis otomobilleri son görüş konumunu paylaşabilir. Kapsanan araç takibinde polis, görüş kaybından sonra kaydedilen noktaya doğru yol seçer ve yaklaştığında daha geniş bir alanı tarar. |
| Trafik | Sivil sürücüler yağmur, sis, gece, çarpışma veya arkadan yaklaşan siren nedeniyle yavaşlayabilir. Ağır çarpışmadan sonra duran araçlar aynı seviyede arkadan yaklaşan trafiği de yavaşlatabilir. Mevcut kırmızı ışık ve trafik sınırları uygulanmaya devam eder. |
| Otomobiller | Islak yol ve ağır hasar yol tutuşunu azaltır. Uzun süreli sert frenleme fren etkisini, ağır hasar da aracın çekişini düşürebilir. |
| Çatışma | Uygun serbest dolaşım karşılaşmalarında oyuncuya ateş eden polis ve çete üyelerinin isabeti mesafe, aydınlık, hava ve can durumuna göre değişir. Açık havada yakın çatışmalar zorlaşır. |
| Şehir | Trafik ve sivil yaya yoğunluğu saat, gün, bölge ve hava durumuna göre değişir. Mevcut durakların, bankların, yiyecek noktalarının ve yağmur barınaklarının kullanım sıklığı bu koşullara uyarlanır. |

## Test kopyasına kurulum

1. Oyunun ayrı bir test kopyasını kullan. Bu sürüm, `gta_sa.exe` dosyası SHA-256 değeri `A559AA772FD136379155EFA71F00C47AAD34BBFEAE6196B0FE1047D0645CBD26` olan Hoodlum 1.0 US ve `InitializeASI` çağıran Ultimate ASI Loader gerektirir. İstersen `Verify-Game.ps1` aracını test kopyasının yolu ile çalıştırarak kontrol edebilirsin.
2. GTA kapalıyken `UltimateLivingWorld.asi` ve `UltimateLivingWorld.ini` dosyalarını test kopyasının `scripts` klasörüne koy. İlk denemede test kopyasındaki `gta_reversed.asi` dosyasını devre dışı bırak. Bu iki eklentinin aynı oyun sürecinde davranışı henüz doğrulanmadı.
3. GTA açıldığında `scripts\UltimateLivingWorld.log` dosyasındaki `Installed` ve `Skipped` satırlarından hangi sistemlerin etkinleştiğini kontrol edebilirsin.

Paketi kaldırmak için oyun kapalıyken kopyaladığın iki dosyayı kaldır. `Enabled=0` tüm eklentiyi devre dışı bırakır. `[Modules]` bölümünde bir sistemin değerini `0` yaparak sonraki açılışta o sistemi kapatabilirsin.

## Kontroller

Windows derlemesi sivil tepkileri, devriyeler arası konum paylaşımı, araç araması, trafik kazası algılama, fren ve tutuş, isabet profilleri ve kentteki kullanım saatleri için otomatik testleri çalıştırır. Ayrıca Windows test sürecinde ASI yüklenir ve oyun olmayan bir yürütülebilir dosyada bağlantıların devre dışı kaldığı kontrol edilir.

Oyun içi doğrulama için serbest dolaşımda bir polis otomobilini peşine tak ve köşeyi dönüp görüş hattını kes. Trafikte duran ağır hasarlı bir aracın arkasından gelen sürücüleri ve yağmurda barınak kullanımını gözle. Görev bayrakları ve ara sahne durumları yeni davranışları sınırlamak için kullanılır. Görevlerin, kayıt yüklemenin ve diğer eklentilerle birlikte çalışmanın oyun içi testleri bekliyor.

Günlük oyun zamanı ilerledikçe NPC kaçışı, devriye rotaları, kazaya bağlı yavaşlama ve etkinlik noktalarının kullanımını sayar. Farklı bir ASI aynı oyun çağrısını önceden değiştirmişse Ultimate Living World ilgili bağlantıyı atlar ve durumu kaydeder. ASI değişikliklerini oyun kapalıyken yap.

## Derleme

`build.cmd` var olan MSVC x86 ortamını kullanır ve paketi `bin/ulw/dist` altında üretir. GitHub Actions’taki **Build** iş akışı aynı adımları çalıştırır. Başarılı çalışmanın **Artifacts** bölümünde **UltimateLivingWorld** paketi çıkar. Arşivde ASI, INI, doğrulama betiği ve bu kılavuz bulunur.

Bireysel NPC iş ve ev rotaları, yeni şehir etkinlikleri, ekonomi, polis helikopterleri ve barikatlar, diğer taşıtların fiziği ve Ultimate Chaos Missions entegrasyonu tam sürüm için bekliyor.
