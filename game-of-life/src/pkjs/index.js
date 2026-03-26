var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #0a0a0a; color: #00ff41; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 4px; color: #00ff41; text-shadow: 0 0 8px rgba(0,255,65,0.4); }\
.subtitle { font-size: 12px; color: #00aa2a; margin-bottom: 20px; }\
.section { margin-bottom: 20px; }\
.section-title { font-size: 14px; color: #00cc33; margin-bottom: 8px; text-transform: uppercase; letter-spacing: 1px; }\
.option { display: flex; align-items: center; padding: 12px 16px; background: #111; border: 1px solid #003300; border-radius: 6px; margin-bottom: 6px; cursor: pointer; }\
.option:hover { background: #1a1a1a; border-color: #00ff41; }\
.option input[type="radio"] { -webkit-appearance: none; appearance: none; width: 18px; height: 18px; border: 2px solid #00cc33; border-radius: 50%; margin-right: 12px; flex-shrink: 0; position: relative; }\
.option input[type="radio"]:checked { border-color: #00ff41; }\
.option input[type="radio"]:checked::after { content: ""; position: absolute; top: 3px; left: 3px; width: 8px; height: 8px; background: #00ff41; border-radius: 50%; box-shadow: 0 0 6px rgba(0,255,65,0.6); }\
.option label { font-size: 15px; flex-grow: 1; cursor: pointer; }\
.option .desc { font-size: 11px; color: #00aa2a; }\
select { width: 100%; padding: 12px 16px; font-size: 15px; background: #111; color: #00ff41; border: 1px solid #003300; border-radius: 6px; -webkit-appearance: none; appearance: none; }\
select:focus { border-color: #00ff41; outline: none; }\
button { width: 100%; padding: 14px; margin-top: 24px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #00ff41; color: #0a0a0a; text-transform: uppercase; letter-spacing: 1px; }\
button:active { background: #00cc33; }\
</style>\
</head>\
<body>\
<h1>Game of Life</h1>\
<p class="subtitle">Cellular Automaton Settings</p>\
\
<div class="section">\
  <div class="section-title">Simulation Speed</div>\
  <div class="option" onclick="document.getElementById(\\\'s0\\\').checked=true">\
    <input type="radio" name="speed" id="s0" value="0">\
    <div><label for="s0">Slow</label><div class="desc">200ms per generation</div></div>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'s1\\\').checked=true">\
    <input type="radio" name="speed" id="s1" value="1" checked>\
    <div><label for="s1">Normal</label><div class="desc">100ms per generation</div></div>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'s2\\\').checked=true">\
    <input type="radio" name="speed" id="s2" value="2">\
    <div><label for="s2">Fast</label><div class="desc">50ms per generation</div></div>\
  </div>\
</div>\
\
<div class="section">\
  <div class="section-title">Starting Seed Pattern</div>\
  <select id="pattern">\
    <option value="0">R-Pentomino (chaos)</option>\
    <option value="1">Pulsar (oscillator)</option>\
    <option value="2">Lightweight Spaceship</option>\
    <option value="3">Diehard (130 gens)</option>\
  </select>\
</div>\
\
<button id="save">Apply Settings</button>\
\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.speed !== undefined) {\
    var r = document.getElementById("s" + cfg.speed);\
    if (r) r.checked = true;\
  }\
  if (cfg.pattern !== undefined) {\
    document.getElementById("pattern").value = cfg.pattern.toString();\
  }\
} catch(e) {}\
document.getElementById("save").addEventListener("click", function() {\
  var speed = 1;\
  var radios = document.getElementsByName("speed");\
  for (var i = 0; i < radios.length; i++) {\
    if (radios[i].checked) { speed = parseInt(radios[i].value); break; }\
  }\
  var pattern = parseInt(document.getElementById("pattern").value);\
  var result = { speed: speed, pattern: pattern };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var speedPreset = parseInt(localStorage.getItem('speedPreset') || '1');
var seedPattern = parseInt(localStorage.getItem('seedPattern') || '0');

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({
    speed: speedPreset,
    pattern: seedPattern
  }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      speedPreset = (config.speed !== undefined) ? config.speed : speedPreset;
      seedPattern = (config.pattern !== undefined) ? config.pattern : seedPattern;
      localStorage.setItem('speedPreset', speedPreset.toString());
      localStorage.setItem('seedPattern', seedPattern.toString());

      Pebble.sendAppMessage({
        'SpeedPreset': speedPreset,
        'SeedPattern': seedPattern
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'SpeedPreset': speedPreset,
    'SeedPattern': seedPattern
  });
});
