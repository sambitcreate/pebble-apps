var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #e74c3c; }\
.option { padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; }\
.option label { font-size: 16px; display: block; margin-bottom: 8px; }\
.option input, .option select { width: 100%; padding: 10px; font-size: 16px; border: 1px solid #555; border-radius: 6px; background: #333; color: #fff; box-sizing: border-box; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #e74c3c; color: #fff; }\
</style>\
</head>\
<body>\
<h1>Pomodoro Settings</h1>\
<div class="option">\
  <label>Work Duration (minutes)</label>\
  <input type="number" id="work" min="1" max="120" value="25">\
</div>\
<div class="option">\
  <label>Short Break (minutes)</label>\
  <input type="number" id="shortBreak" min="1" max="60" value="5">\
</div>\
<div class="option">\
  <label>Long Break (minutes)</label>\
  <input type="number" id="longBreak" min="1" max="60" value="15">\
</div>\
<div class="option">\
  <label>Pomos Before Long Break</label>\
  <input type="number" id="pomos" min="1" max="12" value="4">\
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
  if (cfg.work) document.getElementById("work").value = cfg.work;\
  if (cfg.shortBreak) document.getElementById("shortBreak").value = cfg.shortBreak;\
  if (cfg.longBreak) document.getElementById("longBreak").value = cfg.longBreak;\
  if (cfg.pomos) document.getElementById("pomos").value = cfg.pomos;\
  if (cfg.theme !== undefined) { var r = document.querySelector("input[name=theme][value=\\\"" + cfg.theme + "\\\"]"); if (r) r.checked = true; }\
} catch(e) {}\
document.getElementById("save").addEventListener("click", function() {\
  var themeEl = document.querySelector("input[name=theme]:checked");\
  var result = {\
    work: parseInt(document.getElementById("work").value, 10) || 25,\
    shortBreak: parseInt(document.getElementById("shortBreak").value, 10) || 5,\
    longBreak: parseInt(document.getElementById("longBreak").value, 10) || 15,\
    pomos: parseInt(document.getElementById("pomos").value, 10) || 4,\
    theme: themeEl ? parseInt(themeEl.value, 10) : 0\
  };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var workDuration = parseInt(localStorage.getItem('work'), 10) || 25;
var shortBreak = parseInt(localStorage.getItem('shortBreak'), 10) || 5;
var longBreak = parseInt(localStorage.getItem('longBreak'), 10) || 15;
var pomos = parseInt(localStorage.getItem('pomos'), 10) || 4;
var theme = parseInt(localStorage.getItem('theme'), 10) || 0;

function sendConfig() {
  Pebble.sendAppMessage({
    'WorkDuration': workDuration,
    'ShortBreak': shortBreak,
    'LongBreak': longBreak,
    'PomosBeforeLong': pomos,
    'Theme': theme
  });
}

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({
    work: workDuration,
    shortBreak: shortBreak,
    longBreak: longBreak,
    pomos: pomos,
    theme: theme
  }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      workDuration = config.work || 25;
      shortBreak = config.shortBreak || 5;
      longBreak = config.longBreak || 15;
      pomos = config.pomos || 4;
      theme = (typeof config.theme === 'number') ? config.theme : 0;

      localStorage.setItem('work', workDuration.toString());
      localStorage.setItem('shortBreak', shortBreak.toString());
      localStorage.setItem('longBreak', longBreak.toString());
      localStorage.setItem('pomos', pomos.toString());
      localStorage.setItem('theme', theme.toString());

      sendConfig();
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  sendConfig();
});
