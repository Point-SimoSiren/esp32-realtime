# ESP32 LED + Vesivuotovahti

Tämä projekti ohjaa:
- ESP32:n sisäistä LEDiä (GPIO2) web-käyttöliittymästä
- ulkoista vilkkuvaa LEDiä (GPIO25), kun vesivuoto havaitaan
- vesianturin analogista lukua (GPIO34)

## Pinnikytkennät

| Toiminto | ESP32 pinni | Huomio |
|---|---|---|
| Sisäinen LED (web-ohjaus) | `GPIO2` | Päälle/pois UI:n painikkeista |
| Vesivuotoanturin analogitulo | `GPIO34` | Vain input (ADC) |
| Ulkoinen vuoto-LED | `GPIO25` | Vilkkuu vuototilassa |
| Käyttöjännite | `3V3` | Anturille ja mittauslinjaan |
| Maa | `GND` | Yhteinen maa kaikille |

## Vastukset (tärkeä)

Kytkennässä on kaksi vastusta:

1. `R1` anturilinjaan virtasyötön ja jänniteluvun väliin  
   - Sijoita vastus `3V3`-syötön ja anturin/jännitelukusolmun väliin (solmu, josta menee yhteys `GPIO34` pinniin).  
   - Tarkoitus: rajata virtaa ja stabiloida mittauslinjaa.

2. `R2` ulkoisen LEDin syöttöön  
   - Sijoita vastus sarjaan ulkoisen LEDin kanssa (`GPIO25 -> R2 -> LED -> GND` tai `GPIO25 -> LED -> R2 -> GND`).  
   - Tarkoitus: rajoittaa LED-virtaa.

Tyypillinen arvo LEDin sarjavastukselle on `220R`-`1kR`.  
Anturilinjan vastukselle käytä samaa arvoa kuin toimivassa nykykytkennässäsi.

## Kytkentäluonnos (tekstinä)

1. `GPIO2` = sisäinen LED (ei ulkoista kytkentää pakollinen).
2. `GPIO25` -> `R2` -> ulkoinen LED -> `GND`.
3. `3V3` -> `R1` -> anturin mittaussolmu -> `GPIO34`.
4. Anturin toinen puoli / referenssi -> `GND` (tai anturimoduulin mukainen maa).
5. Kaikilla osilla yhteinen `GND`.

## ASCII-kytkentäkaavio

```text
            ESP32 (DOIT DEVKIT V1)
         +---------------------------+
   3V3 --| 3V3                       |
   GND --| GND                       |
 GPIO34 <-| GPIO34 (ADC IN)          |
 GPIO25 --| GPIO25 (WATER LED OUT)   |
  GPIO2 --| GPIO2  (SISÄINEN LED)    |
         +---------------------------+

Vesivuotoanturin mittauslinja:

3V3 ----[ R1 ]----o----> GPIO34
                  |
               [ ANTURI ]
                  |
                 GND

Ulkoisen vuoto-LEDin linja:

GPIO25 ----[ R2 ]----|>|---- GND
                     LED
```

Huom:
- `R1` on virtasyötön (`3V3`) ja mittaussolmun (`GPIO34` lukupiste) välissä.
- `R2` on aina sarjassa ulkoisen LEDin kanssa.

## Ohjelman ajo (PlatformIO)

1. Rakenna firmware:
   - `C:\Users\Simo\.platformio\penv\Scripts\platformio.exe run`
2. Päivitä web-sivu LittleFS:aan:
   - `C:\Users\Simo\.platformio\penv\Scripts\platformio.exe run -t uploadfs`
3. Uploadaa firmware laitteelle:
   - `C:\Users\Simo\.platformio\penv\Scripts\platformio.exe run -t upload`

## Oletuspinnit koodissa

`src/main.cpp`:
- `LED_PIN = 2`
- `WATER_SENSOR_PIN = 34`
- `WATER_LED_PIN = 25`
- `WATER_THRESHOLD = 1000`
