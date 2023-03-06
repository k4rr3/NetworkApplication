LDFLAGS= -ansi -pedantic -Wall -std=c17
SOLARISLD= -lnsl -lsocket

all: 
	@echo "Per compilar executar: "
	@echo "		make sistema	"
	@echo "On sistema es un de: linux,solaris,altres"


linux: client.c
	gcc client.c -o client -DLINUX $(LDFLAGS)
	
solaris: client.c
	gcc client.c -o client -DSOLARIS $(LDFLAGS) $(SOLARISLD)
	
altres: client.c
	gcc client.c -o client $(LDFLAGS)


clean:
	-rm -fr client
