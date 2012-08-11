var KUMI = (function($, KUMI_LIB) {
    "use strict";
    var kumi = {};
    var wsUri = "ws://127.0.0.1:9002";
    var output;
    var websocket;
    var performanceFps = false;
    var fpsSmoothie, memSmoothie;
    var fpsSeries, memSeries;
    var demoInfo = { effects :[] };

    var controlWidth = 250;
    var timelineHeaderHeight = 40;
    var ms_per_pixel = 10.0;
    var timeline_scales = [1, 5, 10, 20, 50, 100, 250, 500, 1000];
    var cur_timeline_scale = 1;
    var timeline_ofs = 0;

    var effectHeight = 30;
    var paramHeightDefault = 50;
    var paramHeightSingle = 15;
    var paramHeightContainer = 15;

    var isPlaying = false;
    var curTime = 0;

    var playing_id;

    var snapping_size = 50; // ms
    var snapping = true;
    var curSelected;

    var profileResolution = 20;
    var profileUpdating = true;
    var profileRects = [];

    var requestAnimationFrame = window.requestAnimationFrame || window.mozRequestAnimationFrame ||
        window.webkitRequestAnimationFrame || window.msRequestAnimationFrame;

    var button_state = [];

    function toRgb(o) {
        return "rgb(" + (255 * o.r).toFixed() + ", " + (255 * o.g).toFixed() + ", " +(255 * o.b).toFixed() + ")";
    }

    function ptInRect(x, y, top, left, bottom, right) {
        return x >= left && x < right && y >= top && y < bottom;
    }

    function findKeys(keys, time) {
        var t1, t2, i;
        for (i = 0; i < keys.length - 1; ++i) {
            t1 = keys[i].time;
            t2 = keys[i+1].time;
            if (time >= t1 && time < t2) {
                return { k1: keys[i].value, k2: keys[i+1].value, t : (time - t1) / (t2 - t1) };
            }
        }
        return { k1 : keys[i].value, k2 : keys[i].value, t : 0 };
    }

    function evalStep(keys) {
        return function(t) {
            var keyPair = findKeys(keys, t);
            return keyPair.k1;
        };
    }

    function evalLinearFloat(keys) {
        return function(t) {
            var keyPair = findKeys(keys, t);
            return keyPair.k1.x + keyPair.t * (keyPair.k2.x - keyPair.k1.x);
        };
    }

    function evalSplineFloat(keys) {
        return function(t) {

        };
    }

    function evalId(keys) {
        return function(t) {
            return keys[0].value;
        };
    }

    function evalStaticFloat(keys) {
        return function(t) {
            return keys[0].value.x;
        };
    }

    function floatToString(v) {
        return v.toFixed(2);
    }

    function boolToString(v) {
        return v;
    }

    function colorToString(v) {
        return "(" + (255* v.r).toFixed() +
            ", " + (255* v.g).toFixed() +
            ", " + (255* v.b).toFixed() +
            ", " + (255* v.a).toFixed() + ")";
    }

    var evalLookup = {
        "static" : { "bool" : evalId, "float" : evalStaticFloat, "color" : evalId },
        "step" : { "bool" : evalStep, "float" : evalStep },
        "linear" : { "bool" : evalId, "float" : evalLinearFloat },
        "spline" : { "bool" : evalId, "float" : evalLinearFloat }
    };

    var toStringLookup = {
        "bool" : boolToString, "float" : floatToString, "color" : colorToString
    };

    function openWebSocket()
    {
        var initial_open = true;
        var retry_count = 0;
        window.console.log(wsUri);
        websocket = new WebSocket(wsUri);

        websocket.onopen = function(e) {
            // icons from http://findicons.com/pack/2103/discovery
            $('#connection-status').attr('src', 'assets/gfx/connected.png');
            websocket.send('REQ:DEMO.INFO');
            websocket.send('REQ:PARAM.INFO');
            fpsSmoothie.start();
            memSmoothie.start();
            initial_open = false;
        };

        websocket.onclose = function(e) {
            $('#connection-status').attr('src', 'assets/gfx/disconnected.png');
            fpsSmoothie.stop();
            memSmoothie.stop();
            if (initial_open && retry_count < 10) {
                ++retry_count;
                setTimeout(openWebSocket, 2500);
            }
        };

        function drawCharts(data) {
            // get fps/ms
            var fpsOrMs = performanceFps ? data.fps : 1000 * data.ms;
            var suffix = performanceFps ? " fps" : " ms";
            fpsSeries.append(data.timestamp, fpsOrMs);
            $('#cur-fps').text(fpsOrMs.toFixed(2) + suffix);

            // get memory usage
            var mem = data.mem;
            memSeries.append(data.timestamp, mem);
        }

        function drawParamblock(paramBlock) {

            $.each(paramBlock, function(i, block) {
                $.each(block.params, function(j, param) {
                    if (param.minValue) {
                        $("<div/>", {
                            id: param.name
                        }).appendTo("#tweak-div");

                        $("#" + param.name).slider();
                    }
                });

            });


        }

        function drawProfile(prof) {
            var start = prof.startTime;
            var end = prof.endTime;
            var frameTime = end - start;

            var canvas = $('#profile-canvas').get(0);
            var ctx = canvas.getContext('2d');
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            var x = 0;
            var w = canvas.width / profileResolution;

            var pixelsPerSecond = canvas.width * (1000 / profileResolution);

            for (var i = 0; i < profileResolution; ++i) {
                ctx.fillStyle = i % 2 ? "#ccc" : "#aaa";
                ctx.fillRect(x, 0, w, canvas.height);
                x += w;
            }

            var levelHeight = 20;
            var cols = ["#c44", "#4c4", "#44c", "#cc4", "#4cc"];

            var profileCount = 0;
            profileRects = [];

            $.each(prof.threads, function(i, thread) {
                var y = 0;
                $.each(thread.events, function(i, event) {
                    var x = (event.start - start) * pixelsPerSecond;
                    var xEnd = (event.end - event.start) * pixelsPerSecond;

                    ctx.fillStyle = cols[i%cols.length];
                    var curY = y + ((thread.maxDepth + 1) * (levelHeight+10)) - (event.level + 1) * (10 + levelHeight);
                    ctx.fillRect(x, curY, xEnd, levelHeight);

                    ctx.fillStyle = "Black";
                    setFont(ctx, "10px Arial", "middle", "center");
                    ctx.fillText(event.name.slice(0,8), x + xEnd/2, curY + levelHeight/2);
                    profileRects.push({x: x, y: curY, w: xEnd, h: levelHeight, txt: event.name, duration: event.end - event.start});
                    profileCount++;

                });
                y += (thread.maxDepth + 1) * (levelHeight + 10);
            });

        }

        kumi.profileOnMouseMove = function(e) {
            var ofs = $('#profile-canvas').offset();

            $.each(profileRects, function(i, rect) {
                var x = e.clientX - ofs.left;
                var y = e.clientY - ofs.top;
                if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
                    $("#profileItem").text(rect.txt + " " + (rect.duration * 1000).toFixed(3) + "ms");
                    return false;
                }
            });
        };

        websocket.onmessage = function(e) {
            var msg = JSON.parse(e.data);

            if (msg['system.profile']) {
                if (profileUpdating)
                    drawProfile(msg['system.profile']);
            } else if (msg['system.frame']) {
                drawCharts(msg['system.frame']);
            } else if (msg.blocks) {
                drawParamblock(msg.blocks);
            } else if (msg.demo) {
                // append interpolation functions to the parameters
                demoInfo = msg.demo;
                demoInfo.effects.forEach( function(effect) {
                    function decorateParam(param) {
                        if (param.children) {
                            param.children.forEach(decorateParam);
                        }
                        if (param.keys) {
                            param.evalParam = evalLookup[param.anim][param.type](param.keys);
                            param.valueToString = toStringLookup[param.type];
                        }
                    }
                    effect.params.forEach(decorateParam);
                });
            }
        };

        websocket.onerror = function(e) {
            window.console.log("** WS ERROR **", e);
        };
    }

    kumi.onBtnPrev = function() {
        var canvas = $('#timeline-canvas');
        var delta = rawPixelToTime(canvas.width());
        curTime = Math.max(0, curTime - delta);
        timeline_ofs = Math.max(0, timeline_ofs - delta);
        sendTimeInfo();
    };

    kumi.onBtnPrevSkip = function() {
        curTime = 0;
        timeline_ofs = 0;
        sendTimeInfo();
    };


    kumi.onBtnPlay = function(playing) {
        isPlaying = playing;
        if (isPlaying) {
            var start_time = new Date().getTime();
            playing_id = setInterval(function() {
                var now = new Date().getTime();
                curTime += now - start_time;
                start_time = now;
            }, 100);
        } else {
            clearInterval(playing_id);
        }
        sendTimeInfo();
    };

    kumi.onBtnNext = function() {
        var canvas = $('#timeline-canvas');
        var delta = rawPixelToTime(canvas.width());
        curTime = curTime + delta;
        timeline_ofs += delta;
        sendTimeInfo();
    };

    kumi.onBtnNextSkip = function() {
        sendTimeInfo();
    };

    function sendJson(type, data) {
        websocket.send(JSON.stringify( { msg : {
            type : type,
            data : data }
        }));

    }

    function sendTimeInfo() {
        sendJson("time", { is_playing : !!isPlaying, cur_time : curTime} );
    }

    function sendDemoInfo() {
        sendJson("demo", demoInfo);
    }

    function setCanvasDispatch(fn) {
        var canvas = $('#timeline-canvas');
        var fns = ['mouseup', 'mousedown', 'mousemove', 'mouseleave', 'mousewheel'];
        if (setCanvasDispatch.prev_fn) {
            fns.forEach(function(f) {
                if (setCanvasDispatch.prev_fn[f]) canvas.unbind(f, setCanvasDispatch.prev_fn[f]);
            });
        }
        setCanvasDispatch.prev_fn = fn;

        fns.forEach(function(f) {
            if (fn[f]) canvas[f].apply(canvas, [fn[f]]);
        });

        if (fn.enterState)
            fn.enterState();
    }

    var insideTimelineHeader = function(x, y, canvas) {
        // x,y are relative the document
        var ofs = canvas.offset();
        x -= ofs.left;
        y -= ofs.top;
        return ptInRect(x, y, 0, controlWidth, timelineHeaderHeight, canvas.width());
    };

    var insideControl = function(x, y, canvas) {
        // x,y are relative the document
        var ofs = canvas.offset();
        x -= ofs.left;
        y -= ofs.top;
        return ptInRect(0, 0, 0, 0, canvas.height(), controlWidth);
    };

    var insideEffect = function(x, y, canvas) {
        var ofs = canvas.offset();
        x -= ofs.left;
        y -= (ofs.top + timelineHeaderHeight);
        var found_effect = null;
        var start;
        $.each(demoInfo.effects, function(i, e) {
            if (y >= i * effectHeight && y < (i+1) * effectHeight) {
                var sx = timeToPixel(e.start_time) + controlWidth;
                var ex = timeToPixel(e.end_time) + controlWidth;
                if (Math.abs(x-sx) < 5) {
                    found_effect = e;
                    start = true;
                } else if (Math.abs(x-ex) < 5) {
                    found_effect = e;
                    start = false;
                }
            }
        });
        return { effect : found_effect, start : start };
    };

    function snap(value) {
        if (!snapping)
            return value;
        return value -= (value % snapping_size);
    }

    // Normal editor state
    var stateNormal = function(spec) {
        var canvas = $('#timeline-canvas');
        var that = {};
        var button_state = [];

        that.getCanvas = function() {
            return canvas;
        };

        that.enterState = function() {
            canvas.get(0).style.cursor = 'default';
            $('#dbg-time').text('---');
        };

        that.mouseleave = function(event) {
            var i;
            for (i = 0; i < button_state.length; ++i)
                button_state[i] = 0;
        };

        that.mousewheel = function(event, delta) {
            cur_timeline_scale = Math.min(timeline_scales.length-1, Math.max(0, cur_timeline_scale + (delta < 0 ? -1 : 1)));
            ms_per_pixel = timeline_scales[cur_timeline_scale];
        };

        that.mousemove = function(event) {
            var x = event.pageX, y = event.pageY;
            var r = insideEffect(x, y, canvas);
            if (r.effect) {
                canvas.get(0).style.cursor = 'w-resize';
            } else {
                canvas.get(0).style.cursor = 'default';
            }
        };

        that.mousedown = function(event) {
            var ofs = canvas.offset();
            var effect, r;
            var x = event.pageX;
            var y = event.pageY;
            button_state[event.which] = 1;
            if (insideTimelineHeader(x, y, canvas)) {
                // dragging inside the timeline header
                setCanvasDispatch(stateDraggingTimeline({
                    pos : { x : event.pageX, y : event.pageY },
                    move_offset : event.shiftKey
                }));
            } else {
                r = insideEffect(x, y, canvas);
                if (r.effect && event.shiftKey) {
                    // dragging an effect
                    setCanvasDispatch(stateDraggingEffect({
                        effect : r.effect,
                        start : r.start,
                        pos : { x : event.pageX, y : event.pageY }
                    }));
                } else if (insideControl(x, y, canvas)) {
                    // selecting a new property
                    x -= ofs.left;
                    y -= ofs.top;
                    var selected;
                    var curY = timelineHeaderHeight;

                    var checkParams = function checkParams(idx, param) {
                        var curHeight = paramHeight(param);
                        if (y >= curY && y < curY + curHeight) {
                            selected = param;
                        } else {
                            curY += curHeight;
                            if (param.children) {
                                $.each(param.children, checkParams);
                            }
                        }
                        return selected ? false : true; // break iteration if we've found an item
                    };

                    $.each(demoInfo.effects, function(idx, effect) {
                        if (y >= curY && y < curY + effectHeight) {
                            selected = effect;
                        } else {
                            curY += effectHeight;
                            if (effect.params) {
                                $.each(effect.params, checkParams);
                            }
                        }
                        return selected ? false : true;
                    });
                    var cols = ["r", "g", "b", "a"];
                    var labels = ["labelX", "labelY", "labelZ", "labelW"];
                    $.each(["editX", "editY", "editZ", "editW"], function(idx, id) {
                        var cur = $("#" + id);
                        cur.editable("disable");
                        cur.text("-");
                        $("#" + labels[idx]).text("-");
                    });

                    if (curSelected) {
                        curSelected.selected = false;
                    }
                    if (selected) {
                        curSelected = selected;
                        curSelected.selected = true;
                        if (curSelected.type) {
                            if (curSelected.type === "float") {
                                var cur = $("#editX");
                                cur.text(curSelected.valueToString(curSelected.evalParam(0)));
                                cur.editable("enable");
                                $("#labelX").text("x");

                            } else if (curSelected.type === "color") {
                                $.each(["editX", "editY", "editZ", "editW"], function(idx, id) {
                                    var cur = $("#" + id);
                                    cur.editable("enable");
                                    cur.text((255 * curSelected.keys[0].value[cols[idx]]).toFixed());
                                    $("#" + labels[idx]).text(cols[idx]);
                                });
                            }
                        }
                    }
                }
            }
        };

        return that;
    };

    var stateDraggingTimeline = function(spec) {
        var that = stateNormal(spec);
        var start_pos, org_timeline_ofs;

        that.enterState = function() {
            that.start_pos = spec.pos;
            that.move_offset = spec.move_offset;
            that.org_timeline_ofs = timeline_ofs;
        };

        var updateTime = function(pageX) {
            var ofs = that.getCanvas().offset();
            var x = pageX - ofs.left;
            curTime = snap(Math.max(0, pixelToTime(x)));
            sendTimeInfo();
        };

        that.mousemove = function(event) {
            var delta, x;
            if (that.move_offset) {
                delta = rawPixelToTime(event.pageX - that.start_pos.x);
                timeline_ofs = snap(Math.max(0, that.org_timeline_ofs - delta));
            } else {
                updateTime(event.pageX);
            }
        };

        that.mouseup = function(event) {
            button_state[event.which] = 0;
            if (!that.move_offset) {
                updateTime(event.pageX);
            }
            setCanvasDispatch(stateNormal({}));
        };

        that.mouseleave = that.mouseup;

        return that;
    };

    var stateDraggingEffect = function(spec) {
        var start_pos, effect;
        var that = stateNormal(spec);

        var canvas = $('#timeline-canvas').get(0);
        var ctx = canvas.getContext('2d');

        that.enterState = function() {
            that.effect = spec.effect;
            that.start_pos = spec.pos;
            that.dragging_start = spec.start; // are we dragging the start of end time?
            that.org_start = that.effect.start_time;
            that.org_end = that.effect.end_time;
        };

        that.mouseup = function(event) {
            sendDemoInfo();
            setCanvasDispatch(stateNormal({}));
        };

        var updateTime = function(pageX) {
            var ofs = that.getCanvas().offset();
            var x = pageX - ofs.left;
            curTime = snap(Math.max(0, pixelToTime(x)));
            sendTimeInfo();
        };

        that.mousemove = function(event) {
            var delta = rawPixelToTime(event.pageX - that.start_pos.x);
            if (that.dragging_start) {
                that.effect.start_time = snap(Math.max(0, Math.min(that.org_start + delta, that.org_end - 50)));
                $('#dbg-time').text((that.effect.start_time/1000).toFixed(2)+'s');
            } else {
                that.effect.end_time = snap(Math.max(that.org_start + 50, that.org_end + delta));
                $('#dbg-time').text((that.effect.end_time/1000).toFixed(2)+'s');
            }
            sendDemoInfo();
        };

        that.mouseleave = that.mouseup;

        return that;
    };

    function timelineInit() {
        setCanvasDispatch(stateNormal({}));

        requestAnimationFrame(drawTimeline);
    }

    function rawPixelToTime(px) {
        return (px - controlWidth) * ms_per_pixel;
    }

    function pixelToTime(px) {
        return timeline_ofs + (px - controlWidth) * ms_per_pixel;
    }

    function timeToPixel(t) {
        return (t - timeline_ofs) / ms_per_pixel;
    }

    function rawTimeToPixel(t) {
        return t / ms_per_pixel;
    }

    function fillLinearGrad(ctx, x, y, w, h, startColor, endColor) {
        var gradient = ctx.createLinearGradient(x, y, x, y+h);
        gradient.addColorStop(0, startColor);
        gradient.addColorStop(1, endColor);

        ctx.fillStyle = gradient;
        ctx.fillRect(x, y, w, h);
    }

    function setFont(ctx, font, textBaseline, textAlign) {
        ctx.font = font;
        ctx.textBaseline = textBaseline;
        ctx.textAlign = textAlign;
    }

    function drawTimelineTicker(ctx, canvas) {

        setFont(ctx, "8px Arial", "top", "center");

        ctx.fillStyle = '#fff';
        ctx.strokeStyle = '#888';
        for (var i = controlWidth; i < canvas.width; i += 10) {
            var cur = pixelToTime(i);
            var extra = ((i - controlWidth) % 50) === 0;
            if (extra) {
                ctx.fillText(cur, i, 15);
            }
            ctx.beginPath();
            var x = i + 0.5;
            ctx.moveTo(x, 0);
            ctx.lineTo(x, 8 + (extra ? 5 : 2));
            ctx.closePath();
            ctx.stroke();
        }
    }

    function drawTimeMarker(ctx, canvas) {
        ctx.strokeStyle = '#f33';
        ctx.beginPath();
        var m = controlWidth + timeToPixel(curTime);
        ctx.moveTo(m, 0);
        ctx.lineTo(m, canvas.height);
        ctx.closePath();
        ctx.stroke();
    }

    function findActiveKeys(param, startTime, endTime) {
        var keys = [];
        if (param.keys.length === 1) {
            keys.push(param.keys[0]);
        } else {
            for (var i = 0; i < param.keys.length - 1; ++i) {
                if (param.keys[i].time <= startTime && param.keys[i+1].time >= startTime) {
                    // stradling the start
                    keys.push(param.keys[i]);
                    keys.push(param.keys[i+1]);
                } else if (param.keys[i].time >= startTime && param.keys[i+1].time < endTime) {
                    // fully inside
                    keys.push(param.keys[i+1]);
                } else if (param.keys[i].time < endTime && param.keys[i+1].time >= endTime) {
                    // stradling the end
                    keys.push(param.keys[i+1]);
                }
            }
        }
        return keys;
    }

    function paramHeight(param) {
        return param.keys ? (param.keys.length === 1 ? paramHeightSingle : paramHeightDefault) : paramHeightContainer;
    }

    function drawTimelineCanvas() {
        var canvas = $('#timeline-canvas').get(0);
        var ctx = canvas.getContext('2d');
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        ctx.save();

        fillLinearGrad(ctx, 0, 0, canvas.width, 40, '#444', '#ccc');
        ctx.strokeRect(0,0,canvas.width, 40.5);
        drawTimelineTicker(ctx, canvas);

        // draw the effects
        setFont(ctx, "10px Arial", "middle", "left");

        var y = timelineHeaderHeight;
        demoInfo.effects.forEach( function(effect) {

            // draw the effect gradient
            var startTime = Math.max(effect.start_time, pixelToTime(0));
            var endTime = Math.min(effect.end_time, pixelToTime(canvas.width));

            var x = controlWidth + Math.max(0, timeToPixel(effect.start_time));
            var w = Math.max(0,timeToPixel(effect.end_time - effect.start_time));
            fillLinearGrad(ctx, x, y, w, effectHeight, '#558', '#aae');
            y += effectHeight;

            var indent = 10;

            var drawParamValues = function(param) {
                // draw the paramter values over time
                var i, minValue, maxValue;
                var curHeight = paramHeight(param);

                var keys = findActiveKeys(param, startTime, endTime);
                if (keys.length === 0)
                    return;

                if (param.type === "float" && keys.length > 1) {
                    minValue = KUMI_LIB.min(keys, function(x) { return x.value.x; });
                    maxValue = KUMI_LIB.max(keys, function(x) { return x.value.x; });

                    var rescale = function(value) {
                        return y + curHeight - scale * (value - minValue);
                    };

                    // small hack for single value keys
                    if (Math.abs(minValue - maxValue) < 0.01)
                        minValue -= 10;

                    // draw the active keys
                    var scale = curHeight / (maxValue - minValue);
                    for (i = 0; i < keys.length; ++i) {
                        var x = controlWidth + timeToPixel(keys[i].time);
                        if (x >= 0 && x < canvas.width) {
                            ctx.beginPath();
                            ctx.arc(x, rescale(keys[i].value.x), 3, 0, 2 * Math.PI, 0);
                            ctx.fillStyle = "Black";
                            ctx.closePath();
                            ctx.fill();
                        }
                    }

                    // draw the interpolated values
                    var t = Math.max(keys[0].time, startTime);
                    ctx.moveTo(controlWidth + timeToPixel(t), rescale(param.evalParam(t)));
                    for (i = 1; i < keys.length; ++i) {
                        ctx.lineTo(controlWidth + timeToPixel(keys[i].time), rescale(param.keys[i].value.x));
                    }
                    ctx.lineTo(controlWidth + timeToPixel(endTime), rescale(param.evalParam(endTime)));
                    ctx.strokeStyle = "Black";
                    ctx.stroke();
                } else if (param.type === "color") {
                    ctx.fillStyle = toRgb(param.keys[0].value);
                    ctx.fillRect(controlWidth, y, canvas.width, curHeight);
                }

            };

            var drawParams = function drawParams(param) {
                if (param.keys) {
                    drawParamValues(param);
                }

                var curHeight = paramHeight(param);
                var textPos = y + curHeight / 2;
                if (param.selected) {
                    ctx.fillStyle = "#eea";
                    ctx.fillRect(0, y, controlWidth, curHeight);
                }
                ctx.strokeStyle = '#111';
                ctx.strokeRect(0, y + 0.5, canvas.width, curHeight);
                ctx.fillStyle = "Black";
                setFont(ctx, "10px Arial", "middle", "left");
                ctx.fillText(param.name, 5 + indent, textPos);

                // display the current paramter value
                if (param.evalParam) {
                    var value = param.evalParam(curTime);
                    setFont(ctx, "10px Arial", "middle", "right");
                    ctx.fillText(param.valueToString(value), controlWidth - 5, textPos);
                }

                y += curHeight;

                if (param.children) {
                    indent += 10;
                    param.children.forEach(drawParams);
                    indent -= 10;
                }
            };

            effect.params.forEach(drawParams);

        });

        drawTimeMarker(ctx, canvas);

        ctx.strokeStyle = '#111';
        ctx.strokeRect(0, 0, canvas.width, canvas.height);

        ctx.restore();
    }

    function drawTimeline() {

        $('#cur-time').text((curTime/1000).toFixed(2)+'s');
        var canvas = $('#timeline-canvas').get(0);
        // scroll timeline into view
        if (timeToPixel(curTime) > canvas.width) {
            timeline_ofs = pixelToTime(canvas.width);
        }

        drawTimelineCanvas();
        requestAnimationFrame(drawTimeline);
    }

    function initSmoothie() {
        fpsSmoothie = new window.SmoothieChart();
        fpsSeries = new window.TimeSeries();
        fpsSmoothie.streamTo(document.getElementById("fps-canvas"), 0, false);
        fpsSmoothie.addTimeSeries(fpsSeries);

        memSmoothie = new window.SmoothieChart();
        memSeries = new window.TimeSeries();
        memSmoothie.streamTo(document.getElementById("mem-canvas"), 0, false);
        memSmoothie.addTimeSeries(memSeries);

    }

    kumi.setProfileResolution = function(ms) {
        profileResolution = ms;
    };

    kumi.setProfileUpdating = function(status) {
        profileUpdating = status;
    };

    kumi.showFps = function(value) {
        performanceFps = value;
        fpsSeries.resetBounds();
    };

    kumi.onEdited = function(idx, value) {
        // TODO: send deltas
        if (curSelected.type === "float") {
            curSelected.keys[0].value.x = parseFloat(value);
            sendDemoInfo();
            return curSelected.keys[0].value.x.toFixed(2);
        } else if (curSelected.type === "color") {
            var cols = ["r", "g", "b", "a"];
            var v = parseFloat(value);
            if (v >= 0 && v <= 255) {
                curSelected.keys[0].value[cols[idx]] = v / 255;
                sendDemoInfo();
            }
            return (255 * curSelected.keys[0].value[cols[idx]]).toFixed();
        }
    };

    kumi.kbInit = function() {
/*
        var prof = $("#profile-container");
        stage = new Kinetic.Stage( {
            container: "profile-container",
            width: prof.get(0).width,
            height: prof.get(0).height
        });
        layer = new Kinetic.Layer();

        var rect = new Kinetic.Rect({
            x: 50,
            y: 50,
            width: 100,
            height: 100,
            fill: "#00d2ff",
            stroke: "black",
            strokeWidth: 2
        });
        layer.add(rect);

        stage.add(layer);
*/
        output = document.getElementById("output");
        initSmoothie();
        openWebSocket();

        timelineInit();
    };

    return kumi;
}(window.jQuery, window.KUMI_LIB));

