var wsUri = "ws://127.0.0.1:9002";
var output;
var smoothie = new SmoothieChart();
var websocket;
var fps_series = new TimeSeries();
var fps_interval_id;

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
    // icons from http://findicons.com/pack/2103/discovery
    $('#connection-status').attr('src', 'assets/gfx/connected.png');
    //websocket.send('DEMOENGINE.INFO');
    fps_interval_id = setInterval(function() { websocket.send('SYSTEM.FPS'); }, 100);
}

function onClose(e)
{
    clearInterval(fps_interval_id);
    $('#connection-status').attr('src', 'assets/gfx/disconnected.png');
}

function onMessage(e)
{
    var res = JSON.parse(e.data);
    var fps = res['system.fps'];
    if (fps) {
        fps_series.append(new Date().getTime(), fps);
    }
}

function onError(e)
{
    console.error(e);
}

var kbInit = function() {
    output = document.getElementById("output");
    smoothie.streamTo(document.getElementById("fps-canvas"));
    smoothie.addTimeSeries(fps_series);
    openWebSocket();
};
