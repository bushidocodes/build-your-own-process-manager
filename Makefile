build: main.c
	gcc -o pm main.c

clean: pm
	rm -f pm

install_deno:
	sudo apt-get install unzip
	curl -fsSL https://deno.land/x/install/install.sh | sh
	export DENO_INSTALL="$HOME/.deno"
  	export PATH="$DENO_INSTALL/bin:$PATH"

web: 
	deno run --allow-read --allow-net web.ts 8081

run: build
	./pm

proxy:
	deno run --allow-net proxy.ts

remove_deno:
	rm -rf ~/.deno
