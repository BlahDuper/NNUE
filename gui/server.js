const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const { spawn } = require('child_process');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static(path.join(__dirname, 'public')));

const PORT = 3000;

// Path to the compiled engine
const ENGINE_PATH = path.join(__dirname, '..', 'engine.exe');

io.on('connection', (socket) => {
    console.log('[GUI] Client connected');

    // Spawn a fresh engine process for each connection
    let engine = null;
    let engineReady = false;
    let moveHistory = [];
    let pendingBestmove = false;

    function spawnEngine() {
        engine = spawn(ENGINE_PATH, [], {
            cwd: path.join(__dirname, '..'),
            stdio: ['pipe', 'pipe', 'pipe']
        });

        let buffer = '';

        engine.stdout.on('data', (data) => {
            buffer += data.toString();
            const lines = buffer.split('\n');
            buffer = lines.pop(); // Keep incomplete line in buffer

            for (const line of lines) {
                const trimmed = line.trim();
                if (!trimmed) continue;

                console.log('[ENGINE OUT]', trimmed);

                // Forward search info to the client
                if (trimmed.startsWith('info depth')) {
                    socket.emit('engine-info', trimmed);
                }

                // Engine found its best move
                if (trimmed.startsWith('bestmove')) {
                    const parts = trimmed.split(' ');
                    const moveStr = parts[1];
                    pendingBestmove = false;
                    socket.emit('engine-move', moveStr);
                }

                if (trimmed === 'readyok') {
                    engineReady = true;
                    socket.emit('engine-ready');
                }

                if (trimmed === 'uciok') {
                    sendToEngine('isready');
                }
            }
        });

        engine.stderr.on('data', (data) => {
            console.log('[ENGINE ERR]', data.toString().trim());
        });

        engine.on('close', (code) => {
            console.log('[ENGINE] Process exited with code', code);
        });

        // Initialize UCI handshake
        sendToEngine('uci');
    }

    function sendToEngine(cmd) {
        if (engine && engine.stdin.writable) {
            console.log('[ENGINE IN]', cmd);
            engine.stdin.write(cmd + '\n');
        }
    }

    // Client asks us to start a new game
    socket.on('new-game', () => {
        if (engine) {
            engine.kill();
        }
        moveHistory = [];
        pendingBestmove = false;
        spawnEngine();
    });

    // Client made a move and wants the engine to respond
    socket.on('player-move', (data) => {
        // data: { moves: ['e2e4', 'd7d5', ...], thinkTime: 2000 }
        moveHistory = data.moves || [];
        const thinkTime = data.thinkTime || 2000;

        // Build the UCI position command
        let posCmd = 'position startpos';
        if (moveHistory.length > 0) {
            posCmd += ' moves ' + moveHistory.join(' ');
        }

        sendToEngine('isready');
        // Wait a tick for readyok, then send position + go
        const waitAndGo = () => {
            sendToEngine(posCmd);
            sendToEngine(`go movetime ${thinkTime}`);
            pendingBestmove = true;
        };

        // Simple timeout to let isready complete
        setTimeout(waitAndGo, 50);
    });

    socket.on('disconnect', () => {
        console.log('[GUI] Client disconnected');
        if (engine) {
            sendToEngine('quit');
            setTimeout(() => {
                if (engine) engine.kill();
            }, 500);
        }
    });

    // Spawn engine immediately on connection
    spawnEngine();
});

server.listen(PORT, () => {
    console.log(`\n  ♟  NNUE Chess GUI running at http://localhost:${PORT}\n`);
});
