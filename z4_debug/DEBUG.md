# Debug plan

## Pitanja kupcu

1. Koji software koristite za spremanje loga na PC ? - npr. PuTTY, RealTerm, TeraTerm, minicom, screen, custom app. Log-app stall moze imitirati freeze.
2. Koji driver/chip koristite za spajanje na naš uredjaj ? - FTDI, CH340, CP210x. Windows za USB-serial ima povremene hiccup-e.
3. Radi li na PC-u ista periodicno u pozadini ? - npr. antivirus, backup, update, monitoring, Windows Update
4. Gdje je smjesten uredjaj ? Postoji li u blizini stroj s periodicnim ciklusom koji stvara EMI (npr. 3 kW motor) ? - EMI korelacija s duty-cycle-om drugog uredjaja.

## Hipoteze

| # | Hipoteza | Test | Log signature |
| :---       | :---        | :---        | :--- |
| 1 | PC-side stall | PC-timestamp incidenata vs Event Log/procesi | Burst podataka sa istim PC timestampom |
| 2 | USB serial driver hiccup | Zamjena kabela/drivera | isti obrazac kao i #1 |
| 3 | FW issue; operacija koja blokira sampling ~200ms  | Ima li rupa vremenski diskontinuitet, i izostaje li burst nakon nje? Je li trajanje dosljedno isto kroz sve pojave? | Rupa u vremenu, **bez** burst-a nakon nje. ~20 redaka ne postoji, ritam se normalno nastavi. Trajanje dosljedno isto (flash write ciklus je determinističan). Razlika od #1/#2: nema naleta jer ti podaci nikad nisu ni poslani. |
| 4 | FW issue; nepravilna sinkronizacija ADC pool threada i UART threada (data-ready flag missing) | Teče li PC timestamp kontinuirano kroz "rupu", uz ponovljene vrijednosti? Vidljivo iz postojećeg loga; seq. broj u sljedećem FW-u daje konačnu potvrdu. #3 i #4 su slični po cijeni | **Nema** vremenske rupe, promet teče na 10 ms bez prekida. ~20 identičnih uzastopnih vrijednosti. Broj redaka točan, samo je podatak ustajao. |
| 5 | Povremeni HW kvar (loš lem, brown-out, EMI reset) |  Iz loga: pojavljuje li se boot/init poruka odmah nakon rupe? Odskače li prvi uzorak nakon rupe? Konačna potvrda traži povrat uređaja i opremu, zato je zadnja po cijeni. | Rupa bez burst-a, kao #3. Razlika: boot poruka nakon rupe.  Trajanje varijabilnije nego kod #3 (flash write je determinističan, reset nije). |

## Izmjene za sljedeći FW (brža dijagnoza ubuduće)

1. Dodati sekvencijski brojač (1B) ili timestamp (4B) u paket i logirati ga na PC-u. To ce omoguciti da se vidi je li problem u gubitku paketa ili duplikatu.
2. Self-monitoring loop timinga: mjeri vrijeme između ADC ISR-ova, ako je veci od praga logiraj incident.
3. Prijava reset-cause/brown-out-a, ovako je reset vidljiv u logu
4. Data-ready flag (volatile): ADC ISR postavlja flag, UART ISR ga resetira. 


## Napomena

Dolje navedeni linkovi su reference na poznate probleme s USB-serial čipovima i driverima, koji mogu uzrokovati slične simptome kao što su opisani u ovom dokumentu.

https://learn.microsoft.com/en-us/answers/questions/3937558/usb-serial-ch340-chipset-not-working-after-windows

https://siliconlabs.my.site.com/community/s/question/0D58Y00008z3biVSAQ/cp210x-stops-receiving-serial-data