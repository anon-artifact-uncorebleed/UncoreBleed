savedcmd_/home/user/PoC/tools/ptw/ptw.mod := printf '%s\n'   ptw.o | awk '!x[$$0]++ { print("/home/user/PoC/tools/ptw/"$$0) }' > /home/user/PoC/tools/ptw/ptw.mod
