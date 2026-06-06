#!/bin/bash
rm -rf ~/.deno
# Remove the DENO_INSTALL export and PATH entry added by install_deno.sh
sed -i '/DENO_INSTALL/d' ~/.bash_profile 2>/dev/null || true
sed -i '/DENO_INSTALL/d' ~/.bashrc 2>/dev/null || true
