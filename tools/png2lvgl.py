#!/usr/bin/env python3
"""
png2lvgl.py - PNG 转 LVGL v9 C 数组 (LV_COLOR_FORMAT_ARGB8888)

用法:
    python3 tools/png2lvgl.py <输入.png> <输出名> [输出目录]

示例:
    python3 tools/png2lvgl.py apps/demo1/res/image/back.png.png back apps/demo1/page/
    生成: <输出目录>/back.c 和 back.h, 编译进程序后:
        lv_image_set_src(img, &back);

为什么不用运行时加载 PNG:
    - 板上没有文件系统驱动 (LV_USE_FS_POSIX=0) 和 PNG 解码器 (LV_USE_LODEPNG=0)
    - 改 lv_conf.h 会触发 LVGL 全量重编 (500 个文件, 很慢)
    - C 数组零解码开销, 且字节序 (B,G,R,A) 与 fb 的 ARGB8888 一致, 显示零转换
"""
import sys
import os
from PIL import Image

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    src = sys.argv[1]
    name = sys.argv[2]
    outdir = sys.argv[3] if len(sys.argv) > 3 else "."

    img = Image.open(src).convert("RGBA")
    w, h = img.size
    px = img.tobytes()  # 每像素 R,G,B,A

    # LVGL ARGB8888 (小端 uint32 = A<<24|R<<16|G<<8|B) 的字节流是 B,G,R,A
    out = bytearray()
    for i in range(0, len(px), 4):
        r, g, b, a = px[i], px[i + 1], px[i + 2], px[i + 3]
        out += bytes((b, g, r, a))

    data_size = w * h * 4
    print(f"[png2lvgl] {src}: {w}x{h}, RGBA, {data_size} bytes -> {name}.c/.h")

    with open(os.path.join(outdir, name + ".c"), "w") as f:
        f.write(f'#include "lvgl.h"\n\n')
        f.write(f'const lv_image_dsc_t {name} = {{\n')
        f.write(f'    .header.magic = LV_IMAGE_HEADER_MAGIC,\n')
        f.write(f'    .header.cf = LV_COLOR_FORMAT_ARGB8888,\n')
        f.write(f'    .header.w = {w},\n')
        f.write(f'    .header.h = {h},\n')
        f.write(f'    .data_size = {data_size},\n')
        f.write(f'    .data = {{\n')
        for i in range(0, len(out), 12):
            chunk = out[i:i + 12]
            f.write('        ' + ','.join(f'0x{b:02x}' for b in chunk) + ',\n')
        f.write(f'    }},\n')
        f.write(f'}};\n')

    with open(os.path.join(outdir, name + ".h"), "w") as f:
        f.write('#ifndef LV_IMG_%s_H\n' % name.upper())
        f.write('#define LV_IMG_%s_H\n\n' % name.upper())
        f.write('#include "lvgl.h"\n\n')
        f.write(f'extern const lv_image_dsc_t {name};\n\n')
        f.write('#endif\n')

    print(f"[png2lvgl] done: {os.path.join(outdir, name + '.c')} (+{name}.h)")

if __name__ == "__main__":
    main()
