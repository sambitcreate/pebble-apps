var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #ffbf00; }\
.option { display: flex; align-items: center; justify-content: space-between; padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; }\
.option label { font-size: 16px; }\
.toggle { position: relative; width: 50px; height: 28px; }\
.toggle input { opacity: 0; width: 0; height: 0; }\
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #555; border-radius: 28px; transition: 0.3s; }\
.slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px; background: white; border-radius: 50%; transition: 0.3s; }\
input:checked + .slider { background: #ffbf00; }\
input:checked + .slider:before { transform: translateX(22px); }\
.radio-group { display: flex; flex-direction: column; gap: 8px; margin-top: 12px; padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; }\
.radio-group .title { font-size: 16px; margin-bottom: 4px; }\
.radio-option { display: flex; align-items: center; gap: 10px; padding: 8px 0; }\
.radio-option input[type="radio"] { accent-color: #ffbf00; width: 18px; height: 18px; }\
.radio-option label { font-size: 15px; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #ffbf00; color: #1a1a1a; }\
</style>\
</head>\
<body>\
<h1>-  --- .-. ... .  Morse Time</h1>\
<div class="option">\
  <label>24-Hour Format</label>\
  <label class="toggle"><input type="checkbox" id="use24h"><span class="slider"></span></label>\
</div>\
<div class="radio-group">\
  <div class="title">Playback Speed</div>\
  <div class="radio-option"><input type="radio" name="speed" id="speed0" value="0"><label for="speed0">Slow (150 / 450 ms)</label></div>\
  <div class="radio-option"><input type="radio" name="speed" id="speed1" value="1"><label for="speed1">Normal (100 / 300 ms)</label></div>\
  <div class="radio-option"><input type="radio" name="speed" id="speed2" value="2"><label for="speed2">Fast (60 / 180 ms)</label></div>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.use24h) document.getElementById("use24h").checked = true;\
  var speedVal = cfg.speed || 0;\
  var el = document.getElementById("speed" + speedVal);\
  if (el) el.checked = true;\
} catch(e) {\
  document.getElementById("speed1").checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var speed = 1;\
  var radios = document.getElementsByName("speed");\
  for (var i = 0; i < radios.length; i++) { if (radios[i].checked) { speed = parseInt(radios[i].value); break; } }\
  var result = { use24h: document.getElementById("use24h").checked, speed: speed };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var use24h = localStorage.getItem('use24h') === 'true';
var speed = parseInt(localStorage.getItem('speed') || '1', 10);

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ use24h: use24h, speed: speed }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      use24h = config.use24h || false;
      speed = config.speed || 0;
      localStorage.setItem('use24h', use24h.toString());
      localStorage.setItem('speed', speed.toString());

      Pebble.sendAppMessage({
        'TimeFormat': use24h ? 1 : 0,
        'Speed': speed
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'TimeFormat': use24h ? 1 : 0,
    'Speed': speed
  });
});
