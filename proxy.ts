/**
 * proxy.ts
 *
 * Accepts connections at localhost:1337 and reverse proxies to a random endpoint from a pool of workers
 * at localhost:8080, localhost:8081, and localhost:8082
 */
const proxyPort: number = parseInt(Deno.env.get("PROXY_PORT") ?? "1337", 10);
const workers: number[] = (Deno.env.get("WORKER_PORTS") ?? "8080,8081,8082")
  .split(",")
  .map((p) => parseInt(p.trim(), 10));
const BACKEND_TIMEOUT_MS = 500;

console.log(`HTTP proxy running.  Access it at:  http://localhost:${proxyPort}/`);

Deno.serve({ hostname: "0.0.0.0", port: proxyPort }, async (request: Request) => {
  const pathname = new URL(request.url).pathname;
  console.log(`GET localhost:${proxyPort}${pathname}`);

  let targetIdx = Math.floor(Math.random() * workers.length);
  for (let attempt = 0; attempt < workers.length; attempt++) {
    targetIdx = (targetIdx + 1) % workers.length;
    const targetPort = workers[targetIdx];
    try {
      const ac = new AbortController();
      const timer = setTimeout(() => ac.abort(), BACKEND_TIMEOUT_MS);
      const resp = await fetch(`http://localhost:${targetPort}${pathname}`, { signal: ac.signal });
      clearTimeout(timer);
      if (resp.status === 404) {
        return new Response(null, { status: 404 });
      }
      if (resp.status === 200) {
        console.log(`PROXY to localhost:${targetPort}${pathname} SUCCESS`);
        return new Response(resp.body, { status: 200, headers: resp.headers });
      }
      console.log(`Unexpected status: ${resp.status}`);
    } catch {
      console.log(`PROXY to localhost:${targetPort}${pathname} FAIL`);
    }
  }

  return new Response("Bad Gateway", { status: 502 });
});
