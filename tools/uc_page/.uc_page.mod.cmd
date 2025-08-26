savedcmd_/home/user/PoC/tools/uc_page/uc_page.mod := printf '%s\n'   uc_page.o | awk '!x[$$0]++ { print("/home/user/PoC/tools/uc_page/"$$0) }' > /home/user/PoC/tools/uc_page/uc_page.mod
