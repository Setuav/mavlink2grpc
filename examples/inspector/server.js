#!/usr/bin/env node
/**
 * MAVLink Inspector Server
 * Node.js Express server with gRPC client and Socket.IO
 */

const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const path = require('path');
const winston = require('winston');

// Logger setup
const logger = winston.createLogger({
    level: 'info',
    format: winston.format.combine(
        winston.format.timestamp(),
        winston.format.printf(({ timestamp, level, message }) => {
            return `[${timestamp}] ${level.toUpperCase()}: ${message}`;
        })
    ),
    transports: [
        new winston.transports.Console()
    ]
});

// Message storage
class MessageStore {
    constructor() {
        this.messages = new Map();
    }

    addMessage(messageName, data) {
        const now = Date.now();
        
        if (!this.messages.has(messageName)) {
            this.messages.set(messageName, {
                count: 0,
                frequency: 0,
                lastData: {},
                timestamps: [],
                fieldHistory: new Map()
            });
        }

        const msg = this.messages.get(messageName);
        msg.count++;
        msg.lastData = data;
        msg.timestamps.push(now);

        // Keep only last 5 seconds of timestamps
        const cutoff = now - 5000;
        msg.timestamps = msg.timestamps.filter(t => t > cutoff);

        // Store numeric field history for charting
        for (const [key, value] of Object.entries(data)) {
            if (typeof value === 'number') {
                if (!msg.fieldHistory.has(key)) {
                    msg.fieldHistory.set(key, []);
                }
                
                const history = msg.fieldHistory.get(key);
                history.push({ timestamp: now, value });
                
                // Keep max 1000 points per field
                if (history.length > 1000) {
                    history.shift();
                }
            }
        }
    }

    updateFrequencies() {
        const now = Date.now();
        const cutoff = now - 5000;

        for (const [name, msg] of this.messages) {
            msg.timestamps = msg.timestamps.filter(t => t > cutoff);
            msg.frequency = msg.timestamps.length / 5.0;
        }
    }

    getMessageList() {
        return Array.from(this.messages.entries())
            .map(([name, msg]) => ({
                name,
                count: msg.count,
                frequency: parseFloat(msg.frequency.toFixed(1))
            }))
            .sort((a, b) => b.count - a.count);
    }

    getMessageDetail(messageName) {
        const msg = this.messages.get(messageName);
        if (!msg) return null;

        const fieldHistory = {};
        for (const [field, history] of msg.fieldHistory) {
            fieldHistory[field] = history;
        }

        return {
            name: messageName,
            count: msg.count,
            frequency: parseFloat(msg.frequency.toFixed(1)),
            data: msg.lastData,
            fieldHistory
        };
    }

    clear() {
        this.messages.clear();
    }
}

const messageStore = new MessageStore();

// gRPC Client
class GRPCClient {
    constructor(serverAddress) {
        this.serverAddress = serverAddress;
        this.client = null;
        this.call = null;
        this.connected = false;
        this.socketServer = null;
    }

    setSocketServer(io) {
        this.socketServer = io;
    }

    async connect() {
        try {
            // Load proto file
            const PROTO_PATH = path.join(__dirname, '..', '..', 'proto', 'mavlink_bridge.proto');
            const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
                keepCase: true,
                longs: String,
                enums: String,
                defaults: true,
                oneofs: true,
                includeDirs: [path.join(__dirname, '..', '..', 'proto')]
            });

            const proto = grpc.loadPackageDefinition(packageDefinition);
            
            // Create client - note: service name is MavlinkBridge (capital B in proto, but proto loader makes it camelCase)
            this.client = new proto.mavlink.MavlinkBridge(
                this.serverAddress,
                grpc.credentials.createInsecure()
            );

            logger.info(`Connecting to gRPC server at ${this.serverAddress}...`);

            // Test connection with a deadline
            await new Promise((resolve, reject) => {
                const deadline = new Date();
                deadline.setSeconds(deadline.getSeconds() + 5);
                
                this.client.waitForReady(deadline, (error) => {
                    if (error) {
                        reject(error);
                    } else {
                        resolve();
                    }
                });
            });

            this.connected = true;
            logger.info('Connected to gRPC server');
            
            if (this.socketServer) {
                this.socketServer.emit('status', { 
                    state: 'connected', 
                    text: 'Connected' 
                });
            }

            // Start streaming
            this.startStream();

            return true;

        } catch (error) {
            logger.error(`Failed to connect to gRPC server: ${error.message}`);
            this.connected = false;
            
            if (this.socketServer) {
                this.socketServer.emit('status', { 
                    state: 'disconnected', 
                    text: `Connection failed: ${error.message}` 
                });
            }

            return false;
        }
    }

    startStream() {
        try {
            const request = {};
            this.call = this.client.StreamMessages(request);

            this.call.on('data', (message) => {
                this.handleMessage(message);
            });

            this.call.on('error', (error) => {
                logger.error(`gRPC stream error: ${error.message}`);
                this.connected = false;
                
                if (this.socketServer) {
                    this.socketServer.emit('status', { 
                        state: 'disconnected', 
                        text: 'Stream disconnected' 
                    });
                }
            });

            this.call.on('end', () => {
                logger.info('gRPC stream ended');
                this.connected = false;
                
                if (this.socketServer) {
                    this.socketServer.emit('status', { 
                        state: 'disconnected', 
                        text: 'Disconnected' 
                    });
                }
            });

        } catch (error) {
            logger.error(`Failed to start stream: ${error.message}`);
        }
    }

    handleMessage(message) {
        try {
            // Find which message type is set (oneof field)
            let messageType = null;
            let messageData = null;

            // Iterate through all possible message fields
            for (const key of Object.keys(message)) {
                if (message[key] && typeof message[key] === 'object' && Object.keys(message[key]).length > 0) {
                    messageType = key.toUpperCase();
                    messageData = message[key];
                    break;
                }
            }

            if (!messageType || !messageData) {
                return;
            }

            // Convert message to plain object
            const data = {};
            for (const [key, value] of Object.entries(messageData)) {
                // Handle Buffer/Uint8Array fields
                if (value instanceof Buffer || value instanceof Uint8Array) {
                    data[key] = Array.from(value);
                } else if (Array.isArray(value)) {
                    data[key] = value;
                } else {
                    data[key] = value;
                }
            }

            // Store message
            messageStore.addMessage(messageType, data);

            // Emit to WebSocket clients
            if (this.socketServer) {
                this.socketServer.emit('message', {
                    name: messageType,
                    data
                });
            }

        } catch (error) {
            logger.error(`Error handling message: ${error.message}`);
        }
    }

    disconnect() {
        if (this.call) {
            this.call.cancel();
        }
        this.connected = false;
        
        if (this.socketServer) {
            this.socketServer.emit('status', { 
                state: 'disconnected', 
                text: 'Disconnected' 
            });
        }
    }
}

// Express app setup
const app = express();
const server = http.createServer(app);
const io = new Server(server, {
    cors: {
        origin: '*',
        methods: ['GET', 'POST']
    }
});

// Serve static files
app.use(express.static(__dirname));

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

// REST API
app.get('/api/messages', (req, res) => {
    res.json(messageStore.getMessageList());
});

app.get('/api/messages/:name', (req, res) => {
    const detail = messageStore.getMessageDetail(req.params.name);
    if (detail) {
        res.json(detail);
    } else {
        res.status(404).json({ error: 'Message not found' });
    }
});

app.post('/api/clear', (req, res) => {
    messageStore.clear();
    res.json({ status: 'cleared' });
});

// Socket.IO events
io.on('connection', (socket) => {
    logger.info('Client connected');

    socket.emit('status', {
        state: grpcClient.connected ? 'connected' : 'disconnected',
        text: grpcClient.connected ? 'Connected' : 'Disconnected'
    });

    socket.emit('message_list', messageStore.getMessageList());

    socket.on('disconnect', () => {
        logger.info('Client disconnected');
    });

    socket.on('request_detail', (data) => {
        const detail = messageStore.getMessageDetail(data.message);
        if (detail) {
            socket.emit('message_detail', detail);
        }
    });

    socket.on('clear', () => {
        messageStore.clear();
        io.emit('message_list', []);
    });
});

// Parse command line arguments
const args = process.argv.slice(2);
let grpcServer = 'localhost:50051';
let port = 8000;
let host = '0.0.0.0';

for (let i = 0; i < args.length; i++) {
    if (args[i] === '-g' || args[i] === '--grpc-server') {
        grpcServer = args[++i];
    } else if (args[i] === '-p' || args[i] === '--port') {
        port = parseInt(args[++i]);
    } else if (args[i] === '--host') {
        host = args[++i];
    } else if (args[i] === '-h' || args[i] === '--help') {
        console.log('MAVLink Inspector Server');
        console.log('\nUsage: node server.js [options]');
        console.log('\nOptions:');
        console.log('  -g, --grpc-server <addr>  gRPC server address (default: localhost:50051)');
        console.log('  -p, --port <port>         Web server port (default: 8000)');
        console.log('  --host <host>             Web server host (default: 0.0.0.0)');
        console.log('  -h, --help                Show this help');
        process.exit(0);
    }
}

// Create gRPC client
const grpcClient = new GRPCClient(grpcServer);
grpcClient.setSocketServer(io);

// Update frequencies every second
setInterval(() => {
    messageStore.updateFrequencies();
    io.emit('message_list', messageStore.getMessageList());
}, 1000);

// Start server
server.listen(port, host, async () => {
    logger.info(`MAVLink Inspector running at http://${host}:${port}`);
    logger.info(`gRPC server: ${grpcServer}`);
    
    // Connect to gRPC server
    const connected = await grpcClient.connect();
    if (!connected) {
        logger.warn('Failed to connect to gRPC server. Retrying in 5 seconds...');
        
        // Retry connection
        setInterval(async () => {
            if (!grpcClient.connected) {
                await grpcClient.connect();
            }
        }, 5000);
    }
});

// Graceful shutdown
process.on('SIGINT', () => {
    logger.info('\nShutting down...');
    grpcClient.disconnect();
    server.close(() => {
        logger.info('Server closed');
        process.exit(0);
    });
});

process.on('SIGTERM', () => {
    logger.info('\nShutting down...');
    grpcClient.disconnect();
    server.close(() => {
        logger.info('Server closed');
        process.exit(0);
    });
});
