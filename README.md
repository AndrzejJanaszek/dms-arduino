# dms-arduino

Firmware płytki Arduino dla systemu monitorowania dozowania: odczyt czujników temperatury DS18B20 i sygnałów cyfrowych, wysyłanie ich jako JSON przez port szeregowy. To repo definiuje format danych, z którym muszą być zgodni odbiorcy (`dosing-monitoring-system`, `measurement-data-saver`).

Docelowy szkic: `test_multiple_sensors/test_multiple_sensors.ino`. `test1/` to wcześniejszy przykład z jednym czujnikiem.

## Sprzęt

- Płytka: Arduino Uno (`arduino:avr:uno`).
- Czujniki DS18B20 na magistrali 1-Wire, pin 2, rezystor 4,7 kΩ między DATA a 5 V. Obsługiwane maks. 4 czujniki.
- Sygnały cyfrowe: piny 4–9 jako `INPUT_PULLUP` (styk otwarty = `1`, zwarty do GND = `0`).

## Biblioteki

| Biblioteka | Wersja |
|---|---|
| `OneWire` | 2.3.8 |
| `DallasTemperature` | 4.0.6 |
| `ArduinoJson` | 6.21.5 (szkic używa API 6.x; w 7.x kompiluje się z ostrzeżeniami o przestarzałym API) |

Rdzeń: `arduino:avr` 1.8.8.

## Kompilacja i wgrywanie

```
arduino-cli compile --fqbn arduino:avr:uno --warnings all test_multiple_sensors
arduino-cli upload --fqbn arduino:avr:uno -p <port> test_multiple_sensors
```

Albo Arduino IDE 2.x: płytka Arduino Uno, wybrany port, Upload.

## Format wyjścia

Serial 9600 baud, każda linia zakończona `\r\n`.

```
{"temps":[{"a":"28A8F126AB240B1C","t":25.44}, ...]}   co ~1 s
{"pins":{"4":1,"5":1,"6":1,"7":1,"8":1,"9":1}}         co 250 ms, zawsze wszystkie piny
```

- `a`: adres czujnika (16 znaków hex), `t`: temperatura w °C z dwoma miejscami po przecinku.
- Odczyty błędne idą bez zmian: `-127.00` (czujnik nie odpowiada) i `85.00` (wartość po resecie zasilania czujnika). Filtrowanie to zadanie odbiorcy.
- Linie niezaczynające się od `{` to logi z prefiksem poziomu: `[DEBUG]`, `[INFO]`, `[WARNING]`, `[ERROR]`. Pojawiają się po starcie (także po resecie przez DTR przy otwarciu portu) i w trakcie pracy.

## Wykrywanie czujników

Szkic skanuje magistralę przy starcie i ponownie w trakcie pracy:

- co 10 s, gdy nie ma żadnego czujnika albo ostatni odczyt miał błąd (`-127`/`85.0`),
- bezwarunkowo co 60 s (łapie czujniki dołączone do częściowo działającej magistrali).

Czujnik, który nie odpowiadał przy starcie, zostanie więc podjęty bez restartu płytki. Odpięty czujnik daje `-127.00` do najbliższego skanu, potem znika z ramki `temps`. Przy braku czujników wysyłane jest `{"temps":[]}`.

Logi stanu magistrali:

| Linia | Kiedy |
|---|---|
| `[DEBUG] Temperature sensor <i>: <adres>` | przy zmianie zbioru czujników (także przy starcie) |
| `[INFO] Temperature sensors found: N` | przy zmianie liczby czujników na N > 0 |
| `[WARNING] No temperature sensors found` | przy zmianie liczby czujników na 0 |
| `[WARNING] Temperature sensors on bus: N, using first 4` | gdy na magistrali jest więcej niż 4 czujniki |
| `[WARNING] Temperature read failed: <adres>` | raz, gdy odczyt czujnika przechodzi z poprawnego w błędny |
| `[DEBUG] Scan done, sensors: N` | po każdym skanie |

Skan blokuje pętlę: ok. 1 ms przy pustej magistrali, przy 4 czujnikach szacunkowo ok. 105 ms (przeszukanie magistrali i odczyt scratchpadów przy ustawianiu rozdzielczości), plus wypis adresów przy zmianie zbioru (9600 bodów, ok. 1 ms na znak). Ramka `pins` może się wtedy spóźnić o tyle. `sensors.begin()` (wykrycie trybu zasilania, do 3 prób po 50 ms) jest wołane tylko raz, w `setup()`.
