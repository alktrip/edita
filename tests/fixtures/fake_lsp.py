#!/usr/bin/env python3
# Servidor de lenguaje de mentira para las pruebas de Edita (tst_lsp).
#   - diagnósticos: «error» (gravedad 1) y «warn» (2) en cada línea; al guardar, una pista
#   - autocompletado: tres elementos (un fragmento, uno con textEdit e #import) y, tras «.», «member»
#   - hover: «**fake** hover at L:C»
#   - definición («def nombre»), referencias y renombrar (la palabra entera, en los documentos
#     abiertos y en los .py de la carpeta del proyecto), formato («x=1» → «x = 1») y firmas
# Las posiciones van en UTF-16, como pide LSP por defecto.
import json
import os
import re
import sys

docs = {}
saved = set()
root = ""


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


def word_at(uri, line, character):
    text = docs[uri].split("\n")[line]
    for m in re.finditer(r"\w+", text):
        if u16(text[:m.start()]) <= character <= u16(text[:m.end()]):
            return m.group(0)
    return ""


def all_texts():
    texts = dict(docs)
    for name in sorted(os.listdir(root)) if root else []:
        path = os.path.join(root, name)
        uri = "file://" + path
        if name.endswith(".py") and uri not in texts:
            with open(path, encoding="utf-8") as f:
                texts[uri] = f.read()
    return texts


def occurrences(word):
    found = []
    for uri, text in sorted(all_texts().items()):
        for i, line in enumerate(text.split("\n")):
            for m in re.finditer(r"\b%s\b" % re.escape(word), line):
                start = u16(line[:m.start()])
                found.append((uri, {"start": {"line": i, "character": start},
                                    "end": {"line": i, "character": start + u16(word)}}))
    return found


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
        root = params.get("rootPath") or ""
        send({"id": msg["id"], "result": {
            "capabilities": {"positionEncoding": "utf-16",
                             "textDocumentSync": {"openClose": True, "change": 1, "save": True},
                             "completionProvider": {"triggerCharacters": ["."]},
                             "hoverProvider": True, "definitionProvider": True, "referencesProvider": True,
                             "renameProvider": True, "documentFormattingProvider": True,
                             "signatureHelpProvider": {"triggerCharacters": ["(", ","]}},
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
    elif method == "textDocument/definition":
        pos = params["position"]
        word = word_at(params["textDocument"]["uri"], pos["line"], pos["character"])
        result = []
        for uri, rng in occurrences(word):
            line = all_texts()[uri].split("\n")[rng["start"]["line"]]
            if line.lstrip().startswith("def ") or line.lstrip().startswith("class ") or line.lstrip().startswith("multi ="):
                result.append({"targetUri": uri, "targetRange": rng, "targetSelectionRange": rng})
        send({"id": msg["id"], "result": result or None})
    elif method == "textDocument/references":
        pos = params["position"]
        word = word_at(params["textDocument"]["uri"], pos["line"], pos["character"])
        send({"id": msg["id"], "result": [{"uri": uri, "range": rng} for uri, rng in occurrences(word)]})
    elif method == "textDocument/rename":
        pos = params["position"]
        word = word_at(params["textDocument"]["uri"], pos["line"], pos["character"])
        changes = {}
        for uri, rng in occurrences(word):
            changes.setdefault(uri, []).append({"range": rng, "newText": params["newName"]})
        send({"id": msg["id"], "result": {"documentChanges": [
            {"textDocument": {"uri": uri, "version": None}, "edits": edits} for uri, edits in changes.items()]}})
    elif method == "textDocument/formatting":
        edits = []
        for i, line in enumerate(docs[params["textDocument"]["uri"]].split("\n")):
            for m in re.finditer(r"(?<=\w)=(?=\w)", line):
                col = u16(line[:m.start()])
                edits.append({"range": {"start": {"line": i, "character": col}, "end": {"line": i, "character": col + 1}},
                              "newText": " = "})
        send({"id": msg["id"], "result": edits})
    elif method == "textDocument/signatureHelp":
        pos = params["position"]
        line = docs[params["textDocument"]["uri"]].split("\n")[pos["line"]]
        before = line[:pos["character"]]
        m = re.search(r"(\w+)\(([^()]*)$", before)
        if not m:
            send({"id": msg["id"], "result": None})
        else:
            name = m.group(1)
            label = "%s(a, b)" % name
            send({"id": msg["id"], "result": {
                "signatures": [{"label": label, "documentation": "Does things.\n\nMore details.",
                                "parameters": [{"label": [len(name) + 1, len(name) + 2]}, {"label": "b"}]},
                               {"label": "%s(a, b, c)" % name, "parameters": [{"label": "a"}, {"label": "b"}, {"label": "c"}]}],
                "activeSignature": 0, "activeParameter": m.group(2).count(",")}})
    elif method == "shutdown":
        send({"id": msg["id"], "result": None})
    elif method == "exit":
        break
    elif "id" in msg:
        send({"id": msg["id"], "error": {"code": -32601, "message": "not implemented: %s" % method}})
