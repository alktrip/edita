#!/usr/bin/env python3
# Genera los fixtures de tests/fixtures/ con los codecs de Python (independientes
# de glibc/iconv, que es lo que se prueba). Uso: python3 tests/fixtures/generate.py
# Los archivos generados se versionan; solo hay que regenerarlos si cambia este script.
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
ENC = HERE / "encoding"
REF = HERE / "encoding" / "reference"
EOL = HERE / "eol"

SPANISH = (
    "El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y frío; añoraba a su querido cachorro.\n"
    "¿Qué tal está usted? ¡Muy bien, gracias! La cigüeña tocaba el saxofón detrás del palenque de paja.\n"
    "El niño comió ñoquis con salsa de jamón y pidió más: el camarero, con acento andaluz, sonrió.\n"
    "Façade, naïve, déjà vu, crème brûlée: palabras prestadas que también aparecen en español.\n"
)
SPANISH_EURO = SPANISH + "El menú del día cuesta 12,50 € y el café 1,20 €.\n"
RUSSIAN = (
    "Москва - столица России, крупнейший по численности населения город страны. Город расположен\n"
    "на реке Москве в центре Восточно-Европейской равнины. История Москвы насчитывает более восьми\n"
    "веков; впервые город упоминается в летописи под 1147 годом. Сегодня это крупный политический,\n"
    "экономический, культурный и научный центр, где работают тысячи предприятий и университетов.\n"
) * 2
# Pangrama: frase corta llena de letras raras. El uchardet de N++ la toma por
# x-mac-cyrillic; se conserva para documentar ese comportamiento.
RUSSIAN_PANGRAM = (
    "Съешь же ещё этих мягких французских булок, да выпей чаю. Широкая электрификация южных губерний\n"
    "даст мощный толчок подъёму сельского хозяйства. В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!\n"
)
JAPANESE = (
    "いろはにほへと　ちりぬるを　わかよたれそ　つねならむ　うゐのおくやま　けふこえて　あさきゆめみし　ゑひもせす。\n"
    "日本語の文章を正しく判定できるかどうかを確認するための試験用の文書です。漢字、平仮名、片仮名が混在しています。\n"
    "東京都の天気は晴れのち曇り、明日は雨が降るでしょう。カタカナのテキストエディタで編集してください。\n"
)
CHINESE_S = (
    "我能吞下玻璃而不伤身体。这是一个用于测试简体中文编码检测的示例文本，其中包含常用汉字。\n"
    "天地玄黄，宇宙洪荒。日月盈昃，辰宿列张。寒来暑往，秋收冬藏。闰余成岁，律吕调阳。\n"
    "北京是中华人民共和国的首都，也是全国的政治、文化和国际交往中心。今天天气很好，我们去公园散步吧。\n"
)
CHINESE_T = (
    "我能吞下玻璃而不傷身體。這是一個用於測試繁體中文編碼檢測的範例文字，其中包含常用漢字。\n"
    "天地玄黃，宇宙洪荒。日月盈昃，辰宿列張。寒來暑往，秋收冬藏。閏餘成歲，律呂調陽。\n"
    "臺北是臺灣最大的城市之一，擁有許多歷史悠久的廟宇與熱鬧的夜市。今天天氣很好，我們去公園散步吧。\n"
)
KOREAN = (
    "다람쥐 헌 쳇바퀴에 타고파. 키스의 고유조건은 입술끼리 만나야 하고 특별한 기술은 필요치 않다.\n"
    "이 문서는 한국어 인코딩 감지를 시험하기 위한 예제 텍스트입니다. 서울의 날씨는 맑고 따뜻합니다.\n"
    "한글은 조선의 세종대왕이 창제한 문자로, 과학적이고 배우기 쉬운 글자로 알려져 있습니다.\n"
)
GREEK = (
    "Ξεσκεπάζω την ψυχοφθόρα βδελυγμία. Τάχιστη αλώπηξ βαφής ψημένη γη, δρασκελίζει υπέρ νωθρού κυνός.\n"
    "Αυτό είναι ένα δοκιμαστικό κείμενο για τον έλεγχο της ανίχνευσης κωδικοποίησης στα ελληνικά.\n"
    "Η Αθήνα είναι η πρωτεύουσα της Ελλάδας και μία από τις αρχαιότερες πόλεις του κόσμου.\n"
)
MULTI = SPANISH + RUSSIAN + JAPANESE + "Emoji fuera del BMP: 😀 🚀 𝄞\n"
ASCII = "The quick brown fox jumps over the lazy dog.\nPack my box with five dozen liquor jugs.\n" * 3


def write(path: pathlib.Path, data: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main():
    references = {
        "spanish": SPANISH, "spanish-euro": SPANISH_EURO, "russian": RUSSIAN, "japanese": JAPANESE,
        "chinese-s": CHINESE_S, "chinese-t": CHINESE_T, "korean": KOREAN, "greek": GREEK,
        "multi": MULTI, "ascii": ASCII,
    }
    for name, text in references.items():
        write(REF / f"{name}.txt", text.encode("utf-8"))

    # (archivo, texto, codec de Python, prefijo de BOM)
    fixtures = [
        ("utf8.txt", MULTI, "utf-8", b""),
        ("utf8-bom.txt", MULTI, "utf-8", b"\xEF\xBB\xBF"),
        ("utf16le-bom.txt", MULTI, "utf-16-le", b"\xFF\xFE"),
        ("utf16be-bom.txt", MULTI, "utf-16-be", b"\xFE\xFF"),
        ("utf16le-nobom.txt", ASCII, "utf-16-le", b""),
        ("ascii.txt", ASCII, "ascii", b""),
        ("windows-1252.txt", SPANISH_EURO, "cp1252", b""),
        ("iso-8859-15.txt", SPANISH_EURO, "iso8859_15", b""),
        ("windows-1251.txt", RUSSIAN, "cp1251", b""),
        ("koi8-r.txt", RUSSIAN, "koi8_r", b""),
        ("russian-pangram-1251.txt", RUSSIAN_PANGRAM, "cp1251", b""),
        ("shift_jis.txt", JAPANESE, "cp932", b""),
        ("gb2312.txt", CHINESE_S, "gb2312", b""),
        ("big5.txt", CHINESE_T, "big5", b""),
        ("euc-kr.txt", KOREAN, "euc_kr", b""),
        ("iso-8859-7.txt", GREEK, "iso8859_7", b""),
    ]
    for name, text, codec, bom in fixtures:
        write(ENC / name, bom + text.encode(codec))

    write(ENC / "empty.txt", b"")
    # UTF-8 con un byte suelto de Latin-1 (é = 0xE9): no es UTF-8 válido
    write(ENC / "invalid-utf8.txt", "Texto con acento: ".encode() + b"caf\xE9 y m\xE1s\n")
    # Todos los bytes 0x80-0xFF, incluidos los no definidos de 1252 (0x81, 0x8D, 0x8F, 0x90, 0x9D)
    write(ENC / "all-high-bytes.bin", b"Bytes altos: " + bytes(range(0x80, 0x100)) + b"\n")
    # Un NUL hace que N++ no lo considere UTF-8
    write(ENC / "with-nul.bin", b"abc\x00def\n")

    write(EOL / "crlf.txt", b"uno\r\ndos\r\ntres\r\n")
    write(EOL / "lf.txt", b"uno\ndos\ntres\n")
    write(EOL / "cr.txt", b"uno\rdos\rtres\r")
    write(EOL / "mixed-lf-first.txt", b"uno\ndos\r\ntres\r")
    write(EOL / "mixed-crlf-first.txt", b"uno\r\ndos\ntres\r")
    write(EOL / "none.txt", b"una sola linea sin fin")
    write(EOL / "cr-at-end.txt", b"texto\r")


if __name__ == "__main__":
    main()
