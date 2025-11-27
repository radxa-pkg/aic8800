烧写flash：
1. GPIOB0接地，芯片上电
2. m 9 0 400C，关闭写保护
3. 依次执行下面指令，配置bootinfo，关闭counter
    f 1 1 8075000
    f 1 5 0
    f 3
4. x 8060000 1A000，再用Xmodem写入文件
5. 释放GPIOB0（不再接地），芯片重新上电
>. 此后要更新 flash，走驱动 ota 过程