/**
 * webserver.ts
 */
const port: number = parseInt(Deno.env.get("PORT") ?? Deno.args[0] ?? "8080", 10);
const chaosRate: number = parseFloat(Deno.env.get("CHAOS_RATE") ?? "0.05");
const wwwDir = import.meta.dirname + "/www";

console.log(`HTTP webserver running.  Access it at:  http://localhost:${port}/`);

Deno.serve({ hostname: "0.0.0.0", port }, async (request: Request) => {
  const pathname = new URL(request.url).pathname;
  let path = pathname.slice(1) || "index.html";
  console.log(path);

  try {
    const body = await Deno.readFile(`${wwwDir}/${path}`);

    if (Math.random() < chaosRate) {
      console.log("Chaos Monkey strikes again!");
      Deno.exit(1);
    }

    return new Response(body, { status: 200 });
  } catch {
    return new Response(null, { status: 404 });
  }
});
