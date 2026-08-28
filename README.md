# Cytron Chassis RC Control

Arduino projekt pro tankové ovládání podvozku s dvěma DC motory pomocí Cytron ovladačů. Čte PWM vstupy (např. z RC přijímače nebo z výstupů autopilota jako Durandal + QGroundControl) a řídí motory. **Určeno pro Arduino Mega 2560 R3.**

## Hardware

- Arduino Mega 2560 R3
- 2× Cytron MD motor driver (nebo podobné s knihovnou `CytronMotorDriver`)
- Standardní RC přijímač se 2 samostatnými kanály (PWM)
- Podvozek s levým a pravým motorem

## Zapojení

| Signál              | Arduino pin | Poznámka              |
|---------------------|-------------|-----------------------|
| RC CH1 (levý motor) | D2          | interrupt pin (INT0)  |
| RC CH2 (pravý motor)| D3          | interrupt pin (INT1)  |
| Test tlačítko       | D4          | INPUT_PULLUP           |
| Motor L PWM         | D5          | levý motor            |
| Motor L DIR         | D6          | levý motor směr       |
| Motor R PWM         | D9          | pravý motor           |
| Motor R DIR         | D10         | pravý motor směr      |

Mega 2560 má 5V tolerantní piny, takže běžný RC receiver lze připojit přímo (pokud dává 5V PWM).

## Požadovaná knihovna

Nainstaluj knihovnu **Cytron Motor Drivers Library** přes Správce knihoven Arduino IDE:

```
Sketch → Include Library → Manage Libraries → "Cytron Motor Drivers"
```

Nebo stáhni z [Cytron GitHub](https://github.com/CytronTechnologies/CytronMotorDriver).

## Funkce

- Čte RC signály z přijímače pomocí přerušení na pinech D2 a D3.
- Mapuje hodnoty 1000–2000 µs na rychlost -MAX_PWM až MAX_PWM (omezeno dle napětí, při 4S plně nabité ~ -182 až 182).
- **Tank mode:** CH1 přímo ovládá levý motor, CH2 přímo ovládá pravý motor.
- **Failsafe:** zastaví motory, pokud je signál mimo 900–2100 µs, nebo pokud nepřijde nový pulz po dobu 50 ms.
- Atomic read `volatile int` s vypnutými přerušeními – nutné pro 8bit AVR (Mega).
- **Napěťové omezení motoru:** max PWM se spočítá z poměru `MOTOR_VOLTAGE / BATTERY_VOLTAGE_MAX`, aby se při plném plynu neposlalo do 12V motoru víc než 12V (např. na 4S baterii plně nabité 16.8V → ~182 PWM místo 255).

## Test sekvence

Stiskem tlačítka na pinu D4 se spustí testovací sekvence:

1. 2 s motor 1 (L) vpřed
2. 2 s motor 2 (R) vpřed
3. 1 s oba motory vpřed
4. 1 s oba motory vzad

Tlačítko se připojí mezi pin D4 a GND (používá se `INPUT_PULLUP`). Během testu je hlavní smyčka pozastavena (`delay`).

> **Upozornění:** Test sekvence používá `delay()`, takže na ~6 sekund zablokuje hlavní smyčku. Během této doby se **nepřijímá RC signál a nefunguje failsafe** – motory nelze ovládat z vysílače.

## Ladění

Pro kontrolu hodnot můžeš do `setup()` přidat a v `loop()` vypisovat:

```cpp
Serial.begin(115200);
Serial.print("CH1: "); Serial.print(leftInput);
Serial.print(" CH2: "); Serial.println(rightInput);
delay(100);
```

## Poznámky

- Ujisti se, že RC přijímač má společnou GND s Arduinem.
- Tento kód očekává **2 samostatné PWM kanály**, ne PPM/SBUS.

## Nahrání z příkazové řádky (Linux / macOS)

Pomocí `arduino-cli`:

```bash
# Přejdi do adresáře s projektem
cd /cesta/k/cytron-chassis-rc

# 1. Inicializace konfigurace (pokud je poprvé)
arduino-cli config init

# 2. Nainstaluj podporu pro Arduino AVR desky
arduino-cli core install arduino:avr

# 3. Nainstaluj knihovnu pro Cytron ovladače
arduino-cli lib install "Cytron Motor Drivers Library"

# 4. Zkompiluj pro Mega 2560
arduino-cli compile --fqbn arduino:avr:mega .

# 5. Zjisti port desky
arduino-cli board list
# Na Linuxu typicky: /dev/ttyACM0 nebo /dev/ttyUSB0
# Na macOS typicky: /dev/cu.usbmodem...

# 6. Nahraj do desky (nahraď PORT správným portem)
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:mega .
```

**Tip pro Linux:** Pokud upload selhává kvůli právům, přidej uživatele do skupiny `dialout`:

```bash
sudo usermod -a -G dialout $USER
# Poté se odhlásit a znovu přihlásit
```

nebo použij `sudo`:

```bash
sudo arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:mega .
```

## Použití s Durandal + QGroundControl

Tento Arduino kód slouží jako **převodník mezi Durandal a Cytron ovladači**. Durandal sám nedokáže přímo řídit motory v **Cytron PWM_DIR** režimu, proto z něj čteme PWM výstupy a Arduino řídí motory.

### Zapojení

- Výstup z Durandalu pro **pás 1 (levý)** → Arduino pin **D2** (CH1)
- Výstup z Durandalu pro **pás 2 (pravý)** → Arduino pin **D3** (CH2)
- Arduino piny **D5/D6** → levý Cytron driver (PWM/DIR)
- Arduino piny **D9/D10** → pravý Cytron driver (PWM/DIR)

### Nastavení v QGroundControl / ArduPilot

1. **Kalibrace RC** v QGC: namapuj vysílač tak, aby:
   - **CH1 = pás 1 (levý)**
   - **CH2 = pás 2 (pravý)**
2. **Výstupy Durandalu** nastav jako **RCPassThru** pro přímé přeposlání PWM:
   - `SERVO1_FUNCTION = 51` → výstup 1 = RC vstup 1 (pás 1, levý)
   - `SERVO3_FUNCTION = 52` → výstup 3 = RC vstup 2 (pás 2, pravý)

   > Čísla funkcí: `51 = RCPassThru1`, `52 = RCPassThru2`, atd. (51–66 = RCPassThru1–16). Tím se PWM signál z RC vstupu přímo přepošle na daný výstup bez mixingu.
3. Připoj výstupy z Durandalu (např. MAIN OUT 1 a MAIN OUT 3) na Arduino D2 a D3.

### Proč tam je Arduino?

- **Durandal** vydává standardní PWM 1000–2000 µs, ale neumí přímo generovat signál pro **Cytron PWM_DIR** driver (potřebuje PWM + DIR).
- **Arduino** přečte PWM z Durandalu a pomocí knihovny `CytronMotorDriver` řídí směr i rychlost každého motoru.

> QGroundControl slouží pro konfiguraci Durandalu. Arduino převádí jeho PWM výstupy na ovládání motorů.
