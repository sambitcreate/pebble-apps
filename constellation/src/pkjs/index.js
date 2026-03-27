var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #0a0a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #00ffff; }\
.option { display: flex; align-items: center; padding: 16px; background: #1a1a2e; border-radius: 8px; margin-bottom: 12px; cursor: pointer; border: 2px solid transparent; }\
.option.selected { border-color: #00ffff; }\
.option input { display: none; }\
.radio { width: 20px; height: 20px; border-radius: 50%; border: 2px solid #555; margin-right: 14px; flex-shrink: 0; position: relative; }\
.option.selected .radio { border-color: #00ffff; }\
.option.selected .radio::after { content: ""; position: absolute; top: 4px; left: 4px; width: 12px; height: 12px; border-radius: 50%; background: #00ffff; }\
.label { font-size: 16px; }\
.desc { font-size: 12px; color: #888; margin-top: 2px; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #00ffff; color: #0a0a1a; }\
</style>\
</head>\
<body>\
<h1>Constellation Settings</h1>\
<div class="option selected" data-val="0" onclick="pick(this)">\
  <div class="radio"></div>\
  <div><div class="label">Every Second</div><div class="desc">Smooth second hand, star twinkle each second</div></div>\
</div>\
<div class="option" data-val="1" onclick="pick(this)">\
  <div class="radio"></div>\
  <div><div class="label">Every 15 Seconds</div><div class="desc">Second hand updates at :00 :15 :30 :45</div></div>\
</div>\
<div class="option" data-val="2" onclick="pick(this)">\
  <div class="radio"></div>\
  <div><div class="label">No Seconds</div><div class="desc">Hour and minute only, best battery life</div></div>\
</div>\
<div class="option" style="flex-direction: column; align-items: flex-start; cursor: default;">\
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
function pick(el) {\
  document.querySelectorAll(".option").forEach(function(o) { o.classList.remove("selected"); });\
  el.classList.add("selected");\
}\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  document.querySelectorAll(".option[data-val]").forEach(function(o) { o.classList.remove("selected"); });\
  var sel = document.querySelector(".option[data-val=\\"" + cfg.secMode + "\\"]");\
  if (sel) sel.classList.add("selected");\
  var themeVal = cfg.theme || 0;\
  var themeEl = document.querySelector("input[name=theme][value=\\"" + themeVal + "\\"]");\
  if (themeEl) themeEl.checked = true;\
} catch(e) {\
  var defTheme = document.querySelector("input[name=theme][value=\\"0\\"]");\
  if (defTheme) defTheme.checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var val = document.querySelector(".option.selected").getAttribute("data-val");\
  var theme = 0;\
  var themeRadios = document.getElementsByName("theme");\
  for (var i = 0; i < themeRadios.length; i++) { if (themeRadios[i].checked) { theme = parseInt(themeRadios[i].value); break; } }\
  var result = { secMode: parseInt(val, 10), theme: theme };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var secMode = parseInt(localStorage.getItem('secMode') || '0', 10);
var theme = parseInt(localStorage.getItem('theme') || '0', 10);

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ secMode: secMode, theme: theme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      secMode = config.secMode || 0;
      theme = config.theme || 0;
      localStorage.setItem('secMode', secMode.toString());
      localStorage.setItem('theme', theme.toString());

      Pebble.sendAppMessage({ 'SecMode': secMode, 'Theme': theme });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({ 'SecMode': secMode, 'Theme': theme });
});
