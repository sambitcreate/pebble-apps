var CONFIG_HTML = '\
<!DOCTYPE html>\
<html>\
<head>\
<meta name="viewport" content="width=device-width, initial-scale=1">\
<style>\
body { font-family: -apple-system, Helvetica, Arial, sans-serif; background: #1a1a1a; color: #fff; margin: 0; padding: 20px; }\
h1 { font-size: 20px; margin-bottom: 20px; color: #00e5ff; }\
.option { padding: 16px; background: #2a2a2a; border-radius: 8px; margin-bottom: 12px; }\
.option label { display: block; font-size: 16px; margin-bottom: 8px; }\
.option .hint { font-size: 12px; color: #888; margin-top: 4px; }\
.option input[type="number"] { width: 100%; padding: 10px; font-size: 16px; border: 1px solid #444; border-radius: 6px; background: #1a1a1a; color: #fff; box-sizing: border-box; -webkit-appearance: none; }\
.option input[type="number"]:focus { border-color: #00e5ff; outline: none; }\
button { width: 100%; padding: 14px; margin-top: 20px; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; cursor: pointer; background: #00e5ff; color: #1a1a1a; }\
</style>\
</head>\
<body>\
<h1>Tally Counter Settings</h1>\
<div class="option">\
  <label>Goal</label>\
  <input type="number" id="goal" min="0" max="99999" placeholder="0">\
  <div class="hint">Set to 0 for no goal</div>\
</div>\
<div class="option">\
  <label>Select Button Step</label>\
  <input type="number" id="stepSelect" min="1" max="999" placeholder="1">\
  <div class="hint">Increment per select press (default: 1)</div>\
</div>\
<div class="option">\
  <label>Up Button Step</label>\
  <input type="number" id="stepUp" min="1" max="999" placeholder="5">\
  <div class="hint">Increment per up press (default: 5)</div>\
</div>\
<button id="save">Save</button>\
<script>\
var params = window.location.hash.substring(1);\
try {\
  var cfg = JSON.parse(decodeURIComponent(params));\
  if (cfg.goal !== undefined) document.getElementById("goal").value = cfg.goal;\
  if (cfg.stepSelect !== undefined) document.getElementById("stepSelect").value = cfg.stepSelect;\
  if (cfg.stepUp !== undefined) document.getElementById("stepUp").value = cfg.stepUp;\
} catch(e) {}\
document.getElementById("save").addEventListener("click", function() {\
  var result = {\
    goal: parseInt(document.getElementById("goal").value, 10) || 0,\
    stepSelect: parseInt(document.getElementById("stepSelect").value, 10) || 1,\
    stepUp: parseInt(document.getElementById("stepUp").value, 10) || 5\
  };\
  if (result.stepSelect < 1) result.stepSelect = 1;\
  if (result.stepUp < 1) result.stepUp = 1;\
  window.location.href = "pebblejs://close#" + encodeURIComponent(JSON.stringify(result));\
});\
</script>\
</body>\
</html>';

var goal = parseInt(localStorage.getItem('goal'), 10) || 0;
var stepSelect = parseInt(localStorage.getItem('stepSelect'), 10) || 1;
var stepUp = parseInt(localStorage.getItem('stepUp'), 10) || 5;

Pebble.addEventListener('showConfiguration', function() {
  var config = encodeURIComponent(JSON.stringify({
    goal: goal,
    stepSelect: stepSelect,
    stepUp: stepUp
  }));
  Pebble.openURL('data:text/html,' + encodeURIComponent(CONFIG_HTML) + '#' + config);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      goal = config.goal || 0;
      stepSelect = config.stepSelect || 1;
      stepUp = config.stepUp || 5;

      localStorage.setItem('goal', goal.toString());
      localStorage.setItem('stepSelect', stepSelect.toString());
      localStorage.setItem('stepUp', stepUp.toString());

      Pebble.sendAppMessage({
        'Goal': goal,
        'StepSelect': stepSelect,
        'StepUp': stepUp
      });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({
    'Goal': goal,
    'StepSelect': stepSelect,
    'StepUp': stepUp
  });
});
