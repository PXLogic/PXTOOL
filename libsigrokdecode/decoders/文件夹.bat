@echo off
REM 设置输出文件名
set "outputFile=subfolders_list.txt"

REM 使用dir命令获取当前目录下所有子文件夹名称（不含路径）
dir /b /ad > "%outputFile%"

REM 显示完成信息
echo 子文件夹名称列表已保存到 %outputFile%
pause