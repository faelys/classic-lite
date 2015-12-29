/*
 * Copyright (c) 2015, Natacha Porté
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

const settings = {  /* "name in local storage": "form input parameter" */
  "backgroundColor":     "background",
  "handColor":           "hands",
  "hourColor":           "hours",
  "innerColor":          "inner",
  "minuteColor":         "minutes",
  "lowBatteryLevel":     "battlvl",
  "batteryColor":        "battcol",
  "batteryColor2":       "battcol2",
  "bluetoothColor":      "bluetooth",
  "bluetoothVibration":  "vibrate",
  "textColor":           "textcol",
  "textFormat":          "textfmt",
};

function encodeStored(names) {
  var result = "?v=1.2";
  for (var key in names) {
    var value = localStorage.getItem(key);
    if (value != null) {
      result = result + "&" + names[key] + "=" + encodeURIComponent(value);
    }
  }
  return result;
}

Pebble.addEventListener("ready", function() {
  console.log("Classic-Lite PebbleKit JS ready!");
});

Pebble.addEventListener("showConfiguration", function() {
  Pebble.openURL("https://cdn.rawgit.com/faelys/classic-lite/v1.0/config.html" + encodeStored(settings));
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
    9: parseInt(configData["batteryColor2"]),
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
