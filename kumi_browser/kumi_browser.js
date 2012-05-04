var wsUri = "ws://127.0.0.1:9002";
var output;
var smoothie = new SmoothieChart();
var websocket;
var fps_series = new TimeSeries();

function openWebSocket()
{
    console.log(wsUri);
    websocket = new WebSocket(wsUri);
    websocket.onopen = function(evt) { onOpen(evt) };
    websocket.onclose = function(evt) { onClose(evt) };
    websocket.onmessage = function(evt) { onMessage(evt) };
    websocket.onerror = function(evt) { onError(evt) };
}

function onOpen(evt)
{
    writeToScreen("CONNECTED");
    //doSend("SYSTEM.FPS");
}

function onClose(evt)
{
    writeToScreen("DISCONNECTED");
}

function onMessage(e)
{
    var res = JSON.parse(e.data);
    var fps = res['system.fps'];
    if (fps) {
        fps_series.append(new Date().getTime(), fps);
    }
}

function onError(evt)
{
    writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
}

function doSend(message)
{
    writeToScreen("SENT: " + message); 
    websocket.send(message);
}

function writeToScreen(message)
{
    var pre = document.createElement("p");
    pre.style.wordWrap = "break-word";
    pre.innerHTML = message;
    output.appendChild(pre);
}

var kbInit = function() {
    output = document.getElementById("output");
    smoothie.streamTo(document.getElementById("fps-canvas"));
    smoothie.addTimeSeries(fps_series);
    openWebSocket();
    setInterval(function() { websocket.send('SYSTEM.FPS'); }, 100);
};