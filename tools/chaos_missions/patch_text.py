"""Add the campaign's Turkish captions to a local San Andreas text file."""

import argparse
from pathlib import Path
from struct import iter_unpack, pack, pack_into, unpack_from
from zlib import crc32


CAPTIONS = {
    "CZSTART": "Ultimate Chaos",
    "CZPOLIS": "Polis kavsaklari tutuyor. Arka sokaklardan Grove Street'e don.",
    "CZTRUCK": "Ryder: Kisa yol burasi! Dur... Cop kamyonu! Yan sokaktan dolan.",
    "CZSMK1": "Big Smoke: Dur CJ, bunlar eski dostlarimiza benziyor.",
    "CZSMK2": "Big Smoke: Tamam CJ, bunlar kesinlikle dost degil!",
    "CZSMKD": "~1~ saniyede isarete ulas!",
    "CZSMKF": "Big Smoke ile kacis basarisiz oldu.",
    "CZBIKE": "Ballas takviyesi! Hareket halinde kal.",
    "CZPIZZA": "Pizza motoru pesinizde!",
    "CZAIM": "Hareketli hedefler. Mermilerini dikkatli kullan.",
    "CZSWEET": "Takipciler ates aciyor! Sweet'i eve ulastir.",
    "CZCLOCK": "Saat caliyor! Alt kattaki isarete git.",
    "CZQUIET": "Alarm susturuldu.",
    "CZCAT": "Ballas iki yandan geliyor!",
    "CZDEPOT": "Askerler depoya takviye gonderdi!",
    "CZRUN": "Ikisi ayni giyinmis. Isaretli hedefi takip et!",
    "CZPHOTO": "Ikisi birbirine benziyor. Isaretli adami izle!",
    "CZTOUR": "CJ: Takip mi? Turist yolunu sasirmis.",
    "CZHAUL": "Catalina: Onumuzu kesiyor! Treyleri savurma!",
    "CZTRACT": "Traktor yolu kesiyor. Acik taraftan dolan!",
    "CZPOLR": "Polis yolu kapatti. Diger cikisa yonel!",
    "CZFARM": "Polis minibuse yaklasiyor! Ciftlikten cik!",
    "CZMAIL": "Truth: Ajanlar! Dur... Posta arabasiymis.",
    "CZTAIL": "Truth: Bu sefer arkamizdaki bizi izliyor.",
    "CZWORK": "Mikser yolu kapatti. Dozerle diger taraftan gir.",
    "CZJIZZY": "Hedef arabaya kaciyor. Radardaki isareti izle!",
    "CZAIR": "Son 25 saniye. Antenleri koru!",
    "CZDRILL": "Kisa sureli surus sinavi.",
    "CZBOAT": "Yan taraftan eskort teknesi geliyor!",
    "CZDECK": "Guvertede takviye var! Cikisa dikkat et.",
    "CZEXIT": "Cikista takviye! Karsidan gelenlere dikkat.",
    "CZLOOK": "Bir arac daha ayni model. Radardaki hedefi izle.",
    "CZCOP": "Polis araci onunu kesmeye calisiyor!",
    "CZPORT": "Liman cikisinin bir tarafi kapali. Diger yolu kullan.",
    "CZRETRY": "Kontrol noktasina dondun. Sure yeniden basladi.",
    "CZALARM": "Alarm! Jetpack ile cikisa ulas.",
    "CZFOOD": "Big Smoke: Siparisim hazirsa su isi cabuk bitirelim, CJ.",
    "CZVAN": "Ballas takviyesi geliyor! Sokagin girisine dikkat.",
    "CZCASE": "Big Smoke: Vurdum! Bir dakika... O, valizmis!",
    "CZTRAIN": "Bir Vagos yan trenden ana trene gecti!",
    "CZMON": "Duraklar daraldi. Virajlara girmeden hizini dusur!",
    "CZJUMP": "Cesar: Arkada motorlu var! Tirin yaninda sabit kal!",
    "CZHELI": "Ikinci helikopter daha yakindan geliyor!",
    "CZSTRIP": "CJ: Koca havaalaninda bir traktorla mi basliyoruz?",
    "CZFLY": "Ucus denemesi: Bulutlu veya yagmurlu hava.",
    "CZRAIN": "Toreno: Hava acik... Bir dakika, yagmur basladi.",
    "CZCARGO": "Kabinde bir koruma daha var!",
    "CZCRATE": "Ayni gorunen yedek sandiklar var. Isaretli olanlara ates et!",
    "CZMIXED": "Johnny: Viraj da yetmedi, bir de uctuk!",
    "CZQUARY": "Forklift kacis yoluna giriyor. Rampaya dikkat!",
    "CZCHIPS": "Son makine dustu. Guvenlik cikisa geliyor!",
    "CZDESRT": "Iki kamyonet pesinizde. Biri yolunuzu kesmeye calisiyor!",
    "CZALR4": "Tam alarm. Polis takviyesi geliyor!",
    "CZMILLI": "Millie suphelendi. Biraz geri cekil!",
    "CZRELIF": "Cikis koridorunda yeni devriye var!",
    "CZ4BIKE": "Son motor! Polis takviyesi pesinde.",
    "CZAIRTL": "Yuku tasirken bir helikopter pesine takildi!",
    "CZAMB": "Gercek ambulans hizlandi. Refakat araclarina dikkat!",
    "CZMEAT": "Cikis tarafinda silahli adamlar var! Ken'i yaninda tut.",
    "CZHOSP": "Hastane yolunda bir serit kapali. Yavaslayip dolan!",
    "CZORDER": "Kumarhane gorevlisi: Yeni garson siz misiniz? Iki kahve lutfen!",
    "CZCASE2": "Cantayi baska bir ajan aldi. Yeni radar isaretini izle!",
    "CZPILOT": "Pilot: O ucakla bize yetisebilecegini mi sandin?",
    "CZBRAKE": "Pulaski ani fren yapti!",
    "CZBISTR": "Hedefin masasi yakininda korumalar var!",
    "CZVAULT": "Cikista guvenlik takviyesi! Isaretli yolu izle.",
    "CZPOPPA": "Big Poppa'nin koruma araci pesine takilabilir!",
    "CZHYDRA": "Jetler farkli irtifadan yaklasiyor!",
    "CZKART": "Minibusler yolu daraltiyor. Kartla acik taraftan gec!",
    "CZGLEN": "B Dup'un kapisinda silahli Ballas takviyesi var!",
    "CZCAB": "Taksici: Musait! Isyan var diye vardiyayi mi birakayim?",
    "CZRIOT": "Grove Street'e giriste terk edilmis bir arac var!",
    "CZGROVE": "Ballas geri dondu! Iki taraftan yaklasiyorlar.",
    "CZBACK": "Ballas'in destek araci geldi... Sofor kacti!",
    "CZVAGOS": "Cesar: Son sokak! Dogudan ya da batidan yaklasabiliriz.",
    "CZSMKW": "Smoke'un korumalari yan odalardan geliyor!",
    "CZFIRE": "Son cikista silahli adamlar var. Yangina dikkat!",
    "CZRESCU": "Sweet birakmadan once araci tam altina getir!",
    "CZGOOD": "Sweet: Sag ol CJ. Bir dahakine frenleri de dene!",
    "CZROAD": "Tenpenny'nin rotasinda terk edilmis arac var!",
    "CZWRONG": "CJ: Polis mi? Yolu sasirdi galiba.",
}


def read_chunk(data, offset, name):
    marker, size = unpack_from("<4sI", data, offset)
    end = offset + 8 + size
    if marker != name or end > len(data):
        raise ValueError(f"Invalid {name.decode()} chunk at 0x{offset:X}")
    return data[offset + 8:end], end


def patch_text(source, captions):
    if source[:4] != b"\x04\x00\x08\x00":
        raise ValueError("Expected a San Andreas 8-bit GXT file")
    directory, main = read_chunk(source, 4, b"TABL")
    tables = list(iter_unpack("<8sI", directory))
    if not tables or tables[0] != (b"MAIN\0\0\0\0", main):
        raise ValueError("The MAIN text table must follow the directory")
    keys, next_chunk = read_chunk(source, main, b"TKEY")
    old_text, tail = read_chunk(source, next_chunk, b"TDAT")
    entries = {key: offset for offset, key in iter_unpack("<II", keys)}
    all_keys = set(entries)
    if len(entries) * 8 != len(keys):
        raise ValueError("Duplicate MAIN text keys")
    for name, offset in tables[1:]:
        if offset < tail or source[offset:offset + 8] != name:
            raise ValueError(f"Invalid mission text table at 0x{offset:X}")
        mission_keys, _ = read_chunk(source, offset + 8, b"TKEY")
        all_keys.update(key for _, key in iter_unpack("<II", mission_keys))

    data = bytearray(old_text)
    for label, caption in captions.items():
        key = crc32(label.upper().encode("ascii")) ^ 0xFFFFFFFF
        if key in all_keys:
            raise ValueError(f"Caption key is already in use: {label}")
        all_keys.add(key)
        entries[key] = len(data)
        data.extend(caption.encode("ascii") + b"\0")

    new_keys = b"".join(pack("<II", offset, key) for key, offset in sorted(entries.items()))
    delta = len(new_keys) - len(keys) + len(data) - len(old_text)
    prefix = bytearray(source[:main])
    for index, (_, offset) in enumerate(tables[1:], start=1):
        pack_into("<I", prefix, 12 + index * 12 + 8, offset + delta)
    return b"".join((
        prefix, pack("<4sI", b"TKEY", len(new_keys)), new_keys,
        pack("<4sI", b"TDAT", len(data)), data, source[tail:],
    ))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = patch_text(args.baseline.read_bytes(), CAPTIONS)
    with args.output.open("xb") as target:
        target.write(output)


if __name__ == "__main__":
    main()
