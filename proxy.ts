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
  console.log(`GET localhost:${port}${request.url}`);

  let target_idx = Math.floor(Math.random() * servers.length);
  while (true) {
    target_idx = (target_idx + 1) % servers.length;
    let target_port = servers[target_idx];
    try {
      let resp = await fetch(`http://localhost:${target_port}${request.url}`);
      const headers = new Headers();
      if (resp.status == 404) {
        request.respond({ status: 404 });
        break;
      } else if (resp.status == 200) {
        if (request.url.endsWith(".png")) {
          text = new Uint8Array(await resp.arrayBuffer());
          headers.set("content-type", "image/png");
        } else if (request.url.endsWith(".css")) {
          text = await (await resp.blob()).text();
          headers.set("content-type", "text/css");
        } else {
          text = await resp.text();
        }
        console.log(`PROXY to localhost:${target_port}${request.url} SUCCESS`);
        request.respond({ status: 200, body: text, headers });
        break;
      } else {
        console.log(`Unexpected Status: ${resp.status}`);
      }
    } catch (err) {
      console.log(`PROXY to localhost:${target_port}${request.url} FAIL`);
      // console.error(err);
    }
  }
}
