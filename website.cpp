#include "website.h"

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Garagentor Steuerung</title>
</head>
<body>
  <h1>Garagentor Steuerung</h1>
  <h2>Zustand: %STATE%</h2>
  <form action="/toggle" method="POST"><button>Toggle</button></form>
  <form action="/open" method="POST"><button>&Ouml;ffnen</button></form>
  <form action="/close" method="POST"><button>Schlie&szlig;en</button></form>
  <form action="/openTime" method="POST"><button>&Ouml;ffne f&uuml;r Minuten:</button> <input type="number" name="openTime" value="0"> </form>
  <h2>Einstellungen</h2>
  <form action="/settings" method="POST">
    <label>Auto-Stopp bei Feststecken:</label>
    <input type="checkbox" name="autoStop" %AUTO_STOP%><br>
    <label>Maximale Zeit beim &Ouml;ffnen (s):</label>
    <input type="number" name="maxOpenTime" value="%MAX_OPEN_TIME%"><br>
    <label>Maximale Zeit beim Schlie&szlig;en (s):</label>
    <input type="number" name="maxCloseTime" value="%MAX_CLOSE_TIME%"><br>
    <label>Automatisches Schlie&szlig;en:</label>
    <input type="checkbox" name="autoClose" %AUTO_CLOSE%><br>
    <label>Verz&ouml;gerung Auto-Schlie&szlig;en (min):</label>
    <input type="number" name="autoCloseDelay" value="%AUTO_CLOSE_DELAY%"><br>
    <label>Warnzeit vor Schlie&szlig;en (sec):</label>
    <input type="number" name="warningDelay" value="%WARNING_DELAY%"><br>
    <label>Automatisches Schlie&szlig;en:</label>
    <input type="checkbox" name="AutoCloseEnabled" %AUTO_CLOSE_CLOCK%><br>
    <button type="submit">Speichern</button>
  </form><br>
  <form action="/set_auto_close_time" method="POST">
    <label for="time">Automatische Schlie&szlig;zeit einstellen (hh:mm):</label><br>
    <p>Aktuell eingestellte Uhrzeit: <span id="currentTime">%CURRENT_TIME%</span></p>
    <input type="time" id="time" name="time"><br><br>
    <input type="submit" value="Speichern">
  </form>
  <h2>Logs</h2>
  <textarea readonly rows="20" cols="100">%LOGS%</textarea>
</body>
</html>
)rawliteral";

const char* htmlAdminPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Garagentor Steuerung</title>
</head>
<body>
  <h1>Garagentor Steuerung</h1>
  <h2>Zustand: %STATE%</h2>
  <form action="/setState" method="POST"><button>Setze Zustand (ZU = 1; AUF = 5)</button><input type="number" name="newState" value="%AKTUELLER_ZUSTAND%"></form><br>
  <form action="/toggle" method="POST"><button>Toggle</button></form>
  <form action="/open" method="POST"><button>&Ouml;ffnen</button></form>
  <form action="/close" method="POST"><button>Schlie&szlig;en</button></form>
  <form action="/openTime" method="POST"><button>&Ouml;ffne f&uuml;r Minuten:</button> <input type="number" name="openTime" value="0"> </form>
  
  <h2>Einstellungen</h2>
  <form action="/settings" method="POST">
    <label>Auto-Stopp bei Feststecken:</label>
    <input type="checkbox" name="autoStop" %AUTO_STOP%><br>
    <label>Maximale Zeit beim &Ouml;ffnen (s):</label>
    <input type="number" name="maxOpenTime" value="%MAX_OPEN_TIME%"><br>
    <label>Maximale Zeit beim Schlie&szlig;en (s):</label>
    <input type="number" name="maxCloseTime" value="%MAX_CLOSE_TIME%"><br>
    <label>Automatisches Schlie&szlig;en:</label>
    <input type="checkbox" name="autoClose" %AUTO_CLOSE%><br>
    <label>Verz&ouml;gerung Auto-Schlie&szlig;en (min):</label>
    <input type="number" name="autoCloseDelay" value="%AUTO_CLOSE_DELAY%"><br>
    <label>Warnzeit vor Schlie&szlig;en (sec):</label>
    <input type="number" name="warningDelay" value="%WARNING_DELAY%"><br>
    <label>Automatisches Schlie&szlig;en:</label>
    <input type="checkbox" name="AutoCloseEnabled" %AUTO_CLOSE_CLOCK%><br>
    <button type="submit">Speichern</button>
  </form><br>
  <form action="/set_auto_close_time" method="POST">
    <p>Aktuell eingestellte Uhrzeit: <span id="currentTime">%CURRENT_TIME%</span></p>
    <label for="time">Automatische Schlie&szlig;zeit einstellen (hh:mm):</label><br>
    <input type="time" id="time" name="time"><br><br>
    <input type="submit" value="Speichern">
  </form>
  
  <h2>Logs</h2>
  <textarea readonly rows="20" cols="100">%LOGS%</textarea>
  
  <h2>Pins</h2>
  <table border="1">
    <tr>
      <th>Pin</th>
      <th>Zustand</th>
    </tr>
    <tr>
      <td>Relais</td>
      <td>%RELAY_STATE%</td>
    </tr>
    <tr>
      <td>ReadAuf</td>
      <td>%READ_AUF_STATE%</td>
    </tr>
    <tr>
      <td>ReadZu</td>
      <td>%READ_ZU_STATE%</td>
    </tr>
    <tr>
      <td>Piezo</td>
      <td>%PIEZO_STATE%</td>
    </tr>
    <tr>
      <td>Trigger</td>
      <td>%TRIGGER_STATE%</td>
    </tr>
  </table>
  <form action="/" method="GET">
    <button type="submit">Pins erneut auslesen</button>
  </form>

  <h2>Read-Kontakte Konfiguration</h2>
  <form action="/read_config" method="POST">
    <label>Welcher Pin ist der Zu-Kontakt?</label><br>
    <input type="radio" id="pin1" name="zuContact" value="1" %PIN1%>
    <label for="pin1">Pin 1</label><br>
    <input type="radio" id="pin2" name="zuContact" value="2" %PIN2%>
    <label for="pin2">Pin 2</label><br>
    <button type="submit">Speichern</button>
  </form>
</body>
</html>
)rawliteral";