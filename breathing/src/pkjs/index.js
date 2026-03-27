var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a2e; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #4ecdc4; }\
h2 { font-size: 14px; color: #7fc8c2; margin: 16px 0 8px 0; text-transform: uppercase; letter-spacing: 1px; }\
.option { display: flex; align-items: center; padding: 14px 16px; background: #16213e; border-radius: 8px; margin-bottom: 8px; cursor: pointer; }\
.option label { font-size: 16px; flex: 1; cursor: pointer; }\
.option input[type="radio"] { width: 20px; height: 20px; accent-color: #4ecdc4; cursor: pointer; }\
.option.selected { border: 1px solid #4ecdc4; }\
.option:not(.selected) { border: 1px solid transparent; }\
button { width: 100%; padding: 14px; margin-top: 24px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #4ecdc4; color: #1a1a2e; }\
</style>\
</head>\
<body>\
<h1>Breathe Settings</h1>\
<h2>Breathing Pattern</h2>\
<div class="option" data-group="pattern" data-val="0">\
  <label>4-7-8 (Relaxation)</label>\
  <input type="radio" name="pattern" value="0">\
</div>\
<div class="option" data-group="pattern" data-val="1">\
  <label>Box Breathing (Focus)</label>\
  <input type="radio" name="pattern" value="1">\
</div>\
<div class="option" data-group="pattern" data-val="2">\
  <label>Simple (Basic)</label>\
  <input type="radio" name="pattern" value="2">\
</div>\
<h2>Session Duration</h2>\
<div class="option" data-group="duration" data-val="0">\
  <label>1 minute</label>\
  <input type="radio" name="duration" value="0">\
</div>\
<div class="option" data-group="duration" data-val="1">\
  <label>2 minutes</label>\
  <input type="radio" name="duration" value="1">\
</div>\
<div class="option" data-group="duration" data-val="2">\
  <label>5 minutes</label>\
  <input type="radio" name="duration" value="2">\
</div>\
<div class="option" data-group="duration" data-val="3">\
  <label>10 minutes</label>\
  <input type="radio" name="duration" value="3">\
</div>\
<h2>Color Theme</h2>\
<div class="option" style="flex-direction: column; align-items: flex-start;">\
  <div style="display: flex; flex-direction: column; gap: 8px; width: 100%;">\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="0"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#FFAA55;"></span> Warm Sunset</label>\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="1"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAAAFF;"></span> Lavender Dream</label>\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="2"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#55AAFF;"></span> Cool Ocean</label>\
    <label style="display:flex; align-items:center; gap:8px;"><input type="radio" name="theme" value="3"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAFFAA;"></span> Forest Meadow</label>\
  </div>\
</div>\
<button id="save">Save</button>\
<script>\
function sel(el) {\
  var group = el.getAttribute("data-group");\
  var opts = document.querySelectorAll(".option[data-group=" + group + "]");\
  for (var i = 0; i < opts.length; i++) {\
    opts[i].classList.remove("selected");\
    opts[i].querySelector("input").checked = false;\
  }\
  el.classList.add("selected");\
  el.querySelector("input").checked = true;\
}\
var options = document.querySelectorAll(".option");\
for (var i = 0; i < options.length; i++) {\
  options[i].addEventListener("click", function() { sel(this); });\
}\
function getVal(name) {\
  var r = document.querySelector("input[name=" + name + "]:checked");\
  return r ? parseInt(r.value, 10) : 0;\
}\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  var pOpt = document.querySelector(".option[data-group=pattern][data-val=" + JSON.stringify(cfg.pattern) + "]");\
  if (pOpt) sel(pOpt);\
  var dOpt = document.querySelector(".option[data-group=duration][data-val=" + JSON.stringify(cfg.duration) + "]");\
  if (dOpt) sel(dOpt);\
  if (cfg.theme !== undefined) { var r = document.querySelector("input[name=theme][value=\\\"" + cfg.theme + "\\\"]"); if (r) r.checked = true; }\
} catch(e) {\
  sel(document.querySelector(".option[data-group=pattern][data-val]"));\
  sel(document.querySelector(".option[data-group=duration][data-val]"));\
}\
document.getElementById("save").addEventListener("click", function() {\
  var themeEl = document.querySelector("input[name=theme]:checked");\
  var result = { pattern: getVal("pattern"), duration: getVal("duration"), theme: themeEl ? parseInt(themeEl.value, 10) : 0 };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var pattern = parseInt(localStorage.getItem('pattern') || '0', 10);
var duration = parseInt(localStorage.getItem('duration') || '0', 10);
var theme = parseInt(localStorage.getItem('theme') || '0', 10);

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ pattern: pattern, duration: duration, theme: theme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      pattern = (typeof config.pattern === 'number') ? config.pattern : 0;
      duration = (typeof config.duration === 'number') ? config.duration : 0;
      theme = (typeof config.theme === 'number') ? config.theme : 0;
      localStorage.setItem('pattern', pattern.toString());
      localStorage.setItem('duration', duration.toString());
      localStorage.setItem('theme', theme.toString());

      Pebble.sendAppMessage({
        'Pattern': pattern,
        'Duration': duration,
        'Theme': theme
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'Pattern': pattern,
    'Duration': duration,
    'Theme': theme
  });
});
