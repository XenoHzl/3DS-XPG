from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
ROOT=Path(__file__).parent.resolve()
print("Put 3ds.zip in:", ROOT)
print("Serving on port 8000")
ThreadingHTTPServer(("0.0.0.0",8000),
    lambda *a,**k: SimpleHTTPRequestHandler(*a,directory=str(ROOT),**k)).serve_forever()
