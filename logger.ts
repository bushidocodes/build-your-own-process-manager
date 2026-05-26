type Level = "DEBUG" | "INFO" | "WARN" | "ERROR";

const ORDER: Record<Level, number> = { DEBUG: 0, INFO: 1, WARN: 2, ERROR: 3 };
const minLevel = ORDER[(Deno.env.get("LOG_LEVEL") as Level) ?? "INFO"] ??
  ORDER.INFO;

function log(
  level: Level,
  msg: string,
  fields?: Record<string, unknown>,
): void {
  if (ORDER[level] < minLevel) return;
  console.log(
    JSON.stringify({ ts: new Date().toISOString(), level, msg, ...fields }),
  );
}

export const logger = {
  debug: (msg: string, fields?: Record<string, unknown>) =>
    log("DEBUG", msg, fields),
  info: (msg: string, fields?: Record<string, unknown>) =>
    log("INFO", msg, fields),
  warn: (msg: string, fields?: Record<string, unknown>) =>
    log("WARN", msg, fields),
  error: (msg: string, fields?: Record<string, unknown>) =>
    log("ERROR", msg, fields),
};
