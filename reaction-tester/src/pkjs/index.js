var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #ff6633; }\
.group { background: #2a2a2a; border-radius: 8px; padding: 16px; margin-bottom: 16px; }\
.group-label { font-size: 14px; color: #ff6633; text-transform: uppercase; margin-bottom: 12px; letter-spacing: 1px; }\
.radio-row { display: flex; align-items: center; padding: 10px 0; border-bottom: 1px solid #333; cursor: pointer; }\
.radio-row:last-child { border-bottom: none; }\
.radio-row input { display: none; }\
.radio-dot { width: 22px; height: 22px; border-radius: 50%; border: 2px solid #666; margin-right: 12px; display: flex; align-items: center; justify-content: center; flex-shrink: 0; }\
.radio-row input:checked ~ .radio-dot { border-color: #ff6633; }\
.radio-row input:checked ~ .radio-dot::after { content: ""; width: 12px; height: 12px; background: #ff6633; border-radius: 50%; display: block; }\
.radio-label { font-size: 16px; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #ff6633; color: #fff; }\
</style>\
</head>\
<body>\
<h1>Reaction Tester Settings</h1>\
<div class="group">\
  <div class="group-label">Rounds per session</div>\
  <label class="radio-row"><input type="radio" name="rounds" value="5"><span class="radio-dot"></span><span class="radio-label">5 rounds</span></label>\
  <label class="radio-row"><input type="radio" name="rounds" value="10"><span class="radio-dot"></span><span class="radio-label">10 rounds</span></label>\
  <label class="radio-row"><input type="radio" name="rounds" value="15"><span class="radio-dot"></span><span class="radio-label">15 rounds</span></label>\
  <label class="radio-row"><input type="radio" name="rounds" value="20"><span class="radio-dot"></span><span class="radio-label">20 rounds</span></label>\
</div>\
<div class="group">\
  <div class="group-label">Color Theme</div>\
  <label class="radio-row"><input type="radio" name="theme" value="0"><span class="radio-dot"></span><span class="radio-label"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#FFAA55;margin-right:6px;"></span>Warm Sunset</span></label>\
  <label class="radio-row"><input type="radio" name="theme" value="1"><span class="radio-dot"></span><span class="radio-label"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAAAFF;margin-right:6px;"></span>Lavender Dream</span></label>\
  <label class="radio-row"><input type="radio" name="theme" value="2"><span class="radio-dot"></span><span class="radio-label"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#55AAFF;margin-right:6px;"></span>Cool Ocean</span></label>\
  <label class="radio-row"><input type="radio" name="theme" value="3"><span class="radio-dot"></span><span class="radio-label"><span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:#AAFFAA;margin-right:6px;"></span>Forest Meadow</span></label>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  var radios = document.querySelectorAll("input[name=rounds]");\
  for (var i = 0; i < radios.length; i++) {\
    if (parseInt(radios[i].value) === cfg.rounds) radios[i].checked = true;\
  }\
  if (cfg.theme !== undefined) {\
    var tr = document.querySelector("input[name=theme][value=\\"" + cfg.theme + "\\"]");\
    if (tr) tr.checked = true;\
  }\
} catch(e) {}\
if (!document.querySelector("input[name=rounds]:checked")) {\
  document.querySelector("input[name=rounds][value=\\"10\\"]").checked = true;\
}\
if (!document.querySelector("input[name=theme]:checked")) {\
  document.querySelector("input[name=theme][value=\\"0\\"]").checked = true;\
}\
document.getElementById("save").addEventListener("click", function() {\
  var sel = document.querySelector("input[name=rounds]:checked");\
  var themeSel = document.querySelector("input[name=theme]:checked");\
  var result = { rounds: sel ? parseInt(sel.value) : 10, theme: themeSel ? parseInt(themeSel.value) : 0 };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var rounds = parseInt(localStorage.getItem('rounds')) || 10;
var theme = parseInt(localStorage.getItem('theme') || '0', 10);

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ rounds: rounds, theme: theme }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      rounds = config.rounds || 10;
      theme = (config.theme !== undefined) ? config.theme : 0;
      localStorage.setItem('rounds', rounds.toString());
      localStorage.setItem('theme', theme.toString());

      Pebble.sendAppMessage({
        'Rounds': rounds,
        'Theme': theme
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'Rounds': rounds,
    'Theme': theme
  });
});
