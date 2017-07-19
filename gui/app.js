const express = require('express');
const app = express();
const fileUpload = require('express-fileupload');
const bodyParser = require('body-parser');
const sqlite = require('sqlite3').verbose();
const pjson = require('./package.json');
const taskHandler = require('./taskHandler');
const fs = require('fs');

let database;           // For past execution analysis
let isRealTime = false; // true if database is running on server
let port = 8080;

app.use(function(req, res, next) { //allow cross origin requests
    res.setHeader("Access-Control-Allow-Methods", "POST, PUT, OPTIONS, DELETE, GET");
    res.header("Access-Control-Allow-Origin", "http://localhost");
    res.header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    next();
});

// app.use(bodyParser.urlencoded({ extended : false,limit: '50mb' }));
app.use(bodyParser.json({limit: '50mb'}));
app.use(bodyParser.urlencoded({limit: '50mb', extended: true}));

app.use(express.static('./www'));
app.use(bodyParser.json());
app.use(fileUpload());


// get events associated with a particular instance
app.get('/events/:instance', function(req, res) {
    if (!database) {
        res.json({error_code: 1, err_desc: 'No database on server'});
    }
    else {
        let instance = `'${req.params.instance}'`;
        let db = new sqlite.Database(database);
        let id = req.query.id ? ` AND id >= ${req.query.id}` : '';
        let query = `SELECT * FROM SolvingHistory WHERE name=${instance}${id}`;
        db.all(query, function(err, events) {
            if (err) {
                res.json({error_code: 1, err_desc: err});
                return;
            }
            let eventsJson = [];
            events.forEach(function(event) {
                eventsJson.push({
                    id: event.id,
                    ts: event.ts,
                    node: JSON.parse(event.node),
                    event: event.event,
                    solver: event.solver,
                    data: JSON.parse(event.data)
                });
            });
            res.json(eventsJson);
        });
        db.close();
    }
});


app.get('/instances', function(req, res) {
    if (!database) {
        res.json({error_code: 1, err_desc: 'No database on server'});
    }
    else {
        let db = new sqlite.Database(database);
        db.all("SELECT DISTINCT name FROM SolvingHistory", function(err, instances) {
            if (err) {
                res.json({error_code: 1, err_desc: err});
            }
            else {
                res.json(instances);
            }
        });
        db.close();
    }
});


app.get('/info', function(req, res) {
    res.json({
       isRealTime: isRealTime,
       version: taskHandler.getVersion(),
       // TODO: add database name / server address
    });
});


app.post('/upload/database', function(req, res) {
    if (!req.files)
        return res.status(400).send('No files were uploaded.');

    // The name of the input field (i.e. "sampleFile") is used to retrieve the uploaded file
    let sampleFile = req.files['smts-upload-db'];
    let uploadPath = __dirname + '/databases/temp/' + sampleFile.name;

    if (!fs.existsSync(__dirname + '/databases/temp')) {
        fs.mkdirSync(__dirname + '/databases/temp');
    }

    // Use the mv() method to place the file somewhere on your server
    sampleFile.mv(uploadPath, function(err) {
        if (err) {
            return res.status(500).send(err);
        }
        // Set database
        database = './databases/temp/' + sampleFile.name;
        res.redirect('back');
    });
});

app.post('/upload/instance', function(req, res) {
    if (!req.files)
        return res.status(400).send('No files were uploaded.');

    // The name of the input field (i.e. "sampleFile") is used to retrieve the uploaded file
    let sampleFile = req.files['smts-upload-instance'];
    let uploadPath = __dirname + '/benchmarks/temp/' + sampleFile.name;

    if (!fs.existsSync(__dirname + '/benchmarks/temp')) {
        fs.mkdirSync(__dirname + '/benchmarks/temp');
    }

    // Use the mv() method to place the file somewhere on your server
    sampleFile.mv(uploadPath, function(err) {
        if (err) {
            res.status(500).send(err);
        }
        else {
            taskHandler.newInstance(uploadPath);
            res.redirect('back');
        }
    });
});


app.get('/getSolvingInfo', function(req, res) {
    res.json(taskHandler.getCurrent());
});


app.post('/changeTimeout', function(req, res) {
    taskHandler.changeTimeout(req.body.delta);
});

app.post('/stop', function(req, res) {
    taskHandler.stopSolving();
});

process.stdin.resume(); // So the program will not close instantly

// Delete all files in temp directory before killing the process
function exitHandler(options, err) {
    if (options.cleanup) {
        // Delete database temp files
        if (fs.existsSync(`${__dirname}/databases/temp/`)) {
            let files = fs.readdirSync(`${__dirname}/databases/temp/`);
            files.forEach(file => fs.unlinkSync(`${__dirname}/databases/temp/${file}`));
        }

        // Delete benchmarks temp files
        if (fs.existsSync(`${__dirname}/benchmarks/temp/`)) {
            let files = fs.readdirSync(`${__dirname}/benchmarks/temp/`);
            files.forEach(file => fs.unlinkSync(`${__dirname}/benchmarks/temp/${file}`));
        }
    }
    if (err) console.log(err.stack);
    if (options.exit) process.exit();
}

//do something when app is closing
process.on('exit', exitHandler.bind(null, {cleanup: true}));

//catches ctrl+c event
process.on('SIGINT', exitHandler.bind(null, {cleanup: true}));

//catches uncaught exceptions
process.on('uncaughtException', exitHandler.bind(null, {exit: true}));


initialize();


function initialize() {
    if (process.argv[2] && (process.argv[2] === "--help" || process.argv[2] === "-h")) {
        showHelp();
        process.exit();
    }
    else if (process.argv[2] === "-v" || process.argv[2] === "--version") {
        console.log(pjson.version);
        process.exit();
    }
    else {
        for (let i = 2; i < process.argv.length - 1; i++) {
            switch (process.argv[i]) {
                // Port
                case '-p':
                case '--port':
                    let p = parseInt(process.argv[i + 1], 10);
                    if (p >= 0 && p < 65536) {
                        port = p;
                    }
                    else {
                        console.log("Bad or no port provided: 'port' argument must be >= 0 and < 65536");
                    }
                    break;

                // Database
                case '-d':
                case '--database':
                    database = process.argv[i + 1];
                    break;

                // Server
                case '-s':
                case'--server':
                    taskHandler.setPort(process.argv[i + 1]);
                    database = taskHandler.getDatabase();
                    console.log(database);
                    isRealTime = true;
                    if (database === '') {
                        console.log('There is no database on the server provided. Closing SMT Viewer.')
                        process.exit();
                    }
                    break;
            }
        }

        // Quit if no database provided
        if (!database) {
            console.log('Error: no database wa provided');
            process.exit();
        }

        app.listen(port, function() {
            console.log('Server running on ' + port + '...');
        });


    }
}

function showHelp() {
    console.log("Usage: node app.js [-h] [-v] [-s SERVER] [-p PORT] [-d DATABASE]");
    console.log("");
    console.log("Options:");
    console.log("-h, --help                            show help message");
    console.log("-v, --version                         print SMT Viewer version");
    console.log("-s SERVER, --server SERVER            set server ip address");
    console.log("-p PORT, --port PORT                  set port");
    console.log("-d DATABASE, --database DATABASE      set database");
}