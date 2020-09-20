1. Install Deno via `make install_deno`
2. Run the buggy web server directly via `make web`. This is designed to have a 5% chance of failing on an arbitrary request. Open in a browser and refresh until the web server crashes
3. Run the process manager via `make run`
4. Open a new tab and run the reverse proxy via `make proxy`. Open the proxy address in the browser and repeatedly refresh. The proxy + pm allows us to transparently restart our buggy web server without impacting the client

Possible Steps:

1. Main.c is empty in the starter repo. The students look at the web server behavior to understand the point of a process manager.
2. The students implement enough logic to restart the single web server process. The students see that this results in sporadic HTTP request failures.
3. The students implement a pool of web server processes. This likely forces them to think about non-blocking wait calls.
