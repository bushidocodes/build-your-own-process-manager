build: main.c
	gcc -o pm main.c -Wall -Wextra

clean:
	rm -f pm

web:
	deno run --allow-read --allow-net --allow-env web.ts 8081

# Run one worker (Objective 1): ./pm
# Run worker pool (Objective 2): ./pm 8080 8081 8082
run: build
	./pm

proxy:
	deno run --allow-net --allow-env proxy.ts
