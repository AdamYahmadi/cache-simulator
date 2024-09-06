# GRA Praktikum SoSe24

## Erklärung der Aufgabenstellung:

### 1. Recherche:
- Unterschide zwischen dem Direct Mapped und Full Associative Cache.

### 2. Implementierung:
- Implementierung der Cache Modelle in SystemC.

### 3. Präsentation:
- Der Vortrag und die Implementierung bilden den Haupteil des Projekts.

## Zusammenfassung der Erkentnisse der Literaturrecherche:
Die Anzahl der Cache-Ebenen (L1, L2, L3) und deren Größen sind typischerweise konsistent über verschiedene Cache-Architekturen hinweg (direkt abgebildet, setz-assoziativ und vollassoziativ).

### *Zusammenfassung der typischen Werte*
| Komponente | Größe | Latenz (Taktzyklen) |
| ---------------- | ---------------- | ---------------- |
| L1-Cache   | 2 KB - 64 KB | 1 - 4  |
| L2-Cache   | 256 KB - 512 KB  | 10 - 20  |
| L3-Cache   | 1 MB - 32 MB  | 20 - 40  |
|Hauptspeicher (RAM) | 4 GB - mehreren Terabytes | 50 - 150

## Ergebnisse des Projekts:
Ein Python-Programm wurde entwickelt, um die Speicherzugriffe während der Ausführung des Merge-Sort Algorithmus in einer csv Datei aufzuzeichnen.
Die Anfragen wurden simuliert und hier sind die Beobachtungen:

- Der Voll Assoziativer Cache mit LRU-Ersetzungsstrategie übertrifft den Direkt Mapped Cache in Bezug auf Hits und Misses.

- Der Direct Mapped Cache hat im Vergleich zum Voll Assoziativer Cache eine niedrigere durchschnittliche Simulationszeit.

- Die Ergebnisse zeigen, dass der Direct Mapped Cache eine bessere Laufzeit aufweist, während der Voll Assoziativer Cache eine höhere Trefferquote erzielt.

Wir haben auch mehrere kleine Test Beipiele die alle mögliche Fälle an den wir gedacht haben ausprobiert.

Test Beispiel:
W,22,300000
W,11,200
W,4,1
R,22,
W,17,100
R,23,
W,F,400
R,10,
R,24,
R,24,

Dieser Test wurde speziell für ein Voll Assoziativer Cache von 4 cacheLines und 2 cacheLineSize ausgetestet.
