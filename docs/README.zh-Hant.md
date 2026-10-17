<div align="center">

2.2.2：外掛頁面與多語言資源獨立管理，最佳化渲染記憶體、圖片載入和資源回收，支援商店付費模型連結。
  <a href="https://bongocat.pet" target="_blank">
    <img src="../resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>
  
  <h1>
    <a href="https://bongocat.pet" target="_blank" style="text-decoration: none; color: inherit;">BongoCat</a>
  </h1>
</div>


<!-- 專案描述 + 火箭圖示 -->
<p align="center"> 
 💘C/C++ × Rust × SDL3，攪拌均勻，搗碎融合！Bong~ Bongo Cat！！！
</p>
<p align="center">
  <a href="README.en-US.md">English</a> • <a href="../README.md">简体中文</a> • <strong>繁體中文</strong> • <a href="README.fr-FR.md">Français</a> • <a href="README.de-DE.md">Deutsch</a> • <a href="README.ko-KR.md">한국어</a> • <a href="README.pt-BR.md">Português</a> • <a href="README.ru-RU.md">Русский</a> • <a href="README.es-ES.md">Español</a> • <a href="README.id-ID.md">Bahasa Indonesia</a>
</p>
> [!NOTE]
> 本倉庫（**BongoCat-X**）是 [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat) 的 fork，與 Live2D Inc. 無關，不附帶 Cubism SDK 或 Core；使用者須遵守 Live2D 授權。

<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=white"></a>
</p>


<!-- 示範影片 -->
<div align="center" style="margin-bottom: 30px;">
  <video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49
" autoplay loop style="border-radius: 8px; max-width: 800px;"></video>
</div>


> [!TIP]
> 本示範所使用的模型來自 [宇痕冫](https://space.bilibili.com/348616056)。
>
> 🎁 尋找**免費**模型？我們與才華洋溢的模型創作者合作，為您帶來豐富多樣的免費模型，同時持續探索更多有趣的桌面體驗！請瀏覽我們的官方網站：[bongocat.pet](https://bongocat.pet/models)


<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

<p align="center">
    <img src="https://count.getloli.com/@bongocat?name=bongocat&theme=booru-qualityhentais&padding=7&offset=0&align=top&scale=1&pixelated=1&darkmode=auto" width="400">
  </p>

## 💬 QQ 交流群

使用 QQ 掃描下方 QR Code，或搜尋群號 **211957388**，加入 **BongoCat-X** 交流群。

<p align="center"><img src="../resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## 📥 下載


---

<div align="center">

Copyright © 2026 - **BongoCat**\
由 vladelaina 製作\
以 ❤️ 和 ⌨️ 打造

</div>

### 動態渲染外掛

設定 → 外掛支援本地渲染器的安裝、啟用與移除。修改使用中的外掛前，請先切換至其他渲染器。Live2D 使用 C++ 外掛；Inochi2D 使用 Rust Inox2D 外掛，支援 OpenGL、Vulkan（Windows/Linux）及 Metal（macOS），不依賴 wgpu。匯入時先辨識模型內容（`.model3.json`、`.inp`、`.inx`）。所有 JSON 解析與序列化均由 Rust 處理。

Windows 可攜版以 ZIP 壓縮檔發佈，解壓後請將 `plugins` 資料夾保留在執行檔旁。封裝、離線依賴、外掛 ABI、參數綁定與相容性限制，請參閱[動態渲染外掛](model-plugins.md)。

`dev` 每次推送程式碼、資源或建置設定更新時，發佈 GitHub 夜間預發佈版本。僅修改文件時略過，沒有定時觸發。所有平台使用此次推送的提交；夜間版不取代最新正式版。

夜間預發佈固定使用 [nightly 發佈頁](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly)，更新同一標籤並替換固定名稱的附件，不再為每次建置建立新版本；建置失敗的平台保留上次產物。

### Linux Wayland / evdev

Linux Wayland 的實驗性 evdev 輸入可用 `BONGOCAT_ENABLE_EVDEV=1` 啟用。先嘗試一般使用者的唯讀存取；權限不足時，受保護的安裝會透過 `/usr/bin/sudo` 請求認證，開啟裝置後立即降權並重新執行，再初始化 SDL、設定及模型。自動提權要求程式、sudo 及父目錄歸 root 所有且群組與其他使用者不可寫；開發與可攜安裝需要管理員另行授權。可信 sudo 路徑可用 `BONGO_CAT_SUDO_EXECUTABLE` 設定。失敗會停止啟動，不修改永久權限；僅監聽啟動時已有的裝置，熱插拔需要重啟。不要直接以 root 執行或加入 `input` 群組。原始輸入可能包含密碼，鎖屏及切換工作階段不會自動暫停；隱藏寵物也不會停止監聽。

[SECURITY.md](../SECURITY.md#linux-input)

在「偏好設定 → 偏好 → 視窗」啟用「自訂圖片背景」，點選「選擇圖片」匯入 PNG、JPEG、WebP、BMP 或 GIF（靜態首幀）。圖片保持比例、置中填滿視窗，與「純色背景」互斥。圖片儲存在應用程式資料目錄，移動原始檔不影響使用；上限為 64 MB、1600 萬像素，支援 OpenGL、Vulkan 和 Metal。

Live2D 自動重用幾何緩衝與連續重複的渲染狀態；OpenGL 動態上傳、Vulkan 上傳批次與管線快取、Metal 共用緩衝槽及原生動態遮罩無需額外開關。新增路徑的效能與平台行為待實測。

Vulkan 自動管理記憶體：Live2D 預設採用 1 MiB VMA 配置區塊，明確啟用大型配置時採用 16 MiB；Windows 分層呈現只保留一個繪製目標，隱藏或最小化時等待 GPU 完成後釋放回讀快取，恢復繪製時按需重建。畫質與動態紋理解析度仍由各自設定控制。

設定 → 桌寵 → 自訂滑鼠跟隨範圍提供水平/垂直 25–400%，預設關閉，100% 保持原範圍。Mver 繪製貓爪與滑鼠可擴大移動範圍，Live2D 保留作者參數邊界。視線與視窗位置不變。

### 多按鍵顯示與字元裁剪

Mver (OpenGL/Vulkan/Metal) 可分別裁剪手部、氣泡外框和字元，將按住的按鍵以加號連接在同一個氣泡中。外框和尾巴隨模型鏡像，字元保持可讀，單鍵模式也生效。在手部圖片資料夾的 `hand-layout.json` 中設定三個裁剪區域、加號兩側間距和每側顯示數量，修改後重新啟動應用程式。詳見[設定說明與範例](hand-layout.md)。

動作彈窗會檢查動作的 `Sound` 引用及音訊檔案是否存在，僅在帶音訊的動作旁顯示獨立音量控制項。範圍為 0–100%，可拖曳、輸入數值或按兩下恢復 100%；0% 靜音。設定按模型與動作儲存，套用至彈窗播放、快捷鍵、隨機動作及右鍵選單，播放中也可調整；共用同一音訊檔案的其他動作保留各自的音量。

### 偏好選項選單樣式

「偏好」頁使用可摺疊的分組卡片，視窗、模型、應用和外觀選項預設展開。在「外觀」開啟「緊湊版面」可縮小選項間距並保留說明，設定自動儲存。

### 輸入統計外掛

Rust 統計外掛預設隨程式建置，可在「外掛」頁啟停或匯入。「統計」頁提供鍵鼠總量、每日與每小時趨勢、本機儲存及 CSV 匯出。詳見[統計外掛說明](statistics-plugin.md)。 前景應用使用時間提供排行、每小時分布及獨立開關；取樣、儲存與匯出由獨立執行緒處理。 統計入口由外掛在啟用時註冊，停用或解除安裝後自動移除。

統計頁支援自訂單位名稱及移動／滾輪換算倍率，顯示設定可儲存並還原預設值。外觀設定共用一個調色盤，可調整主色、副色、文字顏色和頁面背景顏色。

### 物品外掛

預設附帶獨立物品外掛；在「設定 → 外掛」啟用後顯示「物品」頁面。支援圖片批次匯入／拖放、多物品、位置、縮放、旋轉、透明度、前後層級、鎖定、複製和場景儲存，適用於 OpenGL、Vulkan、Metal。跟隨模型視口並非網格錨定；GIF 僅首幀，最多 16 個物品，僅作用於主桌寵。詳見 [物品外掛](items-plugin.md)。設為 `BONGO_CAT_BUILD_ITEMS=OFF` 可不附帶外掛。

渲染記憶體優先順序: 顯示記憶體優先 / 系統記憶體優先. 僅適用於 Live2D Vulkan，修改後重新載入渲染。顯示記憶體優先著重速度，系統記憶體優先減輕顯示記憶體壓力但可能降低效能。不支援的資源自動回退；其他後端自動分配，內顯使用共享記憶體。

### 鍵盤音效外掛

在「設定 → 外掛」啟用「音效」。點擊按鍵選擇音訊，懸浮顯示檔名，實際按鍵播放音效，綁定自動儲存。音效與物品各有獨立常駐工作執行緒。支援 WAV／MP3／Ogg Vorbis／FLAC，僅主桌寵啟用。詳見 [音效外掛](sounds-plugin.md)；`BONGO_CAT_BUILD_SOUNDS=OFF` 可不附帶此庫。

Rust 模型商店外掛提供預覽圖卡片、作者連結及下載／解除安裝按鈕，下載完成後驗證 SHA-256，通過後才安裝。目錄尚未發布時顯示預留狀態。詳見 [商店外掛](store-plugin.md)。

偏好 → 應用設定中的「自動檢查更新」預設開啟，每次啟動在背景檢查一次。新版本提示保留至開啟設定，在右上角提供「立即更新」按鈕；所有設定 Toast 均可點擊關閉。

模型頁將同一模型包的多個模式合併為一張卡片，可透過選擇框切換；預設模型的標準、鍵盤、手把模式也合併顯示。可一次拖入多個模型 ZIP，在背景排隊解壓縮、掃描及匯入；關閉設定視窗不會中斷匯入。結束應用程式時停止後續匯入，等待目前來源處理完成後釋放執行緒及暫存資源。

快捷鍵現整合於「偏好」頁，進入後以頂部「設定／快捷鍵」切換；社群商店整合於「模型」頁，進入後以「本機模型／商店」切換。側欄不再提供這兩個獨立入口，各子頁保留獨立捲動位置。

啟用 Live2D 外掛而缺少 Cubism Core 時，會顯示兩份官方協議。Windows 使用者點擊「同意並繼續下載」後，應用程式從官方 HTTPS 位址下載 Native R5 SDK，僅提取對應架構的動態 Core 並在本機啟用渲染；「取消並繼續」保留外掛啟用但不渲染。下載失敗可重試，也可手動匯入。應用程式及發行套件不包含 SDK 或 Core。

Windows AMD 顯示卡提供預設關閉的實驗性「DirectComposition 呈現」。DXGI Alpha 合成立即生效，啟用後將 expose 事件節流至 50 毫秒並關閉 VSync，失敗時退回原呈現器。仍使用像素讀回，無法保證遊戲維持 Independent Flip。

模型渲染外掛 ABI 已升級為 v2，請同步更新或重新建置程式與 Live2D／Inox2D 外掛；舊版外掛不相容。Live2D Vulkan 支援按需離屏資源、Rust 串流 PNG 縮放及具備 RGBA8 備援的 BC7 快取，平台限制請見 [原生渲染說明](live2d-vulkan-metal.md)。
