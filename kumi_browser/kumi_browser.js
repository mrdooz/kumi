var wsUri = "ws://127.0.0.1:9002";
var output;
var smoothie;
var websocket;
var fps_series;
var fps_interval_id;
var demo_info;

var timeline_scale = 10;
var timeline_ofs = 0;
var horiz_margin = 10;
var start_move_pos = 0;
var org_timeline_ofs = 0;

var is_playing = false;
var cur_time = 0;

var requestAnimationFrame = window.requestAnimationFrame || window.mozRequestAnimationFrame || window.webkitRequestAnimationFrame || window.msRequestAnimationFrame;

var button_state = [];

function ptInRect(x, y, top, left, bottom, right) {
    return x >= left && x < right && y >= top && y < bottom;
}

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
        $('#cur-fps').text(fps.toFixed(2) + ' fps');
    } else if (res['demo']) {
        demo_info = res['demo'];
    }
}

function onError(e)
{
    console.log(e);
}

function onBtnPrev() {
    cur_time = 0;
    websocket.send(getTimeInfo());
}

var playing_id;

function onBtnPlay(playing) {
    is_playing = playing;
    if (playing) {
        var start_time = new Date().getTime();
        playing_id = setInterval(function() {
            var now = new Date().getTime();
            cur_time += now - start_time;
            start_time = now;
        }, 100);
    } else {
        clearInterval(playing_id);
    }
    websocket.send(getTimeInfo());
}

function onBtnNext() {
}

function getTimeInfo() {
    var o = {};
    o.state = is_playing ? true : false;
    o.cur_time = cur_time;
    return JSON.stringify(o);
}

function timelineInit() {
    var canvas = $('#timeline-canvas');
    var pos = canvas.position();
    
    var rel_pos = function(x) {
        return x - pos.left - horiz_margin;
    };

    var ctx = canvas.get(0).getContext('2d');
    ctx.font = "8px Arial";
    ctx.fillStyle = 'black';
    ctx.textBaseline = 'top';
    
    canvas.mouseleave(function(event) {
        for (var i = 0; i < button_state.length; ++i)
            button_state[i] = 0;
    });
    
    canvas.mousedown(function(event) {
        button_state[event.which] = 1;
        start_move_pos = rel_pos(event.pageX);
        org_timeline_pos = timeline_ofs;
    });

    canvas.mouseup(function(event) {
        button_state[event.which] = 0;
        
        if (event.which == 1) {
            var x = event.pageX - pos.left;
            var y = event.pageY - pos.top;
            if (ptInRect(x, y, 15, horiz_margin, 15+25, canvas.width() - horiz_margin)) {
                cur_time = pixelToTime(x - horiz_margin);
                websocket.send(getTimeInfo());
            }
        }
    });
    
    canvas.mousemove(function(event) {
        if (button_state[2]) {
            var delta = (rel_pos(event.pageX) - start_move_pos);
            timeline_ofs = Math.max(0, org_timeline_pos + delta);
        } else if (button_state[1]) {
            var x = event.pageX - pos.left;
            var y = event.pageY - pos.top;
            if (ptInRect(x, y, 15, horiz_margin, 15+25, canvas.width() - horiz_margin)) {
                cur_time = pixelToTime(x - horiz_margin);
                websocket.send(getTimeInfo());
            }
        }
    });
    
    
    requestAnimationFrame(drawTimeline);
}

function pixelToTime(px) {
    return timeline_ofs + px * timeline_scale;
}

function timeToPixel(t) {
    return (t - timeline_ofs) / timeline_scale;
}

function drawTimeline() {
    var canvas = $('#timeline-canvas').get(0);
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.strokeRect(0, 0, canvas.width, canvas.height);
    $('#cur-time').text((cur_time/1000).toFixed(2));

    ctx.save();
    var d = demo_info;

    ctx.font = "8px Arial";
    ctx.textBaseline = "top";
    ctx.textAlign = 'center';
   
   
    ctx.fillStyle = '#aaa';
    ctx.beginPath();
    ctx.moveTo(horiz_margin + 0.5, 15.5);
    ctx.lineTo(canvas.width - horiz_margin + 0.5, 15.5);
    ctx.lineTo(canvas.width - horiz_margin + 0.5, 15.5 + 25);
    ctx.lineTo(horiz_margin + 0.5, 15.5 + 25);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();
    
    ctx.fillStyle = 'black';
    for (var i = 0; i < canvas.width; i += 10) {
        var cur = pixelToTime(i);
        var extra = i % 50 == 0;
        if (extra) {
            ctx.fillText(cur, horiz_margin + i, 5);
        }
        ctx.beginPath();
        ctx.moveTo(horiz_margin + i + 0.5, 13);
        ctx.lineTo(horiz_margin + i + 0.5, 15 + (extra ? 3 : 2));
        ctx.closePath();
        ctx.stroke();
    }

    
    ctx.strokeStyle = '#f33';
    ctx.beginPath();
    ctx.moveTo(horiz_margin + timeToPixel(cur_time), 5);
    ctx.lineTo(horiz_margin + timeToPixel(cur_time), canvas.height);
    ctx.closePath();
    ctx.stroke();
    
    
    ctx.restore();
    
    requestAnimationFrame(drawTimeline);
}

var kbInit = function() {
    output = document.getElementById("output");
    smoothie = new SmoothieChart();
    fps_series = new TimeSeries();
    smoothie.streamTo(document.getElementById("fps-canvas"), 0, false);
    smoothie.addTimeSeries(fps_series);
    openWebSocket();
    
    timelineInit();
   
};
