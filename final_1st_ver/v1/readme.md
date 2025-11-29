### 期末專題: 智慧訂房間系統
#### 系統說明
本系統：智慧訂房系統。提供即時房況、預約、報到、釋放。三色LED顯示狀態
 (🟢=空閒、🔴=已預約/使用中、🟡=倒數提醒）
- 提供給客人的功能：
  - 查看房況：看板或網頁即時顯示。
  - 立即預約：選擇房間與時段送出。
  - 報到/開始使用：現場按「Check-in」或刷卡後開始計時。
  - 提前釋放/取消：按「Release」或於頁面取消並釋出剩餘時段。
  - 延長申請：無人候補時可延長一次（時長可設定）。
  - 到時提醒：即將到時與逾時通知
  
- 訂房規則
  - 時段單位：30分鐘；每次最多1個時段。
  - 取用時限：預約後5分鐘內需報到；逾時自動取消並釋出。
  - 使用上限：單日最多2次；不得同時持有多筆預約。
  - 延長規則：無人候補可延長1次；先到先得。
  - 釋放規則：提前釋放立即開放；逾時未釋放由系統自動釋放。


#### 硬體架構(include IO)

- 2 client : 處理I/O並回傳給server, 在LED上顯示訂房狀態
- client <---> socket <---> server :  提供訂房服務


- Server端: Raspberry pi 
- 每位房客:
  - Raspberry pi 
  - 2個按鈕(控制確認、取消)
  - 1個七段顯示器(顯示當前房間號碼)
  - 3種顏色LED(顯示當前房間狀態)


#### 軟體設計

system start -> 初始化 kernel module (LED/GPIO) -> 啟動房間狀態管理執行緒 
-> 等待使用者命令(ioctl) -> 更新房間狀態與 LED 顯示(使用 Mutex 同步) 
----> Timer Thread(定期掃描倒數)-----> 更新共享記憶區 (/proc)---> user app 查詢與顯示結果    
  |                             |
  --> Interrupt ISR      --------
      (check-in/Release)

---> process end

#### 結合上課所學

- Scheduler
  - 使用者程式 room_manager、kernel work thread、ISR 共同運作。
  - 以一般排程處理查詢與預約。計時到期與按鍵事件需即時處理。
- Sync（Mutex／Semaphore／Spinlock／Cond.）
  - 以 mutex 保護「房間狀態表與使用次數計數」。
  - 報到與延長採原子檢查後更新（check-then-act 在同一鎖內）。
  - 以條件變數或 wait-queue 喚醒等待端（例如候補清單）。
- Comm
  - 狀態讀取 : 透過 cat 或 read() 讀取狀態。
  - 指令通道：使用 ioctl() 或 write() 傳送指令 {RESERVE, CHECKIN, RELEASE, EXTEND}。

- I/O（Device driver／Device table）
  - GPIO 驅動控制三色 LED；建立 /dev/room 介面。
  - 去抖按鍵做 Check-in／Release，與後端狀態同步。
- INT（Exception／Priority／Context）
  - GPIO 中斷為 top-half 設定 flag ，底半部(workqueue)更新狀態與排程工作。
  - 中斷內不睡眠、不做長耗時操作。
- Timer（TINT／Soft timer／Signal）
  - T_checkin=5 分：預約後未報到自動取消。
  - T_end=30 分：時段到期自動釋放；到期前 5 分轉 LED=黃並通知。

