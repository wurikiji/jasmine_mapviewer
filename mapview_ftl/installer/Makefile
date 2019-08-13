
ALL : installer

installer:	installer-linux.c
	gcc -Wall -W -I../target_spw $< -o $@ -I../include   -I../ftl_greedy
clean:
	rm -rf installer
