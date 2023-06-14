all: httpd

httpd: httpd.c
	gcc httpd.c -lpthread  -o httpd 

clean:
	rm httpd
