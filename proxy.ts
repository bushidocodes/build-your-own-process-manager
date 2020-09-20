/**
 * proxy.ts
 *
 * Accepts connections at localhost:1337 and reverse proxies to a random endpoint from a pool of worker
 * at localhost:8080, localhost:8081, and localhost:8082
 */
import { serve } from "https://deno.land/std@0.70.0/http/server.ts";

let port = 1337;
const server = serve({ hostname: "0.0.0.0", port });
console.log(
  `HTTP webserver running.  Access it at:  http://localhost:${port}/`
);

let servers = [8080, 8081, 8082];

for await (const request of server) {
  let text: string | Uint8Array = "";
  console.log(request.url);

  while (true) {
    let port = servers[Math.floor(Math.random() * servers.length)];
    try {
      let resp = await fetch(`http://localhost:${port}${request.url}`);
      if (request.url.endsWith(".png")) {
        text = new Uint8Array(await resp.arrayBuffer());
        const headers = new Headers();
        headers.set("content-type", "image/png");

        request.respond({ status: 200, body: text, headers });
      } else if (request.url.endsWith(".css")) {
        text = await (await resp.blob()).text();
        const headers = new Headers();
        headers.set("content-type", "text/css");

        request.respond({ status: 200, body: text, headers });
      } else {
        text = await resp.text();
        request.respond({ status: 200, body: text });
      }
      break;
    } catch (err) {
      console.error(err);
    }
  }
}
