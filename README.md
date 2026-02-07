# ESP32 LED + Vesivuotovahti

Tama projekti ohjaa:
- ESP32:n sisaista LEDia (GPIO2) web-kayttoliittymasta
- ulkoista vilkkuvaa LEDia (GPIO25), kun vesivuoto havaitaan
- vesianturin analogista lukua (GPIO34)

## Pinnikytkennat

| Toiminto | ESP32 pinni | Huomio |
|---|---|---|
| Sisainen LED (web-ohjaus) | `GPIO2` | Paalle/pois UI:n painikkeista |
| Vesivuotoanturin analogitulo | `GPIO34` | Vain input (ADC) |
| Ulkoinen vuoto-LED | `GPIO25` | Vilkkuu vuototilassa |
| Kayttojannite | `3V3` | Anturille ja mittauslinjaan |
| Maa | `GND` | Yhteinen maa kaikille |

## Vastukset (tarkea)

Kytkennassa on kaksi vastusta:

1. `R1` anturilinjaan virtasyoton ja janniteluvun valiin  
   - Sijoita vastus `3V3`-syoton ja anturin/jannitelukusolmun valiin (solmu, josta menee yhteys `GPIO34` pinniin).  
   - Tarkoitus: rajata virtaa ja stabiloida mittauslinjaa.

2. `R2` ulkoisen LEDin syottoon  
   - Sijoita vastus sarjaan ulkoisen LEDin kanssa (`GPIO25 -> R2 -> LED -> GND` tai `GPIO25 -> LED -> R2 -> GND`).  
   - Tarkoitus: rajoittaa LED-virtaa.

Tyypillinen arvo LEDin sarjavastukselle on `220R`-`1kR`.  
Anturilinjan vastukselle kayta samaa arvoa kuin toimivassa nykykytkennassasi.

## Kytkentaluonnos (tekstina)

1. `GPIO2` = sisainen LED (ei ulkoista kytkentaa pakollinen).
2. `GPIO25` -> `R2` -> ulkoinen LED -> `GND`.
3. `3V3` -> `R1` -> anturin mittaussolmu -> `GPIO34`.
4. Anturin toinen puoli / referenssi -> `GND` (tai anturimoduulin mukainen maa).
5. Kaikilla osilla yhteinen `GND`.

## ASCII-kytkentakaavio

```text
            ESP32 (DOIT DEVKIT V1)
         +---------------------------+
   3V3 --| 3V3                       |
   GND --| GND                       |
 GPIO34 <-| GPIO34 (ADC IN)          |
 GPIO25 --| GPIO25 (WATER LED OUT)   |
  GPIO2 --| GPIO2  (SISAINEN LED)    |
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
- `R1` on virtasyoton (`3V3`) ja mittaussolmun (`GPIO34` lukupiste) valissa.
- `R2` on aina sarjassa ulkoisen LEDin kanssa.

## Ohjelman ajo (PlatformIO)

1. Rakenna firmware:
   - `C:\Users\Simo\.platformio\penv\Scripts\platformio.exe run`
2. Paivita web-sivu LittleFS:aan:
   - `C:\Users\Simo\.platformio\penv\Scripts\platformio.exe run -t uploadfs`
3. Uploadaa firmware laitteelle:
   - `C:\Users\Simo\.platformio\penv\Scripts\platformio.exe run -t upload`

## Oletuspinnit koodissa

`src/main.cpp`:
- `LED_PIN = 2`
- `WATER_SENSOR_PIN = 34`
- `WATER_LED_PIN = 25`
- `WATER_THRESHOLD = 1000`
