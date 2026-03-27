var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a2e; color: #e0e0e0; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #b388ff; }\
.option { display: flex; align-items: center; justify-content: space-between; padding: 16px; background: #2a2a3e; border-radius: 8px; margin-bottom: 12px; flex-direction: column; align-items: flex-start; }\
.option label { font-size: 16px; margin-bottom: 12px; }\
.theme-list { display: flex; flex-direction: column; gap: 8px; width: 100%; }\
.theme-list label { display: flex; align-items: center; gap: 8px; font-size: 15px; cursor: pointer; }\
.dot { display: inline-block; width: 12px; height: 12px; border-radius: 50%; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #b388ff; color: #1a1a2e; }\
</style>\
</head>\
<body>\
<h1>Tetromino Settings</h1>\
<div class="option">\
  <label>Color Theme</label>\
  <div class="theme-list">\
    <label><input type="radio" name="theme" value="0"> <span class="dot" style="background:#FFAA55;"></span> Warm Sunset</label>\
    <label><input type="radio" name="theme" value="1"> <span class="dot" style="background:#AAAAFF;"></span> Lavender Dream</label>\
    <label><input type="radio" name="theme" value="2"> <span class="dot" style="background:#55AAFF;"></span> Cool Ocean</label>\
    <label><input type="radio" name="theme" value="3"> <span class="dot" style="background:#AAFFAA;"></span> Forest Meadow</label>\
  </div>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  var radios = document.querySelectorAll("input[name=theme]");\
  for (var i = 0; i < radios.length; i++) {\
    if (parseInt(radios[i].value) === cfg.theme) radios[i].checked = true;\
  }\
} catch(e) {}\
if (!document.querySelector("input[name=theme]:checked")) {\
  document.querySelector("input[name=theme][value=\\"0\\"]").checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var sel = document.querySelector("input[name=theme]:checked");\
  var result = { theme: sel ? parseInt(sel.value) : 0 };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var theme = parseInt(localStorage.getItem('theme')) || 0;

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ theme: theme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      theme = (config.theme !== undefined) ? config.theme : 0;
      localStorage.setItem('theme', theme.toString());

      Pebble.sendAppMessage({
        'Theme': theme
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'Theme': theme
  });
});
