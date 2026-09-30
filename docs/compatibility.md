# Compatibility

Fixed baseline: LVGL v9.6.0 (80ca777e37a2b176770726a02e07a6fb79ef0b39),
Luban-Lite v1.3.2 reference (c5807f9e7d18292f920dafaa018b8174635085c4),
D13x/D133ECS with RT-Thread. Other SoCs are not certified. The guard accepts
9.6.x; upgrading the upstream pin still requires validation.

Display/touch use public LVGL entry points. Decoder and GE2D require private
decoder/task/unit definitions through compat/lvgl_aic_private.h. Do not describe
them as private-API independent. Run production-source contracts and target
builds on upgrades, followed by relevant board regression.

Targets select LV_OS_CUSTOM with compat/lv_aic_rtthread_os.h; host builds use
LV_OS_NONE except isolated OS contracts. The RT event adapter avoids SDK kernel
patches. Touch uses OSAL and the RT device interface. RT_USING_EVENT, RT-Thread
and MPP interfaces are required; disable LPKG_USING_LVGL in the application.

Earlier D50T images have software-display/touch, MPP and incremental GE2D
observations. The 2026-09-30 application-owned integration passed six host
contracts and software/MPP/GE2D build/image gates; its board regression is
pending. Mocks do not prove GE pixels, cache coherency, IRQ wakeups or speed.
The solid-fill candidate adds a seventh production-source host contract and
board numeric probes. The resource stage adds an eighth contract covering
compressed memory inputs and cache lifetime, plus board parity/cache probes.
Hardware acceptance for both new capabilities is still pending.
See [capabilities](capabilities.md), [validation](validation.md) and
[transform gates](phase3c-transform.md).
