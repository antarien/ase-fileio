# ase-fileio

**Design:** DSGN_016 (AEC//AIOS — Async File I/O)

[![Layer](https://img.shields.io/badge/Layer-0%20Foundation-blue.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()
[![Header Only](https://img.shields.io/badge/Header-Only-green.svg)]()

> Dateizugriff für Nicht-ECS-Code — lesen, schreiben, Verzeichnisse, Pfade. Auf POSIX gebaut.

Teil von [ASE - Antares Simulation Engine](../../..)

## Warum es dieses Modul gibt

ECS-Systeme sichern ihren Zustand über `ase-persist` und MongoDB, nie über Textdateien. Daneben
steht aber eine Menge Nicht-ECS-Code, der legitim Dateien anfassen muss: Client-Werkzeuge, die
`.gitmodules` lesen, Bauhelfer, die VERSION-Register auswerten, Konfigurationslader, Generatoren,
die ihre Ausgabe schreiben, und die Logablage, die alte Dateien aufräumt. Diese Rufer griffen
früher direkt zu den Strom- und Pfadklassen der Standardbibliothek — und genau die verbieten die
Regeln, weil sie Ausnahmen werfen und eine Kopplung in jede Datei tragen, die sie anfasst.

`ase-fileio` ist die Adresse, auf die diese Regeln zeigen.

## Was sich am 2026-08-22 geändert hat, und warum es hier steht

Bis dahin war dieses Modul **selbst mit den verbotenen Bibliotheksteilen gebaut** — es reichte die
Kopplung weiter, statt sie aufzulösen. Der frühere Text an dieser Stelle nannte das eine Erlaubnis:
*„wraps std::ifstream once, in one whitelisted module"* und *„allowed because /foundation/ is in
the validator whitelist"*.

**Diese Behauptung war falsch, und zwar messbar:** `foundation/ase-utils` liegt in derselben Schicht
und wurde für dieselbe Sache mit zwei CRITICAL-Befunden gemeldet. Es gibt keine
Foundation-Ausnahme. Die Befunde dieses Moduls waren Befunde, keine Sonderstellung.

Seitdem ist alles auf POSIX gebaut: `open`/`read`/`write`, `opendir`/`readdir`, `stat`, `mkdir`,
`unlink`, `getcwd`. Das Modul trägt damit **0 Verstöße** — nicht durch eine Ausnahme, sondern weil
der Umweg nie nötig war.

## Öffentliche API

Vier Header, nach der Frage getrennt, die der Rufer stellt.

### Lesen — `text_reader.hpp`

```cpp
#include <ase/fileio/text_reader.hpp>

std::string blob = ase::fileio::read_text("/pfad/datei.txt");          // leer bei Fehler
std::vector<std::string> lines = ase::fileio::read_lines("/pfad/datei.txt");

if (ase::fileio::file_exists("/pfad/datei.txt")) { /* ... */ }         // NUR reguläre Dateien
```

### Schreiben — `text_writer.hpp`

```cpp
#include <ase/fileio/text_writer.hpp>

bool ok = ase::fileio::write_text("/pfad/datei.txt", content);         // ersetzt den Inhalt
```

Genau eine Schreibform, und das ist gemessen: alle acht Schreibstellen im Baum sind Text mit
Truncation, keine hängt an, keine ist binär. Wer eine der beiden Formen braucht, ergänzt sie hier
zusammen mit seiner Aufrufstelle.

### Pfade — `path.hpp`

```cpp
#include <ase/fileio/path.hpp>

bool any  = ase::fileio::path_exists(p);        // JEDER Eintrag, Verzeichnisse eingeschlossen
bool dir  = ase::fileio::is_directory(p);
std::string parent = ase::fileio::parent_of(p); // rein lexikalisch
bool made = ase::fileio::create_directories(p); // schon vorhanden zählt als Erfolg
```

**Die stille Falle, ausdrücklich:** `path_exists` und `file_exists` beantworten verschiedene Fragen
und liefern beide `bool`. `file_exists` ist wahr nur für eine **reguläre Datei**, `path_exists` für
jeden Eintrag. Kein Übersetzer sagt einem Rufer je, dass er den falschen genommen hat — deshalb
tragen sie verschiedene Namen und stehen einen Header auseinander.

### Verzeichnisse — `directory.hpp`

```cpp
#include <ase/fileio/directory.hpp>

ase::fileio::DirEntry entries[64];
uint32_t n = ase::fileio::list_dir(dir, dir_len, entries, 64);   // flach, ohne Filter
bool gone  = ase::fileio::remove_file(path, path_len);           // idempotent
uint32_t len = ase::fileio::current_dir(buf, ase::fileio::DIR_PATH_MAX);
```

`DirEntry` trägt drei Felder: `name`, `modified_secs` (Unix-Sekunden, direkt aus `stat`) und
`is_regular`. Kein `size`, keine Rekursion, kein Filter — der eine Verbraucher filtert selbst, und
eine Fähigkeit ohne Rufer ist Arbeit ohne Nutzen.

## Zwei Aufrufkonventionen, und warum

Die **Textseite** nimmt `const std::string&` und gibt `std::string` zurück. Die **Byteseite**
(`directory.hpp`) nimmt `(const char*, uint32_t)` und schreibt in einen Puffer des Rufers.

Das ist kein Versehen: jede Seite passt zu ihrem gemessenen Rufer. Alle zwölf Aufrufstellen der
Textfunktionen halten bereits eine `std::string`; der Verbraucher der Verzeichnisseite hält ein
`char[DIR_PATH_MAX]`. Eine Funktion, die einen String zurückgibt, aber keinen annehmen will, ist
nicht strenger, nur seltsamer.

## Fehler

Alle Funktionen sind ausnahmefrei. Ein Fehler zeigt sich am Rückgabewert: leerer String, leere
Liste, `false`, oder `0` Einträge — nie als Ausnahme.

**Ein leeres Ergebnis unterscheidet „nicht da" nicht von „leer".** Wer beides trennen muss, fragt
vorher `file_exists` bzw. `path_exists`. Das ist keine Nachlässigkeit, sondern die Stelle, an der
ein Umbau still etwas verliert — `kernel_env_ldr_sys.cpp` prüft aus genau diesem Grund vor dem
Lesen, sonst sähe eine fehlende `.env` aus wie eine leere.

## Schicht

Layer 0 (Foundation), Regeln in `WRFL_ASE_MODULE_DEPENDENCIES.md`. **Keine ASE-Abhängigkeiten** —
die CMakeLists bindet kein `ase::`-Ziel, wie alle elf Foundation-Module. Header-only:
`target_link_libraries(mein_ziel ... ase::fileio)`.

Ein Foundation-Modul bindet dieses hier **nicht**. Wer in L0 eine Datei lesen muss, hat sie an der
falschen Stelle: das Lesen gehört zum Rufer, das Auswerten ins Modul. `ase-utils/dotenv.hpp` ist
das Beispiel — es parst, der Kernel liest.

## Bauen

Teil des ASE-Wurzelbaus.
