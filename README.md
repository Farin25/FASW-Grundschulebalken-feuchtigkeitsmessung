# Fasw Grundschulbalken Feuchtigkeitsmessung

Eine Box wo der Sensor angeschloßen werden kann und dann ausgelessen wird die Daten sind dann über ein Webinterface einsehbar. 

## Die Funktion
Der ESP macht sien eigenes WLAN auf mit den Namen Grundschulbalken. Im WLAN geht mann dann auf die ip Vom ESP: 192.168.4.1. Unter der IP ist dann das Webinterface wo es dann folgende Funktionen gibt: Aktuelle Live Daten (Temperatur und Luftfeuchtigkeit in %), Zeitpunkt der nächsten messung, Diagramme für Luftfeuchtigkeit und Temperatur, Filterfunktion für die angezeigte Zeit im Diagramm. Boutton zum neuladen des Webinterfaces und einen Boutton zum exportieren der daten als CSV Datei. In der CSV Datei sind dann alle messwerte mit Zeitpunkt und gemessenen wert.

<table>
  <tr>
    <th colspan="4">Schema der CSV Datei</th>
  </tr>
  <tr>
    <th>Tag</th>
    <th>Zeit</th>
    <th>Temperatur</th>
    <th>Luftfeuchtigkeit</th>
  </tr>
  <tr>
    <td>2025-12-08</td>
    <td>11:00</td>
    <td>21.5 °C</td>
    <td>45 %</td>
  </tr>
</table>


## Die Hardware
Der Code läuft auf einem ESP 32. Am ESP 32 Wird dann Sensor und Power Led Angeschloßen. Der Akku besteht aus einem Powerbank modul und einer 18650 Zelle im gehäuse ist aber auch noch platz für eine zweite 18650 Zelle.


### Das Gehäuse 
Das Gehäuse ist 3D Gedruckt die 3D Modellen sind im Ordner [Case](/Case/3D%20Drucke/) zu finden. Das Gehäuse besteht aus Einer Box mit jewals Löchern für Schalter und Kabeln und aus einem Deckel wo es eine möglichkeit gibt eine Power Led mit einzubauen. Die Beiden Teile werden dann mit M6 Schrauben Verschraubt. 


### Schaltplan


| GPIO am ESP | Gerät |
|------------|--------|
|21|SHT 31|
|22|SHT 31|
|GND | Output Powerbankmodul, SHT 31 und Power LED|
| 5V | Schalter, SHT 31 und mit Vorwiderstand Power LED |
| Schalter | Output Powerbankmodul |

Zur Stromversorgung wird ein Powerbank modul mit einer oder zwei 18650 Zeellen. Vom USB A Output vom Powerbank modul wird dann eine USB A Kabel was da eingesteckt wird am ESP Angeschloßen für die Stromversorgung wobei hierbei durch + ein Schalter kommt zum An und Aus Schalten. Der Sensor Wird an GPIO 21 und 22 Angeschloßen. Plus und minus Vom Sensor Kommt dann mit dem USB A Kabel an Plus und Minus vom ESP. Optional kann mit einem Vorwiederstand noch eine Power LED Zwischen Plus und Minus Geschaltet Werden.

# Changelog

## V1.0       
**4.11.2025**  

Erste version relativ einfachgehalten nur Grundfunktionen:  
- Live Datenauslessen
- Debug Logs

## V2.0       
**4.11.2025**  

Wechsel  auf c++.
neue Funktion:
- Modernes Webinterface
- Live Werte im Webiterfaec
- Darstellung der Live werte in Diagrammen
- Exportfunktion

## V2.5       
**4.11.2025** 

Änderungen:
- Kleine verbesserungen und bug fixes. 

neue funktionen:
- mDNS Hinzugefügt. webinbterface erreichbar unter sht31.local
- Verionsnummer hinzugefügt
- Link zum Source Code hinzugefügt

#### Copyright (C) 2025 of [Farin](https://farin-langner.de)
 - Distributed under the terms of the GNU General Public License v3.0
