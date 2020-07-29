INC = -I"/home/pi/includes"
LIB = -L"/home/pi/includes/libs" "/home/pi/includes/libs/libsnap7.so" "/home/pi/includes/libs/libhiredis.so"

plc:
	gcc -o getplcdata getplcdata.c $(LIB) $(INC)