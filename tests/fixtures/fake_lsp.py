#!/usr/bin/env python3
# Servidor de lenguaje de mentira para las pruebas de Edita (tst_lsp).
#   - diagnósticos: «error» (gravedad 1) y «warn» (2) en cada línea; al guardar, una pista
#   - autocompletado: tres elementos (un fragmento, uno con textEdit e #import) y, tras «.», «member»
#   - hover: «**fake** hover at L:C»
# Las posiciones van en UTF-16, como pide LSP por defecto.
import json
import sys

docs = {}
saved = set()


def send(message):
    message["jsonrpc"] = "2.0"
    body = json.dumps(message).encode("utf-8")
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
    sys.stdout.buffer.flush()


def u16(text):
    return len(text.encode("utf-16-le")) // 2


def publish(uri):
    diagnostics = []
    for i, line in enumerate(docs.get(uri, "").split("\n")):
        for word, severity in (("error", 1), ("warn", 2)):
            col = line.find(word)
            if col >= 0:
                start = u16(line[:col])
                diagnostics.append({"range": {"start": {"line": i, "character": start},
                                              "end": {"line": i, "character": start + len(word)}},
                                    "severity": severity, "source": "fake", "code": word.upper(),
                                    "message": "found %s" % word})
    if uri in saved:
        diagnostics.append({"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}},
                            "severity": 4, "message": "saved"})
    send({"method": "textDocument/publishDiagnostics", "params": {"uri": uri, "diagnostics": diagnostics}})


def word_start(uri, line, character):
    text = docs.get(uri, "").split("\n")[line]
    units = 0
    start = 0
    for index, ch in enumerate(text):
        if units >= character:
            break
        if not (ch.isalnum() or ch == "_"):
            start = units + u16(ch)
        units += u16(ch)
    return start


def read_message():
    length = None
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        name, _, value = line.partition(b":")
        if name.lower() == b"content-length":
            length = int(value)
    return json.loads(sys.stdin.buffer.read(length).decode("utf-8"))


print("fake server ready", file=sys.stderr, flush=True)
while True:
    msg = read_message()
    if msg is None:
        break
    method = msg.get("method")
    params = msg.get("params") or {}
    if method == "initialize":
        send({"id": msg["id"], "result": {
            "capabilities": {"positionEncoding": "utf-16",
                             "textDocumentSync": {"openClose": True, "change": 1, "save": True},
                             "completionProvider": {"triggerCharacters": ["."]},
                             "hoverProvider": True},
            "serverInfo": {"name": "fake", "version": "1.0"}}})
    elif method == "textDocument/didOpen":
        doc = params["textDocument"]
        docs[doc["uri"]] = doc["text"]
        publish(doc["uri"])
    elif method == "textDocument/didChange":
        uri = params["textDocument"]["uri"]
        docs[uri] = params["contentChanges"][-1]["text"]
        publish(uri)
    elif method == "textDocument/didSave":
        saved.add(params["textDocument"]["uri"])
        publish(params["textDocument"]["uri"])
    elif method == "textDocument/didClose":
        docs.pop(params["textDocument"]["uri"], None)
    elif method == "textDocument/completion":
        context = params.get("context", {})
        if context.get("triggerKind") == 2 and context.get("triggerCharacter") == ".":
            items = [{"label": "member", "kind": 5}]
        else:
            pos = params["position"]
            uri = params["textDocument"]["uri"]
            start = word_start(uri, pos["line"], pos["character"])
            items = [
                {"label": "alpha", "kind": 3, "insertText": "alpha(${1:x})", "insertTextFormat": 2, "sortText": "1"},
                {"label": " beta", "kind": 6, "sortText": "2"},
                {"label": "Gamma", "kind": 7, "sortText": "3",
                 "textEdit": {"range": {"start": {"line": pos["line"], "character": start}, "end": pos},
                              "newText": "Gamma()"},
                 "additionalTextEdits": [{"range": {"start": {"line": 0, "character": 0},
                                                    "end": {"line": 0, "character": 0}},
                                          "newText": "import gamma\n"}]},
            ]
        send({"id": msg["id"], "result": {"isIncomplete": False, "items": items}})
    elif method == "textDocument/hover":
        pos = params["position"]
        send({"id": msg["id"], "result": {"contents": {"kind": "markdown",
                                                         "value": "**fake** hover at %d:%d" % (pos["line"], pos["character"])}}})
    elif method == "shutdown":
        send({"id": msg["id"], "result": None})
    elif method == "exit":
        break
    elif "id" in msg:
        send({"id": msg["id"], "error": {"code": -32601, "message": "not implemented: %s" % method}})
