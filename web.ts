/**
 * webserver.ts
 */
import { serve } from "https://deno.land/std@0.70.0/http/server.ts";

let port: number = parseInt(Deno.args[0], 10) || 8080;
console.log(`port: ${port}`);
const server = serve({ hostname: "0.0.0.0", port });
console.log(
  `HTTP webserver running.  Access it at:  http://localhost:${port}/`
);
Deno.chdir("www");

for await (const request of server) {
  try {
    let url = request.url.slice(1);
    if (!url) url = "index.html";
    console.log(url);
    let bodyContent = await Deno.readFile(url);

    // 5% chance of spontaneous failure, causing server to crash
    if (Math.floor(Math.random() * Math.floor(20)) == 0) {
      console.log("Chaos Monkey strikes again!");
      Deno.exit(-1);
    }

    request.respond({ status: 200, body: bodyContent });
  } catch (err) {
    request.respond({ status: 404 });
  }
}
