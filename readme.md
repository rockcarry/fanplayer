![fanplayer](logo/fanplayer.png)

fanplayer
=========

A portable video player based on ffmpeg for windows and android platform.

It is a simple and light weight player implemention without SDL or other third party library. player core codes are written in C language, KISS and easy to read.

It directly access to video and audio rendering device on specific platforms. on win32 playform, using gdi & direct3d api implements video rendering, using waveout api implements audio rendering. on android platform, using AudioTrack and ANativeWindow api for audio and video rendering.

Currently, this player can smoothly playback many video files, with low cpu and memory usage (compared with ffplay of offical ffmpeg), high performance, compatibility and stability, and the audio/video also synchronized very well.


features
========
1.  low memory and cpu usage
2.  audio/video synchronized very well
3.  gdi and direct3d rendering on win32 platform
4.  support variable speed playback 1% to 100%, 200% ..
5.  waveform and spectrum visual effect
6.  support stream selection
7.  support network media stream playback
8.  support dshow, gdigrab and vfwcap (avdevice of ffmpeg)
9.  take video snapshot, support save into jpeg and png
10. very fast (async) seek operation taking 0ms delay
11. support video rotation by ffmpeg avfilter
12. support step seek forward/backward operation
13. mediacodec hardware decoding on android
14. dxva2 hardware acceleration on windows
15. rotation for direct3d video rendering
16. support auto-reconnect for live stream playing
17. support avkcp and ffrdp protocol
18. support overlay for win32 platform
19. support drag mouse right button to select area zoom
20. support livedesk ffrdp remote control
21. support yolo-fastest detection


testplayer
==========
testplayer is a simple test player for fanplayer  
hot-keys for testplayer:  
ctrl+O    - open file  
ctrl+E    - switch visual effect  
ctrl+M    - switch between letter box and stretch rect  
ctrl+R    - switch between gdi and d3d  
ctrl+A    - switch audio stream  
ctrl+V    - switch video stream  
ctrl+S    - take a snapshot of video  
ctrl+F    - step seek forward  
ctrl+B    - step seek backward  
ctrl+up   - play speed up  
ctrl+down - play speed down  
ctrl+T    - switch speed type  
ctrl+X    - rotate video (only for d3d rendering mode)  
ctrl+C    - record current playing media to record.mp4 file  
ctrl+W    - set window size to video size  
ctrl+D    - display video definition  
ctrl+0    - restore zoom  
ctrl+L    - enter livedesk ffrdp remote control mode  
ctrl+Y    - enable/disable yolo-fastest detection  


want to learn more?
==================
want to learn more about fanplayer, please visit our wiki.

https://github.com/rockcarry/fanplayer/wiki


contact and discuss
===================
email   : rockcarry@163.com  
qq group: 383930765  



rockcarry
2016.1.7

