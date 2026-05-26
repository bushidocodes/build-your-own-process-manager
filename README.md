## Practical Linux Systems Programming with the Process API

### GWU CSCI 3411 - Fall 2020 - Lab 4

---

## Build your own Process manager

Today, you are going to implement your own process manager from scratch using C
and the Process APIs presented by POSIX, such as `fork`, `exec`, and `wait`.

## Install Deno

Run `source install_deno` to install the Deno runtime. This is a
JavaScript/TypeScript runtime that is evolved from Node.js.

Running the following

```sh
deno --version
```

Should print results similar to the following:

```
sean@MANPUTER:~/projects/process-manager$ deno --version
deno 1.4.1
v8 8.7.75
typescript 4.0.2
```

Note: You are NOT expected to know TypeScript and you will not modify these
scripts. However, the syntax is similar to Java, so you likely can get a rough
idea what they do

## Investigating the Problematic App

Our buggy app is one of the most mission-critical things on the web: Gabe
Parmer's website. If the website goes down, potential PhDs are unable to learn
about Gabe's research and consider applying to join his lab.

Open the `www` directory and look at the contents. This contains the HTML, CSS,
and image for Gabe's site.

These static assets need to be served by a web server, and the buggy web server
is written in TypeScript

Open the `web.ts` script, and notice the following:

The port is configurable via an argument to the script, defaulting to 8080 if
not provided.

```ts
let port: number = parseInt(Deno.args[0], 10) || 8080;
```

The request handler logic takes a random number from 0 to 19, crashing the
program if it is 0. This gives us our 5% failure rate.

```ts
// 5% chance of spontaneous failure, causing server to crash
if (Math.floor(Math.random() * Math.floor(20)) == 0) {
  console.log("Chaos Monkey strikes again!");
  Deno.exit(-1);
}
```

Let's first take a look at the Makefile to see what executing a Deno script
looks like.

```Makefile
web:
	deno run --allow-read --allow-net web.ts 8081
```

The Makefile rule `web` tells `deno` to `run` the script `web.ts`, passing
`8081` as an argument. As we saw above, this is used to set the port the web
server will serve Gabe's website from. The `--allow-read` and `--allow-net` are
scoped permissions that allow Deno script to read (but not write) from the
filesystem and to bind to our network interfaces.

Let's run this.

```sh
make web
```

should yield the following

```sh
sean@MANPUTER:~/projects/process-manager$ make web
deno run --allow-read --allow-net web.ts 8081
port: 8081
HTTP webserver running.  Access it at:  http://localhost:8081/
```

Since you are running from the VSCode terminal, the Remote extensions
automatically proxy from the WSL backend / Multipass VM. This means that you can
hold down control and left click on the URL in the console as if it were a
hyperlink, and you'll open Gabe's webpage in your default browser.

_Note: If you are running WSL, the actual port that the link open may be
slightly different. Be sure to click the link in the console rather than
copy/pasting text into your URL bar._

Your default browser should load Gabe's web page. Hit refresh until the website
doesn't load properly.

Go back to your terminal. You should see the following!

```sh
<snip>
gp.png
index.html
index.css
gp.png
Chaos Monkey strikes again!
Makefile:11: recipe for target 'web' failed
make: *** [web] Error 255
```

Note: the Chaos Monkey is the mascot of the discipline software engineering
[Chaos Engineering](https://en.wikipedia.org/wiki/Chaos_engineering). I suggest
saving this link and reading up on this later.

## Objective 1: Auto-respawn

The first objective is to create a process manager that launches the Deno web
server and relaunches it whenever the web server exits!

You should implement this in `main.c`. The program should invoke the deno script
just as in the `web` rule in the Makefile.

Running `make build` will compile your process manager.

Running `make run` will compile and execute your process manager.

Hints:

- Read the manual pages for `fork`, `exec`, and `wait`
- The arguments probably need to be split into an array of NULL-terminated
  tokens. The first element should be the relative path used to call the
  runtime. The last element should be NULL. This exact syntax might vary
  depending on the `exec` variant you use
  ```c
  char *args[] = {"./deno", "run", "--allow-read", "--allow-net", "web.ts", 8081, NULL};
  ```
- If you are printing to console from a child process, you might need to use
  `fflush` to make sure the text is printed before the process terminates

#### Reference Architecture

```
__________________________
|       deno script      |
|         :8081          |
''''''''''''''''''''''''''
__________________________
|           PM           |
''''''''''''''''''''''''''
```

Success Criteria: If the chaos monkey strikes and your process terminates, your
process manager should automatically restart the process. This should be at the
same IP address, such that once can refresh hitting the website over and over.
Specific requests might sporadically fail, but the website is never fully down.

## Objective 2: Pool of 3 Workers

Enhance your process to spawn a pool of three workers. This allows request to
automatically fail over to another worker if one fails. That makes the error
transparent to the client.

We provide a proxy server called `web.ts` to help redirect traffic across your
pool of workers.

Open this file and notice the following:

1. The script is hardcoded to balance between webservers running on 8080, 8081,
   and 8082. This means that you need to make sure that these URLs are used
   exactly:

   ```ts
   let servers = [8080, 8081, 8082];
   ```

2. The proxy round robins between the three web servers

   ```ts
   let target_idx = Math.floor(Math.random() * servers.length);
   while (true) {
       target_idx = (target_idx + 1) % servers.length;
       let target_port = servers[target_idx];
       <snip>
   }
   ```

3. If the proxy is successful and `request.respond` is executed, the proxy
   breaks the loop. Otherwise, it round robins to the next port until
   successful.
   ```ts
   request.respond({ status: 200, body: text, headers });
   break;
   ```

You can run the proxy with your process manager.

Run `make run` to start your process manager.

Open a second tab and run `make proxy`.

Notice that the proxy attempt to load balance between ports 8080, 8081, and
8082, but because you're only running a server on 8081, it ends up failing over
to 8081 on each request.

```sh
sean@MANPUTER:~/projects/process-manager$ make proxy
deno run --allow-net proxy.ts
HTTP webserver running.  Access it at:  http://localhost:1337/
GET localhost:1337/
PROXY to localhost:8082/ FAIL
PROXY to localhost:8080/ FAIL
PROXY to localhost:8081/ SUCCESS
GET localhost:1337/index.css
PROXY to localhost:8082/index.css FAIL
PROXY to localhost:8080/index.css FAIL
PROXY to localhost:8081/index.css SUCCESS
GET localhost:1337/gp.png
PROXY to localhost:8082/gp.png FAIL
PROXY to localhost:8080/gp.png FAIL
PROXY to localhost:8081/gp.png SUCCESS
```

You now have all the pieces that you need to enhance your process manager to
spawn and monitor a pool of workers.

```
                _______________
                |    Proxy    |
                |    :1337    |
                '''''''''''''''
                       |
        v--------------v---------------v
_______________ _______________ _______________
| deno script | | deno script | | deno script |
|    :8080    | |    :8081    | |    :8082    |
''''''''''''''' ''''''''''''''' '''''''''''''''
_______________________________________________
|                     PM                      |
'''''''''''''''''''''''''''''''''''''''''''''''
```

Hints:

- You need a way to know which the IP address of the node that failed. This
  probably needs to be maintained in some sort of mapping from pid to IP
- You thus need to know the pid of the child process that failed. One of the
  members of the `wait` family of APIs is better for this than others.
- Is the `wait` call you chose blocking or nonblocking? Which should it be?
- What does `WNOHANG` do?

## Cleanup

If you want to remove deno, run the following script

`./remove_deno.sh`

And then open `~/.bash_profile` and remove the following lines:

```bash
export DENO_INSTALL="$HOME/.deno"
export PATH="$PATH:$DENO_INSTALL/bin"
```

## Reference Solution

A reference implementation of `main.c` is provided in encoded form under
`solution/main.c.b64`.

**Only look at this after you have made a genuine attempt at the lab.**

To decode and inspect it:

```sh
base64 -d solution/main.c.b64 > /tmp/solution.c
cat /tmp/solution.c
```

To decode and build it directly:

```sh
base64 -d solution/main.c.b64 > main.c
make build
```
