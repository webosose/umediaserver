var os = require('os');
var http = require('http');
var fs = require('fs');
var path = require('path');

var luna = {
    module: undefined,
    handle: undefined,
    init: function() {
        try {
            this.module = require('palmbus');
            this.handle = new this.module.Handle('com.webos.ums-status.http_proxy', false);
        } catch (err) {
            console.warn('Failed to register on the system bus: %s', err.message);
            this.handle = null;
        }
    },
    call: function(uri, args, callback) {
        if (this.handle) {
            var request = this.handle.call(uri, JSON.stringify(args));
            request.addListener('response', function(message) {
                callback(null, message.payload());
            });
        } else {
            callback(new Error('System bus registration failed'));
        }
    }
};
luna.init();

var PORT = undefined;
var ROOT = undefined;

function handleCmdLineArgs() {
    if (process.argv.length < 3) {
        console.log('Usage: %s %s %s', process.argv[0], process.argv[1], '<PORT_NUMBER>');
        process.exit(1);
    }
    ROOT = path.dirname(process.argv[1]);
    PORT = process.argv[2];
}

handleCmdLineArgs();

function getIpAddress() {
    var ifaces = os.networkInterfaces();
    var ip = undefined;
    Object.keys(ifaces).every(function(ifname) {
        ifaces[ifname].every(function(iface) {
            if (!iface.internal && 'IPv4' === iface.family) {
                ip = iface.address;
                return false;
            }
            return true;
        });
        return ip === undefined;
    });
    return ip;
}

function handleRequest(request, response) {
    function mime(ext) {
        switch (ext) {
            case '.htm':
            case '.html':
                return 'text/html';
            case '.js':
                return 'text/javascript';
            case '.css':
                return 'text/css';
            case '.jpg':
            case '.jpeg':
                return 'image/jpeg';
            case '.png':
                return 'image/png';
        }
        return 'text/plain';
    }
    function error(status, message) {
        response.writeHead(status, {'Content-Type': 'text/html'});
        response.end('<html><head></head><body>' + status + ': ' + message + '</body></html>');
    }
    function reply(mime, data) {
        response.writeHead(200, {'Content-Type': mime});
        response.end(data);
    }
    function file(file) {
        file = ROOT + '/' + file;
        fs.exists(file, function(exists) {
            if (exists) {
                fs.readFile(file, function(err, content) {
                    if (err) {
                        error(500, 'Interval server error: ' + err.message);
                    } else {
                        reply(mime(path.extname(file)), content);
                    }
                });
            } else {
                error(404, 'Not found');
            }
        });
    }
    function update() {
        luna.call('palm://com.webos.media/getActivePipelines', {}, function(err, message) {
            if (err) {
                error(500, 'Internal server error: ' + err.message);
            } else {
                reply('application/json', message);
            }
        });
    }
    switch(request.url) {
        case '/':
            file('ums-status.html');
            break;
        case '/update':
            update();
            break;
        default:
            file(request.url);
    }
}

var server = http.createServer(handleRequest);

server.listen(PORT, function(){
    console.log('Server listening on: http://%s:%s', getIpAddress(), PORT);
});
