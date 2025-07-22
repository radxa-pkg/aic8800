烧写flash：
1. GPIOB0接地，芯片上电
2. m 9 0 4000，关闭写保护
3. f 1 5 0, f 3，关闭counter
4. x 8000000 19000，再用Xmodem写入文件
5. 释放GPIOB0（不再接地），芯片重新上电
>. 此后要更新 flash，走驱动 ota 过程