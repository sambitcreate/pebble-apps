var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #0d1117; color: #e6edf3; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #58a6ff; }\
.option { display: flex; align-items: center; justify-content: space-between; padding: 16px; background: #161b22; border: 1px solid #30363d; border-radius: 8px; margin-bottom: 12px; }\
.option label { font-size: 16px; }\
select { background: #21262d; color: #e6edf3; border: 1px solid #30363d; border-radius: 6px; padding: 8px 12px; font-size: 16px; appearance: auto; }\
.toggle { position: relative; width: 50px; height: 28px; }\
.toggle input { opacity: 0; width: 0; height: 0; }\
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #30363d; border-radius: 28px; transition: 0.3s; }\
.slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px; background: #e6edf3; border-radius: 50%; transition: 0.3s; }\
input:checked + .slider { background: #58a6ff; }\
input:checked + .slider:before { transform: translateX(22px); }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #58a6ff; color: #0d1117; }\
</style>\
</head>\
<body>\
<h1>Stopwatch Settings</h1>\
<div class="option">\
  <label>Max Laps</label>\
  <select id="maxLaps">\
    <option value="10">10</option>\
    <option value="20">20</option>\
    <option value="50">50</option>\
    <option value="99">99</option>\
  </select>\
</div>\
<div class="option">\
  <label>Show Lap Deltas</label>\
  <label class="toggle"><input type="checkbox" id="showLapDeltas"><span class="slider"></span></label>\
</div>\
<div class="option" style="flex-direction: column; align-items: flex-start;">\
  <label style="margin-bottom: 12px;">Color Theme</label>\
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
  if (cfg.maxLaps) document.getElementById("maxLaps").value = cfg.maxLaps.toString();\
  if (cfg.showLapDeltas) document.getElementById("showLapDeltas").checked = true;\
  if (cfg.theme !== undefined) { var r = document.querySelector("input[name=theme][value=\\\"" + cfg.theme + "\\\"]"); if (r) r.checked = true; }\
} catch(e) {}\
document.getElementById("save").addEventListener("click", function() {\
  var themeEl = document.querySelector("input[name=theme]:checked");\
  var result = {\
    maxLaps: parseInt(document.getElementById("maxLaps").value, 10),\
    showLapDeltas: document.getElementById("showLapDeltas").checked,\
    theme: themeEl ? parseInt(themeEl.value, 10) : 0\
  };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var maxLaps = parseInt(localStorage.getItem('maxLaps'), 10) || 20;
var showLapDeltas = localStorage.getItem('showLapDeltas') !== 'false';
var theme = parseInt(localStorage.getItem('theme'), 10) || 0;

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({
    maxLaps: maxLaps,
    showLapDeltas: showLapDeltas,
    theme: theme
  }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      maxLaps = config.maxLaps || 20;
      showLapDeltas = config.showLapDeltas || false;
      theme = (typeof config.theme === 'number') ? config.theme : 0;
      localStorage.setItem('maxLaps', maxLaps.toString());
      localStorage.setItem('showLapDeltas', showLapDeltas.toString());
      localStorage.setItem('theme', theme.toString());

      Pebble.sendAppMessage({
        'MaxLaps': maxLaps,
        'ShowLapDeltas': showLapDeltas ? 1 : 0,
        'Theme': theme
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'MaxLaps': maxLaps,
    'ShowLapDeltas': showLapDeltas ? 1 : 0,
    'Theme': theme
  });
});
