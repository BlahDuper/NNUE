// ============================================================================
// NNUE Chess GUI — Client-side Logic
// ============================================================================

const socket = io();

// Initialize chess.js for local move validation
let game = new Chess();
let board = null;
let moveHistory = [];     // UCI move strings: ['e2e4', 'd7d5', ...]
let sanHistory = [];      // SAN pairs for the move list display
let playerColor = 'white';
let engineThinking = false;

// Piece unicode for captured display
const PIECE_UNICODE = {
    wp: '♙', wn: '♘', wb: '♗', wr: '♖', wq: '♕',
    bp: '♟', bn: '♞', bb: '♝', br: '♜', bq: '♛'
};

// Track captured pieces
let capturedWhite = [];  // White pieces captured (by black)
let capturedBlack = [];  // Black pieces captured (by white)

// ============================================================================
// Board configuration
// ============================================================================

function onDragStart(source, piece, position, orientation) {
    // Only allow dragging when it's the player's turn
    if (game.game_over()) return false;
    if (engineThinking) return false;

    // Only allow the player to move their own pieces
    if (playerColor === 'white' && piece.search(/^b/) !== -1) return false;
    if (playerColor === 'black' && piece.search(/^w/) !== -1) return false;

    // Only allow moves when it's actually the player's turn
    if (playerColor === 'white' && game.turn() !== 'w') return false;
    if (playerColor === 'black' && game.turn() !== 'b') return false;
}

function onDrop(source, target) {
    // See if the move is legal
    let moveObj = game.move({
        from: source,
        to: target,
        promotion: 'q'  // Always promote to queen for simplicity
    });

    // Illegal move
    if (moveObj === null) return 'snapback';

    // Track captures
    if (moveObj.captured) {
        const capturedPiece = (moveObj.color === 'w' ? 'b' : 'w') + moveObj.captured;
        if (moveObj.color === 'w') {
            capturedBlack.push(capturedPiece);
        } else {
            capturedWhite.push(capturedPiece);
        }
        updateCapturedDisplay();
    }

    // Build UCI move string
    let uciMove = source + target;
    if (moveObj.flags.includes('p')) {
        uciMove += 'q';  // Promotion
    }
    moveHistory.push(uciMove);

    // Add to SAN history
    addMoveToHistory(moveObj.san, moveObj.color);

    // Highlight the move
    highlightMove(source, target);

    // Update status
    updateStatus();

    // Check if game is over after player's move
    if (game.game_over()) {
        return;
    }

    // Ask the engine to respond
    requestEngineMove();
}

function onSnapEnd() {
    board.position(game.fen());
}

// ============================================================================
// Engine communication
// ============================================================================

function requestEngineMove() {
    engineThinking = true;
    setStatus('Engine is thinking...', 'thinking');
    clearEngineStats();

    const thinkTime = parseInt(document.getElementById('think-time').value);

    socket.emit('player-move', {
        moves: moveHistory,
        thinkTime: thinkTime
    });
}

socket.on('engine-move', (uciMove) => {
    engineThinking = false;

    // Parse the UCI move
    const from = uciMove.substring(0, 2);
    const to = uciMove.substring(2, 4);
    const promotion = uciMove.length > 4 ? uciMove[4] : null;

    // Make the move in chess.js
    let moveConfig = { from, to };
    if (promotion) moveConfig.promotion = promotion;

    const moveObj = game.move(moveConfig);

    if (!moveObj) {
        console.error('Engine returned illegal move:', uciMove);
        setStatus('Engine error — illegal move!', 'game-over');
        return;
    }

    // Track captures
    if (moveObj.captured) {
        const capturedPiece = (moveObj.color === 'w' ? 'b' : 'w') + moveObj.captured;
        if (moveObj.color === 'w') {
            capturedBlack.push(capturedPiece);
        } else {
            capturedWhite.push(capturedPiece);
        }
        updateCapturedDisplay();
    }

    moveHistory.push(uciMove);
    addMoveToHistory(moveObj.san, moveObj.color);

    // Animate the piece on the board
    board.position(game.fen());
    highlightMove(from, to);

    updateStatus();
});

socket.on('engine-info', (infoLine) => {
    // Parse "info depth 6 score cp -5 nodes 291368 nps 134767 time 2162"
    const parts = infoLine.split(' ');
    for (let i = 0; i < parts.length; i++) {
        if (parts[i] === 'depth' && parts[i + 1]) {
            document.getElementById('stat-depth').textContent = parts[i + 1];
        }
        if (parts[i] === 'cp' && parts[i + 1]) {
            let cp = parseInt(parts[i + 1]);
            // The engine score is always relative to the engine's side.
            // Human convention is that positive score means White is winning.
            // So if the engine is Black (player is White), we negate the score.
            if (playerColor === 'white') {
                cp = -cp;
            }
            const display = (cp > 0 ? '+' : '') + (cp / 100).toFixed(2);
            document.getElementById('stat-score').textContent = display;
        }
        if (parts[i] === 'nodes' && parts[i + 1]) {
            const n = parseInt(parts[i + 1]);
            document.getElementById('stat-nodes').textContent = formatNumber(n);
        }
        if (parts[i] === 'nps' && parts[i + 1]) {
            const n = parseInt(parts[i + 1]);
            document.getElementById('stat-nps').textContent = formatNumber(n);
        }
    }
});

socket.on('engine-ready', () => {
    console.log('Engine is ready');
});

// ============================================================================
// UI helpers
// ============================================================================

function setStatus(text, className) {
    const el = document.getElementById('game-status');
    document.getElementById('status-text').textContent = text;
    el.className = 'game-status' + (className ? ' ' + className : '');
}

function updateStatus() {
    if (game.in_checkmate()) {
        const winner = game.turn() === 'w' ? 'Black' : 'White';
        setStatus(`Checkmate! ${winner} wins.`, 'game-over');
    } else if (game.in_stalemate()) {
        setStatus('Stalemate — draw.', 'game-over');
    } else if (game.in_draw()) {
        setStatus('Draw.', 'game-over');
    } else if (game.in_check()) {
        if (!engineThinking) {
            setStatus('Check! Your turn.', '');
        }
    } else if (!engineThinking) {
        setStatus('Your turn', '');
    }
}

function highlightMove(from, to) {
    // Remove old highlights
    document.querySelectorAll('.highlight-move').forEach(el => {
        el.classList.remove('highlight-move');
    });

    // Add new highlights
    const fromEl = document.querySelector(`.square-${from}`);
    const toEl = document.querySelector(`.square-${to}`);
    if (fromEl) fromEl.classList.add('highlight-move');
    if (toEl) toEl.classList.add('highlight-move');
}

function addMoveToHistory(san, color) {
    const moveListEl = document.getElementById('move-list');

    // Clear placeholder
    const placeholder = moveListEl.querySelector('.move-list-placeholder');
    if (placeholder) placeholder.remove();

    if (color === 'w') {
        // Start a new move pair
        const moveNum = Math.ceil(moveHistory.length / 2);
        const pair = document.createElement('div');
        pair.className = 'move-pair';
        pair.innerHTML = `<span class="move-number">${moveNum}.</span>
                          <span class="move-white">${san}</span>
                          <span class="move-black"></span>`;
        moveListEl.appendChild(pair);
    } else {
        // Fill in the black move on the last pair
        const pairs = moveListEl.querySelectorAll('.move-pair');
        if (pairs.length > 0) {
            const last = pairs[pairs.length - 1];
            last.querySelector('.move-black').textContent = san;
        }
    }

    // Auto-scroll to bottom
    moveListEl.scrollTop = moveListEl.scrollHeight;
}

function updateCapturedDisplay() {
    document.getElementById('captured-white').textContent =
        capturedWhite.map(p => PIECE_UNICODE[p] || '').join('');
    document.getElementById('captured-black').textContent =
        capturedBlack.map(p => PIECE_UNICODE[p] || '').join('');
}

function clearEngineStats() {
    document.getElementById('stat-depth').textContent = '—';
    document.getElementById('stat-score').textContent = '—';
    document.getElementById('stat-nodes').textContent = '—';
    document.getElementById('stat-nps').textContent = '—';
}

function formatNumber(n) {
    if (n >= 1_000_000) return (n / 1_000_000).toFixed(1) + 'M';
    if (n >= 1_000) return (n / 1_000).toFixed(1) + 'K';
    return n.toString();
}

function newGame() {
    game = new Chess();
    moveHistory = [];
    sanHistory = [];
    capturedWhite = [];
    capturedBlack = [];
    engineThinking = false;

    board.position('start');
    setStatus('Your turn — play as White', '');
    clearEngineStats();
    updateCapturedDisplay();

    // Clear move list
    const moveListEl = document.getElementById('move-list');
    moveListEl.innerHTML = '<span class="move-list-placeholder">Moves will appear here...</span>';

    // Remove highlights
    document.querySelectorAll('.highlight-move').forEach(el => {
        el.classList.remove('highlight-move');
    });

    socket.emit('new-game');
}

function newGameAsBlack() {
    newGame();
    playerColor = 'black';
    board.orientation('black');
    setStatus('Engine is thinking...', 'thinking');
    // Ask engine to make the first move
    requestEngineMove();
}

// ============================================================================
// Initialize
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    board = Chessboard('board', {
        draggable: true,
        position: 'start',
        onDragStart: onDragStart,
        onDrop: onDrop,
        onSnapEnd: onSnapEnd,
        pieceTheme: 'https://chessboardjs.com/img/chesspieces/wikipedia/{piece}.png',
        animation: true,
        animationDuration: 200
    });

    // Button handlers
    document.getElementById('btn-new-game').addEventListener('click', () => {
        playerColor = 'white';
        board.orientation('white');
        newGame();
    });
    document.getElementById('btn-play-black').addEventListener('click', newGameAsBlack);
    document.getElementById('btn-flip').addEventListener('click', () => {
        board.flip();
    });

    // Request a fresh engine
    socket.emit('new-game');
});
