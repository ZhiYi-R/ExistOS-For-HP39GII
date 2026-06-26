/**
 * @file System/Utils/sys_llapi.hpp
 * @brief C++23 zero-overhead typed facade over the C LLAPI SWI stubs.
 *
 * 每个包装都是 [[gnu::always_inline]] inline,直接转发到 extern-"C" 的 ll_* naked 桩。
 * always_inline 保证即使在 -Os 下也内联为单条 `bl ll_xxx`(而非多生成一个外联符号),
 * 因此调用点与直接调用 C 桩字节等价 —— 零额外指令、零额外符号。
 *
 * 本头**不替换** Include/llapi_code.h:那是 System 与 Bootloader 共享的 SWI 契约,
 * 且被保持为 C 的 sys_llapi.c 引用。这里只提供平行的强类型视图(std::span 携带缓冲边界、
 * 强枚举表达模式、结构体合并多出参),转发到既有 ll_* 桩。
 *
 * 仅供 C++ TU 使用(.hpp)。C 文件继续 include "sys_llapi.h"。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "sys_llapi.h" // extern "C" ll_* 声明

namespace ll {

#if defined(__GNUC__)
#define LL_FWD [[gnu::always_inline]] inline
#else
#define LL_FWD inline
#endif

// ───────── 强类型 ─────────

/// 显示刷新矩形(含端点),对应 ll_disp_put_area 的 (x0,y0,x1,y1)。
struct Rect {
    std::uint32_t x0, y0, x1, y1;
};

/// IRQ 源,镜像 Include/llapi_code.h 的 LL_IRQ_*。
enum class IrqSource : std::uint32_t {
    serial = LL_IRQ_SERIAL,
    keyboard = LL_IRQ_KEYBOARD,
    timer = LL_IRQ_TIMER,
    mmu = LL_IRQ_MMU,
};

/// ll_mem_phy_info 的返回值 + 双出参,一次性带回。
struct PhysMemInfo {
    std::uint32_t total_compressed; ///< 函数返回值
    std::uint32_t free;             ///< *free 出参
    std::uint32_t total;            ///< *total 出参
};

// ───────── 控制台 / 串口 ─────────

LL_FWD void put_char(char c) { ll_put_ch(c); }

/// ll_put_str2 形参已 const 化(见 sys_llapi.h),直接转发,无需 const_cast。
LL_FWD void put_str(std::string_view s) {
    ll_put_str2(s.data(), static_cast<std::uint32_t>(s.size()));
}

// ───────── 计时 / 休眠 ─────────

LL_FWD std::uint32_t time_us() { return ll_get_time_us(); }
LL_FWD std::uint32_t time_ms() { return ll_get_time_ms(); }
LL_FWD void vm_sleep_ms(std::uint32_t ms) { ll_vm_sleep_ms(ms); }

// ───────── 键盘 ─────────

LL_FWD std::uint32_t vm_check_key() { return ll_vm_check_key(); }
LL_FWD void set_keyboard(bool enable_report) { ll_set_keyboard(enable_report); }

// ───────── 显示 ─────────

/// vram 携带帧缓冲边界;转发取 .data() → 与直接传指针同码。area 含端点。
LL_FWD void disp_flush(std::span<std::uint8_t> vram, Rect area) {
    ll_disp_put_area(vram.data(), area.x0, area.y0, area.x1, area.y1);
}
LL_FWD void disp_set_indicator(int indicate_bit, int bat_int) {
    ll_disp_set_indicator(indicate_bit, bat_int);
}

// ───────── IRQ / 上下文 ─────────

LL_FWD void enable_irq(bool enable) { ll_enable_irq(enable); }
LL_FWD void set_irq_vector(std::uint32_t addr) { ll_set_irq_vector(addr); }
LL_FWD void set_irq_stack(std::uint32_t addr) { ll_set_irq_stack(addr); }
LL_FWD void set_svc_vector(std::uint32_t addr) { ll_set_svc_vector(addr); }
LL_FWD void set_svc_stack(std::uint32_t addr) { ll_set_svc_stack(addr); }

// ───────── CPU 频率 / 降速 ─────────

LL_FWD std::uint32_t cpu_slowdown_enable(int mode) { return ll_cpu_slowdown_enable(mode); }
LL_FWD std::uint32_t cur_freq() { return ll_get_cur_freq(); }

// ───────── 内存 ─────────

/// 合并 ll_mem_phy_info 的返回值与 (*free,*total) 双出参。
LL_FWD PhysMemInfo mem_phy_info() {
    // ll_mem_phy_info 必写两出参,无需初始化 → 与手写 ll_mem_phy_info(&a,&b) 同码(零开销)
    std::uint32_t free;
    std::uint32_t total;
    std::uint32_t comp = ll_mem_phy_info(&free, &total);
    return PhysMemInfo{comp, free, total};
}
LL_FWD float mem_comprate() { return ll_mem_comprate(); }
LL_FWD void mem_swap_enable(bool enable) { ll_mem_swap_enable(static_cast<std::uint32_t>(enable)); }
LL_FWD std::uint32_t mem_swap_size() { return ll_mem_swap_size(); }

// ───────── Flash(页缓冲边界由 span 携带,页数仍显式传) ─────────

LL_FWD int flash_page_read(std::uint32_t start_page, std::uint32_t pages,
                           std::span<std::uint8_t> buffer) {
    return ll_flash_page_read(start_page, pages, buffer.data());
}
LL_FWD int flash_page_write(std::uint32_t start_page, std::uint32_t pages,
                            std::span<const std::uint8_t> buffer) {
    return ll_flash_page_write(start_page, pages, const_cast<std::uint8_t *>(buffer.data()));
}
LL_FWD void flash_sync() { ll_flash_sync(); }
LL_FWD std::uint32_t flash_pages() { return ll_flash_get_pages(); }
LL_FWD std::uint32_t flash_page_size() { return ll_flash_get_page_size(); }

// ───────── 电源 / 传感 / RTC ─────────

LL_FWD std::uint32_t bat_voltage() { return ll_get_bat_voltage(); }
LL_FWD std::uint32_t core_temp() { return ll_get_core_temp(); }
LL_FWD std::uint32_t charge_status() { return ll_get_charge_status(); }
LL_FWD std::uint32_t rtc_get_sec() { return ll_rtc_get_sec(); }
LL_FWD void rtc_set_sec(std::uint32_t v) { ll_rtc_set_sec(v); }

// ───────── 系统 ─────────

LL_FWD void system_idle() { ll_system_idle(); }

#undef LL_FWD

} // namespace ll
