var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: "Courier New", monospace; background: #0a0a0a; color: #33ff33; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 6px; color: #33ff33; text-transform: uppercase; letter-spacing: 2px; text-align: center; }\
.subtitle { font-size: 12px; color: #1a9a1a; text-align: center; margin-bottom: 24px; }\
.group { margin-bottom: 20px; }\
.group-label { font-size: 14px; color: #33ff33; margin-bottom: 10px; text-transform: uppercase; letter-spacing: 1px; border-bottom: 1px solid #1a4a1a; padding-bottom: 6px; }\
.option { display: flex; align-items: center; padding: 12px 16px; background: #111; border: 1px solid #1a4a1a; border-radius: 4px; margin-bottom: 6px; cursor: pointer; }\
.option:hover { background: #1a2a1a; border-color: #33ff33; }\
.option input[type="radio"] { appearance: none; -webkit-appearance: none; width: 16px; height: 16px; border: 2px solid #33ff33; border-radius: 50%; margin-right: 12px; flex-shrink: 0; position: relative; }\
.option input[type="radio"]:checked { background: #33ff33; box-shadow: inset 0 0 0 3px #0a0a0a; }\
.option label { font-size: 15px; color: #33ff33; flex: 1; cursor: pointer; }\
button { width: 100%; padding: 14px; margin-top: 24px; font-size: 16px; font-weight: bold; font-family: "Courier New", monospace; text-transform: uppercase; letter-spacing: 2px; border: 2px solid #33ff33; border-radius: 4px; cursor: pointer; background: #33ff33; color: #0a0a0a; }\
button:active { background: #1a9a1a; }\
</style>\
</head>\
<body>\
<h1>Pixel Invaders</h1>\
<div class="subtitle">-- Configuration --</div>\
<div class="group">\
  <div class="group-label">Starting Lives</div>\
  <div class="option" onclick="document.getElementById(\\\'l3\\\').checked=true">\
    <input type="radio" name="lives" id="l3" value="3">\
    <label for="l3">3 Lives</label>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'l5\\\').checked=true">\
    <input type="radio" name="lives" id="l5" value="5">\
    <label for="l5">5 Lives</label>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'l7\\\').checked=true">\
    <input type="radio" name="lives" id="l7" value="7">\
    <label for="l7">7 Lives</label>\
  </div>\
</div>\
<div class="group">\
  <div class="group-label">Difficulty</div>\
  <div class="option" onclick="document.getElementById(\\\'d0\\\').checked=true">\
    <input type="radio" name="difficulty" id="d0" value="0">\
    <label for="d0">Easy - Slow Aliens</label>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'d1\\\').checked=true">\
    <input type="radio" name="difficulty" id="d1" value="1">\
    <label for="d1">Normal</label>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'d2\\\').checked=true">\
    <input type="radio" name="difficulty" id="d2" value="2">\
    <label for="d2">Hard - Fast Aliens</label>\
  </div>\
</div>\
<div class="group">\
  <div class="group-label">Color Theme</div>\
  <div class="option" onclick="document.getElementById(\\\'t0\\\').checked=true">\
    <input type="radio" name="theme" id="t0" value="0">\
    <label for="t0"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#FFAA55;margin-right:8px;"></span>Warm Sunset</label>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'t1\\\').checked=true">\
    <input type="radio" name="theme" id="t1" value="1">\
    <label for="t1"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAAAFF;margin-right:8px;"></span>Lavender Dream</label>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'t2\\\').checked=true">\
    <input type="radio" name="theme" id="t2" value="2">\
    <label for="t2"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#55AAFF;margin-right:8px;"></span>Cool Ocean</label>\
  </div>\
  <div class="option" onclick="document.getElementById(\\\'t3\\\').checked=true">\
    <input type="radio" name="theme" id="t3" value="3">\
    <label for="t3"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAFFAA;margin-right:8px;"></span>Forest Meadow</label>\
  </div>\
</div>\
<button id="save">Save Settings</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.lives) document.getElementById("l" + cfg.lives).checked = true;\
  if (cfg.difficulty !== undefined) document.getElementById("d" + cfg.difficulty).checked = true;\
  if (cfg.theme !== undefined) {\
    var te = document.getElementById("t" + cfg.theme);\
    if (te) te.checked = true;\
  } else {\
    document.getElementById("t1").checked = true;\
  }\
} catch(e) {\
  document.getElementById("l3").checked = true;\
  document.getElementById("d1").checked = true;\
  document.getElementById("t1").checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var lives = 3;\
  var radios = document.getElementsByName("lives");\
  for (var i = 0; i < radios.length; i++) { if (radios[i].checked) { lives = parseInt(radios[i].value); break; } }\
  var difficulty = 1;\
  var dradios = document.getElementsByName("difficulty");\
  for (var i = 0; i < dradios.length; i++) { if (dradios[i].checked) { difficulty = parseInt(dradios[i].value); break; } }\
  var theme = 1;\
  var tradios = document.getElementsByName("theme");\
  for (var i = 0; i < tradios.length; i++) { if (tradios[i].checked) { theme = parseInt(tradios[i].value); break; } }\
  var result = { lives: lives, difficulty: difficulty, theme: theme };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var cfgLives = parseInt(localStorage.getItem('lives')) || 3;
var cfgDifficulty = parseInt(localStorage.getItem('difficulty'));
if (isNaN(cfgDifficulty)) cfgDifficulty = 1;
var cfgTheme = parseInt(localStorage.getItem('theme'));
if (isNaN(cfgTheme)) cfgTheme = 1;

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ lives: cfgLives, difficulty: cfgDifficulty, theme: cfgTheme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      cfgLives = config.lives || 3;
      cfgDifficulty = (config.difficulty !== undefined) ? config.difficulty : 1;
      cfgTheme = (config.theme !== undefined) ? config.theme : cfgTheme;
      localStorage.setItem('lives', cfgLives.toString());
      localStorage.setItem('difficulty', cfgDifficulty.toString());
      localStorage.setItem('theme', cfgTheme.toString());

      Pebble.sendAppMessage({
        'Lives': cfgLives,
        'Difficulty': cfgDifficulty,
        'Theme': cfgTheme
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'Lives': cfgLives,
    'Difficulty': cfgDifficulty,
    'Theme': cfgTheme
  });
});
