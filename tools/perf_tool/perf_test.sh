DEV=0000:fe:0c.0          # 改成你的 B:D:F

# (1) 冻结整箱
setpci -s $DEV 0x438.l=00000001          # frz = 1

# (2) 清零 CTR0（也可以用 rst 位）
setpci -s $DEV 0x440.l=0
setpci -s $DEV 0x444.l=0

# (3) 配置 Ctrl0 并使能
setpci -s $DEV 0x468.l=00400336          # en=1, umask=3, event=0x36

# (4) 解除冻结
setpci -s $DEV 0x438.l=00000000

