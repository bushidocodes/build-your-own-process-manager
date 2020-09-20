build: main.c
	gcc -o pm main.c

clean: pm
	rm -f pm

install_deno:
	curl -fsSL https://deno.land/x/install/install.sh | sh

web: 
	deno run --allow-read --allow-net web.ts 8081

run: build
	./pm

proxy:
	deno run --allow-net proxy.ts
