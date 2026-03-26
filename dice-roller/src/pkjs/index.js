var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a2e; color: #e0e0e0; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #b388ff; }\
.option { display: flex; align-items: center; justify-content: space-between; padding: 16px; background: #2a2a3e; border-radius: 8px; margin-bottom: 12px; }\
.option label { font-size: 16px; }\
select { font-size: 16px; padding: 8px 12px; border-radius: 6px; border: 1px solid #555; background: #3a3a4e; color: #e0e0e0; }\
.toggle { position: relative; width: 50px; height: 28px; }\
.toggle input { opacity: 0; width: 0; height: 0; }\
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #555; border-radius: 28px; transition: 0.3s; }\
.slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px; background: white; border-radius: 50%; transition: 0.3s; }\
input:checked + .slider { background: #b388ff; }\
input:checked + .slider:before { transform: translateX(22px); }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #b388ff; color: #1a1a2e; }\
</style>\
</head>\
<body>\
<h1>Dice Roller Settings</h1>\
<div class="option">\
  <label>Default Die</label>\
  <select id="dieType">\
    <option value="0">D4</option>\
    <option value="1">D6</option>\
    <option value="2">D8</option>\
    <option value="3">D10</option>\
    <option value="4">D12</option>\
    <option value="5">D20</option>\
  </select>\
</div>\
<div class="option">\
  <label>Show Stats by Default</label>\
  <label class="toggle"><input type="checkbox" id="showStats"><span class="slider"></span></label>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.dieType !== undefined) document.getElementById("dieType").value = cfg.dieType;\
  if (cfg.showStats) document.getElementById("showStats").checked = true;\
} catch(e) {}\
document.getElementById("save").addEventListener("click", function() {\
  var result = {\
    dieType: parseInt(document.getElementById("dieType").value, 10),\
    showStats: document.getElementById("showStats").checked\
  };\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var dieType = parseInt(localStorage.getItem('dieType') || '1', 10);
var showStats = localStorage.getItem('showStats') === 'true';

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({
    dieType: dieType,
    showStats: showStats
  }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      dieType = (config.dieType !== undefined) ? config.dieType : 1;
      showStats = config.showStats || false;
      localStorage.setItem('dieType', dieType.toString());
      localStorage.setItem('showStats', showStats.toString());

      Pebble.sendAppMessage({
        'DieType': dieType,
        'ShowStats': showStats ? 1 : 0
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'DieType': dieType,
    'ShowStats': showStats ? 1 : 0
  });
});
