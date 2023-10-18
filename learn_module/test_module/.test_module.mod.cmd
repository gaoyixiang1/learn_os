cmd_/home/gyx/Desktop/code/code1/test_module.mod := printf '%s\n'   test_module.o | awk '!x[$$0]++ { print("/home/gyx/Desktop/code/code1/"$$0) }' > /home/gyx/Desktop/code/code1/test_module.mod
