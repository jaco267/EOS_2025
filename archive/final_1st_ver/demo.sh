#!/bin/bash

SESSION="lab6"
PORT=8888

make clean
make

# 刪除舊 session
tmux has-session -t $SESSION 2>/dev/null
if [[ $? -eq 0 ]]; then
	tmux kill-session -t $SESSION 2>/dev/null
fi

# 建立新的 session 並啟動 bash
tmux new-session -d -s $SESSION "bash"
sleep 0.3   # 等待 tmux 初始化

# 分割成 5 個 pane（server + 4 clients）
tmux split-window -h -t $SESSION:0  # 上下切
# 上面是 0.0, 下面是 0.1

# Step 2: 下面水平切成 4 個 client pane
# tmux select-pane -t $SESSION:0.1
# tmux split-window -h -t $SESSION:0.1  # 0.1 + 0.2
# tmux split-window -h -t $SESSION:0.2  # 0.2 + 0.3
# tmux split-window -h -t $SESSION:0.3  # 0.3 + 0.4

### 選回上面的 server panel 
tmux select-pane -t $SESSION:0.0

# 啟動 server
tmux send-keys -t $SESSION:0.0 "./room_server" C-m
sleep 1

tmux send-keys -t $SESSION:0.1 "./room_client" C-m
# tmux send-keys -t $SESSION:0.2 "./client 127.0.0.1 $PORT withdraw 1 500" C-m
# tmux send-keys -t $SESSION:0.3 "./client 127.0.0.1 $PORT deposit 2 500" C-m
# tmux send-keys -t $SESSION:0.4 "./client 127.0.0.1 $PORT withdraw 2 500" C-m

tmux select-pane -t $SESSION:0.1
sleep 1
tmux send-keys -t $SESSION:0.1 "status" C-m
tmux send-keys -t $SESSION:0.1 "reserve 0" C-m
tmux send-keys -t $SESSION:0.1 "checkin 0" C-m
sleep 1
tmux send-keys -t $SESSION:0.1 "status" C-m

tmux -2 attach-session -t $SESSION