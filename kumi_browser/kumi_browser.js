var wsUri = "ws://127.0.0.1:9002";
var output;
var smoothie;
var websocket;
var fps_series;
var fps_interval_id;
var demo_info;

var timeline_header_height = 40;
var timeline_margin = 10;
var timeline_scale = 10.0;
var timeline_ofs = 0;
var start_move_pos = 0;
var org_timeline_ofs = 0;

var is_playing = false;
var cur_time = 0;

var playing_id;

var requestAnimationFrame = window.requestAnimationFrame || window.mozRequestAnimationFrame ||
    window.webkitRequestAnimationFrame || window.msRequestAnimationFrame;

var button_state = [];

function ptInRect(x, y, top, left, bottom, right) {
    return x >= left && x < right && y >= top && y < bottom;
}

function openWebSocket()
{
    var initial_open = true;
    var retry_count = 0;
    console.log(wsUri);
    websocket = new WebSocket(wsUri);
        
    websocket.onopen = function(e) { 
        // icons from http://findicons.com/pack/2103/discovery
        $('#connection-status').attr('src', 'assets/gfx/connected.png');
        websocket.send('DEMO.INFO');
        fps_interval_id = setInterval(function() { websocket.send('SYSTEM.FPS'); }, 100);
        smoothie.start();
        initial_open = false;
    };
    
    websocket.onclose = function(e) { 
        clearInterval(fps_interval_id);
        $('#connection-status').attr('src', 'assets/gfx/disconnected.png');
        smoothie.stop();
        if (initial_open && retry_count < 10) {
            ++retry_count;
            setTimeout(openWebSocket, 2500);
        }
    };
    
    websocket.onmessage = function(e) { 
        var res = JSON.parse(e.data);

        if (res['system.fps']) {
            var fps = res['system.fps'];
            fps_series.append(new Date().getTime(), fps);
            $('#cur-fps').text(fps.toFixed(2) + ' fps');
        } else if (res['demo']) {
            demo_info = res['demo'];
        }
    };
    
    websocket.onerror = function(e) { 
        console.log(e);
    };
}

function onBtnPrev() {
    var canvas = $('#timeline-canvas');
    websocket.send(getTimeInfo());
    var delta = rawPixelToTime(canvas.width() -  timeline_margin);
    cur_time = Math.max(0, cur_time - delta);
    timeline_ofs = Math.max(0, timeline_ofs - delta);
    websocket.send(getTimeInfo());
}

function onBtnPrevSkip() {
    cur_time = 0;
    timeline_ofs = 0;
    websocket.send(getTimeInfo());
}


function onBtnPlay(playing) {
    is_playing = playing;
    if (is_playing) {
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
    var canvas = $('#timeline-canvas');
    websocket.send(getTimeInfo());
    var delta = rawPixelToTime(canvas.width() - timeline_margin);
    cur_time = cur_time + delta;
    timeline_ofs += delta;
    websocket.send(getTimeInfo());
}

function onBtnNextSkip() {
    websocket.send(getTimeInfo());
}

function getTimeInfo() {
    var o = {};
    o.state = is_playing ? true : false;
    o.cur_time = cur_time;
    return JSON.stringify(o);
}

function timelineInit() {
    var canvas = $('#timeline-canvas');

    var ctrl = $('#timeline-control');
    ctrl.mouseup(function(e) {
/*
        // http://trentrichardson.com/Impromptu/
        var txt = 'Please enter your name:<br /><input type="text" id="alertName" name="alertName" value="name here" />';

        function mycallbackform(e,v,m,f){
            if(v != undefined)
                alert(v +' ' + f.alertName);
        }

        $.prompt(txt,{
            callback: mycallbackform,
            buttons: { Hey: 'Hello', Bye: 'Good Bye' }
        });
*/
    });

    var rel_pos = function(x) {
        var ofs = canvas.offset();
        return x - ofs.left - timeline_margin;
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
        var ofs = canvas.offset();
        button_state[event.which] = 0;
        
        if (event.which == 1) {
            var x = event.pageX - ofs.left;
            var y = event.pageY - ofs.top;
            if (ptInRect(x, y, 0, timeline_margin, timeline_header_height, canvas.width())) {
                cur_time = pixelToTime(x - timeline_margin);
                websocket.send(getTimeInfo());
            }
        }
    });
    
    canvas.mousemove(function(event) {
        var ofs = canvas.offset();
        if (button_state[2]) {
            var delta = (rel_pos(event.pageX) - start_move_pos);
            timeline_ofs = Math.max(0, org_timeline_pos + delta);
        } else if (button_state[1]) {
            var x = event.pageX - ofs.left;
            var y = event.pageY - ofs.top;
            if (ptInRect(x, y, 0, timeline_margin, timeline_header_height, canvas.width())) {
                cur_time = pixelToTime(x - timeline_margin);
                websocket.send(getTimeInfo());
            }
        }
    });
    
    
    requestAnimationFrame(drawTimeline);
}

function rawPixelToTime(px) {
    return px * timeline_scale;
}

function pixelToTime(px) {
    return timeline_ofs + px * timeline_scale;
}

function timeToPixel(t) {
    return (t - timeline_ofs) / timeline_scale;
}

function drawTimelineCanvas() {
    var canvas = $('#timeline-canvas').get(0);
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.save();
    var d = demo_info;

    ctx.font = "8px Arial";
    ctx.textBaseline = "top";
    ctx.textAlign = 'center';

    var h = 25 - 15;
    var grad = ctx.createLinearGradient(0, 0, 0, 40);
    grad.addColorStop(0, '#444');
    grad.addColorStop(1, '#ccc');
    ctx.fillStyle = grad;
    ctx.fillRect(0,0,canvas.width, 40);
    ctx.strokeRect(0,0,canvas.width, 40.5);

    ctx.fillStyle = '#fff'
    ctx.strokeStyle = '#888'
    for (var i = 0; i < canvas.width; i += 10) {
        var cur = pixelToTime(i);
        var extra = i % 50 == 0;
        if (extra) {
            ctx.fillText(cur, timeline_margin + i, 15);
        }
        ctx.beginPath();
        var x = timeline_margin + i + 0.5;
        ctx.moveTo(x, 0);
        ctx.lineTo(x, 8 + (extra ? 5 : 2));
        ctx.closePath();
        ctx.stroke();
    }

    if (d && d.effects) {
        var y = timeline_header_height;
        d.effects.forEach( function(e) {
            var blue_grad = ctx.createLinearGradient(0, y, 0, y+30);
            blue_grad.addColorStop(0, '#558');
            blue_grad.addColorStop(1, '#aae');

            ctx.fillStyle = blue_grad;
            var delta = e.end_time - e.start_time;
            ctx.fillRect(timeline_margin + timeToPixel(e.start_time), y, timeToPixel(delta), 30);
            y += 15;
        });
    }


    ctx.strokeStyle = '#f33';
    ctx.beginPath();
    var x = timeline_margin + timeToPixel(cur_time);
    ctx.moveTo(x, 5);
    ctx.lineTo(x, canvas.height);
    ctx.closePath();
    ctx.stroke();

    ctx.strokeStyle = '#111';
    ctx.strokeRect(0, 0, canvas.width, canvas.height);

    ctx.restore();
}

function drawTimelineControl() {
    var canvas = $('#timeline-control').get(0);
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.save();
    var d = demo_info;

    ctx.font = "15px Arial";
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'center';

    var h = 25 - 15;
    var grey_grad = ctx.createLinearGradient(0, 0, 0, timeline_header_height);
    grey_grad.addColorStop(0, '#444');
    grey_grad.addColorStop(1, '#ccc');
    ctx.fillStyle = grey_grad;
    ctx.fillRect(0,0,canvas.width, 40);
    ctx.strokeRect(0,0,canvas.width, 40.5);

    if (d && d.effects) {
        var y = timeline_header_height;
        d.effects.forEach( function(e) {
            var blue_grad = ctx.createLinearGradient(0, y, 0, y+30);
            blue_grad.addColorStop(0, '#558');
            blue_grad.addColorStop(1, '#aae');

            ctx.fillStyle = blue_grad;
            ctx.fillRect(0, y, canvas.width, 30);
            ctx.fillStyle = '#000';
            ctx.fillText(e.name, canvas.width/2, y + 15);
            y += 15;
        });
    }

    ctx.strokeStyle = '#111';
    ctx.strokeRect(0, 0, canvas.width, canvas.height);

    ctx.restore();

}

function drawTimeline() {

    $('#cur-time').text((cur_time/1000).toFixed(2)+'s');
    var canvas = $('#timeline-canvas').get(0);
    if (timeToPixel(cur_time) > canvas.width) {
        timeline_ofs = pixelToTime(canvas.width);
    }

    drawTimelineCanvas();
    drawTimelineControl();
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
