"""Print the crashed thread of every FinceptTerminal crash report (macOS .ips)."""
import glob
import json
import os

for path in sorted(glob.glob(os.path.expanduser("~/Library/Logs/DiagnosticReports/FinceptTerminal*"))):
    print("===================", path)
    text = open(path, errors="replace").read()
    head, _, body = text.partition("\n")
    print(head[:500])
    try:
        d = json.loads(body)
    except Exception as exc:  # not the JSON format: show the start of it
        print("unparsed:", exc)
        print(text[:20000])
        continue
    print("exception:", d.get("exception"))
    print("termination:", d.get("termination"))
    print("asi:", d.get("asi"))
    for th in d.get("threads", []):
        if th.get("triggered"):
            ts = th.get("threadState", {})
            print("pc:", ts.get("pc"), "lr:", ts.get("lr"), "far:", ts.get("far"), "esr:", ts.get("esr"))
            print("x0-x3:", [ts.get("x", [{}] * 4)[i].get("value") for i in range(4)] if ts.get("x") else None)
            print("instructionByteStream:", th.get("instructionByteStream"))
    images = d.get("usedImages", [])
    for th in d.get("threads", []):
        if not th.get("triggered"):
            continue
        print("--- crashed thread:", th.get("name"), th.get("queue"))
        for fr in th.get("frames", [])[:50]:
            idx = fr.get("imageIndex", 0)
            image = images[idx].get("name", "?") if idx < len(images) else "?"
            print("   ", image, "|", fr.get("symbol", "?"), "+", fr.get("symbolLocation", ""))
