"use strict";

function throw_error(msg) {
    throw Error(msg);
}

function assert(actual, expected, message) {
    function get_full_type(o) {
        var type = typeof(o);
        if (type === 'object') {
            if (o === null)
                return 'null';
            if (o.constructor && o.constructor.name)
                return o.constructor.name;
        }
        return type;
    }

    if (arguments.length == 1)
        expected = true;

    if (typeof actual === typeof expected) {
        if (actual === expected) {
            if (actual !== 0 || (1 / actual) === (1 / expected))
                return;
        }
        if (typeof actual === 'number') {
            if (isNaN(actual) && isNaN(expected))
                return true;
        }
        if (typeof actual === 'object') {
            if (actual !== null && expected !== null
            &&  actual.constructor === expected.constructor
            &&  actual.toString() === expected.toString())
                return;
        }
    }
    // Should output the source file and line number and extract
    //   the expression from the assert call
    throw_error("assertion failed: got " +
                get_full_type(actual) + ":|" + actual + "|, expected " +
                get_full_type(expected) + ":|" + expected + "|" +
                (message ? " (" + message + ")" : ""));
}

function assert_throws(expected_error, func)
{
    var err = false;
    try {
        func();
    } catch(e) {
        err = true;
        if (!(e instanceof expected_error)) {
            // Should output the source file and line number and extract
            //   the expression from the assert_throws() call
            throw_error("unexpected exception type");
            return;
        }
    }
    if (!err) {
        // Should output the source file and line number and extract
        //   the expression from the assert_throws() call
        throw_error("expected exception");
    }
}

function my_func(a, b)
{
    return a + b;
}

function test_function()
{
    function f(a, b) {
        var i, tab = [];
        tab.push(this);
        for(i = 0; i < arguments.length; i++)
            tab.push(arguments[i]);
        return tab;
    }
    function constructor1(a) {
        this.x = a;
    }
    
    var r, g;
    
    r = my_func.call(null, 1, 2);
    assert(r, 3, "call");

    r = my_func.apply(null, ["abc", 2]);
    assert(r, "abc2", "apply");

    r = new Function("a", "b", "return a + b;");
    assert(r(2,3), 5, "function");

    g = f.bind(1, 2);
//    assert(g.length, 1);
//    assert(g.name, "bound f");
    assert(g(3).toString(), "1,2,3");

    if (0) {
    g = constructor1.bind(null, 1);
    r = new g();
    assert(r.x, 1);
    }
}

function test()
{
    var r, a, b, c, err;

    r = Error("hello");
    assert(r.message, "hello", "Error");

    a = new Object();
    a.x = 1;
    assert(a.x, 1, "Object");

    assert(Object.prototype.constructor, Object, "constructor");
    assert(Object.getPrototypeOf(a), Object.prototype, "getPrototypeOf");
    Object.defineProperty(a, "y", { value: 3, writable: true, configurable: true, enumerable: true });
    assert(a.y, 3, "defineProperty");

    Object.defineProperty(a, "z", { get: function () { return 4; }, set: function(val) { this.z_val = val; }, configurable: true, enumerable: true });
    assert(a.z, 4, "get");
    a.z = 5;
    assert(a.z_val, 5, "set");
/*
    a = { get z() { return 4; }, set z(val) { this.z_val = val; } };
    assert(a.z, 4, "get");
    a.z = 5;
    assert(a.z_val, 5, "set");
*/
    a = {};
    b = Object.create(a);
    assert(Object.getPrototypeOf(b), a, "create");
    c = {u:2};
    Object.setPrototypeOf(a, c);
    assert(Object.getPrototypeOf(a), c, "setPrototypeOf");
    
    a={};
    assert(a.toString(), "[object Object]", "toString");
    assert(Object.prototype.toString.call(1), "[object Number]", "toString");
/*
    a={x:1};
    assert(Object.isExtensible(a), true, "extensible");
    Object.preventExtensions(a);

    err = false;
    try {
        a.y = 2;
    } catch(e) {
        err = true;
    }
    assert(Object.isExtensible(a), false, "extensible");
    assert(typeof a.y, "undefined", "extensible");
    assert(err);
*/

    a = {x: 1};
    assert(a.hasOwnProperty("x"), true);
    assert(a.hasOwnProperty("y"), false);
    a = [1, 2];
    assert(a.hasOwnProperty(1), true);
    assert(a.hasOwnProperty(2), false);
}

function test_enum()
{
    var a, tab;
    a = {x:1, y:1, z:3};
    tab = Object.keys(a);
    assert(tab.toString(), "x,y,z", "keys");
}

function test_array()
{
    var a, err, i, log;

    a = [1, 2, 3];
    assert(a.length, 3, "array");
    assert(a[2], 3, "array1");

    a = new Array(10);
    assert(a.length, 10, "array2");

    a = new Array(1, 2);
    assert(a[0] === 1 && a[1] === 2);

    a = [1, 2, 3];
    a.length = 2;
    assert(a[0] === 1 && a[1] === 2 && a.length === 2);

    a = [];
    a[0] = 10;
    a[1] = 3;
    assert(a.length, 2);
    
/*
    a = [];
    a[1] = 10;
    a[4] = 3;
    assert(a.length, 5);
*/
    
    a = [1,2];
    a.length = 5;
    a[4] = 1;
    a.length = 4;
    assert(a[4] !== 1);

    a = [1,2,3];
    assert(a.join("-"), "1-2-3");
    
    a = [1,2];
    assert(a.push(3, 4), 4);
    assert(a.toString(), "1,2,3,4");

    a = [1,2,3];
    assert(a.pop(), 3);
    assert(a.toString(), "1,2");
    
    /*
    a=[1,2,3,4,5];
    Object.defineProperty(a, "3", { configurable: false });
    err = false;
    try {
        a.length = 2;
    } catch(e) {
        err = true;
    }
    assert(err && a.toString() === "1,2,3,4");
    */
    assert(Array.isArray([]), true);
    assert(Array.isArray({}), false);

    a = [1, 2, 3];
    assert(a.reverse().toString(), "3,2,1");

    a = [1, 2, 3];
    a = a.concat(4, [5, 6], 7);
    assert(a.toString(), "1,2,3,4,5,6,7");

    a = [1, 2, 3];
    assert(a.shift(), 1);
    assert(a.toString(), "2,3");

    a = [3,4];
    assert(a.unshift(1,2), 4);
    assert(a.toString(), "1,2,3,4");

    a = [10, 11, 10, 11]
    assert(a.indexOf(11), 1);
    assert(a.indexOf(9), -1);
    assert(a.indexOf(11, 2), 3);
    assert(a.lastIndexOf(11), 3);
    assert(a.lastIndexOf(11, 2), 1);

    assert([1, 2, 3, 4].slice(1, 3).toString(), "2,3");
    assert([1, 2, 3, 4].slice(1).toString(), "2,3,4");

    log="";
    assert([1, 2, 3, 4].every(function(val, k) { log += val; assert(k, (val - 1)); return val != 5 }), true);
    assert(log, "1234");

    log = "";
    assert([1, 2, 3, 4].some(function(val, k) { log += val; assert(k, (val - 1)); return val == 5 }), false);
    assert(log, "1234");

    log = "";
    assert([1, 2, 3, 4].forEach(function(val, k) { log += val; assert(k, (val - 1)); }), void 0);
    assert(log, "1234");

    log = "";
    a = [1, 2, 3, 4].map(function(val, k) { assert(k, (val - 1)); return val + 1; });
    assert(a.toString(), "2,3,4,5");

    log = "";
    a = [1, 2, 3, 4].filter(function(val, k) { assert(k, (val - 1)); return val == 2 || val == 3; });
    assert(a.toString(), "2,3");
    
    assert(["1", 2, 3, 4].reduce(function(acc, val, k) { assert(k, (val - 1)); return acc + val; }), "1234");
    assert([1, 2, 3, 4].reduce(function(acc, val, k) { assert(k, (val - 1)); return acc + val; }, "0"), "01234");

    assert([1, 2, 3, "4"].reduceRight(function(acc, val, k) { assert(k, (val - 1)); return acc + val; }), "4321");
    assert([1, 2, 3, 4].reduceRight(function(acc, val, k) { assert(k, (val - 1)); return acc + val; }, "5"), "54321");

    a = [1, 2, 3, 4];
    assert(a.splice(1, 2, 10, 11, 12).toString(), "2,3");
    assert(a.toString(), "1,10,11,12,4");

    a = [1, 2, 3, 4];
    assert(a.splice(1, 2, 10).toString(), "2,3");
    assert(a.toString(), "1,10,4");

    a = [5, 4, 3, 2, 1];
    a.sort();
    assert(a[0], 1);
    assert(a.toString(), "1,2,3,4,5");

    a = [1, 2, 3, 4, 5];
    a.sort(function(a, b) { return (a < b) - (a > b) } );
    assert(a.toString(), "5,4,3,2,1");

    /* verify that the sort is stable and that 'undefined' is correctly handled */
    a = [ "b0", "z0", undefined, "b1", "a0", undefined, "z1", "a1", "a2"];
    a.sort(function(a, b) { return (a[0] > b[0]) - (a[0] < b[0]) } );
    assert(a.toString(), "a0,a1,a2,b0,b1,z0,z1,,");
}

/* non standard array behaviors */
function test_array_ext()
{
    var a;
    a = [1, 2, 3];
    assert_throws(TypeError, function () { a[1.2] = 1; } );
    assert_throws(TypeError, function () { a[NaN] = 1; } );
    assert_throws(TypeError, function () { a.NaN = 1; } );
    assert_throws(TypeError, function () { a[Infinity] = 1; } );
    assert_throws(TypeError, function () { a.Infinity = 1; } );
    assert_throws(TypeError, function () { a[-Infinity] = 1; } );
    assert_throws(TypeError, function () { a["1.2"] = 1; } );
    assert_throws(TypeError, function () { a["NaN"] = 1; } );
    assert_throws(TypeError, function () { a["Infinity"] = 1; } );
    assert_throws(TypeError, function () { a["-Infinity"] = 1; } );
}

function test_string()
{
    var a;
    a = String("abc");
    assert(a.length, 3, "string");
    assert(a[1], "b", "string");
    assert(a.charCodeAt(1), 0x62, "string");
    assert(String.fromCharCode(65), "A", "string");
    assert(String.fromCharCode(65, 66, 67), "ABC", "string");
    assert(a.charAt(1), "b");
    assert(a.charAt(-1), "");
    assert(a.charAt(3), "");

    a = "abcd";
    assert(a.substring(1, 3), "bc", "substring");
    a = String.fromCharCode(0x20ac);
    assert(a.charCodeAt(0), 0x20ac, "unicode");
    assert(a, "€", "unicode");
    assert(a, "\u20ac", "unicode");
    assert(a, "\u{20ac}", "unicode");
    assert("a", "\x61", "unicode");
        
    a = "\u{10ffff}";
    assert(a.length, 2, "unicode");
    assert(a, "\u{dbff}\u{dfff}", "unicode");
    assert(a.codePointAt(0), 0x10ffff);
    assert(a.codePointAt(1), 0xdfff);
    assert(String.fromCodePoint(0x10ffff), a);

    assert("a".concat("b", "c", 123), "abc123");

    assert("abcabc".indexOf("cab"), 2);
    assert("abcabc".indexOf("cab2"), -1);
    assert("abc".indexOf("c"), 2);
    assert("abcabc".lastIndexOf("ab"), 3);

    a = "a,b,c".split(",");
    assert(a.length === 3 && a[0] === "a" && a[1] === "b" && a[2] === "c");
    a = ",b,c".split(",");
    assert(a.length === 3 && a[0] === "" && a[1] === "b" && a[2] === "c");
    a = "a,b,".split(",");
    assert(a.length === 3 && a[0] === "a" && a[1] === "b" && a[2] === "");

//    assert((1,eval)('"\0"'), "\0");
    assert("123AbCd€".toLowerCase(), "123abcd€");
    assert("123AbCd€".toUpperCase(), "123ABCD€");
    assert("  ab€cd  ".trim(), "ab€cd");
    assert("  ab€cd  ".trimStart(), "ab€cd  ");
    assert("  ab€cd  ".trimEnd(), "  ab€cd");
    assert("abcabc".replace("b", "a$$b$&"), "aa$bbcabc");
    assert("abcabc".replaceAll("b", "a$$b$&"),"aa$bbcaa$bbc");
}

/* specific tests for internal UTF-8 storage */
function test_string2()
{
    var str = "hé€\u{101234}o";
    assert(str, "h\xe9\u20ac\udbc4\u{de34}o", "parse");
    assert(str.length, 6, "length");
    assert(str.slice(1, 2), "é", "slice");
    assert(str.slice(1, 3), "é€", "slice");
    assert(str.slice(2, 5), "€\u{101234}", "slice");
    assert(str.slice(2, 4), "€\u{dbc4}", "slice");
    assert(str.slice(4, 6), "\u{de34}o", "slice");
    assert("hé€" + "\u{101234}o", str, "concat 1");
    assert("h\xe9\u20ac\udbc4" + "\u{de34}o", str, "concat 2");

    var ch = "\udbc4\u{de34}";
    assert(ch.slice(0, 2), "\udbc4\u{de34}", "slice 1");
    assert(ch.slice(0, 1), "\udbc4", "slice 1");
    assert(ch.slice(1, 2), "\u{de34}", "slice 1");

    assert("\udbc4" + "\u{de34}", "\u{101234}", "concat 3");
    assert("\udbc4" + "o\u{de34}", "\udbc4o\u{de34}", "concat 4");

    assert(str[0], "h", "char 1");
    assert(str[1], "é", "char 2");
    assert(str[3], "\u{dbc4}", "char 3");
    assert(str[4], "\u{de34}", "char 4");
    assert(str.charCodeAt(3), 0xdbc4, "char 4");
    assert("€"[0], "€", "char 5");
    assert("\u{101234}"[0], "\u{dbc4}", "char 6");
    assert("\u{101234}"[1], "\u{de34}", "char 6");

    assert("\udbc4" <= "\udbc4", true);
    assert("\udbc3" < "\u{101234}", true);
    assert("\udbc4" < "\u{101234}", true);
    assert("\udbc5" > "\u{101234}", true);

    assert("\u{101234}" > "\udbc3", true);
    assert("\u{101234}" > "\udbc4", true);
    assert("\u{101234}" < "\udbc5", true);

    assert("\u{101233}" < "\u{101234}", true);
}

function test_math()
{
    var a;
    a = 1.4;
    assert(Math.floor(a), 1);
    assert(Math.ceil(a), 2);
    assert(Math.imul(0x12345678, 123), -1088058456);
    assert(Math.fround(0.1), 0.10000000149011612);
}

function test_number()
{
    assert(+"  123   ", 123);
    assert(+"0b111", 7);
    assert(+"0o123", 83);
    
    assert(parseInt("123"), 123);
    assert(parseInt("  123r"), 123);
    assert(parseInt("0x123"), 0x123);
    assert(parseInt("0o123"), 0);
    assert(parseFloat("0x1234"), 0);
    assert(parseFloat("Infinity"), Infinity);
    assert(parseFloat("-Infinity"), -Infinity);
    assert(parseFloat("123.2"), 123.2);
    assert(parseFloat("123.2e3"), 123200);

    assert((25).toExponential(), "2.5e+1");
    assert((25).toExponential(0), "3e+1");
    assert((-25).toExponential(0), "-3e+1");
    assert((2.5).toPrecision(1), "3");
    assert((-2.5).toPrecision(1), "-3");
    assert((25).toPrecision(1), "3e+1");
    assert((1.125).toFixed(2), "1.13");
    assert((-1.125).toFixed(2), "-1.13");
    assert((-1e-10).toFixed(0), "-0");
}

function test_global_eval()
{
    var r, g_eval = (1,eval);

    r = g_eval("1+1;");
    assert(r, 2, "eval");

    /* z is created as a global variable */
    r = g_eval("var z=2; z;");
    assert(r, 2, "eval");
    assert(z, 2);
    
    assert(g_eval("if (1) 2; else 3;"), 2);
    assert(g_eval("if (0) 2; else 3;"), 3);

    z = 2;
    assert(g_eval("z"), 2);

    g_eval("z = 3");
    assert(z, 3);
}

function test_typed_array()
{
    var buffer, a, i;

    a = new Uint8Array(4);
    assert(a.length, 4);
    for(i = 0; i < a.length; i++)
        a[i] = i;
    assert(a.toString(), "0,1,2,3");
    a[0] = -1;
    assert(a[0], 255);

    a = new Int8Array(3);
    a[0] = 255;
    assert(a[0], -1);

    a = new Int32Array(3);
    a[0] = Math.pow(2, 32) - 1;
    assert(a[0], -1);
    assert(a.BYTES_PER_ELEMENT, 4);

    a = new Uint8ClampedArray(4);
    a[0] = -100;
    a[1] = 1.5;
    a[2] = 0.5;
    a[3] = 1233.5;
    assert(a.toString(), "0,2,0,255");
    
    buffer = new ArrayBuffer(16);
    assert(buffer.byteLength, 16);
    a = new Uint32Array(buffer, 12, 1);
    assert(a.length, 1);
    a[0] = -1;

    a = new Uint16Array(buffer, 2);
    a[0] = -1;

    a = new Float32Array(buffer, 8, 1);
    a[0] = 1;
    
    a = new Uint8Array(buffer);
    
    assert(a.toString(), "0,0,255,255,0,0,0,0,0,0,128,63,255,255,255,255");

    assert(a.buffer, buffer);

    a = new Int32Array([1, 2, 3, 4]);
    assert(a.toString(), "1,2,3,4");
    a.set([10, 11], 2);
    assert(a.toString(), "1,2,10,11");
    a.set([12, 13]);
    assert(a.toString(), "12,13,10,11");
    a.set(new Int32Array([0, 1]), 1);
    assert(a.toString(), "12,0,1,11");

    a = new Uint8Array([1, 2, 3, 4]);
    a = a.subarray(1, 3);
    assert(a.toString(), "2,3");
}

function repeat(a, n)
{
    var i, r;
    r = "";
    while (n != 0) {
        if (n & 1)
            r += a;
        a += a;
        n >>>= 1;
    }
    return r;
}

/* return [s, line_num, col_num] where line_num and col_num are the
   position of the '@' character in 'str'. 's' is str without the '@'
   character */
function get_string_pos(str)
{
    var p, line_num, col_num, s, q, r;
    p = str.indexOf('@');
    assert(p >= 0, true);
    q = 0;
    line_num = 1;
    for(;;) {
        r = str.indexOf('\n', q);
        if (r < 0 || r >= p)
            break;
        q = r + 1;
        line_num++;
    }
    col_num = p - q + 1;
    s = str.slice(0, p) + str.slice(p + 1);
    return [s, line_num, col_num];
}

function check_error_pos(e, expected_error, line_num, col_num, level)
{
    var expected_pos, tab, line;
    level |= 0;
    expected_pos = ":" + line_num + ":" + col_num;
    tab = e.stack.split("\n");
    line = tab[level];
    if (line.slice(-1) == ')')
        line = line.slice(0, -1);
    if (line.indexOf(expected_pos) < 0) {
        throw_error("unexpected line or column number. error=|" + e.message +
                    "| got |" + line + "|, expected |" + expected_pos + "|");
    }
}

function assert_json_error(str, line_num, col_num)
{
    var err = false;
    var expected_pos, tab;

    tab = get_string_pos(str);
    
    try {
        JSON.parse(tab[0]);
    } catch(e) {
        err = true;
        if (!(e instanceof SyntaxError)) {
            throw_error("unexpected exception type");
            return;
        }
        /* XXX: the way quickjs returns JSON errors is not similar to Node or spiderMonkey */
        check_error_pos(e, SyntaxError, tab[1], tab[2]);
    }
    if (!err) {
        throw_error("expected exception");
    }
}

function test_json()
{
    var a, s, n;

    s = '{"x":1,"y":true,"z":null,"a":[1,2,false],"1234":"str"}';
    a = JSON.parse(s);
    assert(a.x, 1);
    assert(a.y, true);
    assert(a.z, null);
    assert(a[1234], "str");
    assert(JSON.stringify(a), s);

    assert(JSON.stringify({x: 1, y: undefined, z:2}), '{"x":1,"z":2}');

    /* larger stack */
    n = 100;
    s = repeat("[", n) + repeat("]", n);
    a = JSON.parse(s);
    assert(JSON.stringify(a), s);

//    assert_json_error('\n"  \\@x"');
//    assert_json_error('\n{ "a": @x }"');
}

function test_large_eval_parse_stack()
{
    var n = 1000;
    var str;
    
    str = repeat("(", n) + "1" + repeat(")", n);
    assert((1,eval)(str), 1);
    
    str = repeat("{", n) + "1;" + repeat("}", n);
    assert((1,eval)(str), 1);
    
    str = repeat("[", n) + "1" + repeat("]", n) + repeat("[0]", n);
    assert((1,eval)(str), 1);
}

function test_regexp()
{
    var a, str, n;

    str = "abbbbbc";
    a = /(b+)c/.exec(str);
    assert(a[0], "bbbbbc");
    assert(a[1], "bbbbb");
    assert(a.index, 1);
    assert(a.input, str);
    a = /(b+)c/.test(str);
    assert(a, true);
    assert(/\x61/.exec("a")[0], "a");
    assert(/\u0061/.exec("a")[0], "a");
    assert(/\ca/.exec("\x01")[0], "\x01");
    assert(/\\a/.exec("\\a")[0], "\\a");
    assert(/\c0/.exec("\\c0")[0], "\\c0");

    a = /(\.(?=com|org)|\/)/.exec("ah.com");
    assert(a.index === 2 && a[0] === ".");

    a = /(\.(?!com|org)|\/)/.exec("ah.com");
    assert(a, null);

    a = /(?=(a+))/.exec("baaabac");
    assert(a.index === 1 && a[0] === "" && a[1] === "aaa");

    a = /(z)((a+)?(b+)?(c))*/.exec("zaacbbbcac");
    assert(a, ["zaacbbbcac","z","ac","a", undefined,"c"]);

//    a = (1,eval)("/\0a/");
//    assert(a.toString(), "/\0a/");
//    assert(a.exec("\0a")[0], "\0a");

//    assert(/{1a}/.toString(), "/{1a}/");
//    a = /a{1+/.exec("a{11");
//    assert(a, ["a{11"]);

    /* test zero length matches */
    a = /(?:(?=(abc)))a/.exec("abc");
    assert(a, ["a", "abc"]);
    a = /(?:(?=(abc)))?a/.exec("abc");
    assert(a, ["a", undefined]);
    a = /(?:(?=(abc))){0,2}a/.exec("abc");
    assert(a, ["a", undefined]);
    a = /(?:|[\w])+([0-9])/.exec("123a23");
    assert(a, ["123a23", "3"]);
    a = /()*?a/.exec(",");
    assert(a, null);

    /* test \b escape */
    assert(/[\q{a\b}]/.test("a\b"), true);
    assert(/[\b]/.test("\b"), true);
    
    /* test case insensitive matching (test262 hardly tests it) */
    assert("aAbBcC".replace(/[^b]/gui, "X"), "XXbBXX");
    assert("aAbBcC".replace(/[^A-B]/gui, "X"), "aAbBXX");

    /* case where lastIndex points to the second element of a
       surrogate pair */
    a = /(?:)/gu;
    a.lastIndex = 1;
    a.exec("🐱");
    assert(a.lastIndex, 0);

    /* test backreferences */
    assert(/(abc)\1/.exec("abcabc"), ["abcabc", "abc"]);
    assert(/(abc)\1/i.exec("aBcaBC"), ["aBcaBC", "aBc"]);

    /* large parse stack */
    n = 10000;
    a = new RegExp(repeat("(?:", n) + "a+" + repeat(")", n));
    assert(a.exec("aa"), ["aa"]);
    
    /* additional functions */
    
    a = "abbbc".match(/b+/);
    assert(a, [ "bbb" ]);
    assert("abcaaad".match(/a+/g), [ "a", "aaa" ]);

    assert("abc".search(/b/), 1);
    assert("abc".search(/d/), -1);

    assert("abbbbcbbd".replace(/b+/, "€$&"), "a€bbbbcbbd");
    assert("abbbbcbbd".replace(/b+/g, "€$&"), "a€bbbbc€bbd");
    assert("abbbbccccd".replace(/(b+)(c+)/g, "_$1_$2_"), "a_bbbb_cccc_d");
    assert("abbbbcd".replace(/b+/g, "_$`_$&_$'_"), "a_a_bbbb_cd_cd");

    assert("abc".split(/b/), ["a", "c"]);
    assert("ab".split(/a*/g), ["", "b"]);
    assert("ab".split(/a*?/g), ["a", "b"]);
    assert("abc".split(/b/), ["a", "c"]);
    assert("A<B>bold</B>and<CODE>coded</CODE>".split(/<(\/)?([^<>]+)>/), ["A", undefined, "B", "bold", "/", "B", "and", undefined, "CODE", "coded", "/", "CODE", ""]);
}

function eval_error(eval_str, expected_error, level)
{
    var err = false;
    var expected_pos, tab;

    tab = get_string_pos(eval_str);
    
    try {
        (1, eval)(tab[0]);
    } catch(e) {
        err = true;
        if (!(e instanceof expected_error)) {
            throw_error("unexpected exception type");
            return;
        }
        check_error_pos(e, expected_error, tab[1], tab[2], level);
    }
    if (!err) {
        throw_error("expected exception");
    }
}

var poisoned_number = {
    valueOf: function() { throw Error("poisoned number") },
};

function test_line_column_numbers()
{
    var f, e, tab;

    /* The '@' character provides the expected position of the
       error. It is removed before evaluating the string. */
    
    /* parsing */
    eval_error("\n 123 @a ", SyntaxError);
    eval_error("\n  @/*  ", SyntaxError);
    eval_error("function f  @a", SyntaxError);
    /* currently regexp syntax errors point to the start of the regexp */
    eval_error("\n  @/aaa]/u", SyntaxError); 

    /* function definitions */
/*    
    tab = get_string_pos("\n   @function f() { }; f;");
    e = (1, eval)(tab[0]);
    assert(e.lineNumber, tab[1]);
    assert(e.columnNumber, tab[2]);
*/
    /* errors */
    tab = get_string_pos('\n  Error@("hello");');
    e = (1, eval)(tab[0]);
    check_error_pos(e, Error, tab[1], tab[2]);
    
    eval_error('\n  throw Error@("hello");', Error);

    /* operators */
    eval_error('\n  1 + 2 @* poisoned_number;', Error, 1);
    eval_error('\n  1 + "café" @* poisoned_number;', Error, 1);
    eval_error('\n  1 + 2 @** poisoned_number;', Error, 1);
    eval_error('\n  2 * @+ poisoned_number;', Error, 1);
    eval_error('\n  2 * @- poisoned_number;', Error, 1);
    eval_error('\n  2 * @~ poisoned_number;', Error, 1);
    eval_error('\n  2 * @++ poisoned_number;', Error, 1);
    eval_error('\n  2 * @-- poisoned_number;', Error, 1);
    eval_error('\n  2 * poisoned_number @++;', Error, 1);
    eval_error('\n  2 * poisoned_number @--;', Error, 1);

    /* accessors */
    eval_error('\n 1 + null@[0];', TypeError); 
    eval_error('\n 1 + null @. abcd;', TypeError); 
    //    eval_error('\n 1 + null @( 1234 );', TypeError);
    eval_error('var obj = { get a() { throw Error("test"); } }\n 1 + obj @. a;',
               Error, 1);
    eval_error('var obj = { set a(b) { throw Error("test"); } }\n obj @. a = 1;',
               Error, 1);
    
    /* variables reference */
    eval_error('\n  1 + @not_def', ReferenceError, 0);

    /* assignments */
    eval_error('1 + (@not_def = 1)', ReferenceError, 0);
    eval_error('1 + (@not_def += 2)', ReferenceError, 0);
    eval_error('var a;\n 1 + (a @+= poisoned_number);', Error, 1);
}

test();
test_string();
test_string2();
test_array();
test_array_ext();
test_enum();
test_function();
test_number();
test_math();
test_typed_array();
test_global_eval();
test_json();
test_regexp();
test_line_column_numbers();
test_large_eval_parse_stack();
