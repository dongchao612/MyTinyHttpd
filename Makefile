all: httpd

httpd: httpd.c
	# make clean
	gcc httpd.c -lpthread  -L lib -lsimplog -o httpd

clean:
	rm httpd default.log
