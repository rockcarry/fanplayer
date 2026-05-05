编译说明：

1. win10/11 wsl 环境
2. 安装 autotools 等工具
3. 安装 i686-w64-mingw32-gcc 编译器
4. 设置 BUILD_HOST 和 CROSS_COMPILE 环境变量
5. 克隆 libavdev（我的 github 上）
6. 克隆 ffmpeg（官网版本） 

7. 执行以下命令：
source envsetup.sh
build_ffmpeg.sh
build_libavdev.sh
build_fanplayer.sh

8. _install 目录下得到构建产物


rockcarry
2026-5-5
