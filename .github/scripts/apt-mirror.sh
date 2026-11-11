#!/usr/bin/env bash
# Los runners de Ubuntu instalan desde una lista de mirrors (mirror+file:/etc/apt/apt-mirrors.txt)
# que empieza por azure.archive.ubuntu.com. Cuando ese nombre no resuelve, apt falla aunque los
# demás funcionen. Esto usa archive.ubuntu.com directamente, con reintentos, y deja en el
# registro qué resuelve el runner, por si vuelve a fallar.
set -u
getent ahostsv4 azure.archive.ubuntu.com archive.ubuntu.com security.ubuntu.com | awk '{print $1, $3}' | sort -u || true
for file in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
    [[ -f $file ]] || continue
    sudo sed -i -e 's|mirror+file:/etc/apt/apt-mirrors.txt|http://archive.ubuntu.com/ubuntu/|g' \
        -e 's|https\?://azure.archive.ubuntu.com/ubuntu/\?|http://archive.ubuntu.com/ubuntu/|g' "$file"
done
# Si el DNS del runner no resuelve los de Ubuntu (pasó el 2026-11-11, con el resto de internet
# bien), sus IP por DNS sobre HTTPS, en /etc/hosts
for host in archive.ubuntu.com security.ubuntu.com; do
    getent ahostsv4 "$host" >/dev/null && continue
    ip=$(curl -fsS --max-time 15 -H 'accept: application/dns-json' \
         "https://cloudflare-dns.com/dns-query?name=$host&type=A" \
         | python3 -c 'import json,sys; print(next(a["data"] for a in json.load(sys.stdin)["Answer"] if a["type"] == 1))') || continue
    echo "$host no resuelve: $ip por DNS sobre HTTPS"
    echo "$ip $host" | sudo tee -a /etc/hosts >/dev/null
done
echo 'Acquire::Retries "5";' | sudo tee /etc/apt/apt.conf.d/80-retries >/dev/null
