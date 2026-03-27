var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #55ff55; }\
.option { padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; }\
.option label.title { font-size: 16px; display: block; margin-bottom: 12px; color: #55ff55; }\
.radio-group { display: flex; flex-direction: column; gap: 10px; }\
.radio-group label { display: flex; align-items: center; font-size: 15px; cursor: pointer; padding: 8px 12px; border-radius: 6px; background: #333; }\
.radio-group label:hover { background: #3a3a3a; }\
.radio-group input[type="radio"] { appearance: none; -webkit-appearance: none; width: 20px; height: 20px; border: 2px solid #55ff55; border-radius: 50%; margin-right: 10px; flex-shrink: 0; position: relative; }\
.radio-group input[type="radio"]:checked { background: #55ff55; box-shadow: inset 0 0 0 3px #1a1a1a; }\
.desc { font-size: 12px; color: #888; margin-left: 30px; margin-top: 2px; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #55ff55; color: #1a1a1a; }\
</style>\
</head>\
<body>\
<h1>Snake Settings</h1>\
<div class="option">\
  <label class="title">Difficulty</label>\
  <div class="radio-group">\
    <label><input type="radio" name="speed" value="250"> Easy</label>\
    <div class="desc">250ms base tick — relaxed pace</div>\
    <label><input type="radio" name="speed" value="200" checked> Normal</label>\
    <div class="desc">200ms base tick — classic speed</div>\
    <label><input type="radio" name="speed" value="150"> Hard</label>\
    <div class="desc">150ms base tick — fast and intense</div>\
  </div>\
</div>\
<div class="option" style="flex-direction: column; align-items: flex-start;">\
  <label class="title" style="margin-bottom: 12px;">Color Theme</label>\
  <div style="display: flex; flex-direction: column; gap: 8px; width: 100%;">\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="0"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#FFAA55;"></span> Warm Sunset</label>\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="1"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAAAFF;"></span> Lavender Dream</label>\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="2"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#55AAFF;"></span> Cool Ocean</label>\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="3"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAFFAA;"></span> Forest Meadow</label>\
  </div>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.baseSpeed) {\
    var radios = document.querySelectorAll("input[name=speed]");\
    for (var i = 0; i < radios.length; i++) {\
      radios[i].checked = (parseInt(radios[i].value) === cfg.baseSpeed);\
    }\
  }\
  if (cfg.theme !== undefined) {\
    var tradios = document.querySelectorAll("input[name=theme]");\
    for (var i = 0; i < tradios.length; i++) {\
      tradios[i].checked = (parseInt(tradios[i].value) === cfg.theme);\
    }\
  } else {\
    var t1 = document.querySelector("input[name=theme][value=\\\"1\\\"]");\
    if (t1) t1.checked = true;\
  }\
} catch(e) {}\
document.getElementById("save").addEventListener("click", function() {\
  var radios = document.querySelectorAll("input[name=speed]");\
  var val = 200;\
  for (var i = 0; i < radios.length; i++) {\
    if (radios[i].checked) { val = parseInt(radios[i].value); break; }\
  }\
  var theme = 1;\
  var tradios = document.querySelectorAll("input[name=theme]");\
  for (var i = 0; i < tradios.length; i++) {\
    if (tradios[i].checked) { theme = parseInt(tradios[i].value); break; }\
  }\
  var result = { baseSpeed: val, theme: theme };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var baseSpeed = parseInt(localStorage.getItem('baseSpeed')) || 200;
var cfgTheme = parseInt(localStorage.getItem('theme'));
if (isNaN(cfgTheme)) cfgTheme = 1;

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ baseSpeed: baseSpeed, theme: cfgTheme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      baseSpeed = config.baseSpeed || 200;
      cfgTheme = (config.theme !== undefined) ? config.theme : cfgTheme;
      localStorage.setItem('baseSpeed', baseSpeed.toString());
      localStorage.setItem('theme', cfgTheme.toString());

      Pebble.sendAppMessage({
        'BaseSpeed': baseSpeed,
        'Theme': cfgTheme
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'BaseSpeed': baseSpeed,
    'Theme': cfgTheme
  });
});
