// config
var _config = {};
// status
var lastData = null;
// scan data
var allIp;
var deviceIps;
var scanTimeout;
// appmsg queue
var appmsgIsSending;
var appmsgQueue = [];
var appmsgSendCount;

function loadDeviceList() {
  deviceIps = [];

  var req = new XMLHttpRequest();
  req.open('GET', "http://heatermeter.com/devices/?fmt=json", true);
  req.timeout = 20000;

  req.ontimeout = function (e) {
    sendErrMsg("Use Pebble app to configure");
  };

  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      var o = JSON.parse(req.responseText);
      if (o.devices && o.devices.length > 0)
        scanDeviceList(o.devices);
      else
        sendErrMsg("Use Pebble app to configure");
    }
  }
  req.send(null);
}

function scanDeviceList(devices) {
  allIp = [];
  if (devices) {
    for (var i=0; i<devices.length; ++i) {
      var v = devices[i];
      for (var n=0; n<v.interfaces.length; ++n) {
        var iface = v.interfaces[n];
        if (iface.packets == 0)
          continue;
        if (allIp.indexOf(iface.addr) != -1)
          continue;
        allIp.push(iface.addr);
      }
    }
  }
  
  scanNextDevice();
}

function scanNextDevice() {
  if (allIp.length == 0) {
    deviceScanComplete();
    return;
  }
    
  var ip = allIp.shift();
  var req = new XMLHttpRequest();
  req.open('GET', 'http://' + ip + '/luci/lm/conf', true);
  req.onload = function(e) {
    //console.log(ip + " readystate=" + req.readyState + " status=" + req.status);
    if (req.readyState == 4 && req.status == 200) {
      clearTimeout(scanTimeout);
      var o = JSON.parse(req.responseText);
      if (o.ucid)
        deviceIps.push(ip);
      scanNextDevice();
    }
  }
  req.send(null);

  scanTimeout = setTimeout(scanNextDevice, 3000);
}

function deviceScanComplete() {
  if (deviceIps.length > 0) {
    _config.host = deviceIps[0];
    localStorage.setItem("host", _config.host);
    sendErrMsg("Connecting to " + _config.host + "...");
    refreshData();
  }
  else
    sendErrMsg("Use Pebble app to configure");
}

function sendAppMessage(msg) {
  appmsgQueue.push(msg);
  sendNextAppMsg();
}

function sendNextAppMsg() {
  if (appmsgIsSending)
    return;
  if (appmsgQueue.length == 0)
    return;

  appmsgIsSending = true;
  appmsgSendCount = 0;
  Pebble.sendAppMessage(appmsgQueue[0], msgAck, msgNak);
}

function msgAck(e) {
  //console.log('Successfully delivered message with transactionId=' + e.data.transactionId);
  appmsgIsSending = false;
  appmsgQueue.shift();
  sendNextAppMsg();
}

function msgNak(e) {
  //console.log('Unable to deliver message with transactionId=' + e.data.transactionId + ' Error is: ' + e.error);
  appmsgIsSending = false;
  ++appmsgSendCount;
  if (appmsgSendCount > 2)
    appmsgQueue.shift();
  sendNextAppMsg();
}

function sendErrMsg(msg) {
  console.log(msg);
  sendAppMessage({"KEY_ERRMSG": msg});
}

function loginToHm() {
  var dest = "http://" + _config.host + "/luci/admin/lm";
  var req = new XMLHttpRequest();
  var params = "username=" + _config.user + "&password=x" + _config.password;
  req.open('POST', dest, true);
  req.withCredentials = true;
  req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  //req.setRequestHeader("Content-length", params.length);
  
  req.onload = function(e) {
    //console.log(req.readyState + " status=" + req.status);
    if (req.readyState == 4 && req.status == 200) {
      console.log(req.getAllResponseHeaders());
    }
  };
  req.send(params);
}

function refreshData() {
  if (_config.host === "")
    return;
  
  var dest = "http://" + _config.host + "/luci/lm/hmstatus";
  console.log("Entering refreshdata loading " + dest);
  var req = new XMLHttpRequest();
  req.open('GET', dest, true);
  req.timeout = 5000;
  
  req.onerror = function(o, e) {
    sendErrMsg("Connect error");
  };
  req.ontimeout = function () {
    sendErrMsg("Request timed out");
  };
  //var refreshTimeout = setTimeout(function () {
  //  sendErrMsg("Request timed out");
  //}, 5000);
  
  req.onload = function(e) {
    console.log(req.readyState + " status=" + req.status);
    if (req.readyState == 4 && req.status == 200) {
      //clearTimeout(refreshTimeout);
      
      //console.log(req.responseText);
      var o = JSON.parse(req.responseText);
      var changes = {};
      var hasChanges = false;
      for (var i=0; i<4; ++i) {
        var name = o.temps[i].n;
        var temp = o.temps[i].c;
        if (lastData === null || lastData.temps[i].n != name) {
          changes["KEY_NAME_" + i.toString()] = name;
          hasChanges = true;
        }
        if (lastData === null || lastData.temps[i].c != temp) {
          if (temp === null)
            temp = "";
          else
            temp = temp.toFixed(1);
          changes["KEY_TEMP_" + i.toString()] = temp;
          hasChanges = true;
        }
      }
      lastData = o;
       
      // changes["KEY_NAME_0"] = "Pit Flames";
      // changes["KEY_TEMP_0"] = "225.1\u00b0";
      // changes["KEY_NAME_1"] = "Butt 7.5 lb";
      // changes["KEY_TEMP_1"] = "167.7\u00b0";
      // changes["KEY_NAME_2"] = "Butt 7.0 lb";
      // changes["KEY_TEMP_2"] = "171.3\u00b0";
      // changes["KEY_NAME_3"] = "Ambient";
      // changes["KEY_TEMP_3"] = "78.1\u00b0";
      
      //console.log(JSON.stringify(changes));
      if (hasChanges)
        sendAppMessage(changes);
    }
    else if (req.readyState == 4) {
      //clearTimeout(refreshTimeout);
      sendErrMsg("Error loading code: " + req.status);
    }
  }; // onload
  req.send(null); 
}

function saveConfig(config) {
  localStorage.setItem("version", "1");
  localStorage.setItem("host", config.host.trim());
  localStorage.setItem("user", config.user.trim());
  localStorage.setItem("password", config.password.trim());
  localStorage.setItem("refreshinterval", config.refreshinterval.trim());
  loadConfig();
}

function loadConfig() {
  _config.version = parseInt(localStorage.getItem("version") || "1");
  _config.host = localStorage.getItem("host") || "";
  _config.user = localStorage.getItem("user") || "root";
  _config.password = localStorage.getItem("password") || "";
  _config.refreshinterval = parseInt(localStorage.getItem("refreshinterval") || "60");
  sendConfigToPebble();
}

function sendConfigToPebble() {
  sendAppMessage({
    "KEY_REFRESH_INTERVAL": _config.refreshinterval
  });
}

Pebble.addEventListener("ready", function(e) {
  console.log("connect!");
  loadConfig();
  if (_config.host === "") {
    sendErrMsg("Discovering HeaterMeter...");
    loadDeviceList();
  }
  else
    refreshData();
});

Pebble.addEventListener("appmessage", function(e) {
  console.log("Received message: " + JSON.stringify(e.payload));
  refreshData();
  //loginToHm();
});

Pebble.addEventListener("showConfiguration", function(e) {
  Pebble.openURL("http://heatermeter.com/devices/pconf.php?" +
    encodeURIComponent(JSON.stringify(_config)));
});

Pebble.addEventListener("webviewclosed",
  function(e) {
    if (e.response) {
      console.log(e.response);
      var config = JSON.parse(decodeURIComponent(e.response));
      saveConfig(config);
      refreshData();
    }
  }
);
