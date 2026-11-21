# Ultimate Living World 0.20.0

Bu geliştirme sürümü klasik GTA San Andreas Hoodlum 1.0 US içindir.

| Sistem | Bu paketteki değişiklik |
| --- | --- |
| Siviller | Silah sesi, yangın ve kazalar farklı tepkiler doğurur. Ağır bir kaza karşısında bazı yayalar bakar, bazıları kısa süre bekler veya uzaklaşır. Kaza güvenli bir mesafeden görülürse uygun bir tanık oyunun telefon animasyonuyla polis için ihbar bırakabilir. |
| Polis | Yaya ve araç ekipleri son görüş konumunu paylaşabilir. Silah sesi ve tanıklar bir olay yeri ihbarı oluşturabilir. Erişilebilir bir ihbara yakın yaya polis kısa süre olay yerini inceleyebilir. Aranma seviyesi yokken araçlı devriyeler gördükleri tehlikeli sürüşe uyarı verebilir. Düşük aranma seviyesinde oyuncu teslim olabilir. Üç yıldızdan itibaren ikinci ekip yol kesme rotası seçebilir. |
| Mağazalar ve tamir | Yemek, giyim, kişisel bakım ve araç modifiye fiyatları bölgeye göre değişir. Menü ve kasa aynı hesabı kullanır. Pay ’n’ Spray ücreti kırsalda $85, Los Santos’ta $100, San Fierro’da $112 ve Las Venturas’ta $124. Oyunun ücretsiz tamir koşulları geçerlidir. `Economy=0` bölgesel fiyatları kapatır. |
| Hava desteği | Serbest dolaşımda polis helikopteri görüş bilgisini devriyelerle paylaşır. Görüş kesilince son bildirilen bölgeyi tarar. Gece projektörü tarama konumuna yönelir. |
| Barikatlar | Oyunun dinamik barikat adayları yol genişliği, araç hızı, takip bilgisi ve kamera görüşüne göre elenir. Oyun yol geometrisini ve barikat araçlarını kendi yönetir. |
| Trafik | Sürücüler profil ve araç tipine göre farklı takip mesafesi bırakabilir. Düz, çok şeritli bir yolda uzun süre yavaş kalan aracın arkasındaki sürücü, yan şeritte güvenli boşluk varsa geçebilir. Ardından uygun mesafede kendi şeridine döner. Yaklaşan dönüş, sinyal, hava ve sirenler hızı etkiler. |
| Araç kurtarma | Ağır bir çarpışmadan sonra uygun sivil sürücü, mevcut yol yüzeyini ve çevresindeki araçları kontrol ederek boş bir yol kenarına yönelebilir. Motoru ağır hasarlı araç tamamen durduğunda sürücü oyunun araçtan inme görevini kullanabilir. |
| Acil servis | Ambulans ve itfaiye ekipleri, oyun tarafından belirlenen olay yerine yaklaşırken hızlarını daha erken düşürür. Islak zeminde ve siste fren daha erken başlar. Oyunun rota ve ekip görevleri kullanılır. |
| Araç fiziği | Otomobil ve motosikletlerin her tekerleği temas ettiği yüzeye, yağmura, kaymaya ve lastik sıcaklığına göre farklı tepki verir. Otomobillerde süspansiyon sert sıkışmalara ve geri sekmeye uyarlanır. Gaz tepkisi ve vites geçiş noktaları sınıfa, gaza ve eğime göre değişir. Bölgesel hasar, motor ısısı ve fren yorgunluğu da sürüşü etkiler. |
| Uçuş ve tekneler | Hafif uçakların, jetlerin ve nakliye uçaklarının dönüş tepkileri hız ve irtifa ile değişir. Ağır helikopterler daha kademeli tepki verir. Sürat tekneleri ve ağır tekneler farklı su direnci ve su üstünde kayma tepkisi kullanır. |
| Çatışma | Art arda vurulan silahlı polis ve çete üyeleri kısa serilere geçer, uygun siper arar ve güvenli bir noktaya ulaşınca kısa süre bekler. Cephane azalırsa veya silah şarjör değiştiriyorsa bekleme uzayabilir. Bir yanda ateş hattı açılırsa NPC o tarafa ilerleyebilir. Ağır yaralanan veya cephanesi tükenen NPC uygun bir yoldan kısa süre geri çekilebilir. |
| Şehir | Trafik ve yaya yoğunluğu saate, güne, bölgeye ve havaya göre değişir. Uygun yayalar sabah duraklara, öğle saatinde yiyecek noktalarına, akşam bank veya merdivenlere, yağmurda sığınaklara yürüyebilir. Oyunun mevcut etkileşim noktaları ve sıra sistemi kullanılır. |
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

Windows derlemesi sivil tepkileri, yaya temposunu, kaza tanığı belleğini, kazadan sonra duran araçların yaya rotalarına etkisini, kara ve hava devriyelerini, barikatları, araç davranışını ve görev geçişlerini otomatik olarak test eder. Windows test süreci ASI’yi yükler ve bağlantıların oyun dışı bir süreçte devre dışı kaldığını kontrol eder.

Yeni yaya davranışını görmek için serbest dolaşımda yakındaki bir sivil otomobil ağır çarpışmadan sonra kaldırıma yakın durduğunda yayaları gözle. Ayrı bir denemede açık hava ile yağmurda sokaktaki yürüme hızını ve konuşma sıklığını karşılaştır. Günlükteki `crashGlances` ve `crashReroutes` sayaçları bu kararların çalışıp çalışmadığını gösterir. Oyun içi testler bekliyor.

Şehir rutinlerini denemek için hafta içi sabah bir durağın yakınında yayaları izle. Öğle saatinde yiyecek tezgâhlarında, yağmurda kapalı bekleme noktalarında aynı karşılaştırmayı yap. `scheduledVisits` başlatılan ziyaretleri kaydeder. Ağır bir kazadan sonra `crashWaits`, `crashDepartures` ve `crashCalls` sayaçları duraklama, uzaklaşma ve tamamlanan telefon çağrılarını gösterir. Telefon ihbarı devriye incelemesine eklenir.

Araçları denemek için serbest dolaşımda aynı motosikletle kuru ve ıslak yolda fren yap. Hasarlı bir otomobilde düşük hızda uzun süre gaz verdikten sonra hızlanmayı dene. Ardından gazı azaltıp motorun soğumasını bekle. Windows testleri ısınma, soğuma, tamir sonrası sıfırlama ve bölgesel hasar hesaplarını kapsar.

Yeni yol tepkilerini karşılaştırmak için aynı aracı asfalt, çim ve kumda sür. Ardından bir spor otomobil ve kamyonla aynı eğimde yarım gazdan tam gaza geç. `surfaceResponses`, `springResponses` ve `driveResponses` sırasıyla zemin, süspansiyon ve şanzıman kararlarının işlendiği çağrıları sayar. Bu davranışlar serbest dolaşımda oyuncunun kullandığı araçlara uygulanır.

Serbest dolaşımda Stuntplane ve AT-400 ile kalkıp yeterli yüksekliğe ulaştığında dönüşlerini karşılaştır. Cargobob ile Maverick'i ileri uçuşta, Jetmax ile Reefer'ı açık suda dene. Uçuş ayarları piste yaklaşınca kademeli azalır. `planeResponses`, `helicopterResponses` ve `boatResponses` ilgili motor hesaplarına uygulanan ayarları sayar. Uçuş ve tekne ayarları `VehiclePhysics` seçeneğine bağlıdır.

Ambulans veya itfaiye serbest dolaşımda bir olaya gelirken yavaşlamasını ve çevredeki trafiği gözle. `emergencyApproaches` olay yerine yaklaşırken değiştirilen gaz ve fren kararlarını, `emergencyYieldCalls` ise acil aracın etkisiyle düşürülen sivil hız sınırlarını sayar. `Emergency=0` acil ekiplerin yaklaşma davranışını, `Traffic=0` sivil araçlara uygulanan hız sınırlarını kapatır.

Trafiği denemek için yağmurda viraja yaklaşan ağır araçları ve bir ışıkta durup yeniden hareket eden kuyruğu izle. `bendLimits`, `followingLimits` ve `queueHornSkips` sayaçları yeni kararları gösterir. Polis takibini denemek için serbest dolaşımda üç yıldızla açık bir yolda birden fazla devriye aracının seni gördüğü bir kovalamacaya gir. `interceptRoutes` yol kesme rotalarını, `patrolBraking` daha fazla takip mesafesi bırakılan kararları sayar.

Sollamayı denemek için iyi görüşte, düz ve aynı yönde birden fazla şeridi olan bir yolda yavaş giden bir aracın arkasındaki sivil trafiği izle. Sürücü önce önündeki aracın hızını izler ve yan şeritte yaklaşan araçları veya yola çıkan yayaları hesaba katar. `lanePasses` ve `laneReturns` oyunun şerit yönlendirmesine gönderilen kararları sayar. Kavşaklarda, ışıklarda ve köprü geçişlerinde oyunun mevcut yönlendirmesi kullanılır.

Serbest dolaşımda boş banketli bir yolda ağır hasar alıp yavaşlayan sivil bir araç yol kenarına yönelirse `roadsideStarts` artar. `roadsideParked` hedefe varıp duran araçları, `roadsideExits` ise oyuna gönderilen sürücü inme görevlerini sayar. Bu davranış `Traffic` seçeneğine bağlıdır.

Teslim olmayı denemek için serbest dolaşımda bir veya iki yıldızdayken araçtan in, yumruğu seç ve hareketsiz dur. Peşindeki bir polis yakındayken **F9** tuşunu basılı tut. Polis yaklaşınca oyun kendi tutuklama akışını kullanır. `voluntaryArrests` başlatılan tutuklamaları, `policeReports`, `drivingWarnings` ve `investigationRoutes` devriye kararlarını gösterir. Silah sesi duyulduğunda polis yaklaşık olay yerini, görgü tanığı olduğunda gecikmeli olarak görülen olay yerini kullanabilir. Aranma yıldızlarını oyunun mevcut suç sistemi yönetir.

Çatışma değişikliklerini denemek için serbest dolaşımda bir bölge savaşında AK-47 veya Micro Uzi taşıyan bir düşmanla karşılaş. Düşmanın ilk atış serilerini ve peş peşe iki isabetten sonraki serilerini karşılaştır. Günlükte `extendedBursts` uzatılan serileri, `shorterBursts` kısaltılanları ve `suppressionHits` baskıyı başlatan veya uzatan isabetleri sayar. `Combat=0` bu davranışları ve isabet ayarını kapatır.

Siper aramayı denemek için dış mekânda duvar köşesi veya duran bir aracın yakınında, yaklaşık 10–20 metre uzaktaki silahlı bir düşmana onu öldürmeden iki kez isabet ettir. `coverSearches` yapılan aramaları, `coverMoves` ise hareket görevi verilen NPC'leri sayar. Bu sürüm oyunun mevcut siper noktalarını kullanır. Hareketin sahada nasıl göründüğünün doğrulanması için oyun testi gerekir.

Bölge savaşında aynı çeteden iki düşman birbirine yakınken birinin yer değiştirmesini ve diğerinin olağan çatışma davranışını sürdürmesini gözle. Ballas siperde daha kısa bekler ve yakın kayıplara daha uzun süre dayanır. Grove Street ve Vagos siperde daha uzun kalır. Kısa sürede birkaç yakın kayıp veren çetenin baskı altındaki üyeleri, uygun bir yol varsa geri çekilebilir. `coverHolds`, `coverPeeks`, `tacticalWithdrawals`, `allyWaits` ve `gangCasualties` sayaçlarını günlükten izle. Oyun, silahın şarjör değiştirme animasyonunu ve cephane miktarını yönetir.

Günlük oyun zamanı ilerledikçe NPC kaçışı, devriye rotaları, hava araması, barikat yerleşim denemeleri, kazaya bağlı yavaşlama ve etkinlik noktalarının kullanımını sayar. `AirSupport` ve `Roadblocks` ayarları yeni sistemleri ayrı ayrı kapatır. Farklı bir ASI aynı oyun çağrısını önceden değiştirmişse Ultimate Living World ilgili bağlantıyı atlar ve durumu kaydeder. ASI değişikliklerini oyun kapalıyken yap.

## Derleme

`build.cmd` var olan MSVC x86 ortamını kullanır ve paketi `bin/ulw/dist` altında üretir. GitHub Actions’taki **Build** iş akışı aynı adımları çalıştırır. Başarılı çalışmanın **Artifacts** bölümünde **UltimateLivingWorld** paketi çıkar. Arşivde ASI, INI, doğrulama betiği ve bu kılavuz bulunur.

Belirli iş ve ev adreslerine giden bireysel NPC rotaları, taksi yolcu sistemi, yeni şehir etkinlikleri ve Chaos Missions görevlerinin oyun içi uyumluluk testleri tam sürüm için bekliyor.
