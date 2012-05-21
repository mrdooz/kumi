var KUMI_LIB = (function($) {
    "use strict";
    var kumi_lib = {};

    var id = function(x) { return x; };

    // return the min of an array, using the fn function to extract the element to compare
    kumi_lib.min = function(a, fn) {
        if (a.length === 0)
            return undefined;
        fn = fn || id;

        var v = fn(a[0]);
        a.forEach(function(e) { v = Math.min(v, fn(e)); });
        return v;
    };

    kumi_lib.max = function(a, fn) {
        if (a.length === 0)
            return undefined;
        fn = fn || id;
        var v = fn(a[0]);
        a.forEach(function(e) { v = Math.max(v, fn(e)); });
        return v;
    };

    return kumi_lib;
}(window.jQuery));

// stuff stolen from JavaScript the Good Parts
if (typeof Object.create !== 'function') {
    Object.create = function (o) {
        var F = function () {};
        F.prototype = o;
        return new F();
    };
}
/*
Object.method('superior', function (name) {
    var that = this,
    method = that[name];
    return function ( ) {
        return method.apply(that, arguments);
    };
});*/
/*
var constructor = function (spec, my) {

    var that, other private instance variables;
    my = my || {};
    
    Add shared variables and functions to my
    that = a new object;
    
    Add privileged methods to that
    return that;
};

var mammal = function (spec) {
    var that = {};
    that.get_name = function ( ) {
        return spec.name;
    };
    that.says = function ( ) {
        return spec.saying || '';
    };
    return that;
};

var myMammal = mammal({name: 'Herb'});

var cat = function (spec) {
    spec.saying = spec.saying || 'meow';
    var that = mammal(spec);
    that.purr = function (n) {
        var i, s = '';
        for (i = 0; i < n; i += 1) {
            if (s) {
                s += '-';
            }
            s += 'r';
        }
        return s;
    };
    that.get_name = function ( ) {
        return that.says( ) + ' ' + spec.name + ' ' + that.says( ); };
    return that;
};
var myCat = cat({name: 'Henrietta'});
*/