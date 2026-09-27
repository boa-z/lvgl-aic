# D50T-2-Lite 开发串口升级

2026-09-27：核对 SDK 配置、调用链和本次 ELF/map；尚未进行实板串口传输或烧录。
板级目标始终是 d13x/d50t-2-lite。本功能属于已有 SDK Bootloader/WRI，
不需要修改驱动，也不需要在 LVGL 中实现升级协议。

## 当前配置已经支持

Bootloader：target/configs/d13x_d50t-2-lite_baremetal_bootloader_defconfig

    CONFIG_AIC_USING_UART0=y
    CONFIG_AICUPG_UART_ENABLE=y
    CONFIG_AICUPG_NAND_ARTINCHIP=y
    CONFIG_AIC_USING_WRI=y
    CONFIG_AIC_USING_WDT=y

这些项已显式启用。AICUPG_SUPPORT 默认 y，AIC_BOOTLOADER_CONSOLE_UART 默认 0，
UART0 默认 115200/8N1；AICUPG_UART_ENABLE 自动选择 AIC_UART_DRV。
UART0 pinmux 使用 PA.0/PA.1。不要在 RT-Thread 应用 defconfig 中添加只属于
Bootloader Kconfig 的 AICUPG_UART_ENABLE。

GE2D 应用：target/configs/d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d_defconfig

    CONFIG_AIC_USING_UART0=y
    CONFIG_RT_CONSOLE_DEVICE_NAME="uart0"
    CONFIG_AIC_USING_WRI=y
    CONFIG_AIC_USING_WDT=y

当前生成配置还启用了 RT_USING_FINSH、FINSH_USING_MSH、FINSH_USING_SYMTAB、
AIC_WRI_DRV 和 AIC_WDT_DRV。Shell 的 aicupg 命令由
bsp/artinchip/drv/wri/drv_wri.c 注册，gotobl 写入 BL_UPGRADE 原因并通过 WDT 重启。
本次 map 已确认应用 cmd_aicupg/命令表和 Bootloader aic_uart_upg_init 为有效链接内容。
因此目前无需为串口升级改动 defconfig。

## 开发使用路径

1. 构建完整测试镜像（脚本会先重建并打包本板 Bootloader）：

       & packages/custom/lvgl-aic/tools/sdk/build.ps1 -Phase ge2d -Jobs 8 -AllowComponentDirty

2. 若板上应用已有 aicupg，在应用串口 Shell 执行：

       aicupg gotobl

   Bootloader 会先检测 USB 主机；未检测到 USB 主机才进入 UART0 升级循环。
   要走串口时断开 OTG 数据连接，保留板供电和开发串口。
   若已经停在 Bootloader 命令行，则使用 aicupg uart 0。
   普通 aicupg（不带 gotobl）写入另一个 reboot reason，不能当成同一路径。

3. 关闭占用串口的终端，使用支持 AIC UART 帧协议的工具；不是 X/YMODEM
   “发送文件”。本机 artinchip-flash 源码 README 已列出 UART 路径，例如：

       artinchip-flash serial-list
       artinchip-flash info --uart COMx
       artinchip-flash burn <本次完整.img> --uart COMx

   COMx 替换为实际开发串口；上面仅是用法，未执行设备访问或烧录。
   工具可自动尝试 gotobl/Bootloader 命令，但板上必须先具备相应入口。
   先用默认 115200 验证，提速需要线缆/USB串口芯片和实板验证。

## 可选开发配置

仅在需要“工具等待期间复位就进入串口升级”时，可在 **Bootloader** defconfig 加：

    CONFIG_AICUPG_FORCE_UPGRADE_SUPPORT=y

源码 aicupg_detect.c 会在 UART0 发出 AIBURNFORCE 握手并等待主机 ACK。
这增加启动升级探测，不是串口升级传输本身的必要开关，本次未启用。
若需要手动打断 Bootloader，现有 AIC_CONSOLE_CTRLC_TMO 默认 30 ms；开发配置可
显式改为 1000，但会改变启动等待时间，本次保持默认。

BootROM/PBP 的 Reset + BOOT 恢复下载属于更早的启动阶段，不由应用 defconfig
控制。当前 Flash 内 Bootloader 若没有 UART 协议，应先按已有恢复流程安装
包含本板 Bootloader 的完整镜像；不能仅更新应用就获得该功能。
串口升级验收应保存镜像 SHA256、工具输出及重启后的完整日志。
