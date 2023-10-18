cmd_/home/gyx/Desktop/code/code4/mod.mod := printf '%s\n'   mod.o | awk '!x[$$0]++ { print("/home/gyx/Desktop/code/code4/"$$0) }' > /home/gyx/Desktop/code/code4/mod.mod
