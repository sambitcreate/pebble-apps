var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #aaaaff; }\
.option { padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; flex-direction: column; align-items: flex-start; }\
.option label.title { font-size: 16px; display: block; margin-bottom: 12px; color: #aaaaff; }\
.theme-group { display: flex; flex-direction: column; gap: 8px; width: 100%; }\
.theme-group label { display: flex; align-items: center; gap: 8px; font-size: 15px; cursor: pointer; padding: 8px 12px; border-radius: 6px; background: #333; }\
.theme-group label:hover { background: #3a3a3a; }\
.theme-group input[type="radio"] { appearance: none; -webkit-appearance: none; width: 20px; height: 20px; border: 2px solid #aaaaff; border-radius: 50%; margin-right: 4px; flex-shrink: 0; position: relative; }\
.theme-group input[type="radio"]:checked { background: #aaaaff; box-shadow: inset 0 0 0 3px #1a1a1a; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #aaaaff; color: #1a1a1a; }\
</style>\
</head>\
<body>\
<h1>Maze Settings</h1>\
<div class="option">\
  <label class="title">Color Theme</label>\
  <div class="theme-group">\
    <label><input type="radio" name="theme" value="0"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#FFAA55;"></span> Warm Sunset</label>\
    <label><input type="radio" name="theme" value="1"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAAAFF;"></span> Lavender Dream</label>\
    <label><input type="radio" name="theme" value="2"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#55AAFF;"></span> Cool Ocean</label>\
    <label><input type="radio" name="theme" value="3"> <span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAFFAA;"></span> Forest Meadow</label>\
  </div>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.theme !== undefined) {\
    var radios = document.querySelectorAll("input[name=theme]");\
    for (var i = 0; i < radios.length; i++) {\
      radios[i].checked = (parseInt(radios[i].value) === cfg.theme);\
    }\
  } else {\
    document.querySelector("input[name=theme][value=\\\"1\\\"]").checked = true;\
  }\
} catch(e) {\
  document.querySelector("input[name=theme][value=\\\"1\\\"]").checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var theme = 1;\
  var radios = document.querySelectorAll("input[name=theme]");\
  for (var i = 0; i < radios.length; i++) {\
    if (radios[i].checked) { theme = parseInt(radios[i].value); break; }\
  }\
  var result = { theme: theme };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var cfgTheme = parseInt(localStorage.getItem('theme'));
if (isNaN(cfgTheme)) cfgTheme = 1;

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ theme: cfgTheme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      cfgTheme = (config.theme !== undefined) ? config.theme : cfgTheme;
      localStorage.setItem('theme', cfgTheme.toString());

      Pebble.sendAppMessage({
        'Theme': cfgTheme
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'Theme': cfgTheme
  });
});
