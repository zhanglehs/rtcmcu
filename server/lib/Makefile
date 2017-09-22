# this Makefile is used for build interlive extern libs

all: libjson
	@echo -e "\033[32m[INF] Make interlive lib successfully. \033[0m"
	@echo

libjson:
	@cd json; \
	if [ ! -x "configure" ]; then \
		chmod u+x configure; \
	fi; \
	if [ ! -f "config.status" ]; then \
		./configure; \
	else \
    	echo "LIBJSON: config.status file exist, skip configure."; \
	fi; \
	make

clean-libjson:
	@cd json; \
	if [ -f "Makefile" ]; then \
		make clean; \
	else \
	    echo -e "\033[33m[WRN] Can not found Makefile in json, skip clean json. \033[0m"; \
	fi;
	     
clean: clean-libjson
	@echo -e "\033[32m[INF] Clean interlive lib successfully. \033[0m"
	@echo

cleanall: clean-libjson
#	cd event; \
#	rm -f Makefile config.h config.log config.status event-config.h libtool sample/Makefile stamp-h1 test/Makefile ;
	@echo -e "\033[32m[INF] Clean interlive lib successfully. \033[0m"
	@echo   

