# Ultimate Living World 0.2.0

Bu geliştirme sürümü klasik GTA San Andreas Hoodlum 1.0 US içindir.

| Sistem | Bu paketteki değişiklik |
| --- | --- |
| Siviller | Silah sesine ve yakındaki yangına mesafe, görüş ve yakın geçmişe göre tepki verir. |
| Polis | Serbest dolaşımda oyuncuyu arayan yaya polisler görüşü kaybedince son gördükleri konuma yönelir. Yakındaki polisler yeni bir görüş bildirimi alabilir ve aynı noktaya ulaştıklarında çevreye dağılır. |
| Trafik | Sivil sürücüler yağmur, sis, gece, yakın zamanda çarpışma veya arkadan yaklaşan siren nedeniyle yavaşlayabilir. Mevcut kırmızı ışık ve trafik hız sınırları uygulanmaya devam eder. |
| Otomobiller | Islak yol ve ağır hasar yol tutuşunu azaltır. Uzun süreli sert frenleme fren etkisini, ağır hasar da aracın çekişini düşürebilir. |
| Çatışma | Uygun serbest dolaşım karşılaşmalarında oyuncuya ateş eden polis ve çete üyelerinin isabeti mesafe, aydınlık, hava ve can durumuna göre değişir. Açık havada yakın çatışmalar zorlaşır. |

## Test kopyasına kurulum

1. Oyunun ayrı bir test kopyasını kullan. Bu sürüm, `gta_sa.exe` dosyası SHA-256 değeri `A559AA772FD136379155EFA71F00C47AAD34BBFEAE6196B0FE1047D0645CBD26` olan Hoodlum 1.0 US ve `InitializeASI` çağıran Ultimate ASI Loader gerektirir. İstersen `Verify-Game.ps1` aracını test kopyasının yolu ile çalıştırarak kontrol edebilirsin.
2. GTA kapalıyken `UltimateLivingWorld.asi` ve `UltimateLivingWorld.ini` dosyalarını test kopyasının `scripts` klasörüne koy. İlk denemede test kopyasındaki `gta_reversed.asi` dosyasını devre dışı bırak. Bu iki eklentinin aynı oyun sürecinde davranışı henüz doğrulanmadı.
3. GTA açıldığında `scripts\UltimateLivingWorld.log` dosyasındaki `Installed` ve `Skipped` satırlarından hangi sistemlerin etkinleştiğini kontrol edebilirsin.

Paketi kaldırmak için oyun kapalıyken kopyaladığın iki dosyayı kaldır. `Enabled=0` tüm eklentiyi devre dışı bırakır. `[Modules]` bölümünde bir sistemin değerini `0` yaparak sonraki açılışta o sistemi kapatabilirsin.

## Kontroller

Windows derlemesi sivil olay bütçesi, tepki belleği ve yangın, polisin görüş kaybı ve ekip içi bildirimleri, trafik limitleri, fren ısısı, yol tutuşu ve isabet profilleri için otomatik testleri çalıştırır. Ayrıca Windows test sürecinde ASI yüklenir ve oyun olmayan bir yürütülebilir dosyada bağlantıların devre dışı kaldığı kontrol edilir.

Oyun içi doğrulama için serbest dolaşımda yakındaki sivillerin tepki süresini, yaya polislerin bir köşeden sonra ne yöne baktığını ve ıslak zeminde fren mesafesini karşılaştır. Sonra bir hikâye görevini başlat. Görev bayrakları ve ara sahne durumları, yeni davranışları sınırlamak için kullanılır. Görevlerin, save yüklemenin ve diğer eklentilerle birlikte çalışmanın oyun içi testleri henüz yapılmadı.

Günlük NPC kaçışı, polis araması ve trafik hız sınırlaması sayımlarını oyun zamanı ilerledikçe yazar. Farklı bir ASI aynı oyun çağrısını önceden değiştirmişse Ultimate Living World ilgili bağlantıyı atlar ve durumu kaydeder. Çalışan oyun sırasında ASI’yi değiştirme.

## Derleme

`build.cmd` var olan MSVC x86 ortamını kullanır ve paketi `bin/ulw/dist` altında üretir. GitHub Actions’taki **Build** iş akışı aynı adımları çalıştırır. Başarılı çalışmanın **Artifacts** bölümünde **UltimateLivingWorld** paketi çıkar. Arşivde ASI, INI, doğrulama betiği ve bu kılavuz bulunur.

Planlanan şehir rutinleri, ekonomi, polis araçları ve helikopter taktikleri, fizik kapsamının diğer taşıtlara genişletilmesi ve Ultimate Chaos Missions entegrasyonu henüz tamamlanmadı.
