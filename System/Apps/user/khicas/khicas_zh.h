/**
 * @file System/Apps/user/khicas/khicas_zh.h
 * @brief Seed of Chinese characters for the KhiCAS glyph table (UTF-8).
 *
 * The KhiCAS text path renders CJK via the system glyph table, which only
 * carries the code points that Scripts/gen_cjk_font.py finds in its --source
 * files. This header is one such source (wired in System/CMakeLists.txt): every
 * Han character appearing here is rasterized into cjk_font_data.h and becomes
 * drawable in KhiCAS.
 *
 * Until the GUI is compiled from source (spike, 阶段 1) and its menu/help
 * literals are translated in place (阶段 2), this file seeds the characters the
 * localized UI will need so the glyphs exist ahead of the strings that use them.
 *
 * NOTE: scanned for code points only — content need not be valid C beyond being
 * a compilable (here, comment-only) header.
 *
 * Category labels and common menu chrome (verified in 阶段 2 against kdisplay.cc):
 *   代数 微积分 矩阵 函数 图形 编程 几何 概率 实数 复数 多项式 方程 求解
 *   帮助 设置 退出 确定 取消 返回 清除 保存 变量 列表 单位 常量 转换 简化
 *   因式分解 展开 积分 求导 极限 级数 行列式 逆 转置 排序 绘图 中文 测试
 */
#ifndef KHICAS_ZH_H
#define KHICAS_ZH_H

#endif /* KHICAS_ZH_H */
