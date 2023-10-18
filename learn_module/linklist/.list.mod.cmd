cmd_/home/gyx/Desktop/code/code3/list.mod := printf '%s\n'   list.o | awk '!x[$$0]++ { print("/home/gyx/Desktop/code/code3/"$$0) }' > /home/gyx/Desktop/code/code3/list.mod
