var KUMI = (function($) {
    "use strict";
    var kumi = {};
    var wsUri = "ws://127.0.0.1:9002";
    var output;
    var smoothie;
    var websocket;
    var fps_series;
    var fps_interval_id;
    var demo_info = { effects :[] };

    var timeline_header_height = 40;
    var timeline_margin = 10;
    var ms_per_pixel = 10.0;
    var timeline_ofs = 0;

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
        window.console.log(wsUri);
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
            } else if (res.demo) {
                demo_info = res.demo;
            }
        };

        websocket.onerror = function(e) {
            window.console.log(e);
        };
    }

    kumi.onBtnPrev = function() {
        var canvas = $('#timeline-canvas');
        websocket.send(getTimeInfo());
        var delta = rawPixelToTime(canvas.width() -  timeline_margin);
        cur_time = Math.max(0, cur_time - delta);
        timeline_ofs = Math.max(0, timeline_ofs - delta);
        websocket.send(getTimeInfo());
    };

    kumi.onBtnPrevSkip = function() {
        cur_time = 0;
        timeline_ofs = 0;
        websocket.send(getTimeInfo());
    };


    kumi.onBtnPlay = function(playing) {
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
    };

    kumi.onBtnNext = function() {
        var canvas = $('#timeline-canvas');
        websocket.send(getTimeInfo());
        var delta = rawPixelToTime(canvas.width() - timeline_margin);
        cur_time = cur_time + delta;
        timeline_ofs += delta;
        websocket.send(getTimeInfo());
    };

    kumi.onBtnNextSkip = function() {
        websocket.send(getTimeInfo());
    };

    function getTimeInfo() {
        var o = {};
        o.state = is_playing ? true : false;
        o.cur_time = cur_time;
        return JSON.stringify(o);
    }

    function setCanvasDispatch(fn) {
        var canvas = $('#timeline-canvas');

        if (fn.mouseup) canvas.mouseup(fn.mouseup);
        if (fn.mousedown) canvas.mousedown(fn.mousedown);
        if (fn.mousemove) canvas.mousemove(fn.mousemove);
        if (fn.mouseleave) canvas.mouseleave(fn.mouseleave);
/*
        ['mouseup', 'mousedown', 'mouseleave', 'mousemove'].forEach( function(f) {
           if (fn[f]) {
                var x = fn[f];
                canvas[f].apply(canvas, fn[f]);
           }
        });
*/
        if (fn.enterState)
            fn.enterState();
    }

    var insideTimeline = function(x, y, canvas) {
        // x,y are relative the document
        var ofs = canvas.offset();
        x -= ofs.left;
        y -= ofs.top;
        return ptInRect(x, y, 0, timeline_margin, timeline_header_height, canvas.width());
    };

    var insideEffect = function(x, y, canvas) {
        var ofs = canvas.offset();
        x -= ofs.left;
        y -= ofs.top;
        var found_effect = null;
        var start;
        demo_info.effects.forEach( function(e) {
            var sx = timeToPixel(e.start_time) + timeline_margin;
            var ex = timeToPixel(e.end_time) + timeline_margin;
            if (Math.abs(x-sx) < 5) {
                found_effect = e;
                start = true;
            } else if (Math.abs(x-ex) < 5) {
                found_effect = e;
                start = false;
            }
        });
        return { effect : found_effect, start : start };
    };

    // Normal editor state
    var stateNormal = function(spec) {
        var canvas = $('#timeline-canvas');
        var that = {};
        var button_state = [];
        var i;

        that.getCanvas = function() {
            return canvas;
        };

        that.mouseleave = function(event) {
            for (i = 0; i < button_state.length; ++i)
                button_state[i] = 0;
        };

        that.mousedown = function(event) {
            var ofs = canvas.offset();
            var effect;
            var x = event.pageX, y = event.pageY;
            var r;
            button_state[event.which] = 1;
            if (insideTimeline(x, y, canvas)) {
                setCanvasDispatch(stateDraggingTimeline({pos : { x : event.pageX, y : event.pageY }}));

            } else {
                r = insideEffect(x, y, canvas);
                if (r.effect)
                    setCanvasDispatch(stateDraggingEffect({
                        effect : r.effect,
                        start : r.start,
                        pos : { x : event.pageX, y : event.pageY }
                    }));
            }
/*
            start_move_timeline_pos = event.pageX;
            start_move_demo_pos = event.pageX;
            org_timeline_ofs = timeline_ofs;
*/
        };

        return that;
    };

    var stateDraggingTimeline = function(spec) {
        var that = stateNormal(spec);
        var start_pos, org_timeline_ofs;

        that.enterState = function() {
            this.start_pos = spec.pos;
            this.org_timeline_ofs = timeline_ofs;
        };

        that.mousemouse = function(event) {
            var ofs = that.getCanvas().offset();
            var x = event.pageX - ofs.left;
            cur_time = pixelToTime(x - timeline_margin);
            window.setTimeout(function() { websocket.send(getTimeInfo()); }, 1);
        };

        that.mouseup = function(event) {
            var ofs = that.getCanvas().offset();
            var x = event.pageX - ofs.left;
            var y = event.pageY - ofs.top;
            button_state[event.which] = 0;
            cur_time = Math.max(0, pixelToTime(x - timeline_margin));

            window.setTimeout(function() {
                websocket.send(getTimeInfo());
                setCanvasDispatch(stateNormal({}));
            }, 1);
        };

        that.mouseleave = that.mouseup;

        return that;
    };

    var stateDraggingEffect = function(spec) {
        var start_pos, effect;
        var that = stateNormal(spec);

        that.enterState = function() {
            this.start_pos = spec.pos;
            this.effect = spec.effect;
        };

        that.mouseup = function(event) {
            setCanvasDispatch(stateNormal({}));
        };

        that.mouseleave = that.mouseup;

        return that;
    };

    function timelineInit() {
        var canvas = $('#timeline-canvas');
        var ctrl = $('#timeline-control');

        var start_move_timeline_pos = 0;
        var start_move_demo_pos = 0;
        var org_timeline_ofs = 0;

        var ctx = canvas.get(0).getContext('2d');
        ctx.font = "8px Arial";
        ctx.fillStyle = 'black';
        ctx.textBaseline = 'top';

        setCanvasDispatch(stateNormal({}));
/*
        var setTime = function(x, y) {
            var ofs = canvas.offset();
            if (ptInRect(x, y, 0, timeline_margin, timeline_header_height, canvas.width())) {
                cur_time = pixelToTime(x - timeline_margin);
                websocket.send(getTimeInfo());
            }
        };

        canvas.mouseleave(function(event) {
            for (var i = 0; i < button_state.length; ++i) {
                button_state[i] = 0;
            }
        });

        canvas.mousedown(function(event) {
            var ofs = canvas.offset();
            button_state[event.which] = 1;
            start_move_timeline_pos = event.pageX;
            start_move_demo_pos = event.pageX;
            org_timeline_ofs = timeline_ofs;
            return false;
        });

        canvas.mouseup(function(event) {
            var ofs = canvas.offset();
            var x = event.pageX - ofs.left;
            var y = event.pageY - ofs.top;
            button_state[event.which] = 0;
            if (event.which === 1 && !event.shiftKey) {
                setTime(x, y);
            }
            return false;
        });

        var inside_timeline = function(x, y) {
            var ofs = canvas.offset();
            return ptInRect(x, y, 0, timeline_margin, timeline_header_height, canvas.width());
        };

        var inside_effect = function(x, y) {
            var found_effect = null;
            demo_info.effects.forEach( function(e) {
                var sx = timeToPixel(e.start_time) + timeline_margin;
                var ex = timeToPixel(e.end_time) + timeline_margin;
                if (Math.abs(x-sx) < 5) {
                    found_effect = e;
                } else if (Math.abs(x-ex) < 5) {
                    found_effect = e;
                }
            });
            return found_effect;
        };

        canvas.mousemove(function(event) {
            var delta;
            var ofs = canvas.offset();
            var x = event.pageX - ofs.left;
            var y = event.pageY - ofs.top;
            var e = inside_effect(x,y);
            if (button_state[1]) {
                if (inside_timeline(x,y)) {
                    if (event.shiftKey) {
                        delta = rawPixelToTime(event.pageX - start_move_timeline_pos);
                        timeline_ofs = Math.max(0, org_timeline_ofs + delta);
                    } else {
                        cur_time = pixelToTime(x - timeline_margin);
                        websocket.send(getTimeInfo());
                    }
                } else if (e) {
                    delta = rawPixelToTime(event.pageX - start_move_demo_pos);
                    start_move_demo_pos = event.pageX;
                    e.start_time += delta;
                }
            } else {
                // check if we're close to the edge of any effect
                if (e) {
                    canvas.get(0).style.cursor = 'w-resize';
                } else {
                    canvas.get(0).style.cursor = 'default';
                }
            }
            return false;
        });
  */
        requestAnimationFrame(drawTimeline);
    }

    function rawPixelToTime(px) {
        return px * ms_per_pixel;
    }

    function pixelToTime(px) {
        return timeline_ofs + px * ms_per_pixel;
    }

    function timeToPixel(t) {
        return (t - timeline_ofs) / ms_per_pixel;
    }

    function rawTimeToPixel(t) {
        return t / ms_per_pixel;
    }

    function drawTimelineCanvas() {
        var canvas = $('#timeline-canvas').get(0);
        var ctx = canvas.getContext('2d');
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        ctx.save();

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

        ctx.fillStyle = '#fff';
        ctx.strokeStyle = '#888';
        for (var i = 0; i < canvas.width; i += 10) {
            var cur = pixelToTime(i);
            var extra = (i % 50) === 0;
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

        var y = timeline_header_height;
        demo_info.effects.forEach( function(e) {
            var blue_grad = ctx.createLinearGradient(0, y, 0, y+30);
            blue_grad.addColorStop(0, '#558');
            blue_grad.addColorStop(1, '#aae');

            ctx.fillStyle = blue_grad;
            var o = timeline_margin + Math.max(0, timeToPixel(e.start_time));
            var w = Math.max(0,timeToPixel(e.end_time - e.start_time));
            ctx.fillRect(o, y, w, 30);
            y += 15;
        });

        ctx.strokeStyle = '#f33';
        ctx.beginPath();
        var m = timeline_margin + timeToPixel(cur_time);
        ctx.moveTo(m, 5);
        ctx.lineTo(m, canvas.height);
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

        var y = timeline_header_height;
        demo_info.effects.forEach( function(e) {
            var blue_grad = ctx.createLinearGradient(0, y, 0, y+30);
            blue_grad.addColorStop(0, '#558');
            blue_grad.addColorStop(1, '#aae');

            ctx.fillStyle = blue_grad;
            ctx.fillRect(0, y, canvas.width, 30);
            ctx.fillStyle = '#000';
            ctx.fillText(e.name, canvas.width/2, y + 15);
            y += 15;
        });

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

    kumi.kbInit = function() {
        output = document.getElementById("output");
        smoothie = new window.SmoothieChart();
        fps_series = new window.TimeSeries();
        smoothie.streamTo(document.getElementById("fps-canvas"), 0, false);
        smoothie.addTimeSeries(fps_series);
        openWebSocket();

        timelineInit();
    };

    return kumi;
}(window.jQuery));

