var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #55ff55; }\
.option { display: flex; align-items: center; justify-content: space-between; padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; }\
.option label { font-size: 16px; }\
.toggle { position: relative; width: 50px; height: 28px; }\
.toggle input { opacity: 0; width: 0; height: 0; }\
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #555; border-radius: 28px; transition: 0.3s; }\
.slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px; background: white; border-radius: 50%; transition: 0.3s; }\
input:checked + .slider { background: #55ff55; }\
input:checked + .slider:before { transform: translateX(22px); }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #55ff55; color: #1a1a1a; }\
</style>\
</head>\
<body>\
<h1>Binary Clock Settings</h1>\
<div class="option">\
  <label>Show Digital Time</label>\
  <label class="toggle"><input type="checkbox" id="showDigital"><span class="slider"></span></label>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try { var cfg = JSON.parse(decodeURIComponent(params)); if (cfg.showDigital) document.getElementById("showDigital").checked = true; } catch(e) {}\
document.getElementById("save").addEventListener("click", function() {\
  var result = { showDigital: document.getElementById("showDigital").checked };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var showDigital = localStorage.getItem('showDigital') === 'true';

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({ showDigital: showDigital }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      showDigital = config.showDigital || false;
      localStorage.setItem('showDigital', showDigital.toString());

      Pebble.sendAppMessage({
        'ShowDigital': showDigital ? 1 : 0
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'ShowDigital': showDigital ? 1 : 0
  });
});
