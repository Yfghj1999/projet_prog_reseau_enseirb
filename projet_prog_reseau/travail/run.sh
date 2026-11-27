#!/bin/bash
# ./run.sh   : compile + lance le serveur et les clients dans tmux
# Ctrl+b r   : recompile + relance tout, sans quitter tmux

PORT=8080
NICKS=(edouard tom thomas)     # un client par pseudo
SESSION=chat

# Se placer dans le dossier du script (nécessaire quand c'est tmux qui l'appelle)
cd "$(dirname "$(readlink -f "$0")")" || exit 1

SERVER_CMD="./bin/server $PORT"
CLIENT_CMD="sleep 0.5; ./bin/client localhost $PORT"

# Tape "/nick <pseudo>" dans chaque client (le panneau 0 est le serveur)
set_nicks() {
    sleep 1
    local panes=($(tmux list-panes -t "$SESSION" -F '#{pane_id}'))
    for i in "${!NICKS[@]}"; do
        tmux send-keys -t "${panes[$((i + 1))]}" "/nick ${NICKS[$i]}" Enter
    done
}

# ---- Mode relance (appelé par Ctrl+b r) ----
if [ "$1" = "reload" ]; then
    if ! out=$(make clean all 2>&1); then
        echo "$out"       # erreurs de compilation affichées dans tmux (q pour fermer)
        exit 1
    fi
    panes=($(tmux list-panes -t "$SESSION" -F '#{pane_id}'))
    tmux respawn-pane -k -t "${panes[0]}" "$SERVER_CMD"
    for p in "${panes[@]:1}"; do
        tmux respawn-pane -k -t "$p" "$CLIENT_CMD"
    done
    set_nicks
    exit 0
fi

# ---- Lancement normal ----
make clean all || exit 1

tmux kill-session -t "$SESSION" 2>/dev/null
tmux new-session -d -s "$SESSION" "$SERVER_CMD"
tmux set-option -t "$SESSION" remain-on-exit on
tmux set-option -t "$SESSION" mouse on

for nick in "${NICKS[@]}"; do
    tmux split-window -t "$SESSION" "$CLIENT_CMD"
    tmux select-layout -t "$SESSION" tiled
done

# Raccourci Ctrl+b r -> ./run.sh reload
tmux bind-key r run-shell -b "'$PWD/run.sh' reload"

set_nicks &
tmux attach -t "$SESSION"