Pebble.addEventListener("ready", function() {
  console.log("Classic-Lite PebbleKit JS ready!");
});

Pebble.addEventListener("showConfiguration", function() {
/* TODO: switch to release version:
  Pebble.openURL("https://cdn.rawgit.com/faelys/classic-lite/v1.0/config.html");
*/
  Pebble.openURL("https://rawgit.com/faelys/classic-lite/master/config.html");
});

Pebble.addEventListener("webviewclosed", function(e) {
  var configData = JSON.parse(decodeURIComponent(e.response));

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
