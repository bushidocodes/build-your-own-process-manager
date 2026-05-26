/**
 * webserver.ts
 */
import { logger } from "./logger.ts";

const port: number = parseInt(Deno.env.get("PORT") ?? Deno.args[0] ?? "8080", 10);
const chaosRate: number = parseFloat(Deno.env.get("CHAOS_RATE") ?? "0.05");
const wwwDir = import.meta.dirname + "/www";

logger.info("HTTP webserver starting", { port, chaosRate });

Deno.serve({ hostname: "0.0.0.0", port }, async (request: Request) => {
  const pathname = new URL(request.url).pathname;
  const path = pathname.slice(1) || "index.html";

  try {
    const body = await Deno.readFile(`${wwwDir}/${path}`);

    if (Math.random() < chaosRate) {
      logger.warn("chaos monkey strike", { path });
      Deno.exit(1);
    }

    logger.info("serve", { path, status: 200 });
    return new Response(body, { status: 200 });
  } catch {
    logger.warn("not found", { path, status: 404 });
    return new Response(null, { status: 404 });
  }
});
