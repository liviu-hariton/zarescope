<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Ghidul de utilizare, afișat în tabul Ajutor al aplicației (Ajutor › Ajutor Zarescope, ⌘?).
     Markdown simplu; src/ui/Guide.h spune ce afișează aplicația. Ține-l în pas cu
     ghidul în engleză (guide.en.md): aceleași secțiuni, în aceeași ordine, cu aceleași id-uri. -->

# Ajutor Zarescope

Zarescope transformă un stick RTL-SDR într-o fereastră spre cerul radio: spectrul în direct, avioanele pe un radar și sateliții meteo Meteor-M, pe care îi înregistrează și îi decodează singur. Doar recepționează; nu transmite niciodată.

<!-- id: start -->
## Primii pași

Tabul Descoperă te poartă prin acești pași și îi bifează pe măsură ce îi faci.

1. Setează-ți locația. Deschide Setările (rotița din dreapta sus sau ⌘,), apoi scrie latitudinea și longitudinea sau apasă Folosește locația mea actuală. Locația decide ce pasaje de sateliți vezi și cât de departe sunt avioanele.
2. Conectează un receptor. Pune stickul în port, apasă eticheta receptorului din dreapta sus (Niciun receptor), apoi Deschide lângă el. Nu ai încă un stick? Deschide o înregistrare (⌘O).
3. Alege ce să recepționezi. În Descoperă, fiecare intrare spune ce vei auzi și ce antenă ajută: radioul FM e cel mai ușor prim semnal.

<!-- id: receiver -->
## Receptorul

Merge orice stick cu cipul RTL2832U: RTL-SDR Blog V3 și V4, Nooelec NESDR SMArt și stickurile generice R820T. Zarescope citește ce tuner are stickul de fapt; dacă nu se potrivește cu modelul pe care pretinde că îl are, panoul receptorului o spune, iar acesta e semnul obișnuit al unei contrafaceri.

| Control | Ce face |
|---|---|
| Câștig | Cât de tare amplifică tunerul. Mai mult aude semnale mai slabe, dar posturile puternice din apropiere îl saturează și se întind pe tot spectrul. Auto lasă tunerul să decidă; începe cu el. |
| Rata de eșantionare | Cât de lată e felia de spectru pe care o captează stickul, în milioane de eșantioane pe secundă (MS/s). 2,4 MS/s arată 2,4 MHz deodată. Avioanele au nevoie de 2 MS/s; presetarea ADS-B o setează. |
| Corecția frecvenței (ppm) | Corectează un stick al cărui ceas merge puțin prea repede sau prea încet, ca posturile să apară unde trebuie. Stickurile cu TCXO (V3, V4) rareori au nevoie de ea. |
| Bias-tee | Trimite curent pe cablul antenei, pentru un amplificator montat la antenă. Oprit dacă nu îl pornești tu, după un avertisment: pe o antenă obișnuită nu face nimic, iar unor echipamente le poate face rău. |

<!-- id: spectrum -->
## Spectrul și waterfall-ul

Jumătatea de sus e spectrul: puterea semnalului pe frecvențele captate de stick, în dBFS (0 e cel mai puternic semnal pe care îl poate măsura stickul, deci semnalele reale sunt numere negative). Waterfall-ul de dedesubt e același lucru în timp: fiecare linie nouă e un moment, care coboară, mai luminoasă unde semnalul e mai puternic. Un post apare ca o bandă; un semnal scurt, ca o convorbire radio, ca o liniuță.

- Scrie o frecvență: dă clic pe frecvența de sus, scrie cifrele, apoi apasă Return.
- Schimbă o singură cifră: derulează deasupra ei sau dă clic pe ea și folosește tastele ↑ și ↓.
- Dă clic pe waterfall sau pe spectru ca să acordezi acolo; derulează deasupra lor ca să pășești în sus și în jos.
- Mărimea FFT (butonul de deasupra spectrului, în dreapta) decide cât de fin e împărțit spectrul: mai multe puncte arată semnale mai înguste, dar se actualizează mai rar.
- Pragurile de jos și de sus ale waterfall-ului decid ce niveluri apar întunecat și ce luminos. Potrivește după semnal le setează după ce e pe ecran.

O înregistrare se redă pe frecvența la care a fost făcută: poți acorda în interiorul ei, dar nu în afara ei.

<!-- id: listening -->
## Ascultarea

Alege cum devine semnalul sunet cu butoanele de mod de sus: WFM pentru radioul FM, NFM pentru stații portabile și radioamatori, AM pentru avioane. Off oprește sunetul.

Frecvența ascultată e marcajul de pe spectru. Butonul cu difuzorul deschide setările de sunet:

| Setare | Ce face |
|---|---|
| Volum | Cât de tare. |
| Squelch | Liniște cât timp semnalul e mai slab decât acest nivel, ca să auzi doar când vorbește cineva. Pune-l puțin peste zgomot. |
| Lățime de bandă | Cât de lată e felia transformată în sunet. Prea îngustă taie vocea; prea lată adaugă zgomot și postul de alături. |
| De-emfază | Doar WFM: 50 µs în Europa, 75 µs în cele două Americi. |

Presetările (săgeata de lângă frecvență sau Receptor › Acordează pe) setează deodată frecvența, modul și lățimea de bandă.

<!-- id: recording -->
## Înregistrarea și redarea

Înregistrează (⇧⌘R) salvează semnalul brut primit de stick, ca eșantioane IQ: un fișier .cu8 și, alături, un mic fișier .json cu frecvența, rata de eșantionare, ora și câștigul. Îl poți reda mai târziu în Zarescope (⌘O) ca și cum stickul ar fi conectat sau îl poți deschide în alte programe SDR. Un minut la 2,4 MS/s înseamnă circa 290 MB, iar bannerul arată mărimea pe măsură ce crește.

Înregistrările merg în dosarul de date: Application Support › Zarescope › recordings, dacă nu ai ales alt dosar în Setări. Schimbarea frecvenței sau a ratei de eșantionare încheie înregistrarea, pentru că un fișier are doar câte una din fiecare.

<!-- id: aircraft -->
## Avioanele

Avioanele își transmit pe 1090 MHz (ADS-B) cine sunt, unde sunt, la ce înălțime și cu ce viteză zboară. Alege Avioane (ADS-B) din presetări: stickul se acordează pe 1090 MHz la 2 MS/s și se deschide tabul Avioane.

Tabelul arată fiecare avion: indicativul, altitudinea, viteza, distanța față de tine și de cât timp nu s-a mai auzit. Radarul le așază în jurul locației tale, cu urma lor în spate; dă clic pe unul ca să-l selectezi în tabel. Avioanele dispar la un minut după ultimul mesaj.

Raza depinde mai ales de antenă: sus și departe de clădiri, o antenă simplă vede o sută de kilometri sau mai mult.

<!-- id: satellites -->
## Sateliții meteo

Tabul Sateliți arată pasajele Meteor-M peste locația ta în următoarele două zile: când începe fiecare, cât de sus urcă (elevația maximă) și cât durează. Cardul arată numărătoarea inversă până la următorul; diagrama cerului arată drumul pasajului selectat pe cerul tău, cu nordul în sus. Cu cinci minute înainte de fiecare pasaj primești o notificare. Orbitele vin de la CelesTrak și se reîmprospătează cel mult de două ori pe zi; vârsta lor apare sub listă.

Pasajele sub elevația minimă (Setări, 20° la început) nu apar în listă: pasajele joase dau rareori o imagine bună.

### Înregistrarea automată

Pornește Înregistrează și decodează automat pasajele, iar Zarescope se ocupă singur de fiecare pasaj:

1. La începutul pasajului preia receptorul, se acordează pe satelit și înregistrează până când satelitul apune. Un banner arată timpul rămas; Oprește înregistrarea o încheie mai devreme.
2. Apoi îți dă receptorul înapoi exact cum îl aveai.
3. SatDump transformă înregistrarea în imagini, în Application Support › Zarescope › products, câte un dosar pentru fiecare pasaj. Bannerul și o notificare spun cum a mers; Arată în Finder deschide dosarul.

Pentru asta e nevoie de SatDump, instalat separat: în Terminal, brew install --cask satdump, apoi Caută din nou SatDump (meniul Receptor). Mac-ul trebuie să rămână treaz și receptorul conectat; un Mac în repaus ratează pasajele. După o decodare reușită înregistrarea se șterge (are circa 2 MB pe secundă), dacă Setările nu spun să fie păstrată.

<!-- id: journal -->
## Jurnalul

Tabul Jurnal păstrează ce s-a recepționat: fiecare pasaj de satelit și rezultatul lui, imaginile, fiecare avion o dată pe zi și înregistrările pe care le-ai făcut. Filtrează după tip sau caută după nume; Arată deschide un fișier sau un dosar în Finder. Exportă CSV și Exportă JSON salvează ce arată filtrul, pentru un tabel de calcul sau alt program.

<!-- id: settings -->
## Setările

| Setare | Ce face |
|---|---|
| Temă | Întunecată sau luminoasă. |
| Mărimea textului | Standard, Mare sau Foarte mare: crește toată interfața, textul și controalele împreună. Și cu ⌘+, ⌘- și ⌘0. |
| Unități de măsură | Cum se afișează altitudinile, vitezele și distanțele: Metrice (m, km/h, km), Aviatice (ft, kt, NM, cum folosesc piloții și controlorii de trafic) sau Imperiale (ft, mph, mi). Altitudinea stației le urmează și ea. |
| Locația stației | Latitudinea, longitudinea și altitudinea; Folosește locația mea actuală o cere de la macOS (îți cere permisiunea prima dată). |
| Elevația minimă a pasajelor | Pasajele mai joase nu apar în listă și nu se înregistrează. |
| Înregistrează și decodează automat pasajele | Același comutator ca în tabul Sateliți. |
| Păstrează înregistrările pasajelor după decodare | Păstrează înregistrarea brută a unui pasaj decodat, circa 2 MB pe secundă. |
| Receptor | Rata de eșantionare cu care se deschide un stick și corecția lui de frecvență. |
| Dosarul de date | Unde merg înregistrările, imaginile, cache-ul orbitelor și jurnalul. Se aplică după repornirea Zarescope. |
| Limba | Engleză sau română. |
| Actualizări | Caută o versiune nouă o dată pe zi: oprită până o pornești. Verifică acum întreabă imediat, la fel ca Zarescope › Caută actualizări…. Zarescope cere de la zarescope.eu doar numărul celei mai noi versiuni; nu se trimite nimic despre tine și nu se descarcă și nu se instalează nimic. Când există o versiune nouă, bara de stare îți spune; un clic pe ea deschide pagina de descărcare. |

Setările se salvează pe măsură ce le schimbi.

<!-- id: shortcuts -->
## Scurtături de tastatură

| Taste | Acțiune |
|---|---|
| ⌘, | Setări |
| ⇧⌘K | Conectează un receptor |
| ⌘O | Deschide o înregistrare |
| ⇧⌘R | Începe sau oprește înregistrarea |
| ⌘1 până la ⌘6 | Descoperă, Spectru, Avioane, Sateliți, Jurnal, Ajutor |
| ⌘+ și ⌘- | Text mai mare sau mai mic |
| ⌘0 | Mărimea standard a textului |
| ⌃⌘F | Ecran complet |
| ⌘? | Acest ghid |
| ⌘W | Închide fereastra Despre sau Zarescope |
| ⌘Q | Ieșire |
| ↑ și ↓ | Pe o cifră a frecvenței: o schimbă. În acest ghid: derulează. |
| Esc | Închide un panou sau fereastra Despre |

<!-- id: troubleshooting -->
## Probleme frecvente

| Problemă | Ce să încerci |
|---|---|
| Niciun receptor găsit | Scoate stickul și pune-l în alt port, fără hub, apoi Caută din nou. Închide celelalte programe SDR: un singur program poate folosi stickul odată. |
| Tunerul nu se potrivește | Stickul pretinde că e un model, dar are tunerul altuia: nu e ce spune că este. Poate merge totuși, dar cu limitele acelui tuner. |
| Nu se aude nimic | Verifică să fie ales un mod (nu Off), volumul și squelch-ul: trage-l spre stânga. Panoul cu difuzorul spune dacă macOS nu are ieșire de sunet. |
| Spectrul e plin de copii ale unui post | Câștigul e prea mare: scade-l sau pune-l pe Auto. |
| Nu apar avioane | Folosește presetarea ADS-B, ca stickul să meargă la 2 MS/s pe 1090 MHz. Pune antena lângă o fereastră, cât mai sus. |
| Un pasaj n-a dat nicio imagine | Semnalul a fost prea slab: așteaptă un pasaj mai înalt (peste 40°) și folosește dipolul în V orizontal, afară, sub cerul liber. Logul pasajului e în dosarul lui (Arată logul). |
| Folosește locația mea actuală nu merge | Pornește Serviciile de localizare pentru Zarescope în System Settings › Privacy & Security sau scrie coordonatele. |
| macOS nu deschide aplicația | Prima dată, deschide System Settings › Privacy & Security și apasă Open Anyway. |

<!-- id: glossary -->
## Cuvinte folosite aici

| Cuvânt | Înțeles |
|---|---|
| SDR | Radio definit prin software: stickul doar digitizează semnalul radio; restul îl face calculatorul. |
| Eșantioane IQ | Semnalul brut digitizat, două numere pentru fiecare moment (I și Q). Ce salvează Înregistrează. |
| dBFS | Decibeli sub scala maximă: 0 e cel mai puternic semnal pe care îl poate măsura stickul; −60 e de un milion de ori mai slab ca putere. |
| FFT | Calculul care transformă semnalul într-un spectru. |
| Squelch | O poartă care ține sunetul oprit cât timp nu transmite nimeni. |
| ADS-B | Semnalul prin care avioanele își transmit poziția, altitudinea și viteza. |
| AOS și LOS | Apariția și pierderea semnalului: când un satelit răsare deasupra orizontului tău și când apune. |
| Elevație | Cât de sus deasupra orizontului: 0° la orizont, 90° drept deasupra capului. |
| LRPT | Semnalul digital cu imagini al sateliților Meteor-M, pe 137,9 MHz. |
| TLE | Elemente pe două linii: descrierea orbitei din care se prezic pasajele. |
| ppm | Părți pe milion: cât de mult greșește ceasul stickului. |
