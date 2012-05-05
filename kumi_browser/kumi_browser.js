var wsUri = "ws://127.0.0.1:9002";
var output;
var smoothie;
var websocket;
var fps_series;
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
    websocket.send('DEMO.INFO');
    fps_interval_id = setInterval(function() { websocket.send('SYSTEM.FPS'); }, 100);
    smoothie.start();
}

function onClose(e)
{
    clearInterval(fps_interval_id);
    $('#connection-status').attr('src', 'assets/gfx/disconnected.png');
    smoothie.stop();
}

function onMessage(e)
{
    var res = JSON.parse(e.data);

    if (res['system.fps']) {
        var fps = res['system.fps'];
        fps_series.append(new Date().getTime(), fps);
    } else if (res['demo']) {
        var demo = res['demo'];
    }
}

function onError(e)
{
    console.log(e);
}

var kbInit = function() {
    output = document.getElementById("output");
    smoothie = new SmoothieChart();
    fps_series = new TimeSeries();
    smoothie.streamTo(document.getElementById("fps-canvas"), 0, false);
    smoothie.addTimeSeries(fps_series);
    openWebSocket();
};
