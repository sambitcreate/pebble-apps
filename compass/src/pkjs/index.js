var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #2b2017; color: #e8dcc8; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 6px; color: #c9a96e; }\
p.subtitle { font-size: 13px; color: #8a7a66; margin-top: 0; margin-bottom: 20px; }\
.group { background: #3a2e22; border-radius: 8px; padding: 4px 0; margin-bottom: 16px; }\
.option { display: flex; align-items: center; padding: 14px 16px; cursor: pointer; }\
.option:not(:last-child) { border-bottom: 1px solid #4a3c2e; }\
.option label { font-size: 16px; flex: 1; cursor: pointer; }\
.option .desc { font-size: 12px; color: #8a7a66; }\
.radio { width: 22px; height: 22px; border-radius: 50%; border: 2px solid #6b5a42; background: transparent; flex-shrink: 0; display: flex; align-items: center; justify-content: center; margin-right: 12px; }\
.radio.checked { border-color: #c9a96e; }\
.radio.checked::after { content: ""; width: 12px; height: 12px; border-radius: 50%; background: #c9a96e; display: block; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #c9a96e; color: #2b2017; }\
</style>\
</head>\
<body>\
<h1>Compass Settings</h1>\
<p class="subtitle">Needle damping / responsiveness</p>\
<div class="group">\
  <div class="option" data-val="95">\
    <div class="radio" id="r95"></div>\
    <div><label>Smooth</label><div class="desc">High damping, steady needle</div></div>\
  </div>\
  <div class="option" data-val="85">\
    <div class="radio" id="r85"></div>\
    <div><label>Normal</label><div class="desc">Balanced responsiveness</div></div>\
  </div>\
  <div class="option" data-val="70">\
    <div class="radio" id="r70"></div>\
    <div><label>Responsive</label><div class="desc">Low damping, quick reaction</div></div>\
  </div>\
</div>\
<button id="save">Save</button>\
<script>\
var selected = 85;\
try {\
  var params = window.location.hash.substring(1);\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.damping) selected = cfg.damping;\
} catch(e) {}\
function updateRadios() {\
  var opts = document.querySelectorAll(".option");\
  for (var i = 0; i < opts.length; i++) {\
    var r = opts[i].querySelector(".radio");\
    if (parseInt(opts[i].getAttribute("data-val")) === selected) {\
      r.className = "radio checked";\
    } else {\
      r.className = "radio";\
    }\
  }\
}\
updateRadios();\
var opts = document.querySelectorAll(".option");\
for (var i = 0; i < opts.length; i++) {\
  opts[i].addEventListener("click", function() {\
    selected = parseInt(this.getAttribute("data-val"));\
    updateRadios();\
  });\
}\
document.getElementById("save").addEventListener("click", function() {\
  var result = { damping: selected };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var damping = parseInt(localStorage.getItem('damping')) || 85;

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ damping: damping }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      damping = config.damping || 85;
      localStorage.setItem('damping', damping.toString());

      Pebble.sendAppMessage({
        'NeedleDamping': damping
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'NeedleDamping': damping
  });
});
