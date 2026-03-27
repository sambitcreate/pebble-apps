var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #55ffff; }\
.option { display: flex; align-items: center; justify-content: space-between; padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; }\
.option label { font-size: 16px; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #55ffff; color: #1a1a1a; }\
</style>\
</head>\
<body>\
<h1>Orbit Settings</h1>\
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
  var themeVal = cfg.theme || 0;\
  var themeEl = document.querySelector("input[name=theme][value=\\"" + themeVal + "\\"]");\
  if (themeEl) themeEl.checked = true;\
} catch(e) {\
  var defTheme = document.querySelector("input[name=theme][value=\\"0\\"]");\
  if (defTheme) defTheme.checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var theme = 0;\
  var themeRadios = document.getElementsByName("theme");\
  for (var i = 0; i < themeRadios.length; i++) { if (themeRadios[i].checked) { theme = parseInt(themeRadios[i].value); break; } }\
  var result = { theme: theme };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var theme = parseInt(localStorage.getItem('theme') || '0', 10);

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ theme: theme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      theme = config.theme || 0;
      localStorage.setItem('theme', theme.toString());

      Pebble.sendAppMessage({ 'Theme': theme });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({ 'Theme': theme });
});
