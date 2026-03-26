var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #0d0b1a; color: #e0d8f0; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #b388ff; }\
.option { display: flex; align-items: center; justify-content: space-between; padding: 16px; background: #1a1533; border-radius: 8px; margin-bottom: 12px; border: 1px solid #2d2654; }\
.option label { font-size: 16px; }\
.toggle { position: relative; width: 50px; height: 28px; }\
.toggle input { opacity: 0; width: 0; height: 0; }\
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #3a3060; border-radius: 28px; transition: 0.3s; }\
.slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px; background: #e0d8f0; border-radius: 50%; transition: 0.3s; }\
input:checked + .slider { background: #b388ff; }\
input:checked + .slider:before { transform: translateX(22px); }\
.radio-group { padding: 16px; background: #1a1533; border-radius: 8px; margin-bottom: 12px; border: 1px solid #2d2654; }\
.radio-group h2 { font-size: 16px; margin: 0 0 12px 0; color: #e0d8f0; font-weight: normal; }\
.radio-option { display: flex; align-items: center; padding: 10px 0; }\
.radio-option input[type="radio"] { appearance: none; -webkit-appearance: none; width: 20px; height: 20px; border: 2px solid #b388ff; border-radius: 50%; margin-right: 12px; cursor: pointer; position: relative; flex-shrink: 0; }\
.radio-option input[type="radio"]:checked { background: #b388ff; box-shadow: inset 0 0 0 3px #1a1533; }\
.radio-option label { font-size: 15px; cursor: pointer; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #b388ff; color: #0d0b1a; }\
</style>\
</head>\
<body>\
<h1>Magic 8-Ball Settings</h1>\
<div class="option">\
  <label>Vibration on Answer</label>\
  <label class="toggle"><input type="checkbox" id="vibration"><span class="slider"></span></label>\
</div>\
<div class="radio-group">\
  <h2>Animation Speed</h2>\
  <div class="radio-option"><input type="radio" name="speed" id="speed-slow" value="70"><label for="speed-slow">Slow</label></div>\
  <div class="radio-option"><input type="radio" name="speed" id="speed-normal" value="40"><label for="speed-normal">Normal</label></div>\
  <div class="radio-option"><input type="radio" name="speed" id="speed-fast" value="20"><label for="speed-fast">Fast</label></div>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.vibration) document.getElementById("vibration").checked = true;\
  var speedVal = cfg.animSpeed || "40";\
  var radio = document.querySelector("input[name=speed][value=\\"" + speedVal + "\\"]");\
  if (radio) radio.checked = true; else document.getElementById("speed-normal").checked = true;\
} catch(e) {\
  document.getElementById("vibration").checked = true;\
  document.getElementById("speed-normal").checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var speed = document.querySelector("input[name=speed]:checked");\
  var result = {\
    vibration: document.getElementById("vibration").checked,\
    animSpeed: speed ? speed.value : "40"\
  };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var vibration = localStorage.getItem('vibration') !== 'false';
var animSpeed = localStorage.getItem('animSpeed') || '40';

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({
    vibration: vibration,
    animSpeed: animSpeed
  }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      vibration = config.vibration || false;
      animSpeed = config.animSpeed || '40';
      localStorage.setItem('vibration', vibration.toString());
      localStorage.setItem('animSpeed', animSpeed);

      Pebble.sendAppMessage({
        'Vibration': vibration ? 1 : 0,
        'AnimSpeed': parseInt(animSpeed, 10)
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'Vibration': vibration ? 1 : 0,
    'AnimSpeed': parseInt(animSpeed, 10)
  });
});
