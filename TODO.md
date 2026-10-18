# TODO — Bug 修复清单

> 来源：静态代码审查，并与本机 Waveshare RP2350-Touch-LCD-3.49 官方示例对照。
> 本机未安装 PlatformIO，未做实际编译验证。
>
> 以下按**建议解决顺序**排列，P0 先修，P2 可最后处理。

## P0 — 高优先级

- [ ] **1. 修正 PCF85063A 内部电容选择（CAP_SEL）**
  - 位置：`PIO/src/clock.cpp:175-177`
  - 现状：
    ```cpp
    /* CTRL1: normal mode, 24-hour, 12.5 pF internal load capacitance. */
    uint8_t ctrl[2] = {0x00, 0x01};
    ```
    实际写入 `CTRL1 (0x00) = 0x00`、`CTRL2 (0x01) = 0x01`，CAP_SEL 仍为 0（7 pF）。
  - 影响：与注释和官方示例不一致。若板载晶振需要 12.5 pF，可能导致 RTC 起振不良或走时偏差。
  - 修复：改为
    ```cpp
    uint8_t ctrl[2] = {0x01, 0x00};
    ```
    或只写寄存器 `0x00 = 0x01`，并检查 I2C 写入是否成功。
  - 验收：RTC 断电重启后仍能正常起振；连续运行 24 小时与参考时间对比误差可接受。

- [ ] **2. 修复开机动画 / 启动画面被遮挡的问题**
  - 位置：
    - `PIO/lib/ui/src/screens/ui_Screen1.c:144`（boot 图创建）
    - `PIO/lib/ui/src/screens/ui_Screen1.c:152`（Moon 图创建）
    - `PIO/lib/ui/src/screens/ui_Screen1.c:17`（boot 文字创建）
    - `PIO/lib/ui/src/ui.c:82-95`（动画 3 秒、delay 1000ms）
    - `PIO/src/main.cpp:162`（启动动画）
    - `PIO/src/main.cpp:186`（1.5 秒后隐藏）
  - 问题：
    1. `ui_Screen1_Image_Moon` 在 boot 图之后创建，位于其上层；`Astronomy::begin()` 在背光打开前就设置了 Moon 图，boot 动画被完全遮住。
    2. `ui_Screen1_Label_bootscreen` 先创建，右侧容器和实时时间/日期标签后创建，实时标签会压在 boot 文字上，启动画面不干净。
    3. 动画有 1 秒 delay，但 1.5 秒后就隐藏，实际最多只显示约 0.5 秒。
  - 修复建议（二选一）：
    - 使用独立的 boot screen；或
    - 启动期间隐藏 `ui_Screen1_Container_Container` 和 `ui_Screen1_Image_Moon`，把 boot 图/文字 `lv_obj_move_foreground()`；将动画 delay 改为 0，隐藏时间改为 3 秒左右。
  - 验收：上电后能看到完整 boot 动画和文字，且不会与实时 UI 文字重叠。

- [ ] **3. `F` 翻转显示后同步翻转触摸坐标**
  - 位置：`PIO/src/main.cpp:110-113`
  - 现状：
    ```cpp
    displayFlipped = !displayFlipped;
    LCD_SetRotation(displayFlipped ? 0xA0 : 0x60);
    ```
  - 问题：`0x60` 与 `0xA0` 是相差 180° 的横屏方向。显示翻转后触摸仍使用旧方向，触摸坐标会镜像。
  - 修复：在 `F` 命令中同时翻转两个触摸轴：
    ```cpp
    Display::TouchInvertX = !Display::TouchInvertX;
    Display::TouchInvertY = !Display::TouchInvertY;
    ```
    如果当前已启用 `TouchSwapXY`，180° 翻转仍然是在交换后同时翻转 X/Y。
  - 验收：执行 `F` 后，屏幕四角和中心触摸位置都与显示一致；再次执行 `F` 恢复。

- [ ] **4. 修正触摸 `S`（交换 XY）时的裁剪顺序**
  - 位置：`PIO/src/display.cpp:52-59`
  - 现状：交换前先执行
    ```cpp
    if (raw_y >= Display::Height) raw_y = Display::Height - 1;
    ```
    如果启用 `TouchSwapXY`，`raw_y` 实际是 640 轴，会被错误裁到 171，导致屏幕大部分区域不可触摸。
  - 修复：先交换，再按最终方向裁剪：
    ```cpp
    if (Display::TouchSwapXY) {
        uint16_t t = raw_x;
        raw_x = raw_y;
        raw_y = t;
    }

    if (raw_x >= Display::Width)  raw_x = Display::Width - 1;
    if (raw_y >= Display::Height) raw_y = Display::Height - 1;
    ```
    然后再应用 `TouchInvertX/Y`。
  - 验收：执行 `S` 后，触摸能覆盖整个 640×172 屏幕，不再只响应左上角一小块区域。

## P1 — 中优先级

- [ ] **5. 增加每月天数的日期校验**
  - 位置：`PIO/src/clock.cpp:87-90`、`PIO/src/clock.cpp:282-288`
  - 问题：当前只检查 `day <= 31`，会接受 `2026-02-31` 等非法日期。界面会显示 `31 Feb 2026`，而天文计算会把它归一化为 3 月 3 日。
  - 修复：增加 `daysInMonth(year, month)`，闰年按 4/100/400 规则处理；`readRtc()` 和 `setDateTime()` 都使用同一套校验。
  - 验收：`T 2026-02-30 12:00:00`、`T 2026-04-31 12:00:00` 被拒绝；闰年 2 月 29 日可正常设置。

- [ ] **6. 将 RTC 年份范围限制到 2069**
  - 位置：`PIO/src/clock.cpp:284`、`PIO/src/clock.cpp:87-88`
  - 问题：PCF85063A 年份寄存器为 `00..99`，基年 1970，实际范围为 1970..2069。当前允许到 2099，`writeRtc()` 里的 `decToBcd(year - 1970)` 在 2070 以上会生成非法 BCD。
  - 修复：将可设置/可读取年份上限改为 2069，并同步更新 README 说明。
  - 验收：`T 2099-01-01 00:00:00` 被拒绝；`T 2069-12-31 23:59:59` 可正常写入和读回。

- [ ] **7. RTC 写入失败时支持软件 fallback 校时**
  - 位置：`PIO/src/clock.cpp:299`
  - 现状：
    ```cpp
    if (!writeRtc(dt)) return false;
    ```
  - 问题：RTC 不存在或 I2C 写失败时，`lastTime` 不会更新，串口 `T` 无法校正软件 fallback 时钟。
  - 修复建议：写 RTC 失败时仍更新 `lastTime` 并刷新 UI，同时打印“RTC 写入失败，仅设置软件时钟”；或者在文档中明确不支持无 RTC 校时。
  - 验收：断开/模拟 RTC 失败后，`T` 命令仍能改变屏幕时间（若采用软件 fallback 方案）。

- [ ] **8. 修正月球平均黄经公式**
  - 位置：`PIO/src/astronomy.cpp:132`
  - 现状：
    ```cpp
    double Lm = normalizeDegrees(N + w + v);
    ```
  - 问题：Schlyter 公式中月球平均黄经应为 `N + w + M`，`v` 是真近点角。当前会影响 `D`、`F` 摄动项。
  - 修复：
    ```cpp
    double Lm = normalizeDegrees(N + w + M);
    ```
  - 验收：与参考天文数据对比，月出/月落时间和月相在可接受误差内（简单算法通常几分钟级）。

- [ ] **9. 修正月相历元常量**
  - 位置：`PIO/src/astronomy.cpp:36`
  - 现状：
    ```cpp
    constexpr double NEW_MOON_EPOCH_JD = 2451550.1; /* 2000-01-06 18:14 UTC */
    ```
  - 问题：2000-01-06 18:14 UTC 对应 JD 约为 `2451550.26`，当前常量约早 3.8 小时，月相偏移约 0.005。
  - 修复：改为 `2451550.26`，或修正注释说明使用的是另一个历元。
  - 验收：月相图片和名称在已知新月/满月日期附近合理。

- [ ] **10. 优化天文事件计算频率**
  - 位置：`PIO/src/astronomy.cpp:258-276`
  - 问题：日出/日落/月出/月落每天只变一次，但当前每 60 秒重新执行 4 次 `findEvent()`，每次约 144 个高度计算，且使用 `double`。RP2350 只有单精度 FPU，`double` 三角函数为软件实现，可能造成周期性卡顿。
  - 修复建议：
    - 仅在 `Clock::dateChanged()` 时重算 4 个事件；
    - 每分钟只更新月相/图片；
    - 或把天文计算改用 `float`。
  - 验收：长时间运行时 UI 无明显周期性卡顿，升落时间在跨天后仍正确更新。

## P2 — 低优先级 / 健壮性

- [ ] **11. `send_cmd()` 结束前等待 PIO FIFO 排空**
  - 位置：`PIO/src/lcd_3in49.c:96-107`
  - 问题：`QSPI_Deselect()` 前没有像 `LCD_WritePixels()` 那样等待 FIFO 空，长初始化命令或 `LCD_SetRotation()` 的单字节命令理论上可能被截断。
  - 修复建议：
    ```cpp
    while (!pio_sm_is_tx_fifo_empty(qspi.pio, qspi.sm)) {
        tight_loop_contents();
    }
    busy_wait_us_32(20);
    QSPI_Deselect(qspi);
    ```
  - 验收：反复执行 `F` 翻转和上电初始化，显示方向/颜色稳定。

- [ ] **12. I2C 读取检查实际字节数**
  - 位置：`PIO/src/clock.cpp:74-75`、`PIO/src/display.cpp:38-42`
  - 问题：目前只判断返回值 `< 0`，短读可能被当成有效数据。
  - 修复：`i2c_read_blocking()` 的返回值与请求长度比较；写入控制寄存器时也检查实际写入字节数。
  - 验收：模拟 I2C 异常时不会把部分数据当作有效 RTC/触摸数据。

- [ ] **13. 处理 `QSPI_1Wrie_Mode()` 的未初始化 SM**
  - 位置：`PIO/src/qspi_pio.c:92-97`、`PIO/src/qspi_pio.c:105-109`
  - 问题：1-wire program 的加载被注释掉，但 `QSPI_1Wrie_Mode()` 仍会启用 `sm_1wire`。当前代码没有调用它，属于潜在问题。
  - 修复：恢复 1-wire program 的 `pio_add_program`/init，或删除 `QSPI_1Wrie_Mode()` 并加断言避免误用。
  - 验收：若保留 1-wire 模式，切换后命令发送正常；若不使用，编译后无未使用/误导接口。

- [ ] **14. 修正 `_ui_screen_delete()` 的判断条件**
  - 位置：`PIO/lib/ui/src/ui_helpers.c:56-63`
  - 现状：
    ```c
    if(*target == NULL) {
        lv_obj_del(*target);
        target = NULL;
    }
    ```
  - 问题：条件写反，且只修改了局部指针。当前项目未使用该函数。
  - 修复：
    ```c
    if (*target != NULL) {
        lv_obj_del(*target);
        *target = NULL;
    }
    ```

- [ ] **15. 修复 CMake/filelist 中缺失的 `ui_events.c`**
  - 位置：`PIO/lib/ui/src/CMakeLists.txt:5`、`PIO/lib/ui/src/filelist.txt`
  - 问题：两个文件都引用了 `ui_events.c`，但目录中只有 `ui_events.h`。PlatformIO 构建不受影响，CMake 构建会失败。
  - 修复：添加 `ui_events.c`，或从 CMake/filelist 中删除该条目。

- [ ] **16. 清理编译警告**
  - `PIO/src/qspi_pio.c`：`buf_temp` 未使用（约 92、112 行）。
  - `PIO/src/astronomy.cpp`：`normalizeRadians()` 未使用。
  - `PIO/src/clock.cpp`：`isAMTime` 赋值后未读取；`applyToUI()` 未使用。
  - `PIO/src/main.cpp`：`ui_imgset_moon_` extern 未使用。
  - `PIO/src/lcd_3in49.c:121`：`uint8_t percent > 100` 永远为假。
  - 修复：删除无用代码或调整类型/判断；建议开启 `-Wall -Wextra` 复查。

- [ ] **17. `B` 命令无参数时不要直接熄屏**
  - 位置：`PIO/src/main.cpp:104-109`
  - 问题：`atoi("")` 返回 0，输入 `B` 会把背光设为 0。
  - 修复：检查 `line[1]` 是否为空/非数字，无参数时打印用法或保持当前亮度。
  - 验收：输入 `B` 不会意外熄屏；`B 0` ~ `B 100` 正常。

- [ ] **18. 软件 fallback 跨天使用循环处理**
  - 位置：`PIO/src/clock.cpp:215`
  - 问题：`if (lastTime.hour >= 24)` 只处理一天；如果一次阻塞超过 24 小时，日期只前进一天。
  - 修复：改为 `while (lastTime.hour >= 24)`，或按 `elapsed` 的天数循环推进日期。
  - 验收：模拟长时间阻塞后，软件时钟日期仍然正确。

- [ ] **19. 复核月相图片索引映射**
  - 位置：`PIO/src/astronomy.cpp:289`
  - 现状：`int frame = (int)(phase * 30.0);`
  - 问题：素材中满月似乎对应 `Moon_15`，`phase=0.5` 会显示 `Moon_16`，视觉上约有半天偏差。
  - 修复建议：根据素材实际编号重新校准，例如使用 `round(phase * 29)` 或建立明确的相位到图片映射表；同时确认 boot 动画使用 `1..28` 是否覆盖预期范围。
  - 验收：新月、上弦、满月、下弦附近的图片与相位名称一致。

- [ ] **20. 固定 PlatformIO 平台版本**
  - 位置：`PIO/platformio.ini:18`
  - 现状：
    ```ini
    platform = https://github.com/maxgerhardt/platform-raspberrypi.git
    ```
  - 问题：跟随默认分支，后续上游变动可能导致构建不可复现或失败。
  - 修复：固定到某个 release tag 或 commit。
  - 验收：清理 `.pio` 后重新构建结果稳定。

## 建议的硬件验证清单

- [ ] 断电重启后 RTC 仍正常走时，CAP_SEL 修复后连续运行 24 小时误差可接受。
- [ ] 上电能看到完整 boot 动画/文字，且不与实时 UI 重叠。
- [ ] 触摸四角、中心、边缘均准确；分别验证默认、`X`、`Y`、`S`、`F` 组合。
- [ ] 执行 `F` 后触摸与显示方向一致，再次 `F` 恢复。
- [ ] 串口 `T` 设置合法/非法日期，验证拒绝逻辑；验证 2069 边界。
- [ ] 跨天时日出/日落/月出/月落和月相自动更新。
- [ ] 长时间运行（≥24 小时）无卡顿、无内存耗尽、无显示异常。
