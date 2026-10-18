# Flix 构建入口转发器（耀坤复刻版目录整理后）
#
# 【为什么需要这个文件】
#   目录整理后，真正的构建脚本在 Firmware/Makefile。但上游 CI（.github/workflows/build.yml）
#   以及很多老习惯都是"在仓库根目录直接跑 make"。若没有本文件，根目录 make 会报
#   "No targets specified and no makefile found"，等于把上游 CI 全跑挂了。
#   所以根目录保留一个只做转发的 Makefile：把 BOARD / PORT / EXTRA 等变量原样传下去。
#
# 【效果】
#   make                      等价于 make -C Firmware
#   make BOARD=esp32:esp32:esp32s3
#   make EXTRA=--output-dir=flix/build/esp32.esp32.esp32s3.opi
#
# 注意：EXTRA 里若带相对路径，是相对 Firmware/ 解析的（如上例的 flix/build）。

.PHONY: all build upload erase monitor core libs upload_proxy cmake \
        build_simulator simulator log plot clean

all:
	$(MAKE) -C Firmware

build upload erase monitor core libs upload_proxy cmake \
build_simulator simulator log plot clean:
	$(MAKE) -C Firmware $@