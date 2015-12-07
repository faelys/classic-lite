const settings = {  /* "name in local storage": "form input parameter" */
  "backgroundColor":     "background",
  "handColor":           "hands",
  "hourColor":           "hours",
  "innerColor":          "inner",
  "minuteColor":         "minutes",
  "lowBatteryLevel":     "battlvl",
  "batteryColor":        "battcol",
  "bluetoothColor":      "bluetooth",
  "bluetoothVibration":  "vibrate",
  "textColor":           "textcol",
  "textFormat":          "textfmt",
};

function encodeStored(names) {
  var first = true;
  var result = "";
  for (var key in names) {
    var value = localStorage.getItem(key);
    if (value != null) {
      result = result + (first ? "?" : "&") + names[key] + "=" + encodeURIComponent(value);
      first = false;
    }
  }
  return result;
}

Pebble.addEventListener("ready", function() {
  console.log("Classic-Lite PebbleKit JS ready!");
});

Pebble.addEventListener("showConfiguration", function() {
/* TODO: switch to release version:
  Pebble.openURL("https://cdn.rawgit.com/faelys/classic-lite/v1.0/config.html");
*/
  Pebble.openURL("https://rawgit.com/faelys/classic-lite/trunk/config.html" + encodeStored(settings));
});

Pebble.addEventListener("webviewclosed", function(e) {
  var configData = JSON.parse(decodeURIComponent(e.response));

  for (var key in settings) {
    localStorage.setItem(key, configData[key]);
  }

  var dict = {
    1: parseInt(configData["backgroundColor"]),
    2: parseInt(configData["batteryColor"]),
    3: parseInt(configData["bluetoothColor"]),
    4: parseInt(configData["handColor"]),
    5: parseInt(configData["hourColor"]),
    6: parseInt(configData["innerColor"]),
    7: parseInt(configData["minuteColor"]),
    8: parseInt(configData["textColor"]),
    11: configData["textFormat"],
    12: parseInt(configData["bluetoothVibration"]),
    13: parseInt(configData["lowBatteryLevel"]),
  };

  Pebble.sendAppMessage(dict, function() {
    console.log("Send successful: " + JSON.stringify(dict));
  }, function() {
    console.log("Send failed!");
  });
});
