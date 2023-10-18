cmd_/home/gyx/Desktop/code/code7/file.mod := printf '%s\n'   file1.o file2.o | awk '!x[$$0]++ { print("/home/gyx/Desktop/code/code7/"$$0) }' > /home/gyx/Desktop/code/code7/file.mod
