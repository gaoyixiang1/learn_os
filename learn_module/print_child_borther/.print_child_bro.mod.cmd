cmd_/home/gyx/Desktop/code/print_child_borther/print_child_bro.mod := printf '%s\n'   print_child_bro.o | awk '!x[$$0]++ { print("/home/gyx/Desktop/code/print_child_borther/"$$0) }' > /home/gyx/Desktop/code/print_child_borther/print_child_bro.mod