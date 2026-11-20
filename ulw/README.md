# Ultimate Living World 0.1.0

İlk geliştirme sürümü, serbest dolaşımda silah sesi duyan yayaların tepkilerini değiştirir. Yakındaki sivil tehlikeden kaçabilir. Daha uzaktaki bir sivil, kişiliğine ve görüş hattına göre sesin geldiği yöne bakabilir. Birkaç saniye içinde tekrar duyulan silah sesi tepkiyi artırabilir.

## Test kurulumu

Paket, SHA-256 değeri `A559AA772FD136379155EFA71F00C47AAD34BBFEAE6196B0FE1047D0645CBD26` olan klasik Hoodlum 1.0 US oyununa ve `InitializeASI` çağıran Ultimate ASI Loader’a yöneliktir. `Verify-Game.ps1` bu dosyaları kontrol eder ve tanınan gta-reversed eklentisini bildirir. Ultimate Living World ile diğer motor eklentilerini birlikte kullanmadan önce bir test kopyasında doğrulayın.

GTA kapalıyken `UltimateLivingWorld.asi` ve `UltimateLivingWorld.ini` dosyalarını test kurulumunun `scripts` klasörüne koyun. Oyun başladığında aynı klasörde `UltimateLivingWorld.log` oluşur. Eklentiyi kaldırmak için GTA kapalıyken kopyaladığınız iki dosyayı kaldırın. `Enabled=0` ile sonraki açılış için devre dışı bırakabilirsiniz.

Serbest dolaşımda yayaların yakınında ve biraz uzağında silah sesine verdikleri tepkileri karşılaştırın. Günlükte yeni bir olay işlendiğinde dönemsel olarak bakma, uzaklaşma ve panik tepkilerinin sayısı görünür. Ardından hikâye görevine girip ilerlemeyi kontrol edin. Bunlar oyun içi test senaryolarıdır.

## Kod ve derleme

`IntelligenceCore.cpp` algı girdilerini değerlendirir, son olayları süresi dolan sabit boyutlu bellekte tutar ve kare başına en fazla altı karar verir. `HoodlumGame.cpp`, gta-reversed’deki `CEventHandler`, `CEventGunShot`, `CPed`, `CWorld`, `CPools`, `CTimer`, `CCutsceneMgr` ve `CTheScripts` arayüzlerinden doğrulanan bağları kullanır. Oyun kendi görev belleğini ve NPC görevlerini yönetir. Yapı veya komut girişleri farklıysa bağlantı devreye girmez.

GTA Reversed motor paketinin Compact EXE gereksinimi vardır. Bu sürüm için bağımsız ASI seçildi. Mevcut Hoodlum yürütülebilir dosyasına uyarlama, yalnızca silah sesi dağıtıcısındaki tek bağlantıyı ve kullanılan motor fonksiyonlarını kapsar.

| Oyun sistemi | Kaynakta incelenen giriş noktası |
| --- | --- |
| Silah sesi tepkisi | `CEventHandler::ComputeShotFiredResponse`, `CEventGunShot::AffectsPed` |
| Karakter ve görüş | `CPed`, `CPedIntelligence`, `CWorld::GetIsLineOfSightClear` |
| Tepki görevleri | `CTaskComplexSmartFleePoint`, `CTaskComplexTurnToFaceEntityOrCoord` |
| Polis için sonraki bağlantılar | `CWanted`, `CCrime`, `CCopPed` |
| Trafik için sonraki bağlantılar | `CCarAI`, `CCarCtrl`, `CPathFind`, `CTrafficLights` |

Yerel motor kaynakları ve çalıştırılabilir dosyanın komutları, boyut ve çağrı denetimlerinde kullanıldı. Karşılaştırma kaynakları [gta-reversed](https://github.com/gta-reversed/gta-reversed), [Plugin-SDK](https://github.com/DK22Pac/plugin-sdk) ve [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).

`build.cmd` var olan bir MSVC x86 ortamıyla ASI’yi oluşturur. Derleme, çekirdek karar testlerini çalıştırır ve ASI’nin sıradan bir Windows sürecinde yanlış yürütülebilir dosyayı reddettiğini doğrular. GitHub Actions’taki Build iş akışının Ultimate Living World çıktısı aynı paketi üretir. Bu testler oyun içi davranışın yerini tutmaz.
